#define TAG "lisa-ws"

#include "lisa_websocket.h"

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "lisa_time.h"
#include "lisa_semaphore.h"

#include <nopoll.h>
#include <nopoll_private.h>

#define WS_SEND_RETRY_COUNT           (CONFIG_LISA_WEBSOCKET_SEND_RETRY_COUNT)
#define WS_SEND_RETRY_DELAY           (CONFIG_LISA_WEBSOCKET_SEND_RETRY_DELAY_NS)
#define SEND_STACK_SIZE               (CONFIG_LISA_WEBSOCKET_THREAD_STACK_SIZE)
#define WS_QUEUE_COUNT                (CONFIG_LISA_WEBSOCKET_QUEUE_COUNT)
#define WS_PINGPONG_TIMEOUT_MS        (CONFIG_LISA_WEBSOCKET_PING_PONG_TIMEOUT_MS)
#define WS_PINGPONG_RETRY_CNT         (CONFIG_LISA_WEBSOCKET_PING_PONG_RETRY_CNT)
#define LISA_WEBSOCKET_FRAME_MAX_SIZE (CONFIG_LISA_WEBSOCKET_FRAGMENT_SIZE)
#define WS_FRAGMENT_RECV_TIMEOUT_MS   (CONFIG_LISA_WEBSOCKET_FRAGMENT_RECV_TIMEOUT_MS)
#define WS_PING_PONG                  (CONFIG_LISA_WEBSOCKET_PING_PONG_MSG)
#define WS_THREAD_PRIO                (CONFIG_LISA_WEBSOCKET_THREAD_PRIO)

struct lisa_ws {
    url_info_t u_info;
    uint32_t timeout;
    bool ws_conn;
    bool ws_stop;
    void *user;
    const char *extra_header;
    lisa_queue_t *tx_msg_queue;
    void (*inter_on_event)(lisa_ws_event_t *event);
    void (*inter_on_data)(lisa_ws_data_t *data);
    lisa_semaphore_t *exiting_sem;
    lisa_semaphore_t *exited_sem;
    lisa_semaphore_t *pong_sem;
    lisa_semaphore_t *connect_sem;
    uint32_t pong_max_time_ms;
    uint8_t ping_pong_lost_cnt;
    lisa_thread_t *thread;
    noPollCtx *nopoll_ctx;
    noPollConn *nopoll_conn;
};

typedef struct {
    lisa_ws_data_type_e m_type; // 0 txt 1 audio
    char *m_msg;
    int m_size;
    uint32_t flash_index;
} lisa_ws_msg_t;

static void lisa_websocket_thread(void *param);

static void _callback_ws_disconnect(lisa_ws_t *handle)
{
    if (handle) {
        handle->ws_conn = false;
        lisa_ws_event_t ws_event;
        ws_event.what = LISA_WS_ON_DISCONNECTED;
        ws_event.user = handle->user;
        handle->inter_on_event(&ws_event);
    }
}

static void _callback_ws_connected(lisa_ws_t *handle)
{
    if (handle) {
        handle->ws_conn = true;
        lisa_ws_event_t ws_event;
        ws_event.what = LISA_WS_ON_CONNECTED;
        ws_event.user = handle->user;
        handle->inter_on_event(&ws_event);
    }
}

static void nopoll_conn_on_close(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data)
{
    LISA_NLOGI("no poll conn closed, status:%d, reason:%s", conn->peer_close_status, conn->peer_close_reason);
}

lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req)
{
    lisa_ws_t *handle = (lisa_ws_t *)lisa_mem_calloc(1, sizeof(lisa_ws_t));
    if (handle == NULL) {
        LISA_NLOGE("handle create failed. ");
        return NULL;
    }
    memset(handle, 0, sizeof(lisa_ws_t));

    if (req != NULL) {
        LISA_NLOGD("port %s", req->port);
        LISA_NLOGD("scheme %s", req->scheme);
        LISA_NLOGD("host %s", req->host);
        LISA_NLOGD("path %s", req->path);

        strcpy((char *)handle->u_info.scheme, req->scheme);
        strcpy((char *)handle->u_info.host, req->host);
        strcpy((char *)handle->u_info.path, req->path);
        strcpy((char *)handle->u_info.port, req->port);

        handle->user = req->user;
        handle->inter_on_event = req->on_event;
        handle->timeout = req->timeout;
        handle->extra_header = req->extra_header;
        handle->inter_on_data = req->on_data;
    }
    handle->pong_max_time_ms = WS_PINGPONG_TIMEOUT_MS;

    handle->tx_msg_queue = lisa_queue_create(WS_QUEUE_COUNT, "ws_tx_msg", sizeof(lisa_ws_msg_t));
    if (handle->tx_msg_queue == NULL) {
        LISA_NLOGE("create tx queue failed!");
        goto err_exit;
    }

    handle->exiting_sem = lisa_semaphore_create(1);
    if (handle->exiting_sem == NULL) {
        LISA_NLOGE("exiting_sem create failed!");
        goto err_exit;
    }
    handle->exited_sem = lisa_semaphore_create(1);
    if (handle->exited_sem == NULL) {
        LISA_NLOGE("exited_sem create failed!");
        goto err_exit;
    }
    handle->pong_sem = lisa_semaphore_create(1);
    if (handle->pong_sem == NULL) {
        LISA_NLOGE("pong_sem create failed!");
        goto err_exit;
    }
    handle->connect_sem = lisa_semaphore_create(1);
    if (handle->connect_sem == NULL) {
        LISA_NLOGE("connect_sem create failed!");
        goto err_exit;
    }

    lisa_thread_attr_t thread_attr;
    thread_attr.name = "ws_send";
    thread_attr.stack_size = SEND_STACK_SIZE;
    thread_attr.priority = WS_THREAD_PRIO;
    handle->thread = lisa_thread_create(&thread_attr, lisa_websocket_thread, (void *)handle);

    if (handle->thread == NULL) {
        LISA_NLOGE("thread create failed");
        goto err_exit;
    }

    return handle;

err_exit:
    if (handle->tx_msg_queue) {
        lisa_queue_delete(handle->tx_msg_queue);
    }

    if (handle->exiting_sem) {
        lisa_semaphore_delete(handle->exiting_sem);
    }

    if (handle->exited_sem) {
        lisa_semaphore_delete(handle->exited_sem);
    }
    if (handle->connect_sem) {
        lisa_semaphore_delete(handle->connect_sem);
    }
    if (handle->pong_sem) {
        lisa_semaphore_delete(handle->pong_sem);
    }
    if (handle->thread) {
        lisa_thread_delete(handle->thread);
    }

    lisa_mem_free(handle);

    return NULL;
}

static int lisa_websocket_fragment_send(noPollConn *conn, noPollOpCode op_code, uint32_t fragment_size,
                                        const uint8_t *data, uint32_t len)
{
    uint32_t remind = len;

    while (remind > 0) {
        uint32_t send_size = remind >= fragment_size ? fragment_size : remind;

        int sended = nopoll_conn_send_frame(conn, send_size == remind, true, op_code, send_size, (noPollPtr *)data, 0);
        LISA_NLOGD("remind:%d, send_size:%d, sended:%d, fin:%d", remind, send_size, sended, send_size == remind);
        if (sended > 0) {
            remind -= sended;
            data += sended;
        } else if (sended == 0) {
            LISA_NLOGW("nopoll_conn_send_frame, err:%d", errno);
            int cnt = WS_SEND_RETRY_COUNT;
            while (nopoll_conn_pending_write_bytes(conn) && cnt) {
                nopoll_conn_complete_pending_write(conn);
                nopoll_sleep(WS_SEND_RETRY_DELAY);
                cnt--;
            }

            if (cnt == 0 && nopoll_conn_pending_write_bytes(conn) < 0) {
                LISA_NLOGE("nopoll_conn_send_frame, retry timeout");
                break;
            }
        } else if (sended == -1) {
            LISA_NLOGE("nopoll_conn_send_frame failed, err:%d", sended);
            break;
        }
        op_code = NOPOLL_CONTINUATION_FRAME;
    }

    return len - remind;
}

