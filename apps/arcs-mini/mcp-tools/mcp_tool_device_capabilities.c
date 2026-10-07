#include <stdio.h>
#include "mcp.h"
#include "miniapp.h"
#include "miniapp_storage.h"
#include "project_version.h"
#ifdef CONFIG_LISA_DISPLAY_DEVICE
#include "lisa_display.h"
#endif
#ifdef CONFIG_GAMEPAD_ENABLE
#include "gamepad.h"
#endif
#include "mcp_miniapp_result.h"

static cJSON *capabilities_list(const char *name)
{
    cJSON *tool = cJSON_Parse("{\"description\":\"查询设备硬件能力及 Lua 小应用运行环境\","
                              "\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"required\":[]}}");
    if (tool && !cJSON_AddStringToObject(tool, "name", name)) {
        cJSON_Delete(tool);
        return NULL;
    }
    return tool;
}

static cJSON *capabilities_call(const char *id, const char *name, cJSON *args)
{
    (void)id;
    if (args && (!cJSON_IsObject(args) || args->child)) {
        return miniapp_mcp_result(name, "expected empty arguments", true);
    }
    cJSON *root = cJSON_Parse("{\"schema_version\":2,\"firmware_info\":{\"type\":\"arcs-mini\"},"
                              "\"runtime\":{},\"hardware\":{\"display\":{},"
                              "\"input\":{\"buttons\":[]},\"audio\":{\"speaker\":true,\"microphone\":true},"
                              "\"lighting\":{\"leds\":[]}}}");
    if (!root) {
        return NULL;
    }
    cJSON *firmware = cJSON_GetObjectItemCaseSensitive(root, "firmware_info");
    cJSON *hardware = cJSON_GetObjectItemCaseSensitive(root, "hardware");
    cJSON *display = cJSON_GetObjectItemCaseSensitive(hardware, "display");
    if (!cJSON_AddStringToObject(firmware, "version", PROJECT_VERSION_STR)) {
        goto failed;
    }
    if (!cJSON_AddBoolToObject(hardware, "network", true)) {
        goto failed;
    }
#ifdef CONFIG_LISA_DISPLAY_DEVICE
    lisa_display_capabilities_t caps;
    lisa_device_t *dev = lisa_device_get("display");
    bool has_display = lisa_device_ready(dev) && lisa_display_get_capabilities(dev, &caps) == 0;
    if (!cJSON_AddBoolToObject(display, "supported", has_display)) {
        goto failed;
    }
    if (has_display && (!cJSON_AddNumberToObject(display, "width_px", caps.width) ||
                        !cJSON_AddNumberToObject(display, "height_px", caps.height))) {
        goto failed;
    }
#else
    if (!cJSON_AddBoolToObject(display, "supported", false)) {
        goto failed;
    }
#endif
    cJSON *buttons = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(hardware, "input"), "buttons");
    cJSON *button = cJSON_Parse("{\"id\":\"function\",\"label\":\"功能键\","
                                 "\"events\":[\"click\",\"double_click\"],"
                                 "\"position\":\"lower_left\",\"reserved_hold_ms\":3000}");
    if (!button || !cJSON_AddItemToArray(buttons, button)) {
        cJSON_Delete(button);
        goto failed;
    }
#ifdef CONFIG_GAMEPAD_ENABLE
    /* 手柄在线时才列出它的按键：小应用可以在运行时按能力表决定 UI 提示。
     * BLE 手柄的连接是动态的，所以这里每次都重新判断，不是启动期一次性写入。 */
    if (gamepad_pad_active()) {
        cJSON *pad = cJSON_Parse(
            "[{\"id\":\"up\",\"label\":\"方向键上\",\"events\":[\"click\"]},"
            "{\"id\":\"down\",\"label\":\"方向键下\",\"events\":[\"click\"]},"
            "{\"id\":\"left\",\"label\":\"方向键左\",\"events\":[\"click\"]},"
            "{\"id\":\"right\",\"label\":\"方向键右\",\"events\":[\"click\"]},"
            "{\"id\":\"back\",\"label\":\"手柄返回\",\"events\":[\"click\"]},"
            "{\"id\":\"settings\",\"label\":\"手柄设置\",\"events\":[\"click\"]}]");
        if (!pad) {
            goto failed;
        }
        /* 拆开逐个并入 buttons（cJSON_AddItemToArray 会把整个数组当一个元素） */
        cJSON *item;
        while ((item = cJSON_DetachItemFromArray(pad, 0)) != NULL) {
            if (!cJSON_AddItemToArray(buttons, item)) {
                cJSON_Delete(item);
                cJSON_Delete(pad);
                goto failed;
            }
        }
        cJSON_Delete(pad);
    }
