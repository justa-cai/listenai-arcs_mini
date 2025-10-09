#ifndef __LISA_EVS_SDK_WS_H__
#define __LISA_EVS_SDK_WS_H__

typedef enum {
	WS_TXT,
	WS_AUDIO,
	WS_IMAGE,
} ws_msg_type_e;

/// websocket 事件回调
typedef struct lisa_evs_websocket_cb {
	/**
	 * @brief 			连接成功回调
	 */
	void (*connected)();
	/**
	 * @brief 			连接失败回调
	 * @param msg		文本
	 */
	void (*connect_failed)(const uint8_t *const msg);
	/**
	 * @brief 			断开连接回调
	 * @param msg		文本
	 */
	void (*disconnected)(const uint8_t *const msg);
	/**
	 * @brief 			收到消息回调
	 * @param action	消息种类
	 * @param msg		文本
	 * @param len		文本长度
	 * @param req_id	请求id
	 */
	void (*message)(const uint8_t *const action, const uint8_t *const msg, int len, const uint8_t *const req_id);
	/**
	 * @brief 			发送失败回调
	 * @param type		类型（WS_TXT、WS_AUDIO、WS_IMAGE）
	 * @param msg		文本
	 */
	void (*send_failed)(ws_msg_type_e type, const uint8_t *const msg);
} lisa_evs_websocket_cb_t;

#endif