static int lisa_websocket_msg_fragment_remain_recv(lisa_ws_t *ins, noPollConn *nopoll_conn, char *buf, uint32_t len)
{
    int recv_len = 0;
    uint32_t start_time = lisa_os_get_tick_ms();

    while ((recv_len < len) && (lisa_os_get_tick_ms() - start_time) < WS_FRAGMENT_RECV_TIMEOUT_MS) {
        noPollMsg *nopoll_msg;
        nopoll_msg = nopoll_conn_get_msg(nopoll_conn);

        if (nopoll_msg == NULL) {
            lisa_thread_mdelay(20);
            continue;
        }

        if (nopoll_msg->op_code == NOPOLL_PONG_FRAME) {
            lisa_semaphore_give(ins->pong_sem);
            nopoll_msg_unref(nopoll_msg);
            continue;
        }

        const unsigned char *msg = nopoll_msg_get_payload(nopoll_msg);
        LISA_NLOGD("msg fragment continue, payload size:%ld, payload:%s", nopoll_msg->payload_size, msg);

        if (nopoll_msg->is_fragment && nopoll_msg->payload_size <= (len - recv_len)) {
            memcpy(buf, msg, nopoll_msg->payload_size);
            recv_len += nopoll_msg->payload_size;
            buf += nopoll_msg->payload_size;
        } else {
            LISA_NLOGE("un expected msg received, %s, opcode:%d", msg, nopoll_msg->op_code);
        }
        nopoll_msg_unref(nopoll_msg);
    }

    if (recv_len != len) {
        LISA_NLOGE("msg fragment recv error, recv len:%d, len:%d", recv_len, len);
        return -1;
    }

    return 0;
}

static int lisa_websocket_msg_recv_proc(lisa_ws_t *ins, noPollConn *nopoll_conn)
{
    noPollMsg *nopoll_msg;
    nopoll_msg = nopoll_conn_get_msg(nopoll_conn);
    bool fragment_msg = false;

    if (nopoll_msg == NULL) {
        return -1;
    }

    /* websocket pong frame */
    if (nopoll_msg->op_code == NOPOLL_PONG_FRAME) {
        lisa_semaphore_give(ins->pong_sem);
        nopoll_msg_unref(nopoll_msg);
        return 0;
    }

    const unsigned char *msg = nopoll_msg_get_payload(nopoll_msg);
    int len = nopoll_msg_get_payload_size(nopoll_msg);

    if (msg == NULL || len <= 0) {
        nopoll_msg_unref(nopoll_msg);
        return 0;
    }

    /* reframe msg if it is a fragment */
    if (nopoll_msg->is_fragment) {
        uint32_t total_size = nopoll_msg->remain_bytes + nopoll_msg->payload_size;
        LISA_NLOGD("msg is a fragment, remain:%d, payload size:%ld, total:%d, payload: %s", nopoll_msg->remain_bytes,
                   nopoll_msg->payload_size, total_size, msg);
        char *buf = lisa_mem_alloc(total_size + 1);
        if (buf == NULL) {
            LISA_NLOGE("fragment msg buf alloc failed");
            nopoll_msg_unref(nopoll_msg);
            return -1;
        }
        memcpy(buf, msg, nopoll_msg->payload_size);
        int err = lisa_websocket_msg_fragment_remain_recv(ins, nopoll_conn, buf + nopoll_msg->payload_size,
                                                          nopoll_msg->remain_bytes);
        if (err) {
            lisa_mem_free((void *)buf);
            return err;
        }

        buf[total_size] = 0;
        msg = buf;
        len = total_size;
        fragment_msg = true;
    }

    /* complete msg received */
    lisa_ws_data_t ws_data;
    ws_data.type = (nopoll_msg->op_code == NOPOLL_TEXT_FRAME ? LISA_WS_TEXT : LISA_WS_BIN);
    ws_data.user = ins->user;
    ws_data.buf = msg;
    ws_data.len = len;
    ins->inter_on_data(&ws_data);
    /* release msg */
    nopoll_msg_unref(nopoll_msg);
    if (fragment_msg) {
        lisa_mem_free((void *)msg);
    }

    return 0;
}

