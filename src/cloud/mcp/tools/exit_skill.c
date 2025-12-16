#include "exit_skill.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "proc_mgr.h"

#define TAG "exit_skill"

/**
 * @brief 退出会话处理函数
 *
 * 该工具用于主动结束当前对话会话，返回到待机状态。
 * 适用于用户明确表示要退出对话的场景，如"退出"、"结束对话"、"再见"等。
 */
static mcp_result_t exit_skill_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);
    
    response->result = MCP_RESULT_SUCCESS;

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Exit skill: Ending current session");

    // 触发会话结束事件
    enter_audio_idle();

    // 创建content数组
    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        LISA_LOGE(TAG, "Exit skill: content_array create fail");
        return MCP_RESULT_SUCCESS;
    }

    // 创建text item
    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        LISA_LOGE(TAG, "Exit skill: text_item create fail");
        cJSON_Delete(content_array);
        return MCP_RESULT_SUCCESS;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    
    return response->result;
}

/**
 * @brief 生成退出会话工具的参数 Schema（无参数）
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_exit_skill_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for exit_skill schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to exit_skill schema");
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

// 使用静态段注册宏注册退出会话工具
MCP_REGISTER_TOOL_STATIC(ls_built_in_exit,
                          "ls.built_in.exit",
                          "退出对话",
                          "1.0",
                          generate_exit_skill_schema,
                          0,
                          exit_skill_handler,
                          false,
                          NULL);