#endif
    cJSON *leds = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(hardware, "lighting"), "leds");
    cJSON *led = cJSON_Parse("{\"id\":\"status\",\"label\":\"状态灯\",\"color\":false}");
    if (!led || !cJSON_AddItemToArray(leds, led)) {
        cJSON_Delete(led);
        goto failed;
    }
#ifdef CONFIG_MINIAPP
    cJSON *runtime = cJSON_GetObjectItemCaseSensitive(root, "runtime");
    cJSON *sdk = cJSON_AddObjectToObject(runtime, "lua_sdk");
    if (!sdk || !cJSON_AddNumberToObject(sdk, "version", MINIAPP_API_VERSION) ||
        !cJSON_AddNumberToObject(sdk, "source_max_bytes", MINIAPP_SOURCE_MAX) ||
        !cJSON_AddNumberToObject(sdk, "heap_max_bytes", MINIAPP_LUA_HEAP_LIMIT)) {
        goto failed;
    }
    cJSON *http = cJSON_AddObjectToObject(sdk, "http");
    cJSON *tts = cJSON_AddObjectToObject(sdk, "tts");
    if (!http || !tts) goto failed;
    cJSON *methods = cJSON_Parse("[\"GET\",\"POST\"]");
    if (!methods || !cJSON_AddItemToObject(http, "methods", methods)) {
        cJSON_Delete(methods);
        goto failed;
    }
    if (!cJSON_AddNumberToObject(http, "max_pending", MINIAPP_HTTP_MAX_REQUESTS) ||
        !cJSON_AddNumberToObject(http, "max_url_bytes", MINIAPP_HTTP_MAX_URL_BYTES) ||
        !cJSON_AddNumberToObject(http, "max_headers", MINIAPP_HTTP_MAX_HEADERS) ||
        !cJSON_AddNumberToObject(http, "max_headers_bytes", MINIAPP_HTTP_MAX_HEADER_BYTES) ||
        !cJSON_AddNumberToObject(http, "max_body_bytes", MINIAPP_HTTP_MAX_BODY_BYTES) ||
        !cJSON_AddNumberToObject(http, "max_response_bytes", MINIAPP_HTTP_MAX_RESPONSE_BYTES) ||
        !cJSON_AddNumberToObject(http, "default_response_bytes", MINIAPP_HTTP_DEFAULT_RESPONSE_BYTES) ||
        !cJSON_AddNumberToObject(http, "max_timeout_ms", MINIAPP_HTTP_MAX_TIMEOUT_MS) ||
        !cJSON_AddNumberToObject(http, "default_timeout_ms", MINIAPP_HTTP_DEFAULT_TIMEOUT_MS) ||
        !cJSON_AddNumberToObject(tts, "text_max_bytes", MINIAPP_TTS_TEXT_MAX) ||
        !cJSON_AddNumberToObject(tts, "max_pending", MINIAPP_TTS_MAX_PENDING) ||
        !cJSON_AddNumberToObject(tts, "timeout_ms", MINIAPP_TTS_TIMEOUT_MS)) {
        goto failed;
    }
    cJSON *storage = cJSON_AddObjectToObject(sdk, "storage");
    if (!storage || !cJSON_AddNumberToObject(storage, "max_bytes", MINIAPP_STORAGE_MAX_BYTES) ||
        !cJSON_AddNumberToObject(storage, "max_apps", MINIAPP_STORAGE_MAX_APPS) ||
        !cJSON_AddNumberToObject(storage, "default_ttl_seconds", MINIAPP_STORAGE_DEFAULT_TTL) ||
        !cJSON_AddNumberToObject(storage, "max_ttl_seconds", MINIAPP_STORAGE_MAX_TTL) ||
        !cJSON_AddNumberToObject(storage, "write_interval_ms", MINIAPP_STORAGE_WRITE_INTERVAL_MS)) {
        goto failed;
    }
#endif
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) {
        return NULL;
    }
    cJSON *result = miniapp_mcp_result(name, text, false);
    cJSON_free(text);
    return result;
failed:
    cJSON_Delete(root);
    return miniapp_mcp_result(name, "device capabilities unavailable", true);
}

MCP_TOOL_DEFINE(ls.built_in.get_device_capabilities, capabilities_list, capabilities_call);
