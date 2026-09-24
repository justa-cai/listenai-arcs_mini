#include <math.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "HTTPCUsr_api.h"
#include "lisa_mem.h"
#include "mbedtls/md5.h"
#include "mcp.h"
#include "miniapp.h"
#include "mcp_miniapp_result.h"

#define INSTALL_NAME "ls.built_in.miniapp_install"
#define INSTALL_CALL_ID_MAX 127
#define DEVICE_CONTROL_NAME "ls.built_in.device_control"
#define BRIGHTNESS_CONTROL_NAME "ls.display_set_brightness"
#define VOLUME_CONTROL_NAME "ls.set_volume"
#define DUPLEX_SWITCH_NAME "ls.built_in.switch_full_duplex_v2"

bool app_mcp_tool_call_allowed(const char *name)
{
    return !miniapp_is_active() || (name &&
           (strcmp(name, INSTALL_NAME) == 0 ||
            strcmp(name, "ls.built_in.miniapp_exit") == 0 ||
            strcmp(name, "ls.built_in.get_device_capabilities") == 0 ||
            strcmp(name, DEVICE_CONTROL_NAME) == 0 ||
            strcmp(name, BRIGHTNESS_CONTROL_NAME) == 0 ||
            strcmp(name, VOLUME_CONTROL_NAME) == 0 ||
            strcmp(name, DUPLEX_SWITCH_NAME) == 0));
}

typedef struct {
    miniapp_package_t package;
    char call_id[INSTALL_CALL_ID_MAX + 1];
    char url[HTTP_CLIENT_MAX_URL_LENGTH];
} install_job_t;

static cJSON *miniapp_install_list(const char *name)
{
    (void)name;
    cJSON *tool = cJSON_Parse(
        "{\"name\":\"" INSTALL_NAME "\","
        "\"description\":\"下载并校验小应用 Lua 文件，启动成功后返回执行结果\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"id\":{\"type\":\"string\",\"description\":\"小应用标识\"},"
        "\"version\":{\"type\":\"string\",\"description\":\"不可变版本标识\"},"
        "\"name\":{\"type\":\"string\",\"description\":\"小应用名称\"},"
        "\"url\":{\"type\":\"string\",\"description\":\"有效期300秒的裸Lua签名下载地址\"},"
        "\"size\":{\"type\":\"integer\",\"minimum\":1,\"description\":\"准确文件字节数\"},"
        "\"hash\":{\"type\":\"string\",\"pattern\":\"^[0-9a-f]{32}$\",\"description\":\"Lua文件MD5\"}},"
        "\"required\":[\"id\",\"version\",\"name\",\"url\",\"size\",\"hash\"]}}");
    if (!tool) {
        return NULL;
    }
    cJSON *properties = mcp_tool_info_properties_get(tool);
    const struct { const char *key; int max; } limits[] = {
        {"id", MINIAPP_ID_MAX}, {"version", MINIAPP_VERSION_MAX},
        {"url", HTTP_CLIENT_MAX_URL_LENGTH - 1},
    };
    for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        cJSON *property = cJSON_GetObjectItemCaseSensitive(properties, limits[i].key);
        if (!cJSON_AddNumberToObject(property, "minLength", 1) ||
            !cJSON_AddNumberToObject(property, "maxLength", limits[i].max)) {
            cJSON_Delete(tool);
            return NULL;
        }
    }
    if (!cJSON_AddNumberToObject(cJSON_GetObjectItemCaseSensitive(properties, "size"),
                                 "maximum", MINIAPP_SOURCE_MAX)) {
        cJSON_Delete(tool);
        return NULL;
    }
    return tool;
}

static bool copy_string(cJSON *args, const char *key, char *out, size_t capacity)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(args, key);
    if (!cJSON_IsString(value) || !value->valuestring || !value->valuestring[0]) {
        return false;
    }
    size_t length = strlen(value->valuestring);
    if (length >= capacity) {
        return false;
    }
    memcpy(out, value->valuestring, length + 1);
    return true;
}

/* Use the underlying HTTP client for bounded reads of both fixed-length and
 * chunked bodies. The convenience chunk API only accepts chunked responses. */
static int download_once(const install_job_t *job, char *source, char *error, size_t error_size)
{
    HTTPParameters *http = lisa_mem_calloc(1, sizeof(*http));
    if (!http) {
        snprintf(error, error_size, "not enough memory for HTTP");
        return -1;
    }
    memcpy(http->Uri, job->url, strlen(job->url) + 1);
    http->HttpVerb = VerbGet;
    http->nTimeout = 15;
    bool opened = HTTPC_open(http) == 0;
    int result = -1;
    HTTP_CLIENT info = {0};
    size_t received = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(60000);
    snprintf(error, error_size, "download failed");
    if (!opened || HTTPC_request(http, NULL) != 0 ||
        HTTPC_get_request_info(http, &info) != 0 || info.HTTPStatusCode != 200) {
        goto done;
    }
    if (info.TotalResponseBodyLength && info.TotalResponseBodyLength != job->package.size) {
        snprintf(error, error_size, "size mismatch");
        goto done;
    }
    for (;;) {
        UINT32 count = 0;
        size_t capacity = job->package.size + 1u - received;
        if (capacity > 1024) {
            capacity = 1024;
        }
        if (!capacity || (int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            snprintf(error, error_size, capacity ? "download timed out" : "size mismatch");
            goto done;
        }
        int rc = HTTPC_read(http, source + received, capacity, &count);
        if (count > capacity) {
            goto done;
        }
        received += count;
        if (received > job->package.size) {
            snprintf(error, error_size, "size mismatch");
            goto done;
        }
        if (rc == HTTP_CLIENT_EOS || (rc == 0 && count == 0)) {
            break;
        }
        if (rc != 0) {
            goto done;
        }
    }
    if (received != job->package.size) {
        snprintf(error, error_size, "size mismatch");
        goto done;
    }
    uint8_t digest[16];
    char hex[33];
    static const char digits[] = "0123456789abcdef";
    mbedtls_md5((const uint8_t *)source, received, digest);
    for (size_t i = 0; i < sizeof(digest); ++i) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }
    hex[32] = '\0';
    if (strcmp(hex, job->package.hash) != 0) {
        snprintf(error, error_size, "hash mismatch");
        goto done;
    }
    source[received] = '\0';
    result = 0;
done:
    if (opened) {
        HTTPC_close(http);
    }
    lisa_mem_free(http);
    return result;
}

