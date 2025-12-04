#ifndef __AIUI_MCP_H__
#define __AIUI_MCP_H__

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"

/**
 * @file aiui_mcp.h
 * @brief AIUI MCP (Model Context Protocol) 框架
 * 
 * 提供通用的 MCP 工具注册、管理和调用机制
 * 支持静态注册、动态路由和异步执行
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 基础数据类型 ==================== */

/**
 * @brief MCP 工具执行结果状态
 */
typedef enum {
    MCP_RESULT_SUCCESS = 0,     /**< 执行成功 */
    MCP_RESULT_ERROR,           /**< 执行错误 */
    MCP_RESULT_INVALID_PARAM,   /**< 参数无效 */
    MCP_RESULT_NOT_FOUND,       /**< 工具未找到 */
    MCP_RESULT_TIMEOUT,         /**< 执行超时 */
    MCP_RESULT_BUSY,            /**< 系统忙碌 */
} mcp_result_t;

/**
 * @brief MCP 工具参数类型
 */
typedef enum {
    MCP_PARAM_STRING,           /**< 字符串参数 */
    MCP_PARAM_INTEGER,          /**< 整数参数 */
    MCP_PARAM_BOOLEAN,          /**< 布尔参数 */
    MCP_PARAM_OBJECT,           /**< 对象参数 */
    MCP_PARAM_ARRAY,            /**< 数组参数 */
} mcp_param_type_t;

/**
 * @brief MCP 工具参数定义
 */
typedef struct {
    const char *name;           /**< 参数名称 */
    mcp_param_type_t type;      /**< 参数类型 */
    bool required;              /**< 是否必须 */
    const char *description;    /**< 参数描述 */
    const char *default_value;  /**< 默认值 (可选) */
    int minimum;                /**< 整数类型的最小值 (可选) */
    int maximum;                /**< 整数类型的最大值 (可选) */
    bool has_min_max;           /**< 是否有最小/最大值约束 */
} mcp_param_def_t;

/**
 * @brief MCP 工具执行参数
 */
typedef struct {
    const char *name;           /**< 参数名 */
    cJSON *value;               /**< 参数值 */
} mcp_param_t;

/**
 * @brief MCP 工具执行上下文
 */
typedef struct {
    const char *tool_name;      /**< 工具名称 */
    const char *call_id;        /**< 调用 ID */
    mcp_param_t *params;        /**< 参数数组 */
    uint32_t param_count;       /**< 参数数量 */
    void *user_data;            /**< 用户数据 */
    uint32_t timeout_ms;        /**< 超时时间 (毫秒) */
} mcp_context_t;

/**
 * @brief MCP 工具执行结果
 */
typedef struct {
    mcp_result_t result;        /**< 执行结果状态 */
    cJSON *content;             /**< 结果内容 */
    const char *error_msg;      /**< 错误消息 (可选) */
    uint32_t exec_time_ms;      /**< 执行时间 (毫秒) */
} mcp_response_t;

/* ==================== 回调函数类型 ==================== */

/**
 * @brief MCP 工具执行函数原型
 * 
 * @param ctx 执行上下文
 * @param response 执行结果 (输出参数)
 * @return mcp_result_t 执行状态
 */
typedef mcp_result_t (*mcp_tool_handler_t)(const mcp_context_t *ctx, mcp_response_t *response);

/**
 * @brief MCP 工具执行完成回调
 * 
 * @param call_id 调用 ID
 * @param response 执行结果
 * @param user_data 用户数据
 */
typedef void (*mcp_completion_callback_t)(const char *call_id, const mcp_response_t *response, void *user_data);

/* ==================== 工具定义结构 ==================== */

/**
 * @brief MCP 工具定义
 */
typedef struct {
    const char *name;                   /**< 工具名称 */
    const char *description;            /**< 工具描述 */
    const char *version;                /**< 工具版本 */
    mcp_param_def_t *input_schema;      /**< 输入参数定义 */
    uint32_t input_count;               /**< 输入参数数量 */
    mcp_tool_handler_t handler;         /**< 执行函数 */
    bool is_async;                      /**< 是否异步执行 */
    void *tool_data;                    /**< 工具私有数据 */
} mcp_tool_def_t;

/**
 * @brief MCP 工具注册信息
 */
typedef struct mcp_tool_registry {
    mcp_tool_def_t *tool_def;           /**< 工具定义 */
    struct mcp_tool_registry *next;     /**< 下一个注册项 */
    bool is_enabled;                    /**< 是否启用 */
    uint64_t call_count;                /**< 调用次数 */
    uint64_t success_count;             /**< 成功次数 */
    uint32_t avg_exec_time_ms;          /**< 平均执行时间 */
} mcp_tool_registry_t;

/* ==================== 核心管理接口 ==================== */

/**
 * @brief 初始化 MCP 框架
 * 
 * @return mcp_result_t 初始化结果
 */
mcp_result_t mcp_init(void);

/**
 * @brief 初始化静态注册的 MCP 工具
 * 
 * 遍历 mcp_tool 段中的所有工具定义并注册
 * 
 * @return mcp_result_t 初始化结果
 */
mcp_result_t mcp_init_static_tools(void);

/**
 * @brief 销毁 MCP 框架
 */
void mcp_deinit(void);

/**
 * @brief 注册 MCP 工具
 * 
 * @param tool_def 工具定义
 * @return mcp_result_t 注册结果
 */
