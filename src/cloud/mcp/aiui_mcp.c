#define TAG "aiui_mcp"

#include "aiui_mcp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_timer.h"
#include "lisa_time.h"
#include "lisa_mutex.h"
#include "lisa_thread.h"
#include "evs_utils.h"

/* ==================== 内部数据结构 ==================== */


/**
 * @brief MCP 框架管理器
 */
typedef struct {
    mcp_tool_registry_t *tool_registry;     /**< 工具注册表 */
    lisa_mutex_t *registry_mutex;           /**< 注册表互斥锁 */
    bool initialized;                       /**< 是否已初始化 */
    uint32_t call_counter;                  /**< 调用计数器 */
} mcp_manager_t;

/* ==================== 全局变量 ==================== */

static mcp_manager_t g_mcp_manager = {0};

/* ==================== 外部符号声明 ==================== */

extern const mcp_tool_def_t __mcp_tool_start[];
extern const mcp_tool_def_t __mcp_tool_end[];

/* ==================== 内部函数声明 ==================== */

static mcp_result_t _validate_tool_def(const mcp_tool_def_t *tool_def);
static mcp_result_t _validate_params(const mcp_tool_def_t *tool_def, const mcp_param_t *params, uint32_t param_count);
static mcp_tool_registry_t *_find_registry_entry(const char *tool_name);
static char *_generate_call_id(void);
static void _update_tool_stats(mcp_tool_registry_t *registry, mcp_result_t result, uint32_t exec_time);

/* ==================== 核心管理接口实现 ==================== */

mcp_result_t mcp_init(void)
{
    if (g_mcp_manager.initialized) {
        LISA_LOGW(TAG, "MCP already initialized");
        return MCP_RESULT_SUCCESS;
    }

    // 初始化互斥锁
    g_mcp_manager.registry_mutex = lisa_mutex_create();
    if (!g_mcp_manager.registry_mutex) {
        LISA_LOGE(TAG, "Failed to create registry mutex");
        return MCP_RESULT_ERROR;
    }


    // 初始化其他成员
    g_mcp_manager.tool_registry = NULL;
    g_mcp_manager.call_counter = 0;
    g_mcp_manager.initialized = true;

    LISA_LOGI(TAG, "MCP framework initialized successfully");
    return MCP_RESULT_SUCCESS;
}

mcp_result_t mcp_init_static_tools(void)
{
    if (!g_mcp_manager.initialized) {
        LISA_LOGE(TAG, "MCP framework not initialized");
        return MCP_RESULT_ERROR;
    }

    // 计算段中工具定义的数量
    const mcp_tool_def_t *tool_start = __mcp_tool_start;
    const mcp_tool_def_t *tool_end = __mcp_tool_end;
    size_t tool_count = tool_end - tool_start;

    LISA_LOGI(TAG, "Initializing %d static MCP tools from section", (int)tool_count);

    if (tool_count == 0) {
        LISA_LOGW(TAG, "No static MCP tools found in section");
        return MCP_RESULT_SUCCESS;
    }

    // 遍历段中的所有工具定义并注册
    uint32_t registered_count = 0;
    uint32_t failed_count = 0;

    for (size_t i = 0; i < tool_count; i++) {
        const mcp_tool_def_t *tool_def = &tool_start[i];
        
        // 验证工具定义的有效性
        if (!tool_def->name || strlen(tool_def->name) == 0) {
            LISA_LOGW(TAG, "Skipping invalid tool definition at index %d (null or empty name)", (int)i);
            failed_count++;
            continue;
        }

        if (!tool_def->handler) {
            LISA_LOGW(TAG, "Skipping tool '%s' (null handler)", tool_def->name);
            failed_count++;
            continue;
        }

        // 注册工具
        mcp_result_t result = mcp_register_tool(tool_def);
        if (result == MCP_RESULT_SUCCESS) {
            LISA_LOGI(TAG, "Successfully registered static tool: %s", tool_def->name);
            registered_count++;
        } else {
            LISA_LOGE(TAG, "Failed to register static tool '%s': %d", tool_def->name, result);
            failed_count++;
        }
    }

    LISA_LOGI(TAG, "Static tool registration completed: %d registered, %d failed", 
              registered_count, failed_count);

    return (failed_count == 0) ? MCP_RESULT_SUCCESS : MCP_RESULT_ERROR;
}

