#define TAG "ota_api"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_http.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include "aiui_base64.h"
#include "project_version.h"
#include "mbedtls/md5.h"

#include "ota_api.h"
#include "kv_user.h"
#include "app_datas.h"

#define DEV_CONF_API "api.listenai.com/external/device/configurations"
#define APP_OTA_API  "api.listenai.com/v1/ota/packages"

#define CONF_URL_LEN 512
#define OTA_HTTP_REQUEST_MAX_ATTEMPTS 1
#define OTA_HTTP_RETRY_DELAY_BASE_MS 300
#define OTA_HTTP_REQUEST_TIMEOUT_SEC 3
#define OTA_HTTP_DOWNLOAD_TIMEOUT_SEC 10

static lisa_http_err_e ota_api_http_perform_with_retry(lisa_http_t *http, const char *purpose)
{
    lisa_http_err_e err = LISA_HTTP_COMMON_ERR;

    if (!http) {
        return LISA_HTTP_PARAM_ERROR;
    }

    for (int attempt = 0; attempt < OTA_HTTP_REQUEST_MAX_ATTEMPTS; ++attempt) {
        err = lisa_http_perform(http);
        if (err == LISA_HTTP_OK) {
            if (attempt > 0) {
                LISA_LOGI(TAG, "%s HTTP succeeded on retry %d/%d", purpose, attempt + 1,
                          OTA_HTTP_REQUEST_MAX_ATTEMPTS);
            }
            return err;
        }

        LISA_LOGW(TAG, "%s HTTP failed on attempt %d/%d: %d",
                  purpose, attempt + 1, OTA_HTTP_REQUEST_MAX_ATTEMPTS, err);
        if (attempt + 1 < OTA_HTTP_REQUEST_MAX_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(OTA_HTTP_RETRY_DELAY_BASE_MS * (attempt + 1)));
        }
    }

    return err;
}

static char *conf_build_param_base64(void)
{
    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return NULL;
    }

    cJSON *firmware_info = cJSON_CreateObject();
    if (!firmware_info) {
        cJSON_Delete(params);
        return NULL;
    }

    cJSON_AddStringToObject(firmware_info, "type", "arcs-mini");
    cJSON_AddStringToObject(firmware_info, "version", PROJECT_VERSION_STR);
    cJSON_AddItemToObject(params, "firmware_info", firmware_info);

    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        return NULL;
    }

    char *params_base64 = (char *)aiui_base64_encode((unsigned char *)params_json);
    cJSON_free(params_json);
    if (!params_base64 || params_base64[0] == '\0') {
        if (params_base64) {
            lisa_mem_free(params_base64);
        }
        return NULL;
    }

    return params_base64;
}

static void *conf_headers(void)
{
    return "Accept: application/json";
}

static void conf_on_data(lisa_http_data_t *data)
{
    LISA_LOGI(TAG, "Config data received: %.*s", data->len, (const char *)data->buf);

    cJSON **p_json = (cJSON **)data->user;
    *p_json = cJSON_ParseWithLength((const char *)data->buf, data->len);
    if (!*p_json) {
        LISA_LOGE(TAG, "Failed to parse JSON");
    }
}

static cJSON *conf_get(void)
{
    struct app_datas *app_datas = get_app_datas();
    char url[CONF_URL_LEN];
    int ret;

    const char *host_suffix = "";
    if (app_datas->device_mode == DEVICE_MODE_STAGING) {
        host_suffix = "staging-";
    } else if (app_datas->device_mode == DEVICE_MODE_INTEGRATION) {
        host_suffix = "integration-";
    }

    char *param_base64 = conf_build_param_base64();
    int url_len = 0;
    if (param_base64) {
        url_len = snprintf(url, sizeof(url), "http://%s%s?product_id=%s&device_id=%s&param=%s", host_suffix,
                           DEV_CONF_API, app_datas->pid, app_datas->did, param_base64);
    } else {
        LISA_LOGW(TAG, "param base64 build failed, request without param");
        url_len = snprintf(url, sizeof(url), "http://%s%s?product_id=%s&device_id=%s", host_suffix, DEV_CONF_API,
                           app_datas->pid, app_datas->did);
    }

    if (param_base64) {
        lisa_mem_free(param_base64);
        param_base64 = NULL;
    }

    if (url_len < 0 || url_len >= (int)sizeof(url)) {
        LISA_LOGE(TAG, "Config URL too long");
        return NULL;
    }

    cJSON *json = NULL;

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = url,
        .timeout = OTA_HTTP_REQUEST_TIMEOUT_SEC,
        .body = NULL,
        .body_len = 0,
        .headers = (uint8_t *)conf_headers,
        .on_data = conf_on_data,
        .user = &json,
    };

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "HTTP init failed");
        return NULL;
    }

    lisa_http_err_e err = ota_api_http_perform_with_retry(http, "Get config");
    if (err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "HTTP perform failed: %d", err);
        lisa_http_cleanup(http);
        return NULL;
    }

    lisa_http_cleanup(http);

    return json;
}