mcp_result_t mcp_register_tool(const mcp_tool_def_t *tool_def);

/**
 * @brief 注销 MCP 工具
 * 
 * @param tool_name 工具名称
 * @return mcp_result_t 注销结果
 */
mcp_result_t mcp_unregister_tool(const char *tool_name);

/**
 * @brief 启用/禁用工具
 * 
 * @param tool_name 工具名称
 * @param enabled 是否启用
 * @return mcp_result_t 操作结果
 */
mcp_result_t mcp_set_tool_enabled(const char *tool_name, bool enabled);

/**
 * @brief 获取已注册工具列表
 * 
 * @param tools 工具列表 (输出参数)
 * @param count 工具数量 (输出参数)
 * @return mcp_result_t 操作结果
 */
mcp_result_t mcp_list_tools(mcp_tool_def_t ***tools, uint32_t *count);

/* ==================== 工具执行接口 ==================== */

/**
 * @brief 设置下次工具调用的call_id
 * 
 * @param call_id 工具调用ID
 */
void mcp_set_next_call_id(const char *call_id);
/**
 * @brief 同步调用 MCP 工具
 * 
 * @param tool_name 工具名称
 * @param params 参数数组
 * @param param_count 参数数量
 * @param response 执行结果 (输出参数)
 * @return mcp_result_t 调用结果
 */
mcp_result_t mcp_call_tool_sync(const char *tool_name, 
                               const mcp_param_t *params, 
                               uint32_t param_count,
                               mcp_response_t *response);

/* ==================== 工具发现接口 ==================== */

/**
 * @brief 查找工具 (从注册表中查找)
 * 
 * @param tool_name 工具名称
 * @return mcp_tool_def_t* 工具定义 (NULL表示未找到)
 */
const mcp_tool_def_t *mcp_find_tool(const char *tool_name);

/**
 * @brief 直接从静态段中查找工具
 * 
 * @param tool_name 工具名称
 * @return mcp_tool_def_t* 工具定义 (NULL表示未找到)
 */
const mcp_tool_def_t *mcp_find_tool_from_section(const char *tool_name);

/**
 * @brief 检查工具是否可用
 * 
 * @param tool_name 工具名称
 * @return bool 是否可用
 */
bool mcp_is_tool_available(const char *tool_name);

/**
 * @brief 获取工具统计信息
 * 
 * @param tool_name 工具名称
 * @param registry 注册信息 (输出参数)
 * @return mcp_result_t 操作结果
 */
mcp_result_t mcp_get_tool_stats(const char *tool_name, mcp_tool_registry_t *registry);

/* ==================== 辅助工具函数 ==================== */

/**
 * @brief 创建字符串参数
 * 
 * @param name 参数名
 * @param value 字符串值
 * @return mcp_param_t 参数对象
 */
mcp_param_t mcp_param_string(const char *name, const char *value);

/**
 * @brief 创建整数参数
 * 
 * @param name 参数名
 * @param value 整数值
 * @return mcp_param_t 参数对象
 */
mcp_param_t mcp_param_int(const char *name, int value);

/**
 * @brief 创建布尔参数
 * 
 * @param name 参数名
 * @param value 布尔值
 * @return mcp_param_t 参数对象
 */
mcp_param_t mcp_param_bool(const char *name, bool value);

/**
 * @brief 释放参数资源
 * 
 * @param param 参数对象
 */
void mcp_param_free(mcp_param_t *param);

/**
 * @brief 释放响应资源
 * 
 * @param response 响应对象
 */
void mcp_response_free(mcp_response_t *response);

/* ==================== 静态注册宏定义 ==================== */

/**
 * @brief 静态注册 MCP 工具的宏定义
 * 
 * 将工具定义直接放入 mcp_tool 段中，在系统初始化时统一注册
 * 
 * 使用示例:
 * MCP_REGISTER_TOOL_STATIC(my_tool, "计算工具", "1.0", my_tool_params, 2, my_tool_handler, false, NULL);
 */
#define MCP_REGISTER_TOOL_STATIC(tool_name, desc, ver, input_params, input_cnt, handler_func, async, data) \
    static const mcp_tool_def_t _mcp_tool_##tool_name __attribute__((used, section(".mcp_tool"))) = { \
        .name = #tool_name, \
        .description = desc, \
        .version = ver, \
        .input_schema = input_params, \
        .input_count = input_cnt, \
        .handler = handler_func, \
        .is_async = async, \
        .tool_data = data \
    };

/**
 * @brief 定义工具参数的宏
 */
#define MCP_PARAM_DEF(param_name, param_type, is_required, desc, default_val) \
    { .name = param_name, .type = param_type, .required = is_required, .description = desc, .default_value = default_val, .has_min_max = false }

/**
 * @brief 定义整数参数带最小/最大值约束的宏
 */
#define MCP_PARAM_DEF_INT_RANGE(param_name, is_required, desc, default_val, min_val, max_val) \
    { .name = param_name, .type = MCP_PARAM_INTEGER, .required = is_required, .description = desc, .default_value = default_val, .minimum = min_val, .maximum = max_val, .has_min_max = true }

/**
 * @brief 定义工具参数数组结束标记
 */
#define MCP_PARAM_DEF_END { .name = NULL, .has_min_max = false }

#ifdef __cplusplus
}
#endif

#endif /* __AIUI_MCP_H__ */