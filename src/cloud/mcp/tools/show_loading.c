#include "lisa_log.h"
#include "aiui_mcp.h"
#include "show_image.h"
#include "assistant_view.h"

#define TAG "show_loading"

static mcp_result_t show_loading_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *text = "请稍等...";

    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "text") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                text = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
    }

    LISA_LOGI(TAG, "Show loading: text=%s", text);

    // FIXME: 现在只考虑了文生图的 loading
    show_image_set_waiting_state(true);
    assistant_view_show_loading(text);

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
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

static cJSON *generate_show_loading_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for show_loading schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        goto fail;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        goto fail;
    }

    cJSON *text = cJSON_CreateObject();
    if (!text) {
        goto fail;
    }

    if (!cJSON_AddStringToObject(text, "type", "string") || !cJSON_AddStringToObject(text, "description", "加载文案")) {
        goto fail;
    }

    if (!cJSON_AddItemToObject(properties, "text", text)) {
        goto fail;
    }

    if (!cJSON_AddItemToObject(root, "properties", properties)) {
        goto fail;
    }

    return root;

fail:
    if (text) {
        cJSON_Delete(text);
    }
    if (properties) {
        cJSON_Delete(properties);
    }
    if (root) {
        cJSON_Delete(root);
    }
    return NULL;
}

MCP_REGISTER_TOOL_STATIC(show_loading, "ls.built_in.show_loading", "显示loading动画", "1.0",
                         generate_show_loading_schema, 1, show_loading_handler, false, NULL);