static int lisa_websocket_msg_normal_send(noPollConn *nopoll_conn, lisa_ws_msg_t *msg)
{
    int err = -1;
    int retry = 0;

    do {
        if (msg->m_type == LISA_WS_TEXT) {
            err = nopoll_conn_send_text(nopoll_conn, msg->m_msg, msg->m_size);
        } else if (msg->m_type == LISA_WS_BIN) {
            err = nopoll_conn_send_binary(nopoll_conn, msg->m_msg, msg->m_size);
        }

        if (err == -1) {
            LISA_NLOGE("websocket msg send failed");
            return err;
        } else if (err == -2) {
            /* need to retry */
            nopoll_sleep(WS_SEND_RETRY_DELAY);
            if (retry++ == 3) {
                LISA_NLOGE("websocket msg send retry failed");
                return -1;
            }
        } else if (err >= 0) {
            /* pending write */
            int cnt = WS_SEND_RETRY_COUNT;
            while (nopoll_conn_pending_write_bytes(nopoll_conn) && cnt) {
                nopoll_conn_complete_pending_write(nopoll_conn);
                nopoll_sleep(WS_SEND_RETRY_DELAY);
                cnt--;
            }

            if ((cnt == 0) && (nopoll_conn_pending_write_bytes(nopoll_conn) > 0)) {
                LISA_NLOGE("nopoll_conn_send_frame, retry timeout");
                return -1;
            }
        }
    } while (err == -2);

    return 0;
}

static int lisa_websocket_msg_send(noPollConn *nopoll_conn, lisa_ws_msg_t *msg)
{
    int err = -1;

    if (nopoll_conn == NULL || msg == NULL) {
        return -1;
    }

    if (msg->m_type == LISA_WS_TEXT && msg->m_size > LISA_WEBSOCKET_FRAME_MAX_SIZE) {
        LISA_NLOGI("text msg too large, len:%d, fragment send", msg->m_size);
        err = lisa_websocket_fragment_send(nopoll_conn, NOPOLL_TEXT_FRAME, LISA_WEBSOCKET_FRAME_MAX_SIZE, msg->m_msg,
                                           msg->m_size);
    } else {
        err = lisa_websocket_msg_normal_send(nopoll_conn, msg);
    }

    return err;
}

static int lisa_websocket_msg_send_proc(lisa_ws_t *ins, noPollConn *nopoll_conn)
{
    lisa_ws_msg_t msg;
    uint8_t cnt = WS_SEND_RETRY_COUNT;

    /* pending msg send first */
    while (nopoll_conn_pending_write_bytes(nopoll_conn) && cnt--) {
        nopoll_conn_complete_pending_write(nopoll_conn);
        nopoll_sleep(WS_SEND_RETRY_DELAY);
    }

    if ((cnt == 0) && (nopoll_conn_pending_write_bytes(nopoll_conn) > 0)) {
        LISA_NLOGE("nopoll_conn_send_frame, retry timeout");
    }

    int err = lisa_queue_pop(ins->tx_msg_queue, &msg, sizeof(lisa_ws_msg_t), 20);
    if (err == 0 && msg.m_msg != NULL) {
        lisa_websocket_msg_send(nopoll_conn, &msg);
        lisa_mem_free(msg.m_msg);
    }

    return 0;
}

bool lisa_websocket_need_to_exit(lisa_ws_t *ws)
{
    return lisa_semaphore_take(ws->exiting_sem, 0) == 0;
}