void mcp_deinit(void)
{
    if (!g_mcp_manager.initialized) {
        return;
    }

    // 清理注册表
    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);
    mcp_tool_registry_t *registry = g_mcp_manager.tool_registry;
    while (registry) {
        mcp_tool_registry_t *next = registry->next;
        lisa_mem_free(registry);
        registry = next;
    }
    g_mcp_manager.tool_registry = NULL;
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);


    // 销毁互斥锁
    lisa_mutex_delete(g_mcp_manager.registry_mutex);

    g_mcp_manager.initialized = false;
    LISA_LOGI(TAG, "MCP framework deinitialized");
}

mcp_result_t mcp_register_tool(const mcp_tool_def_t *tool_def)
{
    if (!g_mcp_manager.initialized) {
        LISA_LOGE(TAG, "MCP not initialized");
        return MCP_RESULT_ERROR;
    }

    if (!tool_def) {
        LISA_LOGE(TAG, "Invalid tool definition");
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证工具定义
    mcp_result_t result = _validate_tool_def(tool_def);
    if (result != MCP_RESULT_SUCCESS) {
        return result;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);

    // 检查工具是否已存在
    mcp_tool_registry_t *existing = _find_registry_entry(tool_def->name);
    if (existing) {
        LISA_LOGW(TAG, "Tool '%s' already registered", tool_def->name);
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_ERROR;
    }

    // 创建注册项
    mcp_tool_registry_t *registry = (mcp_tool_registry_t *)lisa_mem_calloc(1, sizeof(mcp_tool_registry_t));
    if (!registry) {
        LISA_LOGE(TAG, "Failed to allocate memory for registry");
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_ERROR;
    }

    // 复制工具定义
    mcp_tool_def_t *tool_copy = (mcp_tool_def_t *)lisa_mem_calloc(1, sizeof(mcp_tool_def_t));
    if (!tool_copy) {
        lisa_mem_free(registry);
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_ERROR;
    }

    *tool_copy = *tool_def;
    registry->tool_def = tool_copy;
    registry->is_enabled = true;
    registry->call_count = 0;
    registry->success_count = 0;
    registry->avg_exec_time_ms = 0;

    // 添加到链表头部
    registry->next = g_mcp_manager.tool_registry;
    g_mcp_manager.tool_registry = registry;

    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    LISA_LOGI(TAG, "Tool '%s' registered successfully", tool_def->name);
    return MCP_RESULT_SUCCESS;
}

mcp_result_t mcp_unregister_tool(const char *tool_name)
{
    if (!g_mcp_manager.initialized) {
        return MCP_RESULT_ERROR;
    }

    if (!tool_name) {
        return MCP_RESULT_INVALID_PARAM;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);

    mcp_tool_registry_t **current = &g_mcp_manager.tool_registry;
    while (*current) {
        if (strcmp((*current)->tool_def->name, tool_name) == 0) {
            mcp_tool_registry_t *to_remove = *current;
            *current = (*current)->next;
            
            lisa_mem_free(to_remove->tool_def);
            lisa_mem_free(to_remove);
            
            lisa_mutex_unlock(g_mcp_manager.registry_mutex);
            LISA_LOGI(TAG, "Tool '%s' unregistered successfully", tool_name);
            return MCP_RESULT_SUCCESS;
        }
        current = &(*current)->next;
    }

    lisa_mutex_unlock(g_mcp_manager.registry_mutex);
    LISA_LOGW(TAG, "Tool '%s' not found for unregistration", tool_name);
    return MCP_RESULT_NOT_FOUND;
}

mcp_result_t mcp_set_tool_enabled(const char *tool_name, bool enabled)
{
    if (!g_mcp_manager.initialized) {
        return MCP_RESULT_ERROR;
    }

    if (!tool_name) {
        return MCP_RESULT_INVALID_PARAM;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);

    mcp_tool_registry_t *registry = _find_registry_entry(tool_name);
    if (!registry) {
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_NOT_FOUND;
    }

    registry->is_enabled = enabled;
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    LISA_LOGI(TAG, "Tool '%s' %s", tool_name, enabled ? "enabled" : "disabled");
    return MCP_RESULT_SUCCESS;
}

mcp_result_t mcp_list_tools(mcp_tool_def_t ***tools, uint32_t *count)
{
    if (!g_mcp_manager.initialized) {
        return MCP_RESULT_ERROR;
    }

    if (!tools || !count) {
        return MCP_RESULT_INVALID_PARAM;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);

    // 计算工具数量
    uint32_t tool_count = 0;
    mcp_tool_registry_t *registry = g_mcp_manager.tool_registry;
    while (registry) {
        tool_count++;
        registry = registry->next;
    }

    if (tool_count == 0) {
        *tools = NULL;
        *count = 0;
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_SUCCESS;
    }

    // 分配工具指针数组
    mcp_tool_def_t **tool_array = (mcp_tool_def_t **)lisa_mem_calloc(tool_count, sizeof(mcp_tool_def_t *));
    if (!tool_array) {
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_ERROR;
    }

    // 填充工具数组
    registry = g_mcp_manager.tool_registry;
    for (uint32_t i = 0; i < tool_count && registry; i++) {
        tool_array[i] = registry->tool_def;
        registry = registry->next;
    }

    *tools = tool_array;
    *count = tool_count;

    lisa_mutex_unlock(g_mcp_manager.registry_mutex);
    return MCP_RESULT_SUCCESS;
}

/* ==================== 工具执行接口实现 ==================== */

// 全局变量用于临时存储call_id（线程不安全，仅单线程调用）
static char g_temp_call_id_buf[128] = {0};

/**
 * @brief 设置下次工具调用的call_id
 */
void mcp_set_next_call_id(const char *call_id)
{
    if (call_id) {
        strncpy(g_temp_call_id_buf, call_id, sizeof(g_temp_call_id_buf) - 1);
        g_temp_call_id_buf[sizeof(g_temp_call_id_buf) - 1] = '\0';
    } else {
        g_temp_call_id_buf[0] = '\0';
    }
}

mcp_result_t mcp_call_tool_sync(const char *tool_name, 
                               const mcp_param_t *params, 
                               uint32_t param_count,
                               mcp_response_t *response)
{
    if (!g_mcp_manager.initialized) {
        return MCP_RESULT_ERROR;
    }

    if (!tool_name || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 查找工具
    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);
    mcp_tool_registry_t *registry = _find_registry_entry(tool_name);
    if (!registry || !registry->is_enabled) {
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_NOT_FOUND;
    }

    mcp_tool_def_t *tool_def = registry->tool_def;
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    // 验证参数
    mcp_result_t validate_result = _validate_params(tool_def, params, param_count);
    if (validate_result != MCP_RESULT_SUCCESS) {
        return validate_result;
    }

    // 准备执行上下文
    mcp_context_t ctx = {0};
    ctx.tool_name = tool_name;
    ctx.call_id = (g_temp_call_id_buf[0] != '\0') ? g_temp_call_id_buf : _generate_call_id();  // 使用全局临时call_id
    ctx.params = (mcp_param_t *)params;
    ctx.param_count = param_count;

    // 记录开始时间
    uint64_t start_time = lisa_os_get_tick_ms();

    // 执行工具
    mcp_result_t exec_result = tool_def->handler(&ctx, response);
    
    // 计算执行时间
    uint32_t exec_time = (uint32_t)(lisa_os_get_tick_ms() - start_time);
    response->exec_time_ms = exec_time;

    // 更新统计信息
    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);
    _update_tool_stats(registry, exec_result, exec_time);
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    // 释放call_id（仅当是动态生成的，不是静态缓冲区）
    if (ctx.call_id != g_temp_call_id_buf) {
        lisa_mem_free((void *)ctx.call_id);
    }
    
    // 清除临时call_id（在工具执行和清理完毕后）
    g_temp_call_id_buf[0] = '\0';
   
    return exec_result;
}

