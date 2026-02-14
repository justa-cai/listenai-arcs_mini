#define TAG "mcp_integration"

#include "mcp_integration.h"
#include "aiui_mcp.h"
#include "vision_config.h"
#include "text2img_config.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_time.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 内部数据结构 ==================== */

/**
 * @brief MCP 集成管理器
 */
typedef struct {
    mcp_integration_config_t config;        /**< 配置信息 */
    mcp_integration_stats_t stats;          /**< 统计信息 */
    lisa_mutex_t *stats_mutex;              /**< 统计互斥锁 */
    bool initialized;                       /**< 是否已初始化 */
} mcp_integration_manager_t;

/* ==================== 全局变量 ==================== */

static mcp_integration_manager_t g_integration_manager = {0};

/* ==================== 内部函数声明 ==================== */

static void _update_stats(mcp_result_t result, uint32_t response_time_ms);
static mcp_param_t *_convert_json_to_params(const cJSON *params_json, uint32_t *param_count);
static void _free_params(mcp_param_t *params, uint32_t param_count);

/* ==================== 核心集成接口实现 ==================== */

mcp_result_t mcp_integration_init(const mcp_integration_config_t *config)
{
    if (g_integration_manager.initialized) {
        LISA_LOGW(TAG, "MCP integration already initialized");
        return MCP_RESULT_SUCCESS;
    }

    // 使用默认配置如果未提供
    if (config) {
        g_integration_manager.config = *config;
    } else {
        g_integration_manager.config = mcp_integration_get_default_config();
    }

    // 验证配置
    if (!mcp_integration_validate_config(&g_integration_manager.config)) {
        LISA_LOGE(TAG, "Invalid integration configuration");
        return MCP_RESULT_INVALID_PARAM;
    }

    
    // 初始化 vision config
    vision_config_init();

    // 初始化 text2img config (SiliconFlow API key)
    text2img_config_init();

    // 初始化 MCP 框架
    mcp_result_t result = mcp_init();
    if (result != MCP_RESULT_SUCCESS) {
        LISA_LOGE(TAG, "Failed to initialize MCP framework");
        return result;
    }

    // 初始化静态注册的工具
    if (g_integration_manager.config.auto_register_builtin_tools) {
        LISA_LOGI(TAG, "Initializing static MCP tools from section");
        
        // 从 mcp_tool 段中初始化静态注册的工具
        mcp_result_t static_result = mcp_init_static_tools();
        if (static_result != MCP_RESULT_SUCCESS) {
            LISA_LOGW(TAG, "Static tool initialization completed with errors: %d", static_result);
        }
        
        // 验证工具注册成功
        mcp_tool_def_t **tools = NULL;
        uint32_t tool_count = 0;
        if (mcp_list_tools(&tools, &tool_count) == MCP_RESULT_SUCCESS) {
            LISA_LOGI(TAG, "Successfully registered %d MCP tools total", tool_count);
            if (tools) {
                for (uint32_t i = 0; i < tool_count; i++) {
                    if (tools[i]) {
                        LISA_LOGI(TAG, "  - Tool: %s", tools[i]->name);
                    }
                }
                lisa_mem_free(tools);
            }
        } else {
            LISA_LOGW(TAG, "Failed to list registered tools");
        }
    }

    // 创建统计互斥锁
    g_integration_manager.stats_mutex = lisa_mutex_create();
    if (!g_integration_manager.stats_mutex) {
        LISA_LOGE(TAG, "Failed to create stats mutex");
        return MCP_RESULT_ERROR;
    }

    g_integration_manager.initialized = true;

    LISA_LOGI(TAG, "MCP integration initialized successfully");
    return MCP_RESULT_SUCCESS;
}

void mcp_integration_deinit(void)
{
    if (!g_integration_manager.initialized) {
        return;
    }

    // 销毁统计互斥锁
    if (g_integration_manager.stats_mutex) {
        lisa_mutex_delete(g_integration_manager.stats_mutex);
        g_integration_manager.stats_mutex = NULL;
    }

    // 销毁 MCP 框架
    mcp_deinit();

    g_integration_manager.initialized = false;
    LISA_LOGI(TAG, "MCP integration deinitialized");
}