static int lisa_websocket_nopoll_conn(lisa_ws_t *ins)
{
    noPollCtx *nopoll_ctx = NULL;
    noPollConn *nopoll_conn = NULL;
    noPollConnOpts *nopoll_opts = NULL;

    nopoll_ctx = nopoll_ctx_new();
    if (!nopoll_ctx) {
        LISA_NLOGE("nopoll_ctx_new failed");
        return -1;
    }

    nopoll_opts = nopoll_conn_opts_new();
    if (nopoll_opts == NULL) {
        LISA_NLOGE("nopoll_conn_opts_new failed");
        goto err_exit;
    }

    if (ins->extra_header) {
        nopoll_conn_opts_set_extra_headers(nopoll_opts, ins->extra_header);
    }

    if (strcmp(ins->u_info.scheme, "ws") == 0) {
        nopoll_conn = nopoll_conn_new_opts(nopoll_ctx, nopoll_opts, ins->u_info.host, ins->u_info.port,
                                           ins->u_info.host, ins->u_info.path, NULL, NULL);
        nopoll_opts =  NULL;

    } else if (strcmp(ins->u_info.scheme, "wss") == 0) {
        if (!nopoll_conn_opts_set_ssl_certs(nopoll_opts, NULL, 0, NULL, 0, NULL, 0, NULL, 0)) {
            LISA_NLOGE("set_ssl_certs err");
            goto err_exit;
        }
        nopoll_conn_opts_ssl_peer_verify(nopoll_opts, nopoll_false);
        nopoll_conn = nopoll_conn_tls_new(nopoll_ctx, nopoll_opts, ins->u_info.host, ins->u_info.port, ins->u_info.host,
                                          ins->u_info.path, NULL, NULL);
        nopoll_opts =  NULL;
    } else {
        LISA_NLOGE("invalid scheme:%s", ins->u_info.scheme);
        goto err_exit;
    }

    if (nopoll_conn == NULL) {
        LISA_NLOGE("nopoll conn failed");
        goto err_exit;
    }

    if (!nopoll_conn_wait_until_connection_ready(nopoll_conn, ins->timeout)) {
        LISA_NLOGE("wait conn ready timeout");
        goto err_exit;
    }
    nopoll_conn_set_on_close(nopoll_conn, nopoll_conn_on_close, ins);

    ins->nopoll_conn = nopoll_conn;
    ins->nopoll_ctx = nopoll_ctx;

    return 0;

err_exit:
    if (nopoll_conn != NULL) {
        nopoll_conn_close(nopoll_conn);
        nopoll_conn = NULL;
    }

    if (nopoll_ctx) {
        nopoll_ctx_unref(nopoll_ctx);
        nopoll_ctx = NULL;
    }

    if (nopoll_opts) {
        nopoll_conn_opts_unref(nopoll_opts);
    }

    ins->nopoll_conn = NULL;
    ins->nopoll_ctx = NULL;

    return -1;
}

