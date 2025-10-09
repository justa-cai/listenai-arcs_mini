#include "lisa_evs_websocket.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#include "proc/lisa_evs_proc.h"
#include "lisa_evs.h"


#define TAG "lisa_ws"

#define WEBSOCKET_WSS (0)
#define WEBSOCKET_HOST ("ivs.iflyos.cn")
#if WEBSOCKET_WSS
#define WEBSOCKET_PORT ("443")
#define WEBSOCKET_SCHEME ("wss")
#else
#define WEBSOCKET_PORT ("80")
#define WEBSOCKET_SCHEME ("ws")
#endif
#define WEBSOCKET_PATH_FORMAT ("/embedded/v1?device_id=%s&token=%s")

#define QUEUE_COUNT (20)
#define QUEUE_AUDIO_COUNT (QUEUE_COUNT - 2)

#define THREAD_SEND_STACK_SIZE (4 * 1024)
#define THREAD_RECV_STACK_SIZE (4 * 1024)
#define THREAD_LOOP_TIME LISA_OS_WAIT_FOREVER// 20  // socket 发送和接受的轮询时间 20ms
#define MAX_AUDIO_SIZE (320)
#define WS_SEND_RETRY_TXT_COUNT (10)
#define WS_SEND_RETRY_BIN_COUNT (5)
#define WS_SEND_RETRY_DELAY (20000)  // 20ns
#define SEND_AUDIO_BUFFER_SIZE (2024)

#define OS_OK (0)

typedef struct {
	ws_msg_type_e m_type;
	char *m_msg;
	char *m_request_id;
	/* 可能由于从内存池中malloc音频过于频繁，阻塞住了i2s的取音频，所以音频固定大小 */
	uint8_t m_audio[MAX_AUDIO_SIZE];
	char *m_image;
	int m_size;
} msg_chunk_t;

typedef struct {
	char *m_msg;
	int m_size;
} recv_msg_chunk_t;


/************** WS事件回调 *******************/
static void get_ws_event_cb(lisa_ws_event_t *event)
{
	lisa_evs_ws_t *websocket = (lisa_evs_ws_t *)event->user;

	if (event->what == LISA_WS_ON_CONNECTED) {
		websocket->m_context->m_connected = WS_STATUS_CONNECTED;
		LISA_LOGD(TAG, "get_ws_event_cb LISA_WS_ON_CONNECTED");
		websocket->m_callback->websocket_cb->connected();
	} else if (event->what == LISA_WS_ON_DISCONNECTED) {
		websocket->m_context->m_connected = WS_STATUS_NO_CONNECTED;
		LISA_LOGD(TAG, "get_ws_event_cb LISA_WS_ON_DISCONNECTED");
		websocket->m_callback->websocket_cb->disconnected("");
	} else {
		LISA_LOGD(TAG, "get_ws_data_cb unknow typed  [%d]", event->what);
	}
}

/************** WS接收回调 *******************/
static void get_ws_data_cb(lisa_ws_data_t *data)
{
	lisa_evs_ws_t *websocket = (lisa_evs_ws_t *)data->user;

	if (data->len > 0) {
		if (websocket->m_callback) {
			lisa_evs_process(data->buf, data->len, websocket->m_callback);
		}
	}
}

