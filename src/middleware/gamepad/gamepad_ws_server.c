/*
 * Gamepad WebSocket server: 封装 nopoll listener, 监听 0.0.0.0:38200。
 *
 * - 端点 ws://<ip>:38200/gamepad (首版不校验路径, 任一路径均接受)
 * - 单手柄: 同时只服务 1 个连接, 新连接会排队在 backlog
 * - 报文: JSON 文本帧 (doc/gamepad-protocol.md 第 4 节)
 *   + ROM 动态加载: rom_begin/rom_end 文本帧 + 二进制数据帧 (§4.6)
 * - 线程: 独立任务 nopoll accept/收包; 按键写入 gamepad_input (volatile 位图),
 *   不触碰 LVGL。
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"
#include "gamepad_rom.h"
#include "lisa_log.h"
#include "lwip/sockets.h"

#include "nopoll.h"
#include "cJSON.h"

#define TAG "gamepad_ws"

#define WS_TASK_STACK       (4096)
#define WS_RX_MAX           256
#define WS_HEARTBEAT_TIMEOUT_MS (30 * 1000)
#define WS_STATE_PUSH_INTERVAL_MS (2 * 1000)
#define WS_POLL_TIMEOUT_MS  (200)   /* select 轮询粒度: 无数据时也周期跑心跳/状态检查 */

static noPollCtx *s_ctx = NULL;
static noPollConn *s_listener = NULL;
static noPollConn *s_conn = NULL;
static noPollConnOpts *s_opts = NULL;   /* listener 只存指针不接管, 需存活到 stop */
static volatile bool s_running = false;

/* 最近一次成功连接的客户端 (pad_gui 所在 PC) 的 IPv4 文本形式。
 * romlib 在没有显式配置 ROM API 地址时, 用它推导 pad_gui 的同机 HTTP 端口。 */
static char s_last_peer_ip[16] = { 0 };   /* "255.255.255.255" + NUL */

/* ------------------------------------------------------------------ */
/* 报文发送                                                             */
/* ------------------------------------------------------------------ */
static void ws_send_json(noPollConn *conn, const char *json)
{
    if (!conn || !json) {
        return;
    }
    int len = (int)strlen(json);
    int sent = nopoll_conn_send_text(conn, json, len);
    if (sent != len) {
        LISA_LOGW(TAG, "send_text %d/%d (%p)", sent, len, (void *)conn);
    }
}

static void ws_send_welcome(noPollConn *conn)
{
    bool running = false;
    uint32_t fps = 0;

    gamepad_get_game_state(&running, &fps);

    char buf[192];
    /* ble: BLE 手柄正在接管输入, 此时 WS/UDP 的按键会被忽略 —— 让 GUI 能提示用户,
     * 而不是让用户以为按键坏了 */
    snprintf(buf, sizeof(buf),
             "{\"t\":\"welcome\",\"ver\":%d,\"proto\":\"gamepad\",\"dev\":\"%s\","
             "\"fw\":\"%s\",\"state\":\"%s\",\"fps\":%u,\"udp_port\":%u,\"ble\":%s}",
             GAMEPAD_PROTO_VERSION, GAMEPAD_DEV_NAME,
             gamepad_util_get_fw_version(),
             running ? "running" : "idle", (unsigned)fps,
             (unsigned)CONFIG_GAMEPAD_UDP_PORT,
             gamepad_input_ble_active() ? "true" : "false");
    ws_send_json(conn, buf);
}

static void ws_send_state(noPollConn *conn)
{
    bool running = false;
    uint32_t fps = 0;

    gamepad_get_game_state(&running, &fps);

    char buf[96];
    snprintf(buf, sizeof(buf),
             "{\"t\":\"state\",\"run\":%s,\"fps\":%u,\"ble\":%s}",
             running ? "true" : "false", (unsigned)fps,
             gamepad_input_ble_active() ? "true" : "false");
    ws_send_json(conn, buf);
}

/* ROM 传输结果应答 (§4.6) */
static void ws_send_rom_ack(noPollConn *conn, bool ok, const char *msg)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"t\":\"rom_ack\",\"ok\":%s,\"msg\":\"%s\"}",
             ok ? "true" : "false", msg ? msg : "");
    ws_send_json(conn, buf);
}