/* ==================== 工具发现集成实现 ==================== */
bool mcp_integration_is_tool_call(const cJSON *aiui_msg)
{
    if (!aiui_msg) {
        return false;
    }

    // 检查云端消息格式: data.method: "tools/call"
    const cJSON *data = cJSON_GetObjectItem(aiui_msg, "data");
    if (cJSON_IsObject(data)) {
        const cJSON *method = cJSON_GetObjectItem(data, "method");
        if (cJSON_IsString(method)) {
            const char *method_str = cJSON_GetStringValue(method);
            return (strcmp(method_str, "tools/call") == 0);
        }
    }

    return false;
}

mcp_result_t mcp_integration_extract_tool_call(const cJSON *aiui_msg, 
                                             char **tool_name,
                                             mcp_param_t **params, 
                                             uint32_t *param_count)
{
    if (!aiui_msg || !tool_name || !params || !param_count) {
        return MCP_RESULT_INVALID_PARAM;
    }

    *tool_name = NULL;
    *params = NULL;
    *param_count = 0;

    // 按照云端消息格式解析: data.params.name 和 data.params.arguments
    const cJSON *data = cJSON_GetObjectItem(aiui_msg, "data");
    if (!cJSON_IsObject(data)) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const cJSON *data_params = cJSON_GetObjectItem(data, "params");
    if (!cJSON_IsObject(data_params)) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const cJSON *name_json = cJSON_GetObjectItem(data_params, "name");
    const cJSON *params_json = cJSON_GetObjectItem(data_params, "arguments");

    // 提取工具名称
    if (!cJSON_IsString(name_json)) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *name_str = cJSON_GetStringValue(name_json);
    
    // 直接使用云端工具名称，检查是否在已注册工具中存在
    const char *mapped_name = name_str;
    
    *tool_name = (char *)lisa_mem_calloc(1, strlen(mapped_name) + 1);
    if (!*tool_name) {
        return MCP_RESULT_ERROR;
    }
    strcpy(*tool_name, mapped_name);

    // 提取参数
    if (cJSON_IsObject(params_json)) {
        *params = _convert_json_to_params(params_json, param_count);
        if (!*params && *param_count > 0) {
            lisa_mem_free(*tool_name);
            *tool_name = NULL;
            return MCP_RESULT_ERROR;
        }
    }

    return MCP_RESULT_SUCCESS;
}

/* ==================== 统一消息处理接口 ==================== */

cJSON *mcp_integration_process_message(const cJSON *aiui_msg)
{
    if (!aiui_msg || !g_integration_manager.initialized) {
        return NULL;
    }

    // 检查是否为 MCP 相关消息
    const cJSON *action = cJSON_GetObjectItem(aiui_msg, "action");
    if (!cJSON_IsString(action) || strcmp(action->valuestring, "mcp") != 0) {
        return NULL;  // 不是 MCP 消息，不处理
    }

    // 检查初始化请求
    if (mcp_integration_is_initialize_request(aiui_msg)) {
        LISA_LOGI(TAG, "Processing MCP initialize request");
        return mcp_integration_handle_initialize(aiui_msg);
    }
    
    // 检查工具列表请求
    if (mcp_integration_is_tools_list_request(aiui_msg)) {
        LISA_LOGI(TAG, "Processing MCP tools/list request");
        return mcp_integration_handle_tools_list(aiui_msg);
    }
    
    // 检查工具调用
    if (mcp_integration_is_tool_call(aiui_msg)) {
        LISA_LOGI(TAG, "Processing MCP tool call");
        return mcp_integration_handle_tool_call(aiui_msg);
    }

    // 未知的 MCP 消息类型
    LISA_LOGW(TAG, "Unknown MCP message type");
    return NULL;
}

/* ==================== MCP 初始化握手协议实现 ==================== */

bool mcp_integration_is_initialize_request(const cJSON *aiui_msg)
{
    if (!aiui_msg) {
        return false;
    }
    
    const cJSON *data = cJSON_GetObjectItem(aiui_msg, "data");
    if (!data) {
        return false;
    }
    
    const cJSON *method = cJSON_GetObjectItem(data, "method");
    if (!cJSON_IsString(method)) {
        return false;
    }
    
    return strcmp(method->valuestring, "initialize") == 0;
}