/* ==================== 工具发现接口实现 ==================== */

const mcp_tool_def_t *mcp_find_tool(const char *tool_name)
{
    if (!g_mcp_manager.initialized || !tool_name) {
        return NULL;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);
    mcp_tool_registry_t *registry = _find_registry_entry(tool_name);
    const mcp_tool_def_t *tool_def = registry ? registry->tool_def : NULL;
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    return tool_def;
}

const mcp_tool_def_t *mcp_find_tool_from_section(const char *tool_name)
{
    if (!tool_name) {
        return NULL;
    }

    // 计算段中工具定义的数量
    const mcp_tool_def_t *tool_start = __mcp_tool_start;
    const mcp_tool_def_t *tool_end = __mcp_tool_end;
    size_t tool_count = tool_end - tool_start;

    // 遍历段中的所有工具定义
    for (size_t i = 0; i < tool_count; i++) {
        const mcp_tool_def_t *tool_def = &tool_start[i];
        
        if (tool_def->name && strcmp(tool_def->name, tool_name) == 0) {
            return tool_def;
        }
    }

    return NULL;
}

bool mcp_is_tool_available(const char *tool_name)
{
    if (!g_mcp_manager.initialized || !tool_name) {
        return false;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);
    mcp_tool_registry_t *registry = _find_registry_entry(tool_name);
    bool available = (registry && registry->is_enabled);
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    return available;
}