static int ota_api_parse_res_info(cJSON *item, ota_res_info_t *target)
{
    if (!item || !target) {
        return -1;
    }

    memset(target, 0, sizeof(ota_res_info_t));

    cJSON *size = cJSON_GetObjectItem(item, "size");
    cJSON *md5 = cJSON_GetObjectItem(item, "md5");
    cJSON *url = cJSON_GetObjectItem(item, "url");
    if (!cJSON_IsNumber(size) || !cJSON_IsString(md5) || !cJSON_IsString(url)) {
        return -1;
    }

    char *url_val = cJSON_GetStringValue(url);
    if (strlen(url_val) >= OTA_RES_URL_LEN) {
        LISA_LOGE(TAG, "URL too long: %s", url_val);
        return -1;
    }

    for (char *s = cJSON_GetStringValue(md5), *d = target->md5; *s && d < &target->md5[OTA_RES_MD5_LEN]; d++, s += 2) {
        sscanf(s, "%2hhx", d);
    }

    strncpy(target->url, url_val, OTA_RES_URL_LEN - 1);
    target->url[OTA_RES_URL_LEN - 1] = '\0';

    target->size = (uint32_t)cJSON_GetNumberValue(size);

    return 0;
}

int ota_api_get_dev_conf(ota_dev_conf_t *conf)
{
    if (!conf) {
        return -1;
    }

    memset(conf, 0, sizeof(ota_dev_conf_t));

    cJSON *json = conf_get();
    if (!json) {
        LISA_LOGE(TAG, "Get config failed");
        return -1;
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (!data) {
        LISA_LOGE(TAG, "No data field in JSON");
        cJSON_Delete(json);
        return -1;
    }

    cJSON *wakeup_word = cJSON_GetObjectItem(data, "wakeup_word");
    if (wakeup_word) {
        cJSON *resources = cJSON_GetObjectItem(wakeup_word, "resources");
        if (resources) {
            for (int i = 0; i < cJSON_GetArraySize(resources); i++) {
                cJSON *item = cJSON_GetArrayItem(resources, i);
                if (!item) {
                    continue;
                }

                char *name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
                if (!name) {
                    continue;
                }

                ota_res_info_t *target = NULL;
                if (strcmp(name, "wake_word.bin") == 0) {
                    target = &conf->wakeup_word.resource;
                } else {
                    continue;
                }

                if (ota_api_parse_res_info(item, target) != 0) {
                    LISA_LOGE(TAG, "Failed to parse resource info for %s", name);
                    continue;
                }

                LISA_LOGI(TAG, "Found wake word resource (%s), size: %d, md5: " MD5_PRI ", url: %s", name, target->size,
                          MD5_ARG(target->md5), target->url);
            }
        }

        char *text = cJSON_GetStringValue(cJSON_GetObjectItem(wakeup_word, "text"));
        if (text) {
            memset(conf->wakeup_word.text, 0, sizeof(conf->wakeup_word.text));
            strncpy(conf->wakeup_word.text, text, sizeof(conf->wakeup_word.text) - 1);
            LISA_LOGI(TAG, "Wakeup word text: %s", conf->wakeup_word.text);
        }
    }

    cJSON *prompt_tone = cJSON_GetObjectItem(data, "prompt_tone");
    if (ota_api_parse_res_info(prompt_tone, &conf->prompt_tone) != 0) {
        LISA_LOGE(TAG, "Failed to parse prompt_tone resource info");
    } else {
        LISA_LOGI(TAG, "Found prompt_tone resource, size: %d, md5: " MD5_PRI ", url: %s", conf->prompt_tone.size,
                  MD5_ARG(conf->prompt_tone.md5), conf->prompt_tone.url);
    }

    cJSON *emoji = cJSON_GetObjectItem(data, "emoji");
    if (ota_api_parse_res_info(emoji, &conf->emoji) != 0) {
        LISA_LOGE(TAG, "Failed to parse emoji resource info");
    } else {
        LISA_LOGI(TAG, "Found emoji resource, size: %d, md5: " MD5_PRI ", url: %s", conf->emoji.size,
                  MD5_ARG(conf->emoji.md5), conf->emoji.url);
    }

    cJSON_Delete(json);

    return 0;
}

struct download_context {
    const ota_res_info_t *res_info;
    ota_download_cb_t cb;
    int cb_ret;
    uint32_t downloaded;
    mbedtls_md5_context md5_ctx;
    char calc_md5[OTA_RES_MD5_LEN];
};

static void download_on_data(lisa_http_data_t *data)
{
    struct download_context *ctx = (struct download_context *)data->user;
    const ota_res_info_t *res_info = ctx->res_info;

    if (ctx->cb_ret < 0 || !data->len) {
        return;
    }

    mbedtls_md5_update(&ctx->md5_ctx, (const uint8_t *)data->buf, data->len);
    ctx->downloaded += data->len;

    if (ctx->cb) {
        ctx->cb_ret = ctx->cb(res_info, ctx->downloaded - data->len, (const uint8_t *)data->buf, (uint32_t)data->len);
    }
}

int ota_api_download(const ota_res_info_t *res_info, ota_download_cb_t cb)
{
    struct download_context ctx = {
        .res_info = res_info,
        .cb = cb,
        .cb_ret = 0,
        .downloaded = 0,
    };
    int ret = -1;
    lisa_http_t *http = NULL;
    lisa_http_err_e err;

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)res_info->url,
        .timeout = OTA_HTTP_DOWNLOAD_TIMEOUT_SEC,
        .body = NULL,
        .body_len = 0,
        .headers = NULL,
        .on_data = download_on_data,
        .user = (void *)&ctx,
    };

    mbedtls_md5_init(&ctx.md5_ctx);
    mbedtls_md5_starts(&ctx.md5_ctx);

    http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "HTTP init failed");
        goto out;
    }

    err = lisa_http_download(http);
    if (err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "HTTP download failed: %d, received %u / %u bytes", err, ctx.downloaded, res_info->size);
        goto out;
    }

    if (ctx.cb_ret < 0) {
        LISA_LOGE(TAG, "Download completed with error from callback: %d", ctx.cb_ret);
        ret = ctx.cb_ret;
        goto out;
    }

    LISA_LOGI(TAG, "Download received: %u bytes", ctx.downloaded);

    if (ctx.downloaded != res_info->size) {
        LISA_LOGE(TAG, "Download size mismatch: expected %u, got %u", res_info->size, ctx.downloaded);
        goto out;
    }

    mbedtls_md5_finish(&ctx.md5_ctx, (uint8_t *)ctx.calc_md5);
    LISA_LOGI(TAG, "Download MD5 actual: " MD5_PRI, MD5_ARG(ctx.calc_md5));
    LISA_LOGI(TAG, "Download MD5 expect: " MD5_PRI, MD5_ARG(res_info->md5));

    if (memcmp(ctx.calc_md5, res_info->md5, OTA_RES_MD5_LEN) != 0) {
        LISA_LOGE(TAG, "Download MD5 mismatch");
        goto out;
    }

    ret = (int)ctx.downloaded;