bool mcp_integration_is_tools_list_request(const cJSON *aiui_msg)
{
    if (!aiui_msg) {
        return false;
    }
    
    const cJSON *data = cJSON_GetObjectItem(aiui_msg, "data");
    if (!data) {
        return false;
    }
    
    const cJSON *method = cJSON_GetObjectItem(data, "method");
    if (!cJSON_IsString(method)) {
        return false;
    }
    
    return strcmp(method->valuestring, "tools/list") == 0;
}

cJSON *mcp_integration_handle_initialize(const cJSON *init_msg)
{
    if (!init_msg || !g_integration_manager.initialized) {
        return NULL;
    }

    const cJSON *data = cJSON_GetObjectItem(init_msg, "data");
    if (!data) {
        return NULL;
    }
    
    const cJSON *id = cJSON_GetObjectItem(data, "id");
    if (!cJSON_IsString(id)) {
        return NULL;
    }
    
    // Parse vision capabilities from cloud
    const cJSON *params = cJSON_GetObjectItem(data, "params");
    if (params) {
        const cJSON *capabilities = cJSON_GetObjectItem(params, "capabilities");
        if (capabilities) {
            const cJSON *vision = cJSON_GetObjectItem(capabilities, "vision");
            if (vision) {
                const cJSON *url = cJSON_GetObjectItem(vision, "url");
                const cJSON *token = cJSON_GetObjectItem(vision, "token");
                
                if (url && cJSON_IsString(url) && token && cJSON_IsString(token)) {
                    // Save vision config
                    if (vision_config_set(url->valuestring, token->valuestring) == 0) {
                        LISA_LOGI(TAG, "Vision config saved from cloud: url=%s", url->valuestring);
                    } else {
                        LISA_LOGE(TAG, "Failed to save vision config");
                    }
                }
            }
        }
    }

    // 创建初始化响应
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        return NULL;
    }

    // 添加基本字段
    cJSON_AddStringToObject(response, "id", id->valuestring);
    cJSON_AddStringToObject(response, "action", "mcp");
    cJSON_AddStringToObject(response, "method", "initialize");

    // 创建结果对象
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        cJSON_Delete(response);
        return NULL;
    }

    // 添加能力声明
    cJSON *capabilities = mcp_integration_generate_capabilities();
    if (capabilities) {
        cJSON_AddItemToObject(result, "capabilities", capabilities);
    }

    // 添加服务信息
    cJSON *server_info = cJSON_CreateObject();
    if (server_info) {
        cJSON_AddStringToObject(server_info, "name", g_integration_manager.config.server_info.name);
        cJSON_AddStringToObject(server_info, "version", g_integration_manager.config.server_info.version);
        cJSON_AddItemToObject(result, "serverInfo", server_info);
    }

    // 添加服务描述
    cJSON_AddStringToObject(result, "instructions", g_integration_manager.config.server_info.instructions);

    cJSON_AddItemToObject(response, "result", result);

    LISA_LOGI(TAG, "Generated MCP initialize response for ID: %s", id->valuestring);
    return response;
}

cJSON *mcp_integration_generate_capabilities(void)
{
    cJSON *capabilities = cJSON_CreateObject();
    if (!capabilities) {
        return NULL;
    }

    // 工具能力
    cJSON *tools = cJSON_CreateObject();
    if (tools) {
        // 获取已注册的工具列表
        mcp_tool_def_t **tool_defs = NULL;
        uint32_t tool_count = 0;
        
        if (mcp_list_tools(&tool_defs, &tool_count) == MCP_RESULT_SUCCESS && tool_defs) {
            // 添加工具列表摘要信息
            cJSON_AddNumberToObject(tools, "count", tool_count);
            
            // 添加工具类别信息
            cJSON *categories = cJSON_CreateArray();
            if (categories) {
                cJSON_AddItemToArray(categories, cJSON_CreateString("system"));
                cJSON_AddItemToArray(categories, cJSON_CreateString("device"));
                cJSON_AddItemToArray(categories, cJSON_CreateString("utility"));
                cJSON_AddItemToObject(tools, "categories", categories);
            }
            
            lisa_mem_free(tool_defs);
        }
        
        cJSON_AddItemToObject(capabilities, "tools", tools);
    }

    return capabilities;
}

