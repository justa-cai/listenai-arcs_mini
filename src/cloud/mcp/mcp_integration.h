#ifndef __MCP_INTEGRATION_H__
#define __MCP_INTEGRATION_H__

#include "aiui_mcp.h"
#include "cJSON.h"

/**
 * @file mcp_integration.h
 * @brief MCP 与 AIUI 云端交互模块集成接口
 * 
 * 提供 MCP 工具调用与 AIUI 消息处理的桥接功能
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 集成配置 ==================== */

/**
 * @brief MCP 服务信息配置
 */
typedef struct {
    char name[32];                      /**< 服务名称 */
    char version[16];                   /**< 服务版本 */
    char instructions[256];             /**< 服务描述 */
} mcp_server_info_t;

/**
 * @brief MCP 集成配置
 */
typedef struct {
    bool auto_register_builtin_tools;  /**< 是否自动注册内置工具 */
    bool enable_tool_call_logging;     /**< 是否启用工具调用日志 */
    uint32_t max_concurrent_calls;     /**< 最大并发调用数 */
    mcp_server_info_t server_info;     /**< 服务信息 */
} mcp_integration_config_t;

/* ==================== 核心集成接口 ==================== */

/**
 * @brief 初始化 MCP 集成模块
 * 
 * @param config 集成配置
 * @return mcp_result_t 初始化结果
 */
mcp_result_t mcp_integration_init(const mcp_integration_config_t *config);

/**
 * @brief 销毁 MCP 集成模块
 */
void mcp_integration_deinit(void);

/**
 * @brief 统一的 MCP 消息处理接口
 * 
 * 自动识别消息类型（初始化请求或工具调用）并进行相应处理
 * 
 * @param aiui_msg AIUI 消息 JSON 对象
 * @return cJSON* 响应 JSON (需要调用方释放，如果无需响应则返回 NULL)
 */
cJSON *mcp_integration_process_message(const cJSON *aiui_msg);


/**
 * @brief 处理 MCP 初始化握手协议
 * 
 * @param init_msg 初始化消息 JSON 对象
 * @return cJSON* 初始化响应 JSON (需要调用方释放)
 */
cJSON *mcp_integration_handle_initialize(const cJSON *init_msg);

/* ==================== 工具发现集成 ==================== */
/**
 * @brief 检查 AIUI 消息是否为工具调用
 * 
 * @param aiui_msg AIUI 消息 JSON
 * @return bool 是否为工具调用
 */
bool mcp_integration_is_tool_call(const cJSON *aiui_msg);

/**
 * @brief 检查 AIUI 消息是否为 MCP 初始化请求
 * 
 * @param aiui_msg AIUI 消息 JSON
 * @return bool 是否为初始化请求
 */
bool mcp_integration_is_initialize_request(const cJSON *aiui_msg);

/**
 * @brief 检查 AIUI 消息是否为工具列表请求
 * 
 * @param aiui_msg AIUI 消息 JSON
 * @return bool 是否为工具列表请求
 */
bool mcp_integration_is_tools_list_request(const cJSON *aiui_msg);

/**
 * @brief 处理工具列表请求
 * 
 * @param list_msg 工具列表请求消息 JSON 对象
 * @return cJSON* 工具列表响应 JSON (需要调用方释放)
 */
cJSON *mcp_integration_handle_tools_list(const cJSON *list_msg);

/**
 * @brief 处理工具调用请求
 * 
 * @param tool_msg 工具调用请求消息 JSON 对象
 * @return cJSON* 工具调用响应 JSON (需要调用方释放)
 */
cJSON *mcp_integration_handle_tool_call(const cJSON *tool_msg);

/**
 * @brief 生成 MCP 能力声明
 * 
 * @return cJSON* 能力声明 JSON (需要调用方释放)
 */
cJSON *mcp_integration_generate_capabilities(void);

/**
 * @brief 从 AIUI 消息中提取工具调用信息
 * 
 * @param aiui_msg AIUI 消息 JSON
 * @param tool_name 工具名称 (输出参数)
 * @param params 参数数组 (输出参数)
 * @param param_count 参数数量 (输出参数)
 * @return mcp_result_t 提取结果
 */
mcp_result_t mcp_integration_extract_tool_call(const cJSON *aiui_msg, 
                                             char **tool_name,
                                             mcp_param_t **params, 
                                             uint32_t *param_count);
/* ==================== 统计和监控 ==================== */

/**
 * @brief MCP 集成统计信息
 */
typedef struct {
    uint64_t total_calls;           /**< 总调用次数 */
    uint64_t successful_calls;      /**< 成功调用次数 */
    uint64_t failed_calls;          /**< 失败调用次数 */
    uint64_t timeout_calls;         /**< 超时调用次数 */
    uint32_t active_calls;          /**< 当前活跃调用数 */
    uint32_t registered_tools;      /**< 注册工具数量 */
    uint32_t avg_response_time_ms;  /**< 平均响应时间 */
} mcp_integration_stats_t;

/**
 * @brief 获取 MCP 集成统计信息
 * 
 * @param stats 统计信息 (输出参数)
 * @return mcp_result_t 操作结果
 */
mcp_result_t mcp_integration_get_stats(mcp_integration_stats_t *stats);

/**
 * @brief 重置统计信息
 */
void mcp_integration_reset_stats(void);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 获取默认集成配置
 * 
 * @return mcp_integration_config_t 默认配置
 */
mcp_integration_config_t mcp_integration_get_default_config(void);

/**
 * @brief 验证集成配置
 * 
 * @param config 配置对象
 * @return bool 配置是否有效
 */
bool mcp_integration_validate_config(const mcp_integration_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* __MCP_INTEGRATION_H__ */