static void _send_thread(void *param)
{
	lisa_evs_ws_t *websocket = (lisa_evs_ws_t *)param;
	static msg_chunk_t chunk;
	lisa_mutex_lock(websocket->m_context->m_mutex, 0);

	while (websocket->m_enable) {
		memset(&chunk, 0, sizeof(msg_chunk_t));
		lisa_err_t st = lisa_queue_pop(websocket->m_context->m_tx_msg_queue, &chunk,
				sizeof(msg_chunk_t), THREAD_LOOP_TIME);

		// 如果读取到数据,且数据有效 就发送
		if ((st == LISA_OK) && (chunk.m_size > 0)) {
			int ret = 0;
			if (chunk.m_type == WS_TXT) {
				LISA_LOGV(TAG, "chunk.m_msg-------->%s", chunk.m_msg);
				if (websocket->m_context->m_connected == WS_STATUS_CONNECTED) {
					ret = lisa_ws_send_text(websocket->m_context->lisa_ws_ins, chunk.m_msg);
				} else {
					LISA_LOGI(TAG, "m_connected is false ");
				}
			} else if (chunk.m_type == WS_AUDIO) {
				if (websocket->m_audio_send_enable) {
					if (chunk.m_audio != NULL) {
						memcpy(websocket->m_send_audio_buf + websocket->m_buffer_size,
								chunk.m_audio, chunk.m_size);
						websocket->m_buffer_size += chunk.m_size;
						if (websocket->m_buffer_size >= 640) {
							int len = 0;
							len = websocket->m_buffer_size;
							websocket->m_buffer_size = 0;
							if (len > 0) {
								if (websocket->m_context->m_connected == WS_STATUS_CONNECTED) {
									if (websocket->m_speex_enable) {
										// static uint8_t buffer[86];
										// len = lisa_evs_audioencoder_encode(websocket->m_context->m_encoder, websocket->m_send_audio_buf, 640, buffer);
										// ret = lisa_ws_send_binary(websocket->m_context->lisa_ws_ins, buffer, len);
									} else {
										ret = lisa_ws_send_binary(websocket->m_context->lisa_ws_ins,
												websocket->m_send_audio_buf, len);
									}
								} else {
									LISA_LOGI(TAG, "m_connected is false ");
								}
							} else {
								LISA_LOGE(TAG, "audioencoder_encode len = %d", len);
							}
						} else {
							continue;
						}
					}
				} else {
					websocket->m_buffer_size = 0;
				}
			} else if (chunk.m_type == WS_IMAGE) {
				if (websocket->m_image_send_enable) {
					ret = lisa_ws_send_binary(websocket->m_context->lisa_ws_ins, chunk.m_image, chunk.m_size);
					lisa_mem_free(chunk.m_image);
				} else {
					lisa_mem_free(chunk.m_image);
					continue;
				}
			}
			//如果是需要等待的话，则重试
			if (ret) {
				websocket->m_callback->websocket_cb->send_failed(chunk.m_type, chunk.m_request_id);
			}

			if (chunk.m_msg != NULL) {
				lisa_mem_free(chunk.m_msg);
			}
			if (chunk.m_request_id != NULL) {
				lisa_mem_free(chunk.m_request_id);
			}
		} else {
			LISA_LOGD(TAG, "_send_thread  queue timeout");
		}
	}

	while (true) {
		memset(&chunk, 0, sizeof(msg_chunk_t));
		if (lisa_queue_pop(websocket->m_context->m_tx_msg_queue, &chunk, sizeof(msg_chunk_t),
					0) == LISA_OK) {
			if (chunk.m_msg != NULL) {
				lisa_mem_free(chunk.m_msg);
			}
			if (chunk.m_request_id != NULL) {
				lisa_mem_free(chunk.m_request_id);
			}
			if (chunk.m_image) {
				lisa_mem_free(chunk.m_image);
			}
		} else {
			break;
		}
	}

	if (websocket->m_context->m_pid_send != NULL) {
		lisa_thread_delete(websocket->m_context->m_pid_send);
		websocket->m_context->m_pid_send = NULL;
	}
	lisa_mutex_unlock(websocket->m_context->m_mutex);
	LISA_LOGI(TAG, "send thread quit");
	return;
}

lisa_evs_ws_t *lisa_evs_websocket_create(const struct lisa_evs_cb *const evs_cb)
{
	LISA_LOGI(TAG, "lisa evs websocket create");
	lisa_thread_attr_t thread_attr;
	lisa_evs_ws_t *handle = (lisa_evs_ws_t *)lisa_mem_alloc(sizeof(lisa_evs_ws_t));
	if (!handle) {
		return NULL;
	}
	memset((void *)handle, 0, sizeof(lisa_evs_ws_t));

	handle->m_context = (websocket_context_t *)lisa_mem_alloc(sizeof(websocket_context_t));
	if (!handle->m_context) {
		goto ERR_CONTEXT;
	}
	memset((void *)handle->m_context, 0, sizeof(websocket_context_t));

	handle->m_context->m_mutex = lisa_mutex_create();
	handle->m_context->m_rv_mutex = lisa_mutex_create();
	handle->m_context->m_tx_msg_queue =
			lisa_queue_create(QUEUE_COUNT, "lisa_ws_tx_msg", sizeof(msg_chunk_t));
	if (handle->m_context->m_tx_msg_queue == NULL) {
		LISA_LOGE(TAG, "create tx queue failed!");
		goto ERR_TXQUE;
	}

	handle->m_send_audio_buf = lisa_mem_alloc(SEND_AUDIO_BUFFER_SIZE);
	if (!handle->m_send_audio_buf) {
		LISA_LOGE(TAG, "create m_send_audio_buf failed!");
		goto ERR_AUDIOBUF;
	}
	handle->m_buffer_size = 0;
	/* enable websocket send and recv flag */
	handle->m_enable = true;
	handle->m_speex_enable = false;

	/* create ws send & ws receive*/
	thread_attr.name = "lisa_ws_send_thread";
	thread_attr.stack_size = THREAD_SEND_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_ABOVE_NORMAL; // OS_PRIORITY_ABOVE_NORMAL

	handle->m_context->m_pid_send = lisa_thread_create(&thread_attr, _send_thread, (void *)handle);
	LISA_LOGI(TAG, "create_send_thread!");
	if (!handle->m_context->m_pid_send) {
		LISA_LOGE(TAG, "create send thread error");
		goto ERR_SENDPTHREAD;
	};

	// handle->m_context->m_encoder = lisa_evs_audioencoder_create();
	// if (!handle->m_context->m_encoder) {
	// 	goto ERR_ENCODER;
	// }

	handle->m_callback = evs_cb;
	return handle;

ERR_ENCODER:
	lisa_thread_delete(handle->m_context->m_pid_send);
ERR_SENDPTHREAD:
	lisa_mem_free(handle->m_send_audio_buf);
ERR_AUDIOBUF:
	lisa_queue_delete(handle->m_context->m_tx_msg_queue);
ERR_TXQUE:
	lisa_mem_free(handle->m_context);
ERR_CONTEXT:
	lisa_mem_free(handle);
	return NULL;
}