cJSON *mcp_integration_handle_tools_list(const cJSON *list_msg)
{
    if (!list_msg || !g_integration_manager.initialized) {
        LISA_LOGE(TAG, "Invalid parameters for tools/list request");
        return NULL;
    }

    const cJSON *data = cJSON_GetObjectItem(list_msg, "data");
    if (!data) {
        LISA_LOGE(TAG, "No data in tools/list request");
        return NULL;
    }

    const cJSON *id = cJSON_GetObjectItem(data, "id");
    if (!cJSON_IsString(id)) {
        LISA_LOGE(TAG, "No valid ID in tools/list request");
        return NULL;
    }

    // 创建 MCP 协议响应
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        LISA_LOGE(TAG, "Failed to create tools/list response object");
        return NULL;
    }

    // 按照要求的格式添加顶层字段
    cJSON_AddStringToObject(response, "id", id->valuestring);
    cJSON_AddStringToObject(response, "action", "mcp");
    cJSON_AddStringToObject(response, "method", "tools/list");

    // 创建结果对象
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        cJSON_Delete(response);
        return NULL;
    }

    // 创建工具数组
    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
        cJSON_Delete(result);
        cJSON_Delete(response);
        return NULL;
    }

    // 获取已注册的工具列表
    mcp_tool_def_t **tool_defs = NULL;
    uint32_t tool_count = 0;
    
    mcp_result_t list_result = mcp_list_tools(&tool_defs, &tool_count);
    if (list_result == MCP_RESULT_SUCCESS && tool_defs && tool_count > 0) {
        for (uint32_t i = 0; i < tool_count; i++) {
            if (tool_defs[i]) {
                cJSON *tool_obj = cJSON_CreateObject();
                if (tool_obj) {
                    // 添加工具名称
                    cJSON_AddStringToObject(tool_obj, "name", tool_defs[i]->name);
                    
                    // 添加工具描述
                    if (tool_defs[i]->description) {
                        cJSON_AddStringToObject(tool_obj, "description", tool_defs[i]->description);
                    } else {
                        cJSON_AddStringToObject(tool_obj, "description", "No description available");
                    }
                    
                    if (tool_defs[i]->input_schema) {
                    // 添加标准的 JSON Schema 输入参数模式
                    cJSON *input_schema = tool_defs[i]->input_schema();
                    if (input_schema) { 
                        cJSON_AddItemToObject(tool_obj, "inputSchema", input_schema);
                    }
                    }
                    
                    cJSON_AddItemToArray(tools_array, tool_obj);
                }
            }
        }
        
        // 释放工具列表内存
        lisa_mem_free(tool_defs);
    }

    cJSON_AddItemToObject(result, "tools", tools_array);
    
    // 添加分页信息（当前不支持分页，设置为null）
    cJSON_AddNullToObject(result, "nextCursor");
    
    cJSON_AddItemToObject(response, "result", result);

    LISA_LOGI(TAG, "Generated tools/list response with %d tools for ID: %s", tool_count, id->valuestring);
    return response;
}

