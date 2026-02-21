#include "volume_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "player/listen_volume.h"

#define TAG "volume_control"

// 当前音量状态 (0% - 100%)
static int current_volume = 70;

#define DEFAULT_ADJUST_STEP 5
#define STEP_UNIT_MULTIPLIER 10

static int clamp_volume(int volume)
{
    if (volume < 0) {
        return 0;
    }
    if (volume > 100) {
        return 100;
    }
    return volume;
}

static bool equals_ignore_case(const char *a, const char *b)
{
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool parse_int_from_string(const char *str, int *out)
{
    if (!str || !out) {
        return false;
    }

    char *endptr = NULL;
    long value = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        return false;
    }

    *out = (int)value;
    return true;
}

static bool parse_int_from_json(const cJSON *item, int *out)
{
    if (!item || !out) {
        return false;
    }

    if (cJSON_IsNumber(item)) {
        *out = item->valueint;
        return true;
    }

    if (cJSON_IsString(item)) {
        return parse_int_from_string(item->valuestring, out);
    }

    return false;
}

static bool keyword_to_volume(const char *value_str, int *out)
{
    if (!value_str || !out) {
        return false;
    }

    if (equals_ignore_case(value_str, "max")) {
        *out = 100;
        return true;
    }

    if (equals_ignore_case(value_str, "min")) {
        *out = 0;
        return true;
    }

    if (equals_ignore_case(value_str, "mid")) {
        *out = 50;
        return true;
    }

    return false;
}

static int normalize_value_with_unit(int value, const char *unit)
{
    if (!unit || unit[0] == '\0' || strcmp(unit, "百分比") == 0) {
        return value;
    }

    if (strcmp(unit, "档") == 0 || strcmp(unit, "级") == 0) {
        return value * STEP_UNIT_MULTIPLIER;
    }

    return value;
}

static mcp_result_t build_text_response(mcp_response_t *response, const char *text, mcp_result_t result)
{
    if (!response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        cJSON_Delete(content_array);
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", text ? text : "");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = result;
    return result;
}

static mcp_result_t volume_control_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *intent = NULL;
    const char *value_str = NULL;
    const char *unit = NULL;
    int numeric_value = 0;
    bool has_numeric_value = false;
    int legacy_volume = -1;
    
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "intent") == 0 && cJSON_IsString(ctx->params[i].value)) {
            intent = ctx->params[i].value->valuestring;
            continue;
        }

        if (strcmp(ctx->params[i].name, "value") == 0) {
            value_str = cJSON_IsString(ctx->params[i].value) ? ctx->params[i].value->valuestring : NULL;
            has_numeric_value = parse_int_from_json(ctx->params[i].value, &numeric_value);
            continue;
        }

        if (strcmp(ctx->params[i].name, "unit") == 0 && cJSON_IsString(ctx->params[i].value)) {
            unit = ctx->params[i].value->valuestring;
            continue;
        }

        if (strcmp(ctx->params[i].name, "volume") == 0 && cJSON_IsNumber(ctx->params[i].value)) {
            legacy_volume = ctx->params[i].value->valueint;
        }
    }

    int old_volume = listen_get_volume();

    int target_volume = -1;

    if (intent) {
        if (strcmp(intent, "set") == 0) {
            if (value_str && keyword_to_volume(value_str, &target_volume)) {
                target_volume = clamp_volume(target_volume);
            } else if (has_numeric_value) {
                target_volume = clamp_volume(normalize_value_with_unit(numeric_value, unit));
            } else {
                return build_text_response(response, "缺少有效的音量数值", MCP_RESULT_INVALID_PARAM);
            }
        } else if (strcmp(intent, "adjustUp") == 0 || strcmp(intent, "adjustDown") == 0) {
            bool adjust_up = strcmp(intent, "adjustUp") == 0;
            int keyword_volume = -1;

            if (value_str && keyword_to_volume(value_str, &keyword_volume)) {
                target_volume = clamp_volume(keyword_volume);
            } else {
                int delta = DEFAULT_ADJUST_STEP;
                if (has_numeric_value) {
                    delta = normalize_value_with_unit(numeric_value, unit);
                }

                if (delta <= 0) {
                    delta = DEFAULT_ADJUST_STEP;
                }

                target_volume = clamp_volume(adjust_up ? old_volume + delta : old_volume - delta);
            }
        } else {
            return build_text_response(response, "intent参数不支持", MCP_RESULT_INVALID_PARAM);
        }
    } else if (legacy_volume >= 0) {
        target_volume = clamp_volume(legacy_volume);
    } else {
        return build_text_response(response, "缺少intent或volume参数", MCP_RESULT_INVALID_PARAM);
    }

    current_volume = target_volume;

    // 调用实际的音量控制函数
    listen_set_volume(current_volume);
    
    LISA_LOGI(TAG, "Volume changed from %d%% to %d%%", old_volume, current_volume);


    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), "已完成操作，当前音量为: %d%%", listen_get_volume());

    mcp_result_t ret = build_text_response(response, result_msg, MCP_RESULT_SUCCESS);
    if (ret != MCP_RESULT_SUCCESS) {
        return ret;
    }

    return MCP_RESULT_SUCCESS;

}