static int lisa_websocket_nopoll_send_recv_proc(lisa_ws_t *ins)
{
    uint32_t last_pong_time_ms = lisa_os_get_tick_ms();

    while (1) {
        int err;
        int send_cost_time = 0;
        if (lisa_websocket_need_to_exit(ins)) {
            LISA_NLOGE("received exit sem, exiting send recv process");
            break;
        }

        if (!nopoll_conn_is_ok(ins->nopoll_conn)) {
            /* maybe the conn closed by server */
            LISA_NLOGE("invalid nopoll conn");
            break;
        }
        send_cost_time = lisa_os_get_tick_ms();
        /* min block time: 20ms */
        lisa_websocket_msg_send_proc(ins, ins->nopoll_conn);
        send_cost_time = lisa_os_get_tick_ms() - send_cost_time;
        if (lisa_websocket_need_to_exit(ins)) {
            break;
        }
        lisa_websocket_msg_recv_proc(ins, ins->nopoll_conn);

#if CONFIG_NOPOLL_DELIVER_PONG_FRAME_ENABLED
        /* websocket ping pong check */
        if ((lisa_os_get_tick_ms() - last_pong_time_ms) >= ins->pong_max_time_ms / 2) {
            nopoll_conn_send_ping(ins->nopoll_conn);
        }
        err = lisa_semaphore_take(ins->pong_sem, 0);
        if (err == 0) {
            last_pong_time_ms = lisa_os_get_tick_ms();
            ins->ping_pong_lost_cnt = 0;
        } else {
            if ((lisa_os_get_tick_ms() - last_pong_time_ms) >= (ins->pong_max_time_ms + send_cost_time)) {
                LISA_NLOGE("recv websocket pong msg timeout, curr:%d, last:%d, "
                           "timeout:%d, send cost time:%d, lost cnt:%d",
                           lisa_os_get_tick_ms(), last_pong_time_ms, ins->pong_max_time_ms, send_cost_time,
                           ins->ping_pong_lost_cnt);
                last_pong_time_ms = lisa_os_get_tick_ms();
                if (++ins->ping_pong_lost_cnt >= WS_PINGPONG_RETRY_CNT) {
                    ins->ping_pong_lost_cnt = 0;
                    break;
                }
            }
        }
#endif
    }

    LISA_NLOGI("websocket send recv process exiting...");

    /* drop all msg */
    lisa_ws_msg_t msg;
    while (lisa_queue_pop(ins->tx_msg_queue, &msg, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
        if (msg.m_msg != NULL) {
            lisa_mem_free(msg.m_msg);
        }
        memset(&msg, 0, sizeof(lisa_ws_msg_t));
    }

    // TODO: Maybe thread delete by lisa_ws_cleanup, nopoll_conn will mem leak
    /* release nopoll conn */
    if (ins->nopoll_conn != NULL) {
        nopoll_conn_close(ins->nopoll_conn);
        ins->nopoll_conn = NULL;
    }

    if (ins->nopoll_ctx) {
        nopoll_ctx_unref(ins->nopoll_ctx);
        ins->nopoll_ctx = NULL;
    }

    _callback_ws_disconnect(ins);

    /* exit done */
    lisa_semaphore_give(ins->exited_sem);
    LISA_NLOGI("websocket send recv process exit done.");

    return 0;
}

static void lisa_websocket_thread(void *param)
{
    lisa_ws_t *ins = (lisa_ws_t *)param;
    int r;

    while (1) {
        LISA_NLOGI("websocket thread waiting for connect sem");

        while (1) {
            /* wait connect or exit signal */
            r = lisa_semaphore_take(ins->connect_sem, 50);
            if (r == 0) {
                LISA_NLOGI("Got connect signal");
                break;
            }

            r = lisa_semaphore_take(ins->exiting_sem, 50);
            if (r == 0) {
                LISA_NLOGI("Got exit signal");
                lisa_semaphore_give(ins->exited_sem);
                LISA_NLOGI("websocket thread waiting for connect sem");
            }
        }

        lisa_ws_event_t ws_event;
        ws_event.what = LISA_WS_ON_CONNECTING;
        ws_event.user = ins->user;
        ins->inter_on_event(&ws_event);

        if (lisa_websocket_nopoll_conn(ins) == 0) {
            _callback_ws_connected(ins);
        } else {
            _callback_ws_disconnect(ins);
            /* wait next connect msg */
            continue;
        }
        /* block util websocket disconnected */
        lisa_websocket_nopoll_send_recv_proc(ins);
    }
}

lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins)
{
    if (!ins) {
        return LISA_WS_COMMON_ERR;
    }
    if (ins->ws_conn) {
        LISA_NLOGE("websocket already running");
        return LISA_WS_COMMON_ERR;
    }

    lisa_semaphore_reset(ins->exiting_sem);
    lisa_semaphore_give(ins->connect_sem);

    return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins)
{
    int err;
    if (ins == NULL) {
        return LISA_WS_COMMON_ERR;
    }

    if (!ins->ws_conn) {
        LISA_NLOGI("**lisa_ws_disconnect");
        return LISA_WS_OK;
    }

    LISA_NLOGI("waiting for websocket thread exiting...");
    lisa_semaphore_reset(ins->exited_sem);
    lisa_semaphore_give(ins->exiting_sem);

    err = lisa_semaphore_take(ins->exited_sem, 10 * 1000);
    if (err) {
        LISA_NLOGE("lisa websocket disconnect failed");
        return LISA_WS_COMMON_ERR;
    }

    LISA_NLOGI("websocket disconnect done.");

    return LISA_WS_OK;
}

