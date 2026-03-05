/**
 * @file lisa_websocket.h
 * @brief LISA SDK 的 WebSocket 客户端 API
 *
 * 此文件提供 LISA SDK 的 WebSocket 客户端接口，
 * 支持安全和非安全的 WebSocket 连接，采用事件驱动架构和异步数据处理。
 */

#ifndef __LISA_WEBSOCKET__
#define __LISA_WEBSOCKET__

#include <string.h>
#include <stdint.h>
#include "lisa_err.h"

/**
 * @brief 单元测试的测试结果常量
 *
 * 这些常量定义了单元测试的可能结果。
 */
#define LISA_TEST_PASS   (int32_t)(1)   /**< 测试通过 */
#define LISA_TEST_FAILED (int32_t)(0)   /**< 测试失败 */

/**
 * @brief 单元测试结果类型
 *
 * 单元测试结果的类型别名，表示成功或失败。
 */
typedef int32_t unit_test_result_t;

/**
 * @brief WebSocket 操作结果代码
 *
 * WebSocket 操作可能的返回值枚举。
 */
typedef enum {
    LISA_WS_OK = 0,        /**< 操作成功完成 */
    LISA_WS_COMMON_ERR,    /**< 发生一般错误 */
    LISA_WS_DISCONNECT,    /**< WebSocket 连接断开 */
} lisa_ws_err_e;

/**
 * @brief WebSocket 事件类型
 *
 * WebSocket 连接生命周期中可能触发的事件类型枚举。
 */
typedef enum {
    LISA_WS_ON_ERROR,       /**< 发生错误事件 */
    LISA_WS_ON_HEADER,      /**< 收到头部信息（可能发生多次） */
    LISA_WS_ON_CONNECTED,   /**< 连接建立成功 */
    LISA_WS_ON_CONNECTING,  /**< 连接进行中 */
    LISA_WS_ON_DISCONNECTED, /**< 连接终止 */
} lisa_ws_event_e;

/**
 * @brief WebSocket 事件结构
 *
 * 包含 WebSocket 事件信息的结构。
 */
typedef struct {
    lisa_ws_event_e what;  /**< 发生的事件类型 */
    void *user;            /**< 传递给回调函数的用户定义数据 */
} lisa_ws_event_t;

/**
 * @brief WebSocket 数据类型
 *
 * 支持的 WebSocket 数据传输类型枚举。
 */
typedef enum {
    LISA_WS_TEXT,  /**< 文本数据（UTF-8 编码） */
    LISA_WS_BIN,   /**< 二进制数据 */
} lisa_ws_data_type_e;

/**
 * @brief WebSocket 数据结构
 *
 * 包含从服务器接收的 WebSocket 数据的结构。
 */
typedef struct {
    lisa_ws_data_type_e type; /**< 数据类型 */
    const void *buf;         /**< 数据缓冲区指针 */
    uint32_t len;            /**< 数据长度（字节） */
    void *user;              /**< 传递给回调函数的用户定义数据 */
} lisa_ws_data_t;

/**
 * @brief WebSocket 连接请求配置
 *
 * 包含建立 WebSocket 连接所需的所有参数的结构。
 */
typedef struct {
    uint8_t *scheme;             /**< 连接方案："ws" 用于非安全连接，"wss" 用于安全连接 */
    uint8_t *host;               /**< 服务器主机名或 IP 地址 */
    uint8_t *path;               /**< 服务器路径（例如："/websocket"） */
    uint8_t *port;               /**< 服务器端口号 */
    uint32_t timeout;            /**< 连接超时时间（毫秒） */
    const char *extra_header;    /**< 握手期间发送的额外 HTTP 头部 */
    void *user;                  /**< 传递给事件回调函数的用户定义数据 */
    void (*on_event)(lisa_ws_event_t *event); /**< WebSocket 事件的回调函数 */
    void (*on_data)(lisa_ws_data_t *data);    /**< 接收数据的回调函数 */
} lisa_ws_request_t;

/**
 * @brief URL 信息结构
 *
 * 包含解析后的 WebSocket URL 组件的结构。
 */
typedef struct {
    uint8_t scheme[16];  /**< 连接方案："ws" 或 "wss" */
    uint8_t host[512];   /**< 服务器主机名或 IP 地址 */
    uint8_t path[1024];  /**< 服务器路径 */
    uint8_t port[16];    /**< 服务器端口号 */
} url_info_t;

/**
 * @brief WebSocket 实例句柄
 *
 * 表示 WebSocket 连接实例的不透明结构。
 */
typedef struct lisa_ws lisa_ws_t;

/**
 * @brief 初始化 WebSocket 实例
 *
 * 使用提供的配置创建并初始化新的 WebSocket 实例。
 * 如果 req 为 NULL，将使用默认配置。
 *
 * @param req WebSocket 请求配置结构体指针（可以为 NULL）
 * @return 新的 WebSocket 实例指针，失败时返回 NULL
 */
lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req);

/**
 * @brief 创建使用默认配置的新 WebSocket 实例
 *
 * 便利函数，创建使用默认设置的新 WebSocket 实例。
 *
 * @return 新的 WebSocket 实例指针，失败时返回 NULL
 */
static inline lisa_ws_t *lisa_ws_new()
{
    return lisa_ws_init(NULL);
}

/**
 * @brief 连接到 WebSocket 服务器
 *
 * 建立到配置中指定的服务器的 WebSocket 连接。
 *
 * @param ins WebSocket 实例指针
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins);

/**
 * @brief 从 WebSocket 服务器断开连接
 *
 * 优雅地关闭 WebSocket 连接并释放连接资源。
 *
 * @param ins WebSocket 实例指针
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins);

/**
 * @brief 发送文本数据
 *
 * 向 WebSocket 服务器发送文本消息（UTF-8 编码）。
 *
 * @param ins WebSocket 实例指针
 * @param text 要发送的文本消息（以 null 结尾的 UTF-8 字符串）
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text);

/**
 * @brief 发送二进制数据
 *
 * 向 WebSocket 服务器发送二进制数据。
 *
 * @param ins WebSocket 实例指针
 * @param buf 二进制数据缓冲区指针
 * @param len 数据长度（字节）
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len);

/**
 * @brief 清理 WebSocket 实例
 *
 * 释放与 WebSocket 实例关联的所有资源并关闭任何活动连接。
 *
 * @param ins WebSocket 实例指针
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins);

/**
 * @brief 删除 WebSocket 实例
 *
 * 便利函数，清理并删除 WebSocket 实例。
 *
 * @param ins WebSocket 实例指针
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
static inline lisa_ws_err_e lisa_ws_delete(lisa_ws_t *ins)
{
    return lisa_ws_cleanup(ins);
}

/**
 * @brief 获取 WebSocket 配置
 *
 * 检索 WebSocket 实例的当前配置。
 *
 * @param ws WebSocket 实例指针
 * @param req 用于接收配置的结构体指针
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_cfg_get(lisa_ws_t *ws, lisa_ws_request_t *req);

/**
 * @brief 设置 WebSocket 配置
 *
 * 更新 WebSocket 实例的配置。
 *
 * @param ws WebSocket 实例指针
 * @param req 新配置结构体指针
 * @return 成功时返回 LISA_WS_OK，失败时返回错误代码
 */
lisa_ws_err_e lisa_ws_cfg_set(lisa_ws_t *ws, lisa_ws_request_t *req);

#endif //__LISA_WEBSOCKET__