// 获取当前音量的处理函数
static mcp_result_t volume_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取当前音量
    current_volume = listen_get_volume();

    LISA_LOGI(TAG, "Current volume requested: %d%%", current_volume);

    // 构建响应内容
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg),
            "当前音量为: %d%%", current_volume);

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
    cJSON_AddStringToObject(text_item, "text", result_msg);
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 生成设置音量工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_volume_control_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for volume_control schema");
        return NULL;
    }

    // type 字段
    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to volume_control schema");
        cJSON_Delete(root);
        return NULL;
    }

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *text_str = cJSON_CreateString("text");
    if (!text_str) {
        LISA_LOGE(TAG, "Failed to create text string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, text_str);

    cJSON *intent_str = cJSON_CreateString("intent");
    if (!intent_str) {
        LISA_LOGE(TAG, "Failed to create intent string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, intent_str);
    cJSON_AddItemToObject(root, "required", required);

    // properties 字段
    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // text 参数
    cJSON *text_prop = cJSON_CreateObject();
    if (!text_prop) {
        LISA_LOGE(TAG, "Failed to create text property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(text_prop, "type", "string") ||
        !cJSON_AddStringToObject(text_prop, "description", "用户输入的关于音量调节的意图")) {
        LISA_LOGE(TAG, "Failed to add text property fields");
        cJSON_Delete(text_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "text", text_prop);

    // intent 参数
    cJSON *intent_prop = cJSON_CreateObject();
    if (!intent_prop) {
        LISA_LOGE(TAG, "Failed to create intent property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON *intent_enum = cJSON_CreateArray();
    if (!intent_enum) {
        LISA_LOGE(TAG, "Failed to create intent enum array");
        cJSON_Delete(intent_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("set"));
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("adjustUp"));
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("adjustDown"));
    
    if (!cJSON_AddStringToObject(intent_prop, "type", "string")) {
        LISA_LOGE(TAG, "Failed to add intent type");
        cJSON_Delete(intent_enum);
        cJSON_Delete(intent_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(intent_prop, "enum", intent_enum);
    cJSON_AddStringToObject(intent_prop, "description", "调节类型：set（设定到具体值或极值，如：调到10档、最大声、最小声）；adjustUp（调大声、加两档）；adjustDown（调小声、小一点）");
    cJSON_AddItemToObject(properties, "intent", intent_prop);

    // value 参数
    cJSON *value_prop = cJSON_CreateObject();
    if (!value_prop) {
        LISA_LOGE(TAG, "Failed to create value property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(value_prop, "type", "string") ||
        !cJSON_AddStringToObject(value_prop, "description", "具体的数值或状态：1. 数字（如：1, 10, 50）；2. 标准状态位：max（最大声、满格）、min（最小声、静音）、mid（中等音量），如果没有明确参数，则留空，如（大点儿声音）")) {
        LISA_LOGE(TAG, "Failed to add value property fields");
        cJSON_Delete(value_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "value", value_prop);

    // unit 参数
    cJSON *unit_prop = cJSON_CreateObject();
    if (!unit_prop) {
        LISA_LOGE(TAG, "Failed to create unit property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON *unit_enum = cJSON_CreateArray();
    if (!unit_enum) {
        LISA_LOGE(TAG, "Failed to create unit enum array");
        cJSON_Delete(unit_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("档"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("级"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("百分比"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString(""));
    
    if (!cJSON_AddStringToObject(unit_prop, "type", "string")) {
        LISA_LOGE(TAG, "Failed to add unit type");
        cJSON_Delete(unit_enum);
        cJSON_Delete(unit_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(unit_prop, "enum", unit_enum);
    cJSON_AddStringToObject(unit_prop, "description", "描述数值的单位。若指令中提及'档'、'级'、'%'则对应填写，否则留空。");
    cJSON_AddItemToObject(properties, "unit", unit_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    return root;
}

/**
 * @brief 生成获取音量工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_volume_get_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for volume_get schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to volume_get schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "properties", properties);

    // 无 required 参数
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 使用静态段注册宏注册设置音量工具
MCP_REGISTER_TOOL_STATIC(set_volume,
                          "ls.set_volume",
                          "音量控制与调节工具。当用户发出调节音量、控制声音大小的指令时调用。支持绝对值设置（如：调到50%）、相对值调节（如：大声点、调小2档）以及极值控制（如：静音、最大声）。",
                          "1.0",
                          generate_volume_control_schema,
                          1,
                          volume_control_handler,
                          false,
                          NULL);

// 使用静态段注册宏注册获取音量工具
MCP_REGISTER_TOOL_STATIC(get_volume,
                          "ls.get_volume",
                          "获取当前音量.",
                          "1.0",
                          generate_volume_get_schema,
                          0,
                          volume_get_handler,
                          false,
                          NULL);

int get_current_volume(void)
{
    // 返回当前音量
    return listen_get_volume();
}