mcp_result_t mcp_get_tool_stats(const char *tool_name, mcp_tool_registry_t *registry_out)
{
    if (!g_mcp_manager.initialized || !tool_name || !registry_out) {
        return MCP_RESULT_INVALID_PARAM;
    }

    lisa_mutex_lock(g_mcp_manager.registry_mutex, LISA_OS_WAIT_FOREVER);
    mcp_tool_registry_t *registry = _find_registry_entry(tool_name);
    if (!registry) {
        lisa_mutex_unlock(g_mcp_manager.registry_mutex);
        return MCP_RESULT_NOT_FOUND;
    }

    *registry_out = *registry;
    lisa_mutex_unlock(g_mcp_manager.registry_mutex);

    return MCP_RESULT_SUCCESS;
}

/* ==================== 辅助工具函数实现 ==================== */

mcp_param_t mcp_param_string(const char *name, const char *value)
{
    mcp_param_t param = {0};
    param.name = name;
    if (value) {
        param.value = cJSON_CreateString(value);
    } else {
        param.value = cJSON_CreateNull();
    }
    return param;
}

mcp_param_t mcp_param_int(const char *name, int value)
{
    mcp_param_t param = {0};
    param.name = name;
    param.value = cJSON_CreateNumber(value);
    return param;
}

mcp_param_t mcp_param_bool(const char *name, bool value)
{
    mcp_param_t param = {0};
    param.name = name;
    param.value = cJSON_CreateBool(value);
    return param;
}

void mcp_param_free(mcp_param_t *param)
{
    if (param && param->value) {
        cJSON_Delete(param->value);
        param->value = NULL;
    }
}

void mcp_response_free(mcp_response_t *response)
{
    if (response) {
        if (response->content) {
            cJSON_Delete(response->content);
            response->content = NULL;
        }
        // error_msg 通常是静态字符串，不需要释放
    }
}

/* ==================== 内部辅助函数实现 ==================== */