out:
    if (http) {
        lisa_http_cleanup(http);
    }
    mbedtls_md5_free(&ctx.md5_ctx);
    return ret;
}

/* ==================== 系统 OTA（/v1/ota/packages） ==================== */

static char *s_app_ota_headers = NULL;

static void *app_ota_headers_cb(void)
{
    return (void *)s_app_ota_headers;
}

static void hex_of(const uint8_t *bin, size_t len, char *out)
{
    static const char k[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = k[(bin[i] >> 4) & 0x0F];
        out[i * 2 + 1] = k[bin[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

/* 严格解析 32 个十六进制字符的 md5。长度不足/超长/含非 hex 字符都返回 -1。 */
static int parse_md5_hex(const char *s, char *out)
{
    if (!s || !out) {
        return -1;
    }

    if (strlen(s) != OTA_RES_MD5_LEN * 2) {
        return -1;
    }

    for (size_t i = 0; i < OTA_RES_MD5_LEN * 2; i++) {
        if (!isxdigit((unsigned char)s[i])) {
            return -1;
        }
    }

    for (size_t i = 0; i < OTA_RES_MD5_LEN; i++) {
        if (sscanf(s + i * 2, "%2hhx", (unsigned char *)&out[i]) != 1) {
            return -1;
        }
    }
    return 0;
}

static int build_app_ota_headers(void)
{
    struct app_datas *app_datas = get_app_datas();
    if (!app_datas || app_datas->pid[0] == '\0' || app_datas->did[0] == '\0') {
        LISA_LOGE(TAG, "Missing pid/did for app OTA");
        return -1;
    }
    if (app_datas->sid[0] == '\0') {
        LISA_LOGE(TAG, "Missing product secret (sid) for app OTA");
        return -1;
    }

    char ts[24];
    snprintf(ts, sizeof(ts), "%ld", (long)time(NULL));

    /* checksum = md5(product_secret + deviceid + curtime) */
    mbedtls_md5_context md5_ctx;
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);
    mbedtls_md5_update(&md5_ctx, (const uint8_t *)app_datas->sid, strlen(app_datas->sid));
    mbedtls_md5_update(&md5_ctx, (const uint8_t *)app_datas->did, strlen(app_datas->did));
    mbedtls_md5_update(&md5_ctx, (const uint8_t *)ts, strlen(ts));
    uint8_t digest[16];
    mbedtls_md5_finish(&md5_ctx, digest);
    mbedtls_md5_free(&md5_ctx);

    char checksum[33];
    hex_of(digest, sizeof(digest), checksum);

    const char *fmt = "Accept: application/json\r\n"
                      "X-ProductID: %s\r\n"
                      "X-DeviceID: %s\r\n"
                      "X-CurTime: %s\r\n"
                      "X-CheckSum: %s";
    size_t header_len = strlen(fmt) + strlen(app_datas->pid) + strlen(app_datas->did) + strlen(ts) + strlen(checksum) +
                        1;

    if (s_app_ota_headers) {
        lisa_mem_free(s_app_ota_headers);
    }
    s_app_ota_headers = lisa_mem_calloc(1, header_len);
    if (!s_app_ota_headers) {
        return -1;
    }
    snprintf(s_app_ota_headers, header_len, fmt, app_datas->pid, app_datas->did, ts, checksum);

    return 0;
}

static void free_app_ota_headers(void)
{
    if (s_app_ota_headers) {
        lisa_mem_free(s_app_ota_headers);
        s_app_ota_headers = NULL;
    }
}

struct app_check_ctx {
    cJSON *json;
};

static void app_check_on_data(lisa_http_data_t *data)
{
    struct app_check_ctx *ctx = (struct app_check_ctx *)data->user;
    if (!ctx || !data->buf || data->len <= 0) {
        return;
    }

    LISA_LOGI(TAG, "App OTA check response: %.*s", data->len, (const char *)data->buf);

    if (ctx->json) {
        cJSON_Delete(ctx->json);
        ctx->json = NULL;
    }
    ctx->json = cJSON_ParseWithLength((const char *)data->buf, data->len);
}

int ota_api_check_app(ota_app_package_t *pkg)
{
    if (!pkg) {
        return -1;
    }

    memset(pkg, 0, sizeof(*pkg));
    pkg->available = false;

    struct app_datas *app_datas = get_app_datas();
    if (!app_datas) {
        return -1;
    }

    if (build_app_ota_headers() != 0) {
        return -1;
    }

    const char *host_suffix = "";
    if (app_datas->device_mode == DEVICE_MODE_STAGING) {
        host_suffix = "staging-";
    } else if (app_datas->device_mode == DEVICE_MODE_INTEGRATION) {
        host_suffix = "integration-";
    }

    char url[CONF_URL_LEN];
    int url_len = snprintf(url, sizeof(url), "https://%s%s", host_suffix, APP_OTA_API);
    if (url_len < 0 || url_len >= (int)sizeof(url)) {
        LISA_LOGE(TAG, "App OTA URL too long");
        free_app_ota_headers();
        return -1;
    }

    struct app_check_ctx ctx = {.json = NULL};
    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .timeout = OTA_HTTP_REQUEST_TIMEOUT_SEC,
        .body = NULL,
        .body_len = 0,
        .headers = (uint8_t *)app_ota_headers_cb,
        .on_data = app_check_on_data,
        .user = &ctx,
    };

    int ret = -1;
    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "HTTP init failed");
        goto out;
    }

    lisa_http_err_e err = ota_api_http_perform_with_retry(http, "App OTA check");
    lisa_http_cleanup(http);

    if (err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "App OTA check HTTP failed: %d", err);
        goto out;
    }

    if (!ctx.json) {
        LISA_LOGW(TAG, "App OTA check: empty/invalid response (assumed up-to-date)");
        ret = 0;  /* 视为无更新 */
        goto out;
    }

    cJSON *url_item = cJSON_GetObjectItem(ctx.json, "url");
    cJSON *ver_item = cJSON_GetObjectItem(ctx.json, "version");
    cJSON *verno_item = cJSON_GetObjectItem(ctx.json, "version_number");
    cJSON *md5_item = cJSON_GetObjectItem(ctx.json, "md5_checksum");
    cJSON *description_item = cJSON_GetObjectItem(ctx.json, "description");
    cJSON *size_item = cJSON_GetObjectItem(ctx.json, "size");

    if (!cJSON_IsString(url_item) || url_item->valuestring[0] == '\0') {
        LISA_LOGI(TAG, "App OTA: no update available");
        ret = 0;
        goto out;
    }

    if (strlen(url_item->valuestring) >= OTA_RES_URL_LEN) {
        LISA_LOGE(TAG, "App OTA url too long");
        goto out;
    }

    if (!cJSON_IsNumber(verno_item)) {
        LISA_LOGE(TAG, "App OTA: server missing required 'version_number', refuse");
        goto out;
    }
    uint32_t server_verno = (uint32_t)cJSON_GetNumberValue(verno_item);
    if (server_verno <= PROJECT_VERSION_NUMBER) {
        LISA_LOGI(TAG, "App OTA: server version_number=%u <= local=%u, not an upgrade",
                  server_verno, (uint32_t)PROJECT_VERSION_NUMBER);
        ret = 0;
        goto out;
    }

    /* md5_checksum 是后台人工录入字段，可能为空字符串甚至格式错误。
     * 空 → 跳过校验；合法 32-hex → 正常校验；其它 → 拒绝下载（明显是数据错）。 */
    if (cJSON_IsString(md5_item) && md5_item->valuestring[0] != '\0') {
        if (parse_md5_hex(md5_item->valuestring, pkg->md5) != 0) {
            LISA_LOGE(TAG, "App OTA md5 malformed (%s), refuse to download", md5_item->valuestring);
            goto out;
        }
        pkg->has_md5 = true;
    } else {
        pkg->has_md5 = false;
    }

    strncpy(pkg->url, url_item->valuestring, OTA_RES_URL_LEN - 1);
    pkg->url[OTA_RES_URL_LEN - 1] = '\0';

    if (cJSON_IsString(ver_item)) {
        strncpy(pkg->version, ver_item->valuestring, OTA_APP_VERSION_LEN - 1);
    }

    pkg->version_number = server_verno;

    if (cJSON_IsString(description_item) && description_item->valuestring != NULL) {
        strncpy(pkg->release_notes, description_item->valuestring, sizeof(pkg->release_notes) - 1);
        pkg->release_notes[sizeof(pkg->release_notes) - 1] = '\0';
    }

    if (cJSON_IsNumber(size_item)) {
        double size_value = cJSON_GetNumberValue(size_item);
        if (size_value > 0) {
            pkg->size = (uint32_t)size_value;
        }
    }
    
    cJSON *pid_item = cJSON_GetObjectItem(ctx.json, "package_id");
    if (cJSON_IsString(pid_item)) {
        strncpy(pkg->package_id, pid_item->valuestring, OTA_APP_PACKAGE_ID_LEN - 1);
    }

    pkg->available = true;
    LISA_LOGI(TAG, "App OTA available: package_id=%s version=%s version_number=%u (local=%u) size=%u md5=%s url=%s",
              pkg->package_id, pkg->version, pkg->version_number, (uint32_t)PROJECT_VERSION_NUMBER,
              pkg->size, pkg->has_md5 ? "set" : "<empty, skip verify>", pkg->url);

    ret = 0;