static int lisa_websocket_msg_send_to_thread(lisa_ws_t *ins, int type, const char *msg, uint32_t len)
{
    if (ins == NULL || !ins->ws_conn) {
        return LISA_WS_COMMON_ERR;
    }

    lisa_ws_msg_t chunk = {0};
    chunk.m_size = len;
    chunk.m_type = type;
    chunk.m_msg = lisa_mem_alloc(len + 1);
    if (chunk.m_msg == NULL) {
        LISA_NLOGE("send txt malloc failed!");
        return LISA_WS_COMMON_ERR;
    }
    memcpy(chunk.m_msg, msg, len);
    chunk.m_msg[len] = 0;

    lisa_err_t st = lisa_queue_push(ins->tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0);
    if (st != LISA_OK) {
        lisa_mem_free(chunk.m_msg);
        LISA_NLOGE("send txt failed! ret: %d", st);
        return LISA_WS_COMMON_ERR;
    }

    return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text)
{
    if (text == NULL) {
        return -1;
    }

    return lisa_websocket_msg_send_to_thread(ins, LISA_WS_TEXT, text, strlen(text));
}

lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len)
{
    if (ins == NULL || !ins->ws_conn) {
        return LISA_WS_COMMON_ERR;
    }

    if (lisa_queue_waiting(ins->tx_msg_queue) >= WS_QUEUE_COUNT - 5) {
        LISA_NLOGE("queue has more data, ignore this frame");
        return LISA_WS_COMMON_ERR;
    }

    return lisa_websocket_msg_send_to_thread(ins, LISA_WS_BIN, buf, len);
}

lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins)
{
    if (ins == NULL) {
        return LISA_WS_COMMON_ERR;
    }

    lisa_ws_disconnect(ins);

    if (ins->thread) {
        lisa_thread_delete(ins->thread);
        ins->thread = NULL;
    }

    if (ins->tx_msg_queue) {
        /* drop all msg */
        lisa_ws_msg_t msg;
        while (lisa_queue_pop(ins->tx_msg_queue, &msg, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
            if (msg.m_msg != NULL) {
                lisa_mem_free(msg.m_msg);
            }
            memset(&msg, 0, sizeof(lisa_ws_msg_t));
        }
        lisa_queue_delete(ins->tx_msg_queue);
        ins->tx_msg_queue = NULL;
    }

    if (ins->exiting_sem) {
        lisa_semaphore_delete(ins->exiting_sem);
        ins->exiting_sem = NULL;
    }

    if (ins->exited_sem) {
        lisa_semaphore_delete(ins->exited_sem);
        ins->exited_sem = NULL;
    }

    if (ins->pong_sem) {
        lisa_semaphore_delete(ins->pong_sem);
        ins->pong_sem = NULL;
    }
    if (ins->connect_sem) {
        lisa_semaphore_delete(ins->connect_sem);
        ins->connect_sem = NULL;
    }

    lisa_mem_free(ins);

    return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_cfg_set(lisa_ws_t *ws, lisa_ws_request_t *req)
{
    if (ws == NULL || req == NULL || ws->ws_conn) {
        return -1;
    }

    strcpy((char *)ws->u_info.scheme, req->scheme);
    strcpy((char *)ws->u_info.host, req->host);
    strcpy((char *)ws->u_info.path, req->path);
    strcpy((char *)ws->u_info.port, req->port);

    ws->user = req->user;
    ws->inter_on_event = req->on_event;
    ws->timeout = req->timeout;
    ws->extra_header = req->extra_header;
    ws->inter_on_data = req->on_data;

    return 0;
}

lisa_ws_err_e lisa_ws_cfg_get(lisa_ws_t *ws, lisa_ws_request_t *req)
{
    if (ws == NULL || req == NULL) {
        return -1;
    }

    req->scheme = (char *)ws->u_info.scheme;
    req->host = (char *)ws->u_info.host;
    req->path = (char *)ws->u_info.path;
    req->port = (char *)ws->u_info.port;

    req->user = ws->user;
    req->on_event = ws->inter_on_event;
    req->timeout = ws->timeout;
    req->on_data = ws->inter_on_data;

    return 0;
}