/* ------------------------------------------------------------------ */
/* 报文解析 (WS 线程上下文; 只写共享位图, 禁止阻塞/LVGL)                  */
/* ------------------------------------------------------------------ */
static void ws_handle_text(const char *text, int len)
{
    cJSON *root = cJSON_ParseWithLength(text, len);
    if (!root) {
        LISA_LOGW(TAG, "bad json (%d bytes)", len);
        return;
    }

    const cJSON *t = cJSON_GetObjectItem(root, "t");
    const char *type = cJSON_IsString(t) ? t->valuestring : NULL;

    if (type && strcmp(type, "k") == 0) {
        const cJSON *k = cJSON_GetObjectItem(root, "k");
        const cJSON *v = cJSON_GetObjectItem(root, "v");
        if (cJSON_IsString(k)) {
            /* 走网络入口: BLE 手柄占用时整体让位 */
            gamepad_input_net_key(k->valuestring, cJSON_IsTrue(v) || (cJSON_IsNumber(v) && v->valueint != 0));
        }
    } else if (type && strcmp(type, "ping") == 0) {
        const cJSON *ts = cJSON_GetObjectItem(root, "ts");
        char buf[64];
        if (cJSON_IsNumber(ts)) {
            /* ts 为毫秒时间戳, 可能超出 int32; 用 double 原样回显 */
            snprintf(buf, sizeof(buf), "{\"t\":\"pong\",\"ts\":%lld}",
                     (long long)ts->valuedouble);
        } else {
            snprintf(buf, sizeof(buf), "{\"t\":\"pong\"}");
        }
        ws_send_json(s_conn, buf);
    } else if (type && strcmp(type, "reset") == 0) {
        LISA_LOGI(TAG, "rx reset");
        /* 走网络入口: 客户端复位不应清掉 BLE 手柄正按住的键 */
        gamepad_input_net_reset();
    } else if (type && strcmp(type, "cmd") == 0) {
        const cJSON *c = cJSON_GetObjectItem(root, "c");
        if (cJSON_IsString(c) && strcmp(c->valuestring, "exit") == 0) {
            LISA_LOGW(TAG, "rx cmd exit -> request game quit");
            gamepad_input_set_exit();
            ws_send_state(s_conn);
        } else if (cJSON_IsString(c) && strcmp(c->valuestring, "reset") == 0) {
            /* NES 主机 Reset: 设备重载当前 ROM (等价按主机复位键) */
            LISA_LOGI(TAG, "rx cmd reset -> request game reset");
            gamepad_input_set_reset();
        } else {
            LISA_LOGW(TAG, "unknown cmd: %s", c->valuestring);
        }
    } else if (type && strcmp(type, "hello") == 0) {
        LISA_LOGI(TAG, "client hello");
    } else if (type && strcmp(type, "rom_begin") == 0) {
        const cJSON *size = cJSON_GetObjectItem(root, "size");
        const cJSON *crc = cJSON_GetObjectItem(root, "crc32");
        uint32_t crc32 = cJSON_IsNumber(crc) ? (uint32_t)crc->valuedouble : 0U;
        if (!cJSON_IsNumber(size)) {
            ws_send_rom_ack(s_conn, false, "rom_begin: missing size");
        } else if (gamepad_rom_begin((uint32_t)size->valuedouble, crc32) != 0) {
            ws_send_rom_ack(s_conn, false, "rom_begin failed (bad size / oom)");
        }
        /* 成功不单独应答, 等 rom_end 一起 ack */
    } else if (type && strcmp(type, "rom_end") == 0) {
        const char *err = NULL;
        if (gamepad_rom_end(&err) == 0) {
            bool running = false;
            gamepad_get_game_state(&running, NULL);
            ws_send_rom_ack(s_conn, true, running ? "staged, game restarting" : "staged");
        } else {
            ws_send_rom_ack(s_conn, false, err ? err : "rom_end failed");
        }
    } else if (type && strcmp(type, "rom_cancel") == 0) {
        gamepad_rom_cancel();
    } else {
        LISA_LOGW(TAG, "unknown type: %s", type ? type : "(null)");
    }

    cJSON_Delete(root);
}

/* ------------------------------------------------------------------ */
/* 单连接服务循环                                                       */
/* ------------------------------------------------------------------ */

/* 非阻塞轮询: 有数据才调 get_msg, 无数据时返回 false 让上游跑心跳/状态检查。
 * nopoll_conn_get_msg 在阻塞 socket 上会永久阻塞, 若不先 select 会导致:
 *   - 客户端静默时心跳超时永远触发不了;
 *   - 单个僵死连接永久占住 gp_ws 任务, 新客户端全部卡在 backlog 收不到欢迎帧。 */
static bool ws_poll_readable(noPollConn *conn)
{
    NOPOLL_SOCKET fd = nopoll_conn_socket(conn);
    if (fd < 0 || fd >= FD_SETSIZE) {
        return true;    /* fd 异常时退回阻塞语义, 至少不误判 */
    }
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = WS_POLL_TIMEOUT_MS * 1000;
    int ret = lwip_select(fd + 1, &rfds, NULL, NULL, &tv);
    return (ret > 0) && FD_ISSET(fd, &rfds);
}