cJSON *mcp_integration_handle_tool_call(const cJSON *tool_msg)
{
    if (!tool_msg || !g_integration_manager.initialized) {
        LISA_LOGE(TAG, "Invalid parameters for tool call request");
        return NULL;
    }

    const cJSON *data = cJSON_GetObjectItem(tool_msg, "data");
    if (!data) {
        LISA_LOGE(TAG, "No data in tool call request");
        return NULL;
    }

    // 提取调用 ID
    const cJSON *id = cJSON_GetObjectItem(data, "id");
    if (!cJSON_IsString(id)) {
        LISA_LOGE(TAG, "No valid ID in tool call request");
        return NULL;
    }

    const char *call_id = id->valuestring;

    // 记录开始时间用于统计
    uint32_t start_time = lisa_os_get_tick_ms();

    // 提取工具调用信息
    char *tool_name = NULL;
    mcp_param_t *params = NULL;
    uint32_t param_count = 0;

    mcp_result_t extract_result = mcp_integration_extract_tool_call(tool_msg, &tool_name, &params, &param_count);
    if (extract_result != MCP_RESULT_SUCCESS) {
        LISA_LOGE(TAG, "Failed to extract tool call information: %d", extract_result);
        
        // 创建错误响应
        cJSON *error_response = cJSON_CreateObject();
        if (error_response) {
            cJSON_AddStringToObject(error_response, "id", call_id);
            cJSON_AddStringToObject(error_response, "action", "mcp");
            cJSON_AddStringToObject(error_response, "method", "tools/call");
            
            cJSON *error_result = cJSON_CreateObject();
            if (error_result) {
                cJSON_AddNumberToObject(error_result, "code", extract_result);
                cJSON_AddStringToObject(error_result, "message", "Failed to extract tool call information");
                cJSON_AddItemToObject(error_response, "error", error_result);
            }
        }
        return error_response;
    }

    LISA_LOGI(TAG, "Calling tool: %s with %d parameters for ID: %s", tool_name, param_count, call_id);

    // 设置call_id并同步调用工具
    mcp_set_next_call_id(call_id);
    mcp_response_t mcp_response = {0};
    mcp_result_t call_result = mcp_call_tool_sync(tool_name, params, param_count, &mcp_response);

    // 计算执行时间
    uint32_t exec_time = lisa_os_get_tick_ms() - start_time;

    // 更新统计信息
    _update_stats(call_result, exec_time);

    // 创建标准响应格式
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        LISA_LOGE(TAG, "Failed to create tool call response object");
        lisa_mem_free(tool_name);
        _free_params(params, param_count);
        return NULL;
    }

    // 添加基本响应字段
    cJSON_AddStringToObject(response, "id", call_id);
    cJSON_AddStringToObject(response, "action", "mcp");
    cJSON_AddStringToObject(response, "method", "tools/call");

    // 成功响应
    cJSON *result = cJSON_CreateObject();
    if (result) {
        // 创建 content 数组
        cJSON *content_array = cJSON_CreateArray();
        if (content_array) {
            // 如果 mcp_response.content 存在，将其内容提取到 content 数组中
            if (mcp_response.content) {
                // 检查 content 是否已经是数组格式
                if (cJSON_IsArray(mcp_response.content)) {
                    // 直接复制数组内容
                    cJSON *content_copy = cJSON_Duplicate(mcp_response.content, true);
                    if (content_copy) {
                        // 将复制的数组替换到 content_array
                        cJSON_Delete(content_array);
                        content_array = content_copy;
                    }
                } else {
                    // 如果不是数组，包装为数组项
                    cJSON *content_copy = cJSON_Duplicate(mcp_response.content, true);
                    if (content_copy) {
                        cJSON_AddItemToArray(content_array, content_copy);
                    }
                }
            } else {
                // 默认成功消息
                cJSON *text_item = cJSON_CreateObject();
                if (text_item) {
                    cJSON_AddStringToObject(text_item, "type", "text");
                    if (call_result == MCP_RESULT_SUCCESS) {
                        cJSON_AddStringToObject(text_item, "text", "Tool executed successfully");
                    } else {
                        cJSON_AddStringToObject(text_item, "text", "Tool execution failed");
                    }
                    cJSON_AddItemToArray(content_array, text_item);
                }
            }

            cJSON_AddItemToObject(result, "content", content_array);
        }

        if (call_result == MCP_RESULT_SUCCESS) {
            // 添加 isError 标志
            cJSON_AddBoolToObject(result, "isError", false);
        } else {
            cJSON_AddBoolToObject(result, "isError", true);
        }

        cJSON_AddItemToObject(response, "result", result);
    }

    // 清理资源
    lisa_mem_free(tool_name);
    _free_params(params, param_count);

    return response;
}

/* ==================== 统计和监控实现 ==================== */

