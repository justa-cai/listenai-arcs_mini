#define TAG "RWS"

#include "lisa_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "lisa_time.h"
#include <nopoll.h>
#ifdef SEN_AUDIO_BY_ICO
#include "ico_codec.h"
#endif
#ifdef SEND_AUDIO_BY_SPEEX
#include "lisa_audioencoder.h"
#endif

#define WS_SEND_RETRY_TXT_COUNT (10)
#define WS_SEND_RETRY_BIN_COUNT (5)
#define WS_SEND_RETRY_DELAY (10000)  // 20ms
#define SEND_STACK_SIZE (12 * 1024)
#define RECV_STACK_SIZE (8 * 512)
#define WS_QUEUE_COUNT 250


#if (defined SEND_AUDIO_BY_SPEEX) || (defined SEN_AUDIO_BY_ICO)
#define SEND_AUDIO_BUFFER_SIZE 640

typedef struct lisa_audio_speex_s {
#ifdef SEND_AUDIO_BY_SPEEX
	lisa_audioencoder_t *m_audio_encoder;
#endif
#ifdef SEN_AUDIO_BY_ICO
	ivStatus m_audio_encoder;
#endif
	char *m_send_audio_buf;
	char *m_speex_buffer;
	int m_buffer_size;
} lisa_audio_speex_t;

static lisa_audio_speex_t *s_audio_speex = NULL;
#endif

typedef struct {
	lisa_ws_data_type_e m_type;	// 0 txt 1 audio
	char *m_msg;
	int m_size;
	uint32_t flash_index;
} lisa_ws_msg_t;

static lisa_queue_t *s_msg_queue = NULL;

static bool s_audio_enable = false;

/**
 * @brief websocket 断连回调
 * 
 */
static void _callback_ws_disconnect(lisa_ws_t *handle)
{
	if (handle) {
		lisa_ws_event_t ws_event;
		ws_event.what = LISA_WS_ON_DISCONNECTED;
		ws_event.user = handle->user;
		handle->inter_on_event(&ws_event);
	}
}

/**
 * @brief wesocket 连接关闭消息事件回调
 * 
 * @param ctx 
 * @param conn 
 * @param user_data 
 */
static void _on_ws_close(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data)
{
    LISA_LOGI(TAG, "_on_ws_close");
	lisa_ws_t *ins = (lisa_ws_t *)user_data;
	if (ins->ws_stop) {
		ins->ws_stop = true;
		_callback_ws_disconnect((lisa_ws_t *)user_data);
	}
}

/**
 * @brief  初始化websocket实例
 * 
 * @param req 
 * @return lisa_ws_t* 
 */
lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req)
{
    lisa_ws_t *handle =  (lisa_ws_t *)lisa_mem_calloc(1, sizeof(lisa_ws_t));
    if (handle == NULL) {
		LISA_LOGE(TAG, "handle create failed. ");
		return NULL;
	}

	LISA_LOGD(TAG, "port %s", req->port);
	LISA_LOGD(TAG, "scheme %s", req->scheme);
	LISA_LOGD(TAG, "host %s", req->host);
	LISA_LOGD(TAG, "path %s", req->path);

	strcpy((char *)handle->u_info.scheme, req->scheme);
	strcpy((char *)handle->u_info.host, req->host);
	strcpy((char *)handle->u_info.path, req->path);
	strcpy((char *)handle->u_info.port, req->port);
	handle->user = req->user;
	handle->inter_on_event = req->on_event;
	handle->timeout = req->timeout;
    handle->extra_header = req->extra_header;

#ifdef SEN_AUDIO_BY_ICO
	if (s_audio_speex == NULL) {
		s_audio_speex = lisa_mem_alloc(sizeof(lisa_audio_speex_t));
		s_audio_speex->m_audio_encoder = ico_encode_init();
		s_audio_speex->m_send_audio_buf = lisa_mem_alloc(SEND_AUDIO_BUFFER_SIZE);
		s_audio_speex->m_speex_buffer = lisa_mem_alloc(40);
	}
	s_audio_speex->m_buffer_size = 0;
#endif

#ifdef SEND_AUDIO_BY_SPEEX
	if (s_audio_speex == NULL) {
		s_audio_speex = lisa_mem_alloc(sizeof(lisa_audio_speex_t));
		s_audio_speex->m_audio_encoder = lisa_audioencoder_create();
		s_audio_speex->m_send_audio_buf = lisa_mem_alloc(SEND_AUDIO_BUFFER_SIZE);
		s_audio_speex->m_speex_buffer = lisa_mem_alloc(86);
	}
	s_audio_speex->m_buffer_size = 0;
#endif

	handle->inter_on_data = req->on_data;
	handle->th_alive = false;
	if (!s_msg_queue) {
		s_msg_queue = lisa_queue_create(WS_QUEUE_COUNT, "ws_tx_msg", sizeof(lisa_ws_msg_t));
	}
	handle->tx_msg_queue = s_msg_queue;
	if (handle->tx_msg_queue == NULL) {
		LISA_LOGE(TAG, "create tx queue failed!");
		goto ERR_TXQUE;
	}

	return handle;

ERR_RXQUE:
	if (handle->tx_msg_queue) {
		lisa_queue_delete(handle->tx_msg_queue);
		handle->tx_msg_queue = NULL;
	}
ERR_TXQUE:
	lisa_mem_free(handle);
	return handle;
}