void lisa_evs_websocket_destroy(lisa_evs_ws_t *handle)
{
	if (handle != NULL && handle->m_context != NULL) {
		handle->m_enable = false;

		lisa_evs_websocket_disconnect(handle);

		//等待任务线程结束
		lisa_mutex_lock(handle->m_context->m_mutex, LISA_WAIT_FOREVER);
		lisa_mutex_unlock(handle->m_context->m_mutex);
		lisa_mutex_lock(handle->m_context->m_rv_mutex, LISA_WAIT_FOREVER);
		lisa_mutex_unlock(handle->m_context->m_rv_mutex);
		lisa_thread_mdelay(10);
		if (handle->m_context->m_pid_send != NULL) {
			lisa_thread_delete(handle->m_context->m_pid_send);
			handle->m_context->m_pid_send = NULL;
		}
		if (handle->m_context->m_pid_recv != NULL) {
			lisa_thread_delete(handle->m_context->m_pid_recv);
			handle->m_context->m_pid_recv = NULL;
		}
		LISA_LOGI(TAG, "delete send & recv");
		if (handle->m_send_audio_buf) {
			lisa_mem_free(handle->m_send_audio_buf);
		}
		if (handle->m_context->m_tx_msg_queue) {
			lisa_queue_delete(handle->m_context->m_tx_msg_queue);
		}
		if (handle->m_context->m_rv_mutex) {
			lisa_mutex_delete(handle->m_context->m_rv_mutex);
		}
		if (handle->m_context->m_mutex) {
			lisa_mutex_delete(handle->m_context->m_mutex);
		}
		// if (handle->m_context->m_encoder) {
		// 	lisa_evs_audioencoder_destroy(handle->m_context->m_encoder);
		// }
		lisa_mem_free(handle->m_context);
		handle->m_context = NULL;
		lisa_mem_free(handle);
	}
}

void lisa_evs_websocket_reset_size(lisa_evs_ws_t *handle)
{
	handle->m_buffer_size = 0;
}

int lisa_evs_websocket_connect(lisa_evs_ws_t *handle, const uint8_t *const device_id, const uint8_t *const token)
{
	LISA_LOGD(TAG, "lisa evs websocket connect [BEGIN]");

	lisa_ws_request_t lisa_ws_req;
	if (handle == NULL || handle->m_context == NULL) {
		LISA_LOGE(TAG, "lisa evs websocket handle or context is null");
		return -1;
	}
	if (handle->m_context->m_connected != WS_STATUS_NO_CONNECTED) {
		LISA_LOGE(TAG, "lisa evs websocket already running");
		return -2;
	}

	strcpy(handle->m_context->m_device_id, device_id);
	strcpy(handle->m_context->m_token, token);

	char *path = (char *)lisa_mem_calloc(1, 256);
	if (path == NULL) {
		LISA_LOGE(TAG, "lisa websocket path error");
		return -3;
	}
	sprintf(path, WEBSOCKET_PATH_FORMAT, handle->m_context->m_device_id,
			handle->m_context->m_token);

	lisa_ws_req.scheme = WEBSOCKET_SCHEME;
	lisa_ws_req.host = WEBSOCKET_HOST;
	lisa_ws_req.path = path;
	lisa_ws_req.timeout = WS_SEND_RETRY_DELAY;
	lisa_ws_req.port = WEBSOCKET_PORT;
	lisa_ws_req.on_data = get_ws_data_cb;
	lisa_ws_req.on_event = get_ws_event_cb;
	lisa_ws_req.user = (void *)handle;
	lisa_ws_req.extra_header = NULL;
	handle->m_context->lisa_ws_ins = lisa_ws_init(&lisa_ws_req);

	if (path) {
		lisa_mem_free(path);
	}

	if (!handle->m_context->lisa_ws_ins) {
		LISA_LOGE(TAG, "lisa evs websocket init error");
		return -1;
	}

	if (lisa_ws_connect(handle->m_context->lisa_ws_ins)) {
		LISA_LOGE(TAG, "lisa evs websocket connect error");
		return -1;
	}
	handle->m_context->m_connected = WS_STATUS_CONNECTING;
	LISA_LOGD(TAG, "lisa evs websocket connect [END]");
	return 0;
}