/* ------------------------------------------------------------------ */
/* 客户端 IP 记录 (供 gamepad_get_server_addr 判定 ROM API 主机)         */
/* ------------------------------------------------------------------ */

bool gamepad_ws_peer_ip(char *buf, uint32_t len)
{
    if (!buf || len == 0 || s_last_peer_ip[0] == '\0') {
        return false;
    }
    snprintf(buf, len, "%s", s_last_peer_ip);
    return true;
}

/* 记录本连接对端 IP; 拿不到时保留旧值 (多半还是同一台 PC, 仍可用) */
static void ws_record_peer_ip(noPollConn *conn)
{
    NOPOLL_SOCKET fd = nopoll_conn_socket(conn);
    if (fd < 0) {
        return;
    }
    struct sockaddr_in sa;
    socklen_t slen = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    if (getpeername(fd, (struct sockaddr *)&sa, &slen) != 0 || sa.sin_family != AF_INET) {
        return;
    }
    snprintf(s_last_peer_ip, sizeof(s_last_peer_ip), "%s", inet_ntoa(sa.sin_addr));
    LISA_LOGI(TAG, "peer ip: %s", s_last_peer_ip);
}

static void ws_serve_conn(noPollConn *conn)
{
    bool running = false;
    uint32_t fps = 0;
    bool rx_continuation_is_text = true;    /* continuation 片的类型跟随上一数据帧 */

    LISA_LOGI(TAG, "client connected");
    ws_record_peer_ip(conn);

    /* 网络入口: BLE 手柄占用时让位 (连上就 reset 会把 BLE 按住的键清掉) */
    gamepad_input_net_reset();
    ws_send_welcome(conn);

    uint32_t last_rx_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    uint32_t last_state_ms = last_rx_ms;

    while (s_running && nopoll_conn_is_ok(conn)) {
        /* 内层连续排空接收缓冲: ROM 传输 (数百个 4KB 二进制帧) 时不逐帧 sleep 5ms */
        while (s_running && nopoll_conn_is_ok(conn) && ws_poll_readable(conn)) {
            noPollMsg *msg = nopoll_conn_get_msg(conn);
            if (msg) {
                /* 关键: 不能跳过 fragment。nopoll 的 __nopoll_conn_receive 是
                 * 单次 recv 短读, 大帧 (4KB chunk 跨多个 TCP 段) 会拆成多个
                 * 消息递送: 首片带原 opcode (is_fragment), 后续片 opcode=0
                 * (continuation) 且复用 previous_msg 重组路径。
                 * 按 opcode + 延续状态路由, continuation 跟随上一数据帧的类型。 */
                const noPollOpCode op = nopoll_msg_opcode(msg);
                const unsigned char *payload = nopoll_msg_get_payload(msg);
                int size = nopoll_msg_get_payload_size(msg);
                if (payload && size > 0) {
                    last_rx_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
                    if (op == NOPOLL_TEXT_FRAME) {
                        rx_continuation_is_text = true;
                    } else if (op == NOPOLL_BINARY_FRAME) {
                        rx_continuation_is_text = false;
                    }
                    if (op == NOPOLL_TEXT_FRAME ||
                        (op == NOPOLL_CONTINUATION_FRAME && rx_continuation_is_text)) {
                        ws_handle_text((const char *)payload, size);
                    } else if (op == NOPOLL_BINARY_FRAME ||
                               op == NOPOLL_CONTINUATION_FRAME) {
                        /* ROM 数据分块 (§4.6); 溢出/无传输时 gamepad_rom_data 内部自弃 */
                        gamepad_rom_data(payload, (uint32_t)size);
                    }
                    /* close/ping/pong 等控制帧: nopoll 内部处理, 忽略 */
                }
                nopoll_msg_unref(msg);
            } else if (!nopoll_conn_is_ok(conn)) {
                break;
            }
        }

        /* 心跳超时: 30s 无帧断开 */
        uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (now - last_rx_ms > WS_HEARTBEAT_TIMEOUT_MS) {
            LISA_LOGI(TAG, "heartbeat timeout, closing");
            break;
        }

        /* 游戏状态变化推送 (低频) + UDP 收包统计 (丢包率遥测, §4.5.1) */
        if (now - last_state_ms >= WS_STATE_PUSH_INTERVAL_MS) {
            bool cur_running = false;
            uint32_t cur_fps = 0;
            gamepad_get_game_state(&cur_running, &cur_fps);
            if (cur_running != running || cur_fps != fps) {
                running = cur_running;
                fps = cur_fps;
                ws_send_state(conn);
            }

            uint32_t udp_seq = 0;
            uint32_t udp_rx = 0;
            if (gamepad_udp_get_stats(&udp_seq, &udp_rx)) {
                char ubuf[80];
                snprintf(ubuf, sizeof(ubuf),
                         "{\"t\":\"udp_stat\",\"seq\":%u,\"rx\":%u}",
                         (unsigned)udp_seq, (unsigned)udp_rx);
                ws_send_json(conn, ubuf);
            }
            last_state_ms = now;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    LISA_LOGI(TAG, "client disconnected");
    /* 走网络入口: 客户端断线只清网络侧按键, 不碰 BLE 手柄 */
    gamepad_input_net_on_disconnect();
    gamepad_rom_cancel();   /* 半途断线: 丢弃未完成的 ROM 传输 */
    nopoll_conn_shutdown(conn);
    nopoll_conn_close(conn);
}

/* ------------------------------------------------------------------ */
/* 服务任务                                                             */
/* ------------------------------------------------------------------ */
static void ws_task(void *arg)
{
    (void)arg;

    s_ctx = nopoll_ctx_new();
    if (!s_ctx) {
        LISA_LOGE(TAG, "nopoll_ctx_new failed");
        vTaskDelete(NULL);
        return;
    }

    /* LWIP 未就绪时 listener 创建会失败; 循环重试直到成功 */
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)CONFIG_GAMEPAD_WS_PORT);

    /* 关键: nopoll 默认要求客户端带 Origin 头 (RFC 语义为 MUST),
     * 而标准 WebSocket 客户端 (浏览器 fetch / Python websockets) 默认不带,
     * 会导致 upgrade 校验失败直接断连。这里显式关闭 Origin 校验。 */
    s_opts = nopoll_conn_opts_new();
    if (s_opts) {
        nopoll_conn_opts_skip_origin_check(s_opts, nopoll_true);
    }

    while (s_running && !s_listener) {
        s_listener = nopoll_listener_new_opts(s_ctx, s_opts, "0.0.0.0", port_str);
        if (s_listener && nopoll_conn_is_ok(s_listener)) {
            break;
        }
        if (s_listener) {
            nopoll_conn_close(s_listener);
            s_listener = NULL;
        }
        LISA_LOGW(TAG, "listener :%u not ready, retry", (unsigned)CONFIG_GAMEPAD_WS_PORT);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (!s_running || !s_listener) {
        nopoll_ctx_unref(s_ctx);
        s_ctx = NULL;
        if (s_opts) {
            nopoll_conn_opts_free(s_opts);
            s_opts = NULL;
        }
        vTaskDelete(NULL);
        return;
    }
    LISA_LOGI(TAG, "ws server listening on :%u%s",
              (unsigned)CONFIG_GAMEPAD_WS_PORT, GAMEPAD_WS_PATH);

    while (s_running) {
        /* 阻塞 accept; ws 握手在 wait_until_connection_ready 中完成 */
        noPollConn *conn = nopoll_conn_accept(s_ctx, s_listener);
        if (!conn || !nopoll_conn_is_ok(conn)) {
            if (s_running) {
                LISA_LOGW(TAG, "accept failed");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        /* 关键: 等待 HTTP upgrade 握手完成 (is_ready 内部泵握手处理),
         * 未就绪就发数据会打断 101 交互导致客户端握手失败 */
        if (!nopoll_conn_wait_until_connection_ready(conn, 5000)) {
            LISA_LOGW(TAG, "handshake timeout, closing");
            nopoll_conn_shutdown(conn);
            nopoll_conn_close(conn);
            continue;
        }
        LISA_LOGI(TAG, "client connected: %s", nopoll_conn_get_requested_url(conn));

        s_conn = conn;
        ws_serve_conn(conn);
        s_conn = NULL;
    }

    nopoll_conn_close(s_listener);
    nopoll_ctx_unref(s_ctx);
    s_listener = NULL;
    s_ctx = NULL;
    if (s_opts) {
        nopoll_conn_opts_free(s_opts);
        s_opts = NULL;
    }
    vTaskDelete(NULL);
}

int gamepad_ws_server_start(void)
{
    if (s_running) {
        return 0;
    }
    s_running = true;
    if (xTaskCreate(ws_task, "gp_ws", WS_TASK_STACK, NULL,
                    configMAX_PRIORITIES - 4, NULL) != pdPASS) {
        s_running = false;
        LISA_LOGE(TAG, "ws task create failed");
        return -1;
    }
    return 0;
}

void gamepad_ws_server_stop(void)
{
    s_running = false;
    if (s_listener) {
        /* 解开 accept 阻塞 */
        nopoll_conn_shutdown(s_listener);
        nopoll_conn_close(s_listener);
        s_listener = NULL;
    }
}