static int _nopoll_complete_pending_write(noPollConn *conn, int max_retry)
{
	int tries = 0;
	while (tries < max_retry && errno == NOPOLL_EWOULDBLOCK && nopoll_conn_pending_write_bytes(conn) > 0) {
		nopoll_sleep(WS_SEND_RETRY_DELAY);
		if (nopoll_conn_complete_pending_write(conn) == 0) return 0;
		tries++;
	}
	return 1;
}

static uint8_t send_buffer[2560];
static int buffering_cnt = 0;

static int lisa_websocket_fragment_send(noPollConn *conn, noPollOpCode op_code, uint32_t fragment_size,
                                        const uint8_t *data, uint32_t len)
{
    uint32_t remind = len;

    while (remind > 0) {
        uint32_t send_size = remind >= fragment_size ? fragment_size : remind;

        int sended = nopoll_conn_send_frame(conn, send_size == remind, true, op_code, send_size, (noPollPtr *)data, 0);
        LISA_LOGD(TAG, "remind:%d, send_size:%d, sended:%d, fin:%d", remind, send_size, sended, send_size == remind);
        if (sended > 0) {
            remind -= sended;
            data += sended;
        } else if (sended == 0 || sended == -1 || sended == -2) {
            LISA_LOGD(TAG, "nopoll_conn_send_frame, err:%d", sended);
            int cnt = 10;
            while (nopoll_conn_pending_write_bytes(conn) && cnt) {
                nopoll_conn_complete_pending_write(conn);
                nopoll_sleep(WS_SEND_RETRY_DELAY);
                cnt--;
            }

            if (cnt == 0 && nopoll_conn_pending_write_bytes(conn) < 0) {
                LISA_LOGE(TAG, "nopoll_conn_send_frame, retry timeout");
                break;
            }
        } else {
            LISA_LOGE(TAG, "nopoll_conn_send_frame failed, err:%d", sended);
            break;
        }
        op_code = NOPOLL_CONTINUATION_FRAME;
    }

    return len - remind;
}