static mcp_result_t _validate_tool_def(const mcp_tool_def_t *tool_def)
{
    if (!tool_def->name || strlen(tool_def->name) == 0) {
        LISA_LOGE(TAG, "Tool name cannot be empty");
        return MCP_RESULT_INVALID_PARAM;
    }

    if (!tool_def->handler) {
        LISA_LOGE(TAG, "Tool handler cannot be NULL");
        return MCP_RESULT_INVALID_PARAM;
    }

    if (tool_def->input_count > 0 && !tool_def->input_schema) {
        LISA_LOGE(TAG, "Input schema required when input_count > 0");
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证参数定义
    for (uint32_t i = 0; i < tool_def->input_count; i++) {
        const mcp_param_def_t *param_def = &tool_def->input_schema[i];
        if (!param_def->name || strlen(param_def->name) == 0) {
            LISA_LOGE(TAG, "Parameter name cannot be empty");
            return MCP_RESULT_INVALID_PARAM;
        }
    }

    return MCP_RESULT_SUCCESS;
}

static mcp_result_t _validate_params(const mcp_tool_def_t *tool_def, const mcp_param_t *params, uint32_t param_count)
{
    if (!tool_def) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 检查必需参数
    for (uint32_t i = 0; i < tool_def->input_count; i++) {
        const mcp_param_def_t *param_def = &tool_def->input_schema[i];
        if (param_def->required) {
            bool found = false;
            for (uint32_t j = 0; j < param_count; j++) {
                if (params[j].name && strcmp(params[j].name, param_def->name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LISA_LOGE(TAG, "Required parameter '%s' not provided", param_def->name);
                return MCP_RESULT_INVALID_PARAM;
            }
        }
    }

    // 检查参数类型匹配
    for (uint32_t i = 0; i < param_count; i++) {
        const mcp_param_t *param = &params[i];
        if (!param->name) {
            continue;
        }

        // 查找参数定义
        const mcp_param_def_t *param_def = NULL;
        for (uint32_t j = 0; j < tool_def->input_count; j++) {
            if (strcmp(tool_def->input_schema[j].name, param->name) == 0) {
                param_def = &tool_def->input_schema[j];
                break;
            }
        }

        if (!param_def) {
            LISA_LOGW(TAG, "Unknown parameter '%s' provided", param->name);
            continue;
        }

        // 验证参数类型
        if (param->value) {
            bool type_valid = false;
            switch (param_def->type) {
                case MCP_PARAM_STRING:
                    type_valid = cJSON_IsString(param->value);
                    break;
                case MCP_PARAM_INTEGER:
                    type_valid = cJSON_IsNumber(param->value);
                    break;
                case MCP_PARAM_BOOLEAN:
                    type_valid = cJSON_IsBool(param->value);
                    break;
                case MCP_PARAM_OBJECT:
                    type_valid = cJSON_IsObject(param->value);
                    break;
                case MCP_PARAM_ARRAY:
                    type_valid = cJSON_IsArray(param->value);
                    break;
                default:
                    type_valid = false;
                    break;
            }

            if (!type_valid) {
                LISA_LOGE(TAG, "Parameter '%s' type mismatch", param->name);
                return MCP_RESULT_INVALID_PARAM;
            }
        }
    }

    return MCP_RESULT_SUCCESS;
}

static mcp_tool_registry_t *_find_registry_entry(const char *tool_name)
{
    if (!tool_name) {
        return NULL;
    }

    mcp_tool_registry_t *registry = g_mcp_manager.tool_registry;
    while (registry) {
        if (registry->tool_def && strcmp(registry->tool_def->name, tool_name) == 0) {
            return registry;
        }
        registry = registry->next;
    }

    return NULL;
}

static char *_generate_call_id(void)
{
    char *call_id = (char *)lisa_mem_calloc(1, 64);
    if (call_id) {
        snprintf(call_id, 64, "mcp_%08x_%08x", 
                (uint32_t)lisa_os_get_tick_ms(), 
                ++g_mcp_manager.call_counter);
    }
    return call_id;
}


static void _update_tool_stats(mcp_tool_registry_t *registry, mcp_result_t result, uint32_t exec_time)
{
    if (!registry) {
        return;
    }

    registry->call_count++;
    
    if (result == MCP_RESULT_SUCCESS) {
        registry->success_count++;
    }

    // 更新平均执行时间 (使用滑动平均)
    if (registry->call_count == 1) {
        registry->avg_exec_time_ms = exec_time;
    } else {
        registry->avg_exec_time_ms = (registry->avg_exec_time_ms * 7 + exec_time) / 8;
    }
}