out:
    if (ctx.json) {
        cJSON_Delete(ctx.json);
    }
    free_app_ota_headers();
    return ret;
}

struct app_download_ctx {
    const ota_app_package_t *pkg;
    ota_app_download_cb_t cb;
    void *user;
    int cb_ret;
    uint32_t downloaded;
    mbedtls_md5_context md5_ctx;
};

static void app_download_on_data(lisa_http_data_t *data)
{
    struct app_download_ctx *ctx = (struct app_download_ctx *)data->user;
    if (!ctx || ctx->cb_ret != 0 || !data->buf || data->len <= 0) {
        return;
    }

    if (ctx->pkg->has_md5) {
        mbedtls_md5_update(&ctx->md5_ctx, (const uint8_t *)data->buf, data->len);
    }
    uint32_t chunk_offset = ctx->downloaded;
    ctx->downloaded += data->len;

    if (ctx->cb) {
        ctx->cb_ret = ctx->cb(ctx->user, chunk_offset, (const uint8_t *)data->buf, (uint32_t)data->len,
                              ctx->pkg->size);
    }
}

int ota_api_download_app(const ota_app_package_t *pkg, ota_app_download_cb_t cb, void *user)
{
    if (!pkg || !pkg->available || pkg->url[0] == '\0') {
        return -1;
    }

    struct app_download_ctx ctx = {
        .pkg = pkg,
        .cb = cb,
        .user = user,
        .cb_ret = 0,
        .downloaded = 0,
    };

    if (pkg->has_md5) {
        mbedtls_md5_init(&ctx.md5_ctx);
        mbedtls_md5_starts(&ctx.md5_ctx);
    }

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)pkg->url,
        .timeout = OTA_HTTP_DOWNLOAD_TIMEOUT_SEC,
        .body = NULL,
        .body_len = 0,
        .headers = NULL,
        .on_data = app_download_on_data,
        .user = &ctx,
    };

    int ret = -1;
    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "App OTA HTTP init failed");
        goto out;
    }

    lisa_http_err_e err = lisa_http_download(http);
    lisa_http_cleanup(http);

    if (err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "App OTA download failed: %d (got %u bytes)", err, ctx.downloaded);
        goto out;
    }

    if (ctx.cb_ret != 0) {
        LISA_LOGE(TAG, "App OTA download aborted by callback: %d", ctx.cb_ret);
        ret = ctx.cb_ret;
        goto out;
    }

    if (pkg->has_md5) {
        uint8_t calc_md5[OTA_RES_MD5_LEN];
        mbedtls_md5_finish(&ctx.md5_ctx, calc_md5);
        LISA_LOGI(TAG, "App OTA MD5 actual: " MD5_PRI, MD5_ARG(calc_md5));
        LISA_LOGI(TAG, "App OTA MD5 expect: " MD5_PRI, MD5_ARG(pkg->md5));

        if (memcmp(calc_md5, pkg->md5, OTA_RES_MD5_LEN) != 0) {
            LISA_LOGE(TAG, "App OTA MD5 mismatch");
            goto out;
        }
    } else {
        LISA_LOGW(TAG, "App OTA: server md5 empty, skipped verification");
    }

    LISA_LOGI(TAG, "App OTA downloaded %u bytes", ctx.downloaded);
    ret = (int)ctx.downloaded;

out:
    if (pkg->has_md5) {
        mbedtls_md5_free(&ctx.md5_ctx);
    }
    return ret;
}
