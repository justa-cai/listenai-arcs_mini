#ifndef __LISA_EVS_SDK_WEBSOCKET_H__
#define __LISA_EVS_SDK_WEBSOCKET_H__

#include <stdbool.h>
#include <stdint.h>
#include "lisa_evs_error.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "lisa_semaphore.h"
#include "lisa_thread.h"
#include "lisa_time.h"
#include "lisa_typedef.h"

#include "lisa_websocket.h"

#define MAX_TOKEN_SIZE (128)
#define MAX_DID_SIZE (128)

typedef enum {
	WS_STATUS_NO_CONNECTED = 0,
	WS_STATUS_CONNECTING,
	WS_STATUS_CONNECTED,
} ws_connect_status_e;

typedef struct websocket_context_s {
	lisa_thread_t *m_pid_send;
	lisa_thread_t *m_pid_recv;
	lisa_mutex_t *m_mutex;
	lisa_mutex_t *m_rv_mutex;
	lisa_ws_t *lisa_ws_ins;
	// evs_audioencoder_t *m_encoder;

	char m_device_id[MAX_DID_SIZE];
	char m_token[MAX_TOKEN_SIZE];
	ws_connect_status_e m_connected;
	bool m_break_conn;  //是否主动断开连接
	lisa_queue_t *m_tx_msg_queue;
} websocket_context_t;

typedef struct lisa_evs_ws_s {
	bool m_enable;  // ws enable
	const struct lisa_evs_cb *m_callback;
	struct websocket_context_s *m_context;
	uint8_t *m_send_audio_buf;
	int m_buffer_size;
	bool m_audio_send_enable;
	bool m_speex_enable;
	bool m_image_send_enable;
} lisa_evs_ws_t;

lisa_evs_ws_t *lisa_evs_websocket_create(const struct lisa_evs_cb *const evs_cb);
/**
 * @brief 					连接函数
 * @param  handle           句柄
 * @param  device_id		设备ID
 * @param  token            access token
 * @return lisa_err_t
 */
int lisa_evs_websocket_connect(lisa_evs_ws_t *handle, const uint8_t *const device_id, const uint8_t *const token);
int lisa_evs_websocket_send_text(lisa_evs_ws_t *handle, const uint8_t *const msg, int len, const uint8_t *const req_id);
int lisa_evs_websocket_send_audio(lisa_evs_ws_t *handle, const uint8_t *const audio, int len);
int lisa_evs_websocket_send_image(lisa_evs_ws_t *handle, const uint8_t *const image, int len);
/**
 * @brief 					断开连接函数
 * @param  handle           句柄
 * @return lisa_err_t
 */
int lisa_evs_websocket_disconnect(lisa_evs_ws_t *handle);
void lisa_evs_websocket_begin_audio(lisa_evs_ws_t *handle, bool speex_enable);
void lisa_evs_websocket_begin_image(lisa_evs_ws_t *handle);
void lisa_evs_websocket_end_audio(lisa_evs_ws_t *handle);
void lisa_evs_websocket_end_image(lisa_evs_ws_t *handle);

void lisa_evs_websocket_reset_size(lisa_evs_ws_t *handle);
void lisa_evs_websocket_destroy(lisa_evs_ws_t *handle);

#endif