static void install_worker(void *argument)
{
    install_job_t *job = argument;
    char error[128] = {0};
    bool success = miniapp_matches(&job->package);
    char *source = NULL;
    if (!success) {
        source = lisa_mem_alloc(job->package.size + 1);
        if (!source) {
            snprintf(error, sizeof(error), "not enough memory for source");
        } else if (download_once(job, source, error, sizeof(error)) == 0) {
            success = miniapp_install(&job->package, source, error, sizeof(error)) == 0;
        }
    }
    lisa_mem_free(source);
    char message[180];
    snprintf(message, sizeof(message), success ? "miniapp installed and started" :
             "miniapp install failed: %s", error);
    cJSON *result = miniapp_mcp_result(INSTALL_NAME, message, !success);
    if (result) {
        (void)mcp_tool_call_result_response(job->call_id, result);
        cJSON_Delete(result);
    }
    miniapp_install_end();
    lisa_mem_free(job);
    vTaskDelete(NULL);
}

static cJSON *miniapp_install_call(const char *id, const char *name, cJSON *args)
{
    if (!id || !id[0] || strlen(id) > INSTALL_CALL_ID_MAX || !cJSON_IsObject(args)) {
        return miniapp_mcp_result(name, "invalid call ID or arguments", true);
    }
    install_job_t *job = lisa_mem_calloc(1, sizeof(*job));
    if (!job) {
        return miniapp_mcp_result(name, "not enough memory for install", true);
    }
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(args, "size");
    bool valid = copy_string(args, "id", job->package.id, sizeof(job->package.id)) &&
        copy_string(args, "version", job->package.version, sizeof(job->package.version)) &&
        copy_string(args, "url", job->url, sizeof(job->url)) &&
        copy_string(args, "hash", job->package.hash, sizeof(job->package.hash)) &&
        strlen(job->package.hash) == 32 &&
        strspn(job->package.hash, "0123456789abcdef") == 32 &&
        cJSON_IsNumber(size) && isfinite(size->valuedouble) &&
        size->valuedouble >= 1 && size->valuedouble <= MINIAPP_SOURCE_MAX &&
        size->valuedouble == (uint32_t)size->valuedouble;
    if (valid) {
        const char *host = NULL;
        if (strncmp(job->url, "https://", 8) == 0) host = job->url + 8;
        if (strncmp(job->url, "http://", 7) == 0) host = job->url + 7;
        valid = host && host[0] && host[0] != '/' &&
                !strpbrk(job->url, "\r\n\t ");
    }
    if (!valid) {
        lisa_mem_free(job);
        return miniapp_mcp_result(name, "invalid id, version, url, size or hash", true);
    }
    job->package.size = (uint32_t)size->valuedouble;
    memcpy(job->call_id, id, strlen(id) + 1);
    if (!miniapp_install_begin()) {
        lisa_mem_free(job);
        return miniapp_mcp_result(name, "miniapp runtime unavailable or install in progress", true);
    }
    if (xTaskCreate(install_worker, "miniapp.install", 4096, job, 4, NULL) != pdPASS) {
        miniapp_install_end();
        lisa_mem_free(job);
        return miniapp_mcp_result(name, "failed to start install worker", true);
    }
    return NULL; /* Async response after download, verification and startup. */
}

MCP_TOOL_DEFINE(ls.built_in.miniapp_install, miniapp_install_list, miniapp_install_call);

static cJSON *miniapp_exit_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "退出当前运行的小应用并返回桌面。用户要求退出小应用、关闭小游戏或返回桌面时调用。"
        "无需参数；没有小应用运行时不执行操作。不删除小应用存档，不用于退出普通语音对话。");
    if (!tool) {
        return NULL;
    }
    cJSON *schema = cJSON_GetObjectItemCaseSensitive(tool, "inputSchema");
    /* The common helper supplies this value as a string; use a JSON boolean. */
    cJSON_DeleteItemFromObjectCaseSensitive(schema, "additionalProperties");
    if (!cJSON_AddBoolToObject(schema, "additionalProperties", false)) {
        cJSON_Delete(tool);
        return NULL;
    }
    return tool;
}

static cJSON *miniapp_exit_call(const char *id, const char *name, cJSON *args)
{
    (void)id;
    /* MCP permits omitting arguments for tools without parameters. */
    if (args && (!cJSON_IsObject(args) || cJSON_GetArraySize(args) != 0)) {
        return miniapp_mcp_result(name, "miniapp_exit takes no arguments", true);
    }
    if (!miniapp_is_active()) {
        return miniapp_mcp_result(name, "no miniapp is running", false);
    }
    bool accepted = miniapp_exit() == 0;
    return miniapp_mcp_result(name, accepted ? "miniapp exit requested" :
                             "failed to request miniapp exit", !accepted);
}

MCP_TOOL_DEFINE(ls.built_in.miniapp_exit, miniapp_exit_list, miniapp_exit_call);
