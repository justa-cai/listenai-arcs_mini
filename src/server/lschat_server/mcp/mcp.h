#ifndef __MCP_H__
#define __MCP_H__

/*
 * 应用层定义MCP工具使用示例
 *
 * #include <stdbool.h>
 *
 * #include "lisa_log.h"
 *
 * #include "mcp.h"
 * #include "cJSON.h"
 *
 * static cJSON *led_switch_list(const char *name)
 * {
 *
 *     cJSON *tool = mcp_tool_list_info_create_default(name, "控制LED开关状态，可以是开启或关闭");
 *     if (!tool) {
 *         return NULL;
 *     }
 *
 *     cJSON *properties = mcp_tool_info_properties_get(tool);
 *     if (!properties) {
 *         cJSON_Delete(tool);
 *         return NULL;
 *     }
 *
 *     mcp_tool_info_add_property(tool, "action", "LED开关操作，可以是 'on'、'turn_on'、'off' 或 'turn_off'", "string",
 *                                true);
 *
 *     return tool;
 * }
 *
 * static cJSON *led_switch_call(const char *id, const char *name, cJSON *args)
 * {
 *     const cJSON *action = mcp_tool_call_args_get(args, "action");
 *     if (!action) {
 *         LOGE("mcp tool call args get action failed");
 *         return NULL;
 *     }
 *
 *     cJSON *result = mcp_tool_call_result_create(name);
 *     if (!result) {
 *         return NULL;
 *     }
 *
 *     cJSON_AddStringToObject(result, "content", "LED执行失败");
 *
 *     mcp_tool_call_result_response(id, result);
 *
 *     return NULL;
 * }
 *
 * MCP_TOOL_DEFINE(builtin_led_ctrl, led_switch_list, led_switch_call);
 */
#include <stdint.h>
#include "cJSON.h"

struct mcp_tool {
    /* 工具名称 */
    char *name;
    /*
     * 工具信息查询接口
     *
     * 返回值为JSON对象，包含工具名称、描述、输入参数等信息
     *
     * 返回值示例：
     *  {
     *      "name": "builtin_led_ctrl",
     *      "description": "控制LED开关状态，可以是开启或关闭",
     *      "inputSchema": {
     *          "type": "object",
     *          "properties": {
     *              "action": {
     *                  "type": "string",
     *                  "description": "LED开关操作，可以是 'on'、'turn_on'、'off' 或 'turn_off'"
     *              }
     *          },
     *          "required": [
     *              "action"
     *          ],
     *          "additionalProperties": false
     *      }
     *  }
     */
    cJSON *(*list)(const char *name);
    /*
     * 工具调用接口
     * 若直接返回结果, 表示为同步调用, 返回NULL视为异步调用, 需要通过mcp_tool_call_result_response通知结果
     *
     * 返回值示例：
     *  {
     *      "content": "LED已打开"
     *  }
     */
    cJSON *(*call)(const char *id, const char *name, cJSON *args);
};

/* 应用层可使用, 动态添加工具 */
int mcp_tool_add(const struct mcp_tool *tool);
int mcp_tool_remove(const char *name);

/* 应用层可使用, 异步调用结果通知 */
int mcp_tool_call_result_response(const char *id, cJSON *result);

/* 应用层可使用, 辅助构建工具信息 */
cJSON *mcp_tool_list_info_create_default(const char *name, const char *desc);
cJSON *mcp_tool_info_properties_get(cJSON *tool);
cJSON *mcp_tool_info_required_get(cJSON *tool);
void mcp_tool_info_add_property(cJSON *tool, const char *name, const char *desc, const char *type, uint8_t required);
void mcp_tool_info_add_json_property(cJSON *tool, const char *name, cJSON *property, uint8_t required);

/* 应用层可使用, 辅助构建工具调用结果 */
cJSON *mcp_tool_call_result_create(const char *name);
cJSON *mcp_tool_call_args_get(cJSON *args, const char *name);

/* 应用层可使用, 定义静态工具 */
#define MCP_TOOL_CONCAT_IMPL(a, b) a##b
#define MCP_TOOL_CONCAT(a, b) MCP_TOOL_CONCAT_IMPL(a, b)

#define MCP_TOOL_DEFINE(__name__, __list__, __call__)                                                                  \
    const static struct mcp_tool MCP_TOOL_CONCAT(__mcp_static_tool_, __LINE__) __attribute__((used, section(".mcp_tool"))) = { \
        .name = #__name__,                                                                                             \
        .list = __list__,                                                                                              \
        .call = __call__,                                                                                              \
    };

/* 应用层不可使用, 处理消息 */
cJSON *mcp_process(cJSON *msg);
/* 应用层不可使用, 初始化 */
int mcp_init(void);

#endif