static void _send_thread(void *param)
{
	char *recv_cached = NULL;
	int recv_cached_size = 0;
    noPollMsg *nopoll_msg = NULL;
	lisa_ws_t *ins = (lisa_ws_t*)param;
	ins->th_alive = true;
	noPollCtx *nopoll_ctx = nopoll_ctx_new();
	noPollConn *nopoll_conn = NULL;
	noPollConnOpts *nopoll_opts = NULL;
    if (!nopoll_ctx) {
        LISA_LOGE(TAG, "nopoll_ctx_new failed");
        goto EXIT;
    }
    nopoll_opts = nopoll_conn_opts_new();
	if (nopoll_opts == NULL) {
		LISA_LOGE(TAG, "nopoll_conn_opts_new failed");
		goto EXIT;
	}
    if (ins->extra_header) {
        nopoll_conn_opts_set_extra_headers(nopoll_opts, ins->extra_header);
    }
	if (strcmp(ins->u_info.scheme, "ws") == 0) {
		nopoll_conn = nopoll_conn_new_opts(nopoll_ctx,
					nopoll_opts, ins->u_info.host, ins->u_info.port, ins->u_info.host, ins->u_info.path, NULL,
					NULL);
	} else if (strcmp(ins->u_info.scheme, "wss") == 0) {
		if (!nopoll_conn_opts_set_ssl_certs(nopoll_opts, NULL, 0, NULL, 0, NULL, 0, NULL, 0)) {
			LISA_LOGE(TAG, "set_ssl_certs err");
			goto EXIT;
		}
		nopoll_conn_opts_ssl_peer_verify(nopoll_opts, nopoll_false);
		nopoll_conn = nopoll_conn_tls_new(nopoll_ctx,
					nopoll_opts, ins->u_info.host, ins->u_info.port, ins->u_info.host, ins->u_info.path, NULL,
					NULL);
	} else {
		goto EXIT;
	}
    if (nopoll_conn == NULL) {
		LISA_LOGE(TAG, "nopoll_conn_tls_new failed");
		_callback_ws_disconnect(ins);
		goto EXIT;
	}
	if (!nopoll_conn_wait_until_connection_ready(nopoll_conn, ins->timeout)) {
		LISA_LOGE(TAG, "connection timeout");
		_callback_ws_disconnect(ins);
		goto EXIT;
	}
	nopoll_conn_set_on_close(nopoll_conn, _on_ws_close, ins);
	ins->ws_conn = true;
	ins->ws_stop = false;

#if (defined SEND_AUDIO_BY_SPEEX) || (defined SEN_AUDIO_BY_ICO)
	s_audio_speex->m_buffer_size = 0;
#endif

    lisa_ws_event_t ws_event;
    ws_event.what = LISA_WS_ON_CONNECTED;
	ws_event.user = ins->user;
    ins->inter_on_event(&ws_event);
	
	lisa_ws_msg_t chunk;
	uint32_t start_time = 0;
	uint32_t recv_time = 0;
	uint32_t send_b_count = 0;
	while (!ins->ws_stop) {
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
		lisa_err_t st = lisa_queue_pop(ins->tx_msg_queue, &chunk,
				sizeof(lisa_ws_msg_t), 20);
		if (st == LISA_OK) {
			if (chunk.m_size > 0) {
				int ret = 0;
				if (chunk.m_type == LISA_WS_TEXT) {
					if (chunk.m_size > 512) {
						ret = lisa_websocket_fragment_send(nopoll_conn, NOPOLL_TEXT_FRAME, 512, (const uint8_t *)chunk.m_msg, chunk.m_size);
					} else {
						ret = nopoll_conn_send_text(nopoll_conn, chunk.m_msg, chunk.m_size);
						LISA_LOGI(TAG, "send text ret: %d, text: %s", ret, chunk.m_msg);
						start_time = lisa_os_get_tick_ms();
						// printk("---- send text: %d bytes, start time: %d ms\r\n", chunk.m_size, start_time);
						if (ret <= 0) {
							nopoll_sleep(WS_SEND_RETRY_DELAY);
							_nopoll_complete_pending_write(nopoll_conn, 10);
						}
						LISA_LOGI(TAG, "send text end ret: %d", ret);
					}
					if (chunk.m_msg != NULL) {
						lisa_mem_free(chunk.m_msg);
					}
				} else if (chunk.m_type == LISA_WS_TTS) {
					LISA_LOGI(TAG, "send tts: %s", chunk.m_msg);
					ret = nopoll_conn_send_binary(nopoll_conn, chunk.m_msg, chunk.m_size);
					int need_flush_data_size =  nopoll_conn_pending_write_bytes(nopoll_conn);
					while (need_flush_data_size > 0 && !ins->ws_stop) {
						nopoll_sleep(WS_SEND_RETRY_DELAY);
						if (nopoll_conn_complete_pending_write(nopoll_conn) == 0) {
							LISA_LOGD(TAG, "complete flush all pending data");
							break;
						}
						need_flush_data_size = nopoll_conn_pending_write_bytes(nopoll_conn);
					}
					if (ret < 0 && !ins->ws_stop) {
						_nopoll_complete_pending_write(nopoll_conn, 5); // max cost 100ms
					}
					if (chunk.m_msg != NULL) {
						lisa_mem_free(chunk.m_msg);
					}
				} else {
					if (!s_audio_enable) {
						if (chunk.m_msg != NULL) {
                            buffering_cnt = 0;
							lisa_mem_free(chunk.m_msg);
						}
						continue;
					}
				#if (defined SEND_AUDIO_BY_SPEEX) || (defined SEN_AUDIO_BY_ICO)
					if (chunk.m_size <= 640) {
						int copy_size = 640 - s_audio_speex->m_buffer_size;
						if (chunk.m_size < copy_size) {
							memcpy(s_audio_speex->m_send_audio_buf + s_audio_speex->m_buffer_size, chunk.m_msg, chunk.m_size);
							s_audio_speex->m_buffer_size += chunk.m_size;
							if (chunk.m_msg != NULL) {
								lisa_mem_free(chunk.m_msg);
							}
						} else {
							memcpy(s_audio_speex->m_send_audio_buf + s_audio_speex->m_buffer_size, chunk.m_msg, copy_size);
							s_audio_speex->m_buffer_size += copy_size;
							#ifdef SEND_AUDIO_BY_SPEEX
							int len = lisa_audioencoder_encode(s_audio_speex->m_audio_encoder,
									s_audio_speex->m_send_audio_buf, s_audio_speex->m_buffer_size,
									s_audio_speex->m_speex_buffer);
							#endif
							#ifdef SEN_AUDIO_BY_ICO
							short ico_len = 0;
							ico_codec_encode((short *)s_audio_speex->m_send_audio_buf,
									(void *)s_audio_speex->m_speex_buffer, &ico_len);
							int len = ico_len << 1;
							#endif
							if (len > 0) {
								ret = nopoll_conn_send_binary(nopoll_conn, s_audio_speex->m_speex_buffer, len);
								int need_flush_data_size =  nopoll_conn_pending_write_bytes(nopoll_conn);
								uint32_t time = lisa_os_get_tick_ms();
								while (need_flush_data_size > 0 && !ins->ws_stop && s_audio_enable) {
									nopoll_sleep(WS_SEND_RETRY_DELAY);
									if (nopoll_conn_complete_pending_write(nopoll_conn) == 0) {
										LISA_LOGD(TAG, "complete flush all pending data");
										break;
									}
									need_flush_data_size = nopoll_conn_pending_write_bytes(nopoll_conn);

									if (lisa_os_get_tick_ms() - time > 3 * 1000) {
										LISA_LOGE(TAG, "send audio timeout");
										lisa_mem_free(chunk.m_msg);
										if (!ins->ws_stop) {
											_callback_ws_disconnect(ins);
											ins->ws_stop = true;
										}
										goto EXIT;
									}
								}
								//如果是需要等待的话，则重试
								if (ret < 0 && !ins->ws_stop && s_audio_enable) {
									_nopoll_complete_pending_write(nopoll_conn, 5); // max cost 100ms
								}
							}
							s_audio_speex->m_buffer_size = chunk.m_size - copy_size;
							if (s_audio_speex->m_buffer_size > 0) {
								memcpy(s_audio_speex->m_send_audio_buf, chunk.m_msg + copy_size, s_audio_speex->m_buffer_size);
							}
							if (chunk.m_msg != NULL) {
								lisa_mem_free(chunk.m_msg);
							}
						}
					} else {
						LISA_LOGE(TAG, "unsupport size : %d!", chunk.m_size);
					}
                #else
                    uint32_t start = lisa_os_get_tick_ms();
                    if (buffering_cnt <= sizeof(send_buffer) && !ins->ws_stop && s_audio_enable) {
                        memcpy(&send_buffer[buffering_cnt], chunk.m_msg, chunk.m_size);
                        buffering_cnt += chunk.m_size;
                        lisa_mem_free(chunk.m_msg);
                        if (buffering_cnt == sizeof(send_buffer)) {
                            ret = nopoll_conn_send_binary(nopoll_conn, send_buffer, buffering_cnt);
                            buffering_cnt = 0;
                        } else {
                            goto __RECV;
                        }
                    }
//                    printk("---- send: %d bytes, time: %d ms\r\n", sizeof(send_buffer), lisa_os_get_tick_ms() - start);
					int need_flush_data_size =  nopoll_conn_pending_write_bytes(nopoll_conn);
					while (need_flush_data_size > 0 && !ins->ws_stop && s_audio_enable) {
						nopoll_sleep(WS_SEND_RETRY_DELAY);
						if (nopoll_conn_complete_pending_write(nopoll_conn) == 0) {
							LISA_LOGD(TAG, "complete flush all pending data");
							break;
						}
						need_flush_data_size = nopoll_conn_pending_write_bytes(nopoll_conn);
					}

					send_b_count += 1;
					 uint32_t now = lisa_os_get_tick_ms();
					 if(send_b_count % 10 == 0)
					 	printf("--hky-- send: %d bytes, time: %d ms, total: %d ms\n", sizeof(send_buffer), now - start, now - start_time);
					//如果是需要等待的话，则重试
					if (ret < 0 && !ins->ws_stop && s_audio_enable) {
						_nopoll_complete_pending_write(nopoll_conn, 5); // max cost 100ms
					}
					// LISA_LOGE(TAG, "send bin ret: %d cost: %ld\n", ret, lisa_os_get_ticks() - start);
//					if (chunk.m_msg != NULL) {
//						lisa_mem_free(chunk.m_msg);
//					}
				#endif
				}
			}
		} else {
			#if (defined SEND_AUDIO_BY_SPEEX) || (defined SEN_AUDIO_BY_ICO)
			#else
            if (buffering_cnt > 0 && !ins->ws_stop && s_audio_enable) {
                int ret = nopoll_conn_send_binary(nopoll_conn, send_buffer, buffering_cnt);
                buffering_cnt = 0;
                int need_flush_data_size =  nopoll_conn_pending_write_bytes(nopoll_conn);
                while (need_flush_data_size > 0 && !ins->ws_stop && s_audio_enable) {
                    nopoll_sleep(WS_SEND_RETRY_DELAY);
                    if (nopoll_conn_complete_pending_write(nopoll_conn) == 0) {
                        LISA_LOGD(TAG, "complete flush all pending data");
                        break;
                    }
                    need_flush_data_size = nopoll_conn_pending_write_bytes(nopoll_conn);
                }

                //如果是需要等待的话，则重试
                if (ret < 0 && !ins->ws_stop && s_audio_enable) {
                    _nopoll_complete_pending_write(nopoll_conn, 5); // max cost 100ms
                }
//                printk("---send rest, %d\r\n", buffering_cnt);
            }
			#endif
        }
__RECV:
		nopoll_msg = nopoll_conn_get_msg(nopoll_conn);
		if (nopoll_msg != NULL) {
			const unsigned char *msg = nopoll_msg_get_payload(nopoll_msg);
			const int len = nopoll_msg_get_payload_size(nopoll_msg);
			if (msg == NULL || len <= 0) {
//				LISA_LOGE(TAG, "recv txt is empty!");
				nopoll_msg_unref(nopoll_msg);
				continue;
			}
			if (nopoll_msg_is_fragment(nopoll_msg)) {
				recv_cached = lisa_mem_realloc(recv_cached, len + recv_cached_size);
				if (recv_cached) {
					memcpy(recv_cached + recv_cached_size, msg, len);
					recv_cached_size += len;
					if (nopoll_msg_is_final(nopoll_msg)) {
						lisa_ws_data_t ws_data;
						ws_data.type = LISA_WS_TEXT;
						ws_data.user = ins->user;
						ws_data.buf = (uint8_t *)recv_cached;
						ws_data.len = recv_cached_size;
						ins->inter_on_data(&ws_data);
						lisa_mem_free(recv_cached);
						recv_cached = NULL;
						recv_cached_size = 0;
					}
				}
				
			} else {
				lisa_ws_data_t ws_data;
				ws_data.type = LISA_WS_TEXT;
				ws_data.user = ins->user;
				ws_data.buf = msg;
				ws_data.len = len;
				ins->inter_on_data(&ws_data);
				nopoll_msg_unref(nopoll_msg);
				if (recv_cached) {
					lisa_mem_free(recv_cached);
					recv_cached = NULL;
					recv_cached_size = 0;
				}
			}
			
		} else {
			if (!nopoll_conn_is_ok(nopoll_conn)) {
				if (!ins->ws_stop) {
					_callback_ws_disconnect(ins);
					ins->ws_stop = true;
				}
				break;
			}
		}
	}
	ins->ws_conn = false;
EXIT:
	if (recv_cached) {
		lisa_mem_free(recv_cached);
	}
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	while (lisa_queue_pop(ins->tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
		if (chunk.m_msg != NULL) {
			lisa_mem_free(chunk.m_msg);
		}
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	}
    if (nopoll_conn != NULL) {
		nopoll_conn_close(nopoll_conn);
		nopoll_conn = NULL;
	}

	if (nopoll_ctx) {
		nopoll_ctx_unref(nopoll_ctx);
		nopoll_ctx = NULL;
	}

	ins->th_alive = false;
    return;
}

/**
 * @brief 进行websocket连接
 * 
 * @param ins 
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins)
{
    if (!ins) {
        return LISA_WS_COMMON_ERR;
    }
	if (ins->ws_conn || ins->th_alive) {
		LISA_LOGE(TAG, "websocket already running");
		return LISA_WS_COMMON_ERR;
	}
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ws_send";
	thread_attr.stack_size = SEND_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_ABOVE_NORMAL; // OS_PRIORITY_HIGH
	lisa_thread_t * pid_send = lisa_thread_create(&thread_attr, _send_thread, (void *)ins);
	if (pid_send == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

/**
 * @brief 断开websocket连接
 * 
 * @param ins 
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins)
{
    if (!ins || ins->ws_stop) {
        return LISA_WS_COMMON_ERR;
    }
	ins->ws_stop = true;
	lisa_thread_mdelay(10);
    return LISA_WS_OK;
}

/**
 * @brief 通过websocket发送文本
 * 
 * @param ins 
 * @param text 
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text)
{
	if (ins == NULL || !ins->ws_conn) {
		return LISA_WS_COMMON_ERR;
	}
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	chunk.m_type = LISA_WS_TEXT;
	int len = strlen(text);
	char* msg_copy = lisa_mem_alloc(len + 1);
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "send txt malloc failed!");
		return LISA_WS_COMMON_ERR;
	}
	strcpy(msg_copy, text);
	chunk.m_msg = msg_copy;
	chunk.m_size = len;
	lisa_err_t st = lisa_queue_push(ins->tx_msg_queue, &chunk,
					sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send txt failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_tts(lisa_ws_t *ins, const uint8_t *text)
{
	if (ins == NULL || !ins->ws_conn) {
		return LISA_WS_COMMON_ERR;
	}
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	chunk.m_type = LISA_WS_TTS;
	int len = strlen(text);
	char* msg_copy = lisa_mem_alloc(len + 1);
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "send txt malloc failed!");
		return LISA_WS_COMMON_ERR;
	}
	strcpy(msg_copy, text);
	chunk.m_msg = msg_copy;
	chunk.m_size = len;
	lisa_err_t st = lisa_queue_push(ins->tx_msg_queue, &chunk,
					sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send txt failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_bin(lisa_ws_t *ins, const uint8_t *bin, uint32_t len)
{
	if (ins == NULL || !ins->ws_conn) {
		return LISA_WS_COMMON_ERR;
	}
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	chunk.m_type = LISA_WS_TTS;
	char* msg_copy = lisa_mem_alloc(len);
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "send txt malloc failed!");
		return LISA_WS_COMMON_ERR;
	}
	memcpy(msg_copy, bin, len);
	chunk.m_msg = msg_copy;
	chunk.m_size = len;
	lisa_err_t st = lisa_queue_push(ins->tx_msg_queue, &chunk,
					sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send txt failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

/**
 * @brief 通过websocket发送二进制流
 * 
 * @param ins 
 * @param buf 
 * @param len 
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len)
{
	if (ins == NULL || !ins->ws_conn) {
		return LISA_WS_COMMON_ERR;
	}

	if (lisa_queue_waiting(ins->tx_msg_queue) >= WS_QUEUE_COUNT - 5) {
		LISA_LOGE(TAG, "queue has more data, ignore this frame");
		return LISA_WS_COMMON_ERR;
	}
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));

	chunk.m_type = LISA_WS_BIN;
	char* msg_copy = lisa_mem_alloc(len);
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "send byte malloc failed!");
		return LISA_WS_COMMON_ERR;
	}
	memcpy(msg_copy, buf, len);
	chunk.m_msg = msg_copy;
	chunk.m_size = len;
	lisa_err_t st = lisa_queue_push(ins->tx_msg_queue, &chunk,
					sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send binary to queue failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_start(lisa_ws_t *ins)
{
	s_audio_enable = true;
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_stop(lisa_ws_t *ins)
{
	s_audio_enable = false;
	return LISA_WS_OK;
}

/**
 * @brief 销毁websocket实例
 * 
 * @param ins 
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins)
{
    if (ins) {
		lisa_ws_disconnect(ins);
		while (ins->th_alive) {
			ins->ws_stop = true;
			lisa_thread_mdelay(100);
		}
		// lisa_queue_delete(ins->tx_msg_queue);
		s_audio_enable = false;
		ins->tx_msg_queue = NULL;
		lisa_mem_free(ins);
		return LISA_WS_OK;
	}
	return LISA_WS_COMMON_ERR;
}
