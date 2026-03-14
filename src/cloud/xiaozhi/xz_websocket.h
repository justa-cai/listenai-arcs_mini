/**
 * @file xz_websocket.h
 * @brief 小智云端 WebSocket 客户端
 * @note 基于 mbedTLS + lwIP 的 WebSocket 实现
 */

#ifndef __XZ_WEBSOCKET_H__
#define __XZ_WEBSOCKET_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** WebSocket 连接句柄 */
typedef struct xz_websocket_s *xz_websocket_t;

/** WebSocket 事件类型 */
typedef enum {
    XZ_WS_EVENT_CONNECTED,      /**< 连接成功 */
    XZ_WS_EVENT_DISCONNECTED,   /**< 连接断开 */
    XZ_WS_EVENT_ERROR,          /**< 连接错误 */
} xz_ws_event_type_e;

/** WebSocket 数据类型 */
typedef enum {
    XZ_WS_DATA_TEXT,            /**< 文本数据 */
    XZ_WS_DATA_BINARY,          /**< 二进制数据 */
    XZ_WS_DATA_PONG,            /**< PONG 响应 (心跳) */
} xz_ws_data_type_e;

/** WebSocket 事件回调 */
typedef void (*xz_ws_event_cb)(xz_websocket_t ws, xz_ws_event_type_e event, void *user);

/** WebSocket 数据回调 */
typedef void (*xz_ws_data_cb)(xz_websocket_t ws, xz_ws_data_type_e type,
                              const void *data, uint32_t len, void *user);

/** WebSocket 配置 */
typedef struct {
    const char *url;            /**< WebSocket URL (wss://host:port/path) */
    const char *token;          /**< Authorization token */
    const char *device_id;      /**< 设备 ID */
    const char *client_id;      /**< 客户端 ID */
    uint32_t timeout_ms;        /**< 连接超时时间 (毫秒) */
    void *user;                 /**< 用户数据 */
    xz_ws_event_cb on_event;    /**< 事件回调 */
    xz_ws_data_cb on_data;      /**< 数据回调 */
} xz_ws_config_t;

/**
 * @brief 创建 WebSocket 客户端
 * @param config 配置
 * @return WebSocket 句柄，失败返回 NULL
 */
xz_websocket_t xz_ws_create(const xz_ws_config_t *config);

/**
 * @brief 销毁 WebSocket 客户端
 * @param ws WebSocket 句柄
 */
void xz_ws_destroy(xz_websocket_t ws);

/**
 * @brief 连接到 WebSocket 服务器
 * @param ws WebSocket 句柄
 * @return 0 成功, -1 失败
 */
int xz_ws_connect(xz_websocket_t ws);

/**
 * @brief 断开 WebSocket 连接
 * @param ws WebSocket 句柄
 * @return 0 成功, -1 失败
 */
int xz_ws_disconnect(xz_websocket_t ws);

/**
 * @brief 发送文本消息
 * @param ws WebSocket 句柄
 * @param text 文本内容
 * @return 0 成功, -1 失败
 */
int xz_ws_send_text(xz_websocket_t ws, const char *text);

/**
 * @brief 发送二进制消息
 * @param ws WebSocket 句柄
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 0 成功, -1 失败
 */
int xz_ws_send_binary(xz_websocket_t ws, const void *data, uint32_t len);

/**
 * @brief 检查连接状态
 * @param ws WebSocket 句柄
 * @return true 已连接, false 未连接
 */
bool xz_ws_is_connected(xz_websocket_t ws);

/**
 * @brief 获取用户数据
 * @param ws WebSocket 句柄
 * @return 用户数据指针
 */
void *xz_ws_get_user_data(xz_websocket_t ws);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_WEBSOCKET_H__ */