mcp_result_t mcp_integration_get_stats(mcp_integration_stats_t *stats)
{
    if (!g_integration_manager.initialized || !stats) {
        return MCP_RESULT_INVALID_PARAM;
    }

    lisa_mutex_lock(g_integration_manager.stats_mutex, LISA_OS_WAIT_FOREVER);
    *stats = g_integration_manager.stats;
    
    // 更新注册工具数量
    mcp_tool_def_t **tools = NULL;
    uint32_t tool_count = 0;
    if (mcp_list_tools(&tools, &tool_count) == MCP_RESULT_SUCCESS) {
        stats->registered_tools = tool_count;
        if (tools) {
            lisa_mem_free(tools);
        }
    }
    
    lisa_mutex_unlock(g_integration_manager.stats_mutex);

    return MCP_RESULT_SUCCESS;
}

void mcp_integration_reset_stats(void)
{
    if (!g_integration_manager.initialized) {
        return;
    }

    lisa_mutex_lock(g_integration_manager.stats_mutex, LISA_OS_WAIT_FOREVER);
    memset(&g_integration_manager.stats, 0, sizeof(mcp_integration_stats_t));
    lisa_mutex_unlock(g_integration_manager.stats_mutex);

    LISA_LOGI(TAG, "Integration statistics reset");
}

/* ==================== 辅助函数实现 ==================== */

mcp_integration_config_t mcp_integration_get_default_config(void)
{
    mcp_integration_config_t config = {0};
    config.auto_register_builtin_tools = true;
    config.enable_tool_call_logging = true;
    config.max_concurrent_calls = 5;
    
    // 默认服务信息
    strncpy(config.server_info.name, "arcs-mini", sizeof(config.server_info.name) - 1);
    strncpy(config.server_info.version, "1.0.0", sizeof(config.server_info.version) - 1);
    strncpy(config.server_info.instructions, "设备端MCP服务，提供系统信息查询、设备控制等工具调用功能", 
            sizeof(config.server_info.instructions) - 1);
    
    return config;
}

bool mcp_integration_validate_config(const mcp_integration_config_t *config)
{
    if (!config) {
        return false;
    }

    if (config->max_concurrent_calls == 0 || config->max_concurrent_calls > 100) {
        return false;
    }

    return true;
}

/* ==================== 内部函数实现 ==================== */


static void _update_stats(mcp_result_t result, uint32_t response_time_ms)
{
    lisa_mutex_lock(g_integration_manager.stats_mutex, LISA_OS_WAIT_FOREVER);

    if (result == MCP_RESULT_SUCCESS) {
        g_integration_manager.stats.successful_calls++;
    } else if (result == MCP_RESULT_TIMEOUT) {
        g_integration_manager.stats.timeout_calls++;
    } else {
        g_integration_manager.stats.failed_calls++;
    }

    // 更新平均响应时间
    uint64_t total_completed = g_integration_manager.stats.successful_calls + 
                              g_integration_manager.stats.failed_calls + 
                              g_integration_manager.stats.timeout_calls;
    
    if (total_completed == 1) {
        g_integration_manager.stats.avg_response_time_ms = response_time_ms;
    } else {
        g_integration_manager.stats.avg_response_time_ms = 
            (g_integration_manager.stats.avg_response_time_ms * 7 + response_time_ms) / 8;
    }

    lisa_mutex_unlock(g_integration_manager.stats_mutex);
}

static mcp_param_t *_convert_json_to_params(const cJSON *params_json, uint32_t *param_count)
{
    if (!params_json || !param_count) {
        return NULL;
    }

    *param_count = cJSON_GetArraySize(params_json);
    if (*param_count == 0) {
        return NULL;
    }

    mcp_param_t *params = (mcp_param_t *)lisa_mem_calloc(*param_count, sizeof(mcp_param_t));
    if (!params) {
        *param_count = 0;
        return NULL;
    }

    uint32_t index = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, params_json) {
        if (index >= *param_count) break;

        const char *key = item->string;
        if (key) {
            params[index].name = key;
            params[index].value = cJSON_Duplicate(item, true);
            index++;
        }
    }

    *param_count = index;
    return params;
}

static void _free_params(mcp_param_t *params, uint32_t param_count)
{
    if (!params) {
        return;
    }

    for (uint32_t i = 0; i < param_count; i++) {
        if (params[i].value) {
            cJSON_Delete(params[i].value);
        }
    }

    lisa_mem_free(params);
}
