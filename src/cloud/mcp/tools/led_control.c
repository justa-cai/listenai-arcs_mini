#include "led_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "../../led/led.h"

#define TAG "led_control"

// LED状态管理
static int led_state = 0;  // 0=关闭，1=开启
static char led_blink_mode[16] = "off";  // "off", "normal", "fast", "slow"

// 初始化LED控制模块
__attribute__((constructor)) static void led_control_init(void)
{
    app_led_init();
}

// LED开关控制处理函数
static mcp_result_t led_switch_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    int value_found = 0;
    int new_state = 0;

    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "value") == 0) {
            if (cJSON_IsBool(ctx->params[i].value)) {
                value_found = 1;
                new_state = cJSON_IsTrue(ctx->params[i].value) ? 1 : 0;
            }
        }
    }

    if (!value_found) {
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需的 value 参数");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int old_state = led_state;
    led_state = new_state;

    // 如果LED关闭，同时停止闪烁
    if (led_state == 0) {
        strcpy(led_blink_mode, "off");
    }

    // 调用LED控制API
    if (led_state) {
        app_led_on();
    } else {
        app_led_off();
    }

    LISA_LOGI(TAG, "LED state changed from %s to %s",
              old_state ? "ON" : "OFF",
              led_state ? "ON" : "OFF");

    // 创建content数组
    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }

    // 创建text item
    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        cJSON_Delete(content_array);
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

// LED闪烁控制处理函数
static mcp_result_t led_blink_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *mode = NULL;
    
    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "mode") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                mode = ctx->params[i].value->valuestring;
            }
        }
    }

    if (!mode) {
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需的 mode 参数");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证闪烁模式
    if (strcmp(mode, "off") != 0 &&
        strcmp(mode, "normal") != 0 &&
        strcmp(mode, "fast") != 0 &&
        strcmp(mode, "slow") != 0) {
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "错误：mode 只能是 'off'、'normal'、'fast' 或 'slow'");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    char old_mode[16];
    strcpy(old_mode, led_blink_mode);
    strcpy(led_blink_mode, mode);

    // 如果设置闪烁模式，自动开启LED
    if (strcmp(mode, "off") != 0) {
        led_state = 1;
    }

    // 根据闪烁模式设置LED
    if (strcmp(mode, "normal") == 0) {
        app_led_blink(500, 500);
    } else if (strcmp(mode, "fast") == 0) {
        app_led_blink(200, 200);
    } else if (strcmp(mode, "slow") == 0) {
        app_led_blink(1000, 1000);
    } else {  // off
        // 停止闪烁，保持当前状态
        app_led_stop();
        if (led_state) {
            app_led_on();
        } else {
            app_led_off();
        }
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "LED闪烁已停止");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }

    LISA_LOGI(TAG, "LED blink mode changed from %s to %s", old_mode, led_blink_mode);

    // 创建content数组
    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 创建text item
    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        cJSON_Delete(content_array);
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 生成LED开关工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_led_switch_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for led_switch schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to led_switch schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // value 参数
    cJSON *value_prop = cJSON_CreateObject();
    if (!value_prop) {
        LISA_LOGE(TAG, "Failed to create value property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(value_prop, "type", "boolean") ||
        !cJSON_AddStringToObject(value_prop, "description", "LED开关状态，打开为true，关闭为false")) {
        LISA_LOGE(TAG, "Failed to add value property fields");
        cJSON_Delete(value_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "value", value_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *value_str = cJSON_CreateString("value");
    if (!value_str) {
        LISA_LOGE(TAG, "Failed to create value string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, value_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

/**
 * @brief 生成LED闪烁工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_led_blink_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for led_blink schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to led_blink schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // mode 参数
    cJSON *mode_prop = cJSON_CreateObject();
    if (!mode_prop) {
        LISA_LOGE(TAG, "Failed to create mode property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(mode_prop, "type", "string") ||
        !cJSON_AddStringToObject(mode_prop, "description", "LED闪烁模式，可以是 'off'、'normal'、'fast' 或 'slow'")) {
        LISA_LOGE(TAG, "Failed to add mode property fields");
        cJSON_Delete(mode_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "mode", mode_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *mode_str = cJSON_CreateString("mode");
    if (!mode_str) {
        LISA_LOGE(TAG, "Failed to create mode string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, mode_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 注册LED开关控制工具
MCP_REGISTER_TOOL_STATIC(led_switch,
                         "ls.led_switch",
                         "控制LED开关状态，可以是开启或关闭",
                         "1.0",
                         generate_led_switch_schema,
                         1,
                         led_switch_handler,
                         false,
                         NULL);

// 注册LED闪烁控制工具
MCP_REGISTER_TOOL_STATIC(led_blink,
                         "ls.led_blink",
                         "控制LED闪烁模式，可以是关闭、普通、快速或慢速",
                         "1.0",
                         generate_led_blink_schema,
                         1,
                         led_blink_handler,
                         false,
                         NULL);

int get_led_state(void)
{
    // 返回LED当前状态
    // 注意：这里返回的是软件状态，不是实际硬件状态
    return led_state;
}

const char* get_led_blink_mode(void)
{
    // 返回LED当前闪烁模式
    return led_blink_mode;
}