int lisa_evs_websocket_disconnect(lisa_evs_ws_t *handle)
{
	LISA_LOGD(TAG, "lisa evs websocket disconnect [BEGIN]");
	if (handle == NULL || handle->m_context == NULL) {
		return -1;
	}

	handle->m_context->m_connected = WS_STATUS_NO_CONNECTED;
	handle->m_context->m_break_conn = true;
	if (handle->m_context->lisa_ws_ins) {
		lisa_ws_disconnect(handle->m_context->lisa_ws_ins);
		lisa_ws_cleanup(handle->m_context->lisa_ws_ins);
		handle->m_context->lisa_ws_ins = NULL;
	}
	LISA_LOGD(TAG, "lisa evs websocket disconnect [END]");
	return 0;
}

void lisa_evs_websocket_end_audio(lisa_evs_ws_t *handle)
{
	LISA_LOGD(TAG, "lisa evs websocket end audio");
	handle->m_audio_send_enable = false;
}

void lisa_evs_websocket_begin_audio(lisa_evs_ws_t *handle, bool speex_enable)
{
	LISA_LOGD(TAG, "lisa evs websocket begin audio");
	handle->m_audio_send_enable = true;
	handle->m_speex_enable = speex_enable;
}

int lisa_evs_websocket_send_text(lisa_evs_ws_t *handle, const uint8_t *const msg, int len, const uint8_t *const req_id)
{
	LISA_LOGV(TAG, "lisa evs websocket send text [BEGIN] [%s]", msg);
	if (handle == NULL || handle->m_context == NULL ||
			handle->m_context->m_connected != WS_STATUS_CONNECTED) {
		LISA_LOGE(TAG, "lisa evs websocket send text fail, param is invalid");
		return -1;
	}

	int ret = lisa_ws_send_text(handle->m_context->lisa_ws_ins, msg);
	if(ret != 0) {
		handle->m_callback->websocket_cb->send_failed(WS_IMAGE, NULL);
		return -1;
	}
	return 0;
}

int lisa_evs_websocket_send_audio(lisa_evs_ws_t *handle, const uint8_t *const audio, int len)
{
	uint32_t vail_queue = 0;
	if (handle == NULL || handle->m_context == NULL ||
			handle->m_context->m_connected != WS_STATUS_CONNECTED) {
		// LISA_LOGE(TAG, "evs_websocket_send_bin failed");
		return -1;
	}

	//防止音频过多导致text无法发送，把音频控制在QUEUE_COUNT - 1大小内
	vail_queue = lisa_queue_waiting(handle->m_context->m_tx_msg_queue);
	// LISA_LOGE(TAG, "before send_audio wait_queue: %d", vail_queue);
	if (vail_queue >= QUEUE_AUDIO_COUNT) {
		// LISA_LOGE(TAG, "queue has more data, ignore this frame");
		return -1;
	}
	// LISA_LOGE(TAG, "lisa_evs_websocket_send_audio");
	static msg_chunk_t chunk;
	memset(&chunk, 0, sizeof(msg_chunk_t));
	chunk.m_msg = NULL;
	chunk.m_request_id = NULL;
	chunk.m_type = WS_AUDIO;
	memcpy(chunk.m_audio, audio, len);
	chunk.m_size = len;

	lisa_err_t st =
			lisa_queue_push(handle->m_context->m_tx_msg_queue, &chunk, sizeof(msg_chunk_t), 0);
	if (st != OS_OK) {
		LISA_LOGE(TAG, "send audio failed! ret: %d", st);
		return -1;
	}
	return 0;
}

int lisa_evs_websocket_send_image(lisa_evs_ws_t *handle, const uint8_t *const image, int len)
{
	if (handle == NULL || handle->m_context == NULL ||
			handle->m_context->m_connected != WS_STATUS_CONNECTED) {
		// LISA_LOGE(TAG, "evs_websocket_send_bin failed");
		return -1;
	}

	if (handle->m_image_send_enable) {
		int ret = lisa_ws_send_binary(handle->m_context->lisa_ws_ins, image, len);
		if(ret != 0) {
			handle->m_callback->websocket_cb->send_failed(WS_IMAGE, NULL);
			return -1;
		}
	}
	return 0;
}

void lisa_evs_websocket_begin_image(lisa_evs_ws_t *handle)
{
	LISA_LOGD(TAG, "lisa_evs_websocket_begin_image");
	handle->m_image_send_enable = true;
}

void lisa_evs_websocket_end_image(lisa_evs_ws_t *handle)
{
	LISA_LOGD(TAG, "lisa_evs_websocket_end_image");
	handle->m_image_send_enable = false;
}
