/*
 * ss_core.c - SoundSense WebSocket streaming client
 *
 * One management task drives the whole session lifecycle:
 *
 *   OFF -> (enable) -> RETRY_WAIT -> CONNECTING -> STREAMING
 *                 ^                          |
 *                 +------- on disconnect ----+
 *
 * - connect() is async: we wait for LISA_WS_ON_CONNECTED before hello
 *   (10 s protocol deadline);
 * - audio frames: drain ss_audio ring in 50 ms chunks (1600 B) and
 *   lisa_ws_send_binary; failures drop the frame (never block);
 * - downlink JSON (result/event) arrives on the ws thread via on_data:
 *   parsed there, events appended to the ring under a mutex;
 * - reconnect backoff: 5 s doubling up to 60 s, reset on welcome.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "cJSON.h"
#include "FreeRTOS.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "listen_system.h"
#include "semphr.h"
#include "task.h"

#include "ss_audio.h"
#include "ss_core.h"

#include "sys_wifi.h"

#define TAG "ss"

#define SS_KV_ENABLED "ss.enabled"
#define SS_KV_SERVER "ss.server_url"
#define SS_DEFAULT_SERVER "ws://192.168.1.169:8000/v1/stream"

#define SS_SEND_CHUNK 1600u   /* 50 ms of mono 16 kHz s16 */
#define SS_SEND_PERIOD_MS 50u
#define SS_BACKOFF_MIN_S 5u
#define SS_BACKOFF_MAX_S 60u
#define SS_TASK_STACK 8192

static struct {
    volatile ss_state_t state;
    bool enabled;
    char server_url[128];

    SemaphoreHandle_t lock;      /* protects counters + event ring */

    /* stats */
    uint32_t enable_tick;
    uint32_t send_frames, send_drops, result_count, event_count, reconnect_count;
    int32_t last_probs_x1000[SS_CLASS_NUM];
    uint32_t class_event_count[SS_CLASS_NUM];
    char class_last_time[SS_CLASS_NUM][8];
    bool class_last_is_start[SS_CLASS_NUM];

    /* event ring */
    ss_event_t ring[SS_EVENT_RING];
    uint32_t ring_head; /* next write */
    uint32_t ring_count;
} st;

static const char *const k_class_names[SS_CLASS_NUM] = {"snoring", "baby_cry"};

/* ------------------------------------------------------------------ */
/* downlink handling (ws thread)                                       */
/* ------------------------------------------------------------------ */

static void ss_handle_event_msg(cJSON *root)
{
    cJSON *class_json = cJSON_GetObjectItem(root, "class");
    cJSON *ev_json = cJSON_GetObjectItem(root, "event");
    cJSON *dur_json = cJSON_GetObjectItem(root, "duration_ms");
    cJSON *peak_json = cJSON_GetObjectItem(root, "peak_prob");
    cJSON *ts_json = cJSON_GetObjectItem(root, "ts");

    if (!cJSON_IsString(class_json) || !cJSON_IsString(ev_json)) {
        return;
    }

    ss_event_t ev;
    memset(&ev, 0, sizeof(ev));
    snprintf(ev.class_name, sizeof(ev.class_name), "%s", class_json->valuestring);
    ev.is_start = strcmp(ev_json->valuestring, "class_start") == 0;
    ev.duration_ms = cJSON_IsNumber(dur_json) ? (uint32_t)dur_json->valuedouble : 0;
    ev.peak_prob_x100 = cJSON_IsNumber(peak_json)
                            ? (uint8_t)(peak_json->valuedouble * 100 + 0.5)
                            : 0;
    ev.ts_ms = cJSON_IsNumber(ts_json) ? (uint32_t)ts_json->valuedouble : 0;

    xSemaphoreTake(st.lock, portMAX_DELAY);
    st.ring[st.ring_head] = ev;
    st.ring_head = (st.ring_head + 1) % SS_EVENT_RING;
    if (st.ring_count < SS_EVENT_RING) {
        st.ring_count++;
    }
    st.event_count++;
    /* per-class running totals: count only class_start = one detection episode;
     * format time immediately via ls_sys_get_localtime (TZ-correct, no math) */
    struct tm cal;
    char time_str[8] = "";
    if (ls_sys_time_is_valid() && ls_sys_get_localtime(&cal) == 0) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", cal.tm_hour, cal.tm_min);
    }
    for (int c = 0; c < SS_CLASS_NUM; c++) {
        if (strcmp(ev.class_name, k_class_names[c]) == 0) {
            if (ev.is_start) {
                st.class_event_count[c]++;
            }
            if (time_str[0] != '\0') {
                snprintf(st.class_last_time[c], sizeof(st.class_last_time[c]),
                         "%s", time_str);
            }
            st.class_last_is_start[c] = ev.is_start;
        }
    }
    xSemaphoreGive(st.lock);

    LISA_LOGI(TAG, "event: %s %s dur=%ums peak=%u%%", ev.class_name,
              ev.is_start ? "START" : "END", ev.duration_ms, ev.peak_prob_x100);

#ifdef CONFIG_SOUNDSENSE_SCREEN
    extern void ss_screen_on_event(const ss_event_t *ev);
    ss_screen_on_event(&ev);
#endif
}

static void ss_handle_result_msg(cJSON *root)
{
    cJSON *smoothed = cJSON_GetObjectItem(root, "smoothed");
    if (!cJSON_IsObject(smoothed)) {
        return;
    }

    xSemaphoreTake(st.lock, portMAX_DELAY);
    for (int i = 0; i < SS_CLASS_NUM; i++) {
        cJSON *p = cJSON_GetObjectItem(smoothed, k_class_names[i]);
        if (cJSON_IsNumber(p)) {
            st.last_probs_x1000[i] = (int32_t)(p->valuedouble * 1000 + 0.5);
        }
    }
    st.result_count++;
    xSemaphoreGive(st.lock);
}

/* ------------------------------------------------------------------ */
/* raw-socket minimal WebSocket client (RFC 6455)                      */
/* bypasses lisa_websocket/nopoll: handshake + frame send/recv by hand */
/* ------------------------------------------------------------------ */

static int st_sock = -1;

/* ---- SHA-1 (compact) + Base64 for the WS handshake key ---- */
static uint32_t ss_rol(uint32_t val, int shift) { return (val << shift) | (val >> (32 - shift)); }

static void ss_sha1(const uint8_t *data, size_t len, uint8_t out[20])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t total_bits = (uint64_t)len * 8;

    /* pad: data + 0x80 + zeros + 8-byte length */
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *buf = lisa_mem_alloc(padded_len);
    if (!buf) { return; }
    memset(buf, 0, padded_len);
    memcpy(buf, data, len);
    buf[len] = 0x80;
    for (int i = 0; i < 8; i++) {
        buf[padded_len - 1 - i] = (uint8_t)(total_bits >> (i * 8));
    }

    for (size_t off = 0; off < padded_len; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)buf[off + 4 * i] << 24) | ((uint32_t)buf[off + 4 * i + 1] << 16) |
                   ((uint32_t)buf[off + 4 * i + 2] << 8) | (uint32_t)buf[off + 4 * i + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = ss_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)     { f = (b & c) | ((~b) & d);           k = 0x5A827999; }
            else if (i < 40){ f = b ^ c ^ d;                       k = 0x6ED9EBA1; }
            else if (i < 60){ f = (b & c) | (b & d) | (c & d);    k = 0x8F1BBCDC; }
            else            { f = b ^ c ^ d;                       k = 0xCA62C1D6; }
            uint32_t temp = ss_rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = ss_rol(b, 30); b = a; a = temp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    lisa_mem_free(buf);

    for (int i = 0; i < 5; i++) {
        out[4 * i] = (uint8_t)(h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(h[i] >> 8);
        out[4 * i + 3] = (uint8_t)h[i];
    }
}

static const char k_b64_tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int ss_base64(const uint8_t *src, size_t len, char *dst, size_t dst_size)
{
    size_t out_len = ((len + 2) / 3) * 4 + 1;
    if (out_len > dst_size) { return -1; }
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)src[i] << 16;
        if (i + 1 < len) v |= (uint32_t)src[i + 1] << 8;
        if (i + 2 < len) v |= src[i + 2];
        dst[j++] = k_b64_tab[(v >> 18) & 0x3F];
        dst[j++] = k_b64_tab[(v >> 12) & 0x3F];
        dst[j++] = (i + 1 < len) ? k_b64_tab[(v >> 6) & 0x3F] : '=';
        dst[j++] = (i + 2 < len) ? k_b64_tab[v & 0x3F] : '=';
    }
    dst[j] = '\0';
    return 0;
}

/* simple SHA-1 + Base64 for the WS handshake key (RFC 6455 §4.2.2) */
static void ss_sha1(const uint8_t *data, size_t len, uint8_t out[20]);
static int ss_base64(const uint8_t *src, size_t len, char *dst, size_t dst_size);

static int ss_ws_connect(const char *host, const char *port_str, const char *path)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)atoi(port_str));

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        LISA_LOGE(TAG, "ws: bad host '%s'", host);
        return -1;
    }

    st_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (st_sock < 0) {
        LISA_LOGE(TAG, "ws: socket()=%d", st_sock);
        return -1;
    }

    /* 5s connect timeout */
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(st_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(st_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(st_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        LISA_LOGE(TAG, "ws: connect() failed");
        close(st_sock);
        st_sock = -1;
        return -1;
    }
    LISA_LOGI(TAG, "ws: TCP connected to %s:%s", host, port_str);

    /* send HTTP upgrade: key = base64(16 random bytes), NOT sha1 of them */
    uint8_t rand16[16];
    for (int i = 0; i < 16; i++) {
        rand16[i] = (uint8_t)(rand() & 0xFF);
    }
    char key_b64[32];
    ss_base64(rand16, 16, key_b64, sizeof(key_b64));

    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, port_str, key_b64);

    if (send(st_sock, req, req_len, 0) != req_len) {
        LISA_LOGE(TAG, "ws: send() upgrade failed");
        close(st_sock);
        st_sock = -1;
        return -1;
    }

    /* read response until \r\n\r\n */
    char resp[1024];
    int resp_len = 0;
    while (resp_len < (int)sizeof(resp) - 1) {
        int n = recv(st_sock, resp + resp_len, sizeof(resp) - 1 - resp_len, 0);
        if (n <= 0) {
            LISA_LOGE(TAG, "ws: recv() upgrade resp failed n=%d", n);
            close(st_sock);
            st_sock = -1;
            return -1;
        }
        resp_len += n;
        resp[resp_len] = '\0';
        if (strstr(resp, "\r\n\r\n")) {
            break;
        }
    }

    if (strncmp(resp, "HTTP/1.1 101", 12) != 0 && strncmp(resp, "HTTP/1.0 101", 12) != 0) {
        LISA_LOGE(TAG, "ws: upgrade rejected: %.60s", resp);
        close(st_sock);
        st_sock = -1;
        return -1;
    }

    LISA_LOGI(TAG, "ws: handshake 101 OK");
    return 0;
}

/* build a masked WS frame header (RFC 6455: client frames MUST be masked) */
static int ss_ws_build_hdr(uint8_t *hdr, uint8_t opcode, size_t len, uint8_t mask[4])
{
    int hdr_len = 0;
    hdr[0] = 0x80 | opcode; /* FIN + opcode */
    mask[0] = (uint8_t)(rand() & 0xFF); mask[1] = (uint8_t)(rand() & 0xFF);
    mask[2] = (uint8_t)(rand() & 0xFF); mask[3] = (uint8_t)(rand() & 0xFF);

    if (len < 126) {
        hdr[1] = 0x80 | (uint8_t)len;
        hdr_len = 2;
    } else if (len < 65536) {
        hdr[1] = 0x80 | 126;
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t)(len & 0xFF);
        hdr_len = 4;
    } else {
        return -1;
    }
    memcpy(hdr + hdr_len, mask, 4);
    return hdr_len + 4;
}

/* send a WS text frame (masked, small payload fits on stack) */
static int ss_ws_send_text(const char *text)
{
    if (st_sock < 0) {
        return -1;
    }
    size_t len = strlen(text);
    uint8_t hdr[8];
    uint8_t mask[4];
    int hdr_len = ss_ws_build_hdr(hdr, 0x01, len, mask);
    if (hdr_len < 0) {
        return -1;
    }

    char masked[256]; /* hello/welcome/pong are all < 256 bytes */
    if (len > sizeof(masked)) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        masked[i] = text[i] ^ mask[i % 4];
    }

    if (send(st_sock, hdr, hdr_len, 0) != hdr_len) return -1;
    if (len > 0 && send(st_sock, masked, len, 0) != (int)len) return -1;
    return 0;
}

/* send a WS binary frame (masked in-place: the chunk buffer is scratch) */
static int ss_ws_send_binary(uint8_t *data, size_t len)
{
    if (st_sock < 0) {
        return -1;
    }
    uint8_t hdr[8];
    uint8_t mask[4];
    int hdr_len = ss_ws_build_hdr(hdr, 0x02, len, mask);
    if (hdr_len < 0) {
        return -1;
    }

    /* mask in-place: caller passes a scratch buffer we own this cycle */
    for (size_t i = 0; i < len; i++) {
        data[i] ^= mask[i % 4];
    }

    if (send(st_sock, hdr, hdr_len, 0) != hdr_len) return -1;
    if (len > 0 && send(st_sock, data, len, 0) != (int)len) return -1;
    return 0;
}

/* try to read a WS text frame (non-blocking, returns payload length or -1) */
#define SS_WS_BUF_SIZE 2048
static uint8_t *s_rx_buf; /* allocated in PSRAM at init (SRAM is scarce) */
static uint32_t s_rx_len;

static int ss_ws_read_text(char *out, size_t out_size)
{
    if (st_sock < 0) {
        return -1;
    }

    /* try to read more data */
    uint8_t tmp[1024];
    int n = recv(st_sock, tmp, sizeof(tmp), MSG_DONTWAIT);
    if (n > 0) {
        if (s_rx_len + n > SS_WS_BUF_SIZE) {
            s_rx_len = 0; /* overflow: drop */
        }
        memcpy(s_rx_buf + s_rx_len, tmp, n);
        s_rx_len += n;
    } else if (n == 0) {
        return -2; /* connection closed */
    }

    /* try to parse a complete text frame */
    if (s_rx_len < 2) {
        return -1;
    }

    uint8_t opcode = s_rx_buf[0] & 0x0F;
    bool is_masked = (s_rx_buf[1] & 0x80) != 0;
    uint64_t payload_len = s_rx_buf[1] & 0x7F;
    uint32_t hdr_size = 2;

    if (payload_len == 126) {
        if (s_rx_len < 4) return -1;
        payload_len = (s_rx_buf[2] << 8) | s_rx_buf[3];
        hdr_size = 4;
    } else if (payload_len == 127) {
        if (s_rx_len < 10) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | s_rx_buf[2 + i];
        }
        hdr_size = 10;
    }
    if (is_masked) {
        hdr_size += 4; /* skip mask key (server frames are typically unmasked) */
    }

    if (s_rx_len < hdr_size + payload_len) {
        return -1; /* incomplete */
    }

    if (opcode == 0x1 && payload_len < out_size) { /* text */
        const uint8_t *payload = s_rx_buf + hdr_size;
        if (is_masked) {
            const uint8_t *mask = s_rx_buf + hdr_size - 4;
            for (uint64_t i = 0; i < payload_len; i++) {
                out[i] = payload[i] ^ mask[i % 4];
            }
        } else {
            memcpy(out, payload, payload_len);
        }
        out[payload_len] = '\0';

        /* consume this frame from the buffer */
        uint32_t consumed = hdr_size + payload_len;
        memmove(s_rx_buf, s_rx_buf + consumed, s_rx_len - consumed);
        s_rx_len -= consumed;
        return (int)payload_len;
    }

    /* skip non-text frames */
    uint32_t consumed = hdr_size + payload_len;
    memmove(s_rx_buf, s_rx_buf + consumed, s_rx_len - consumed);
    s_rx_len -= consumed;
    return -1;
}

static void ss_ws_close(void)
{
    if (st_sock >= 0) {
        /* send WS close frame (opcode 8) */
        uint8_t close_frame[2] = {0x88, 0x80}; /* FIN+close, MASK+len0 */
        uint8_t mask[4] = {0, 0, 0, 0};
        send(st_sock, close_frame, 2, 0);
        send(st_sock, mask, 4, 0);
        close(st_sock);
        st_sock = -1;
    }
    s_rx_len = 0;
}

/* ------------------------------------------------------------------ */
/* downlink JSON handling                                              */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* session lifecycle (management task)                                 */
/* ------------------------------------------------------------------ */

static int ss_parse_url(const char *url, char *scheme, char *host, char *port, char *path)
{
    if (sscanf(url, "%7[^:]://%63[^:/]:%7[0-9]/%63s", scheme, host, port, path) < 3) {
        return -1;
    }
    /* nopoll sends "GET <path> HTTP/1.1" verbatim: the path MUST start
     * with '/' or the request line is malformed and the server resets */
    if (path[0] != '/') {
        memmove(path + 1, path, strlen(path) + 1);
        path[0] = '/';
    }
    return 0;
}

static int ss_connect_once(void)
{
    static char scheme[8], host[64], port[8], path[64];
    if (ss_parse_url(st.server_url, scheme, host, port, path) != 0) {
        LISA_LOGE(TAG, "bad server url: %s", st.server_url);
        return -1;
    }

    if (ss_ws_connect(host, port, path) != 0) {
        st.state = SS_STATE_RETRY_WAIT;
        return -1;
    }
    st.state = SS_STATE_CONNECTING;

    char hello[192];
    snprintf(hello, sizeof(hello),
             "{\"type\":\"hello\",\"protocol_version\":\"1.0\",\"sample_rate\":16000,"
             "\"channels\":1,\"format\":\"pcm_s16le\",\"client\":\"arcs-mini\"}");
    if (ss_ws_send_text(hello) != 0) {
        goto fail;
    }

    /* wait for welcome (read text frames until we see it, up to 10s) */
    bool got_welcome = false;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(10000);
    while (!got_welcome && xTaskGetTickCount() < deadline) {
        char text[1024];
        int n = ss_ws_read_text(text, sizeof(text));
        if (n == -2) {
            LISA_LOGW(TAG, "connection closed during welcome wait");
            goto fail;
        }
        if (n > 0) {
            cJSON *root = cJSON_Parse(text);
            if (root) {
                cJSON *type = cJSON_GetObjectItem(root, "type");
                if (cJSON_IsString(type) && strcmp(type->valuestring, "welcome") == 0) {
                    got_welcome = true;
                } else if (cJSON_IsString(type) && strcmp(type->valuestring, "result") == 0) {
                    ss_handle_result_msg(root);
                } else if (cJSON_IsString(type) && strcmp(type->valuestring, "event") == 0) {
                    ss_handle_event_msg(root);
                } else if (cJSON_IsString(type) && strcmp(type->valuestring, "error") == 0) {
                    cJSON *msg = cJSON_GetObjectItem(root, "message");
                    LISA_LOGE(TAG, "server error: %s",
                              cJSON_IsString(msg) ? msg->valuestring : "?");
                }
                cJSON_Delete(root);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (!got_welcome) {
        LISA_LOGW(TAG, "welcome timeout");
        goto fail;
    }

    st.state = SS_STATE_STREAMING;
    /* WiFi is up by now (connection succeeded): disable standby power save
     * to prevent DTIM sleep from breaking the audio stream */
    sys_wifi_set_standby_power_save(false);
    xSemaphoreTake(st.lock, portMAX_DELAY);
    st.reconnect_count++;
    xSemaphoreGive(st.lock);
    LISA_LOGI(TAG, "streaming to %s", st.server_url);
    return 0;

fail:
    ss_ws_close();
    st.state = SS_STATE_RETRY_WAIT;
    return -1;
}

static void ss_task(void *arg)
{
    (void)arg;
    uint32_t backoff_s = SS_BACKOFF_MIN_S;

    while (1) {
        if (!st.enabled) {
            st.state = SS_STATE_OFF;
            vTaskDelay(pdMS_TO_TICKS(500));
            backoff_s = SS_BACKOFF_MIN_S;
            continue;
        }

        if (st.state == SS_STATE_STREAMING) {
            /* 1. push audio */
            static uint8_t chunk[SS_SEND_CHUNK];
            uint32_t n = ss_audio_read(chunk, sizeof(chunk), SS_SEND_PERIOD_MS);
            if (n > 0) {
                if (ss_ws_send_binary(chunk, n) == 0) {
                    xSemaphoreTake(st.lock, portMAX_DELAY);
                    st.send_frames++;
                    xSemaphoreGive(st.lock);
                    backoff_s = SS_BACKOFF_MIN_S;
                } else {
                    xSemaphoreTake(st.lock, portMAX_DELAY);
                    st.send_drops++;
                    xSemaphoreGive(st.lock);
                    LISA_LOGW(TAG, "send failed, reconnecting");
                    ss_ws_close();
                    st.state = SS_STATE_RETRY_WAIT;
                    continue;
                }
            }

            /* 2. drain downlink text frames */
            char text[1024];
            int rn;
            while ((rn = ss_ws_read_text(text, sizeof(text))) > 0) {
                cJSON *root = cJSON_Parse(text);
                if (root) {
                    cJSON *type = cJSON_GetObjectItem(root, "type");
                    if (cJSON_IsString(type)) {
                        const char *t = type->valuestring;
                        if (strcmp(t, "result") == 0) {
                            ss_handle_result_msg(root);
                        } else if (strcmp(t, "event") == 0) {
                            ss_handle_event_msg(root);
                        } else if (strcmp(t, "ping") == 0) {
                            char pong[64];
                            snprintf(pong, sizeof(pong), "{\"type\":\"pong\",\"ts\":%lld}",
                                     (long long)xTaskGetTickCount() * 1000 / configTICK_RATE_HZ);
                            ss_ws_send_text(pong);
                        } else if (strcmp(t, "error") == 0) {
                            cJSON *msg = cJSON_GetObjectItem(root, "message");
                            LISA_LOGW(TAG, "server error: %s",
                                      cJSON_IsString(msg) ? msg->valuestring : "?");
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            if (rn == -2) { /* connection closed */
                LISA_LOGW(TAG, "connection closed by server");
                ss_ws_close();
                st.state = SS_STATE_RETRY_WAIT;
                continue;
            }
            continue;
        }

        if (st.state == SS_STATE_RETRY_WAIT) {
            vTaskDelay(pdMS_TO_TICKS(backoff_s * 1000u));
            backoff_s = backoff_s >= SS_BACKOFF_MAX_S ? SS_BACKOFF_MAX_S : backoff_s * 2;
            if (!st.enabled) {
                continue;
            }
        }

        ss_connect_once();
    }
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

static void ss_load_config(void)
{
    char *url = NULL;
    int enabled = 0;

    snprintf(st.server_url, sizeof(st.server_url), "%s", SS_DEFAULT_SERVER);
    if (lisa_kv_get_string(SS_KV_SERVER, &url) == 0 && url && url[0] != '\0') {
        snprintf(st.server_url, sizeof(st.server_url), "%s", url);
    }
    if (url) {
        lisa_kv_free(url);
    }
    if (lisa_kv_get_int(SS_KV_ENABLED, &enabled) == 0 && enabled == 1) {
        st.enabled = true;
    }
}

int ss_core_init(void)
{
    static bool inited;
    if (inited) {
        return 0;
    }
    inited = true;

    memset(&st, 0, sizeof(st));
    st.state = SS_STATE_OFF;
    st.lock = xSemaphoreCreateMutex();
    s_rx_buf = lisa_mem_alloc(SS_WS_BUF_SIZE);
    if (!st.lock || !s_rx_buf) {
        return -1;
    }

    ss_load_config();

    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"ss.core",
        .stack_size = SS_TASK_STACK,
        .priority = LISA_OS_PRIORITY_LOW,
    };
    if (!lisa_thread_create(&attr, ss_task, NULL)) {
        return -1;
    }

    if (st.enabled) {
        ss_audio_start();
        st.enable_tick = xTaskGetTickCount();
        st.state = SS_STATE_RETRY_WAIT;
    }
    LISA_LOGI(TAG, "core init: enabled=%d server=%s", st.enabled, st.server_url);
    return 0;
}

void ss_core_set_enabled(bool enable)
{
    if (enable == st.enabled) {
        return;
    }
    st.enabled = enable;
    lisa_kv_set_int(SS_KV_ENABLED, enable ? 1 : 0);

    if (enable) {
        ss_audio_start();
        st.enable_tick = xTaskGetTickCount();
        st.send_frames = st.send_drops = st.result_count = st.event_count = 0;
        st.state = SS_STATE_RETRY_WAIT;
    } else {
        ss_audio_stop();
        /* restore WiFi power save only if it was disabled during streaming */
        if (st.state == SS_STATE_STREAMING) {
            ss_ws_send_text("{\"type\":\"bye\"}");
            ss_ws_close();
        }
        st.state = SS_STATE_OFF;
    }
    LISA_LOGI(TAG, "monitoring %s", enable ? "ON" : "OFF");
}

bool ss_core_get_enabled(void)
{
    return st.enabled;
}

void ss_core_set_server(const char *url)
{
    if (!url || url[0] == '\0') {
        return;
    }
    snprintf(st.server_url, sizeof(st.server_url), "%s", url);
    lisa_kv_set_string(SS_KV_SERVER, st.server_url);
    /* force a reconnect with the new url */
    if (st.state == SS_STATE_STREAMING || st.state == SS_STATE_CONNECTING) {
        ss_ws_close();
        st.state = SS_STATE_RETRY_WAIT;
    }
}

const char *ss_core_get_server(void)
{
    return st.server_url;
}

void ss_core_get_status(ss_status_t *out)
{
    if (!out) {
        return;
    }
    xSemaphoreTake(st.lock, portMAX_DELAY);
    out->state = st.state;
    out->enabled = st.enabled;
    snprintf(out->server_url, sizeof(out->server_url), "%s", st.server_url);
    out->uptime_s = st.enabled ? (xTaskGetTickCount() - st.enable_tick) / pdMS_TO_TICKS(1000) : 0;
    out->send_frames = st.send_frames;
    out->send_drops = st.send_drops;
    out->result_count = st.result_count;
    out->event_count = st.event_count;
    out->reconnect_count = st.reconnect_count;
    for (int i = 0; i < SS_CLASS_NUM; i++) {
        out->last_probs_x1000[i] = st.last_probs_x1000[i];
        out->class_event_count[i] = st.class_event_count[i];
        memcpy(out->class_last_time[i], st.class_last_time[i], 8);
        out->class_last_is_start[i] = st.class_last_is_start[i];
    }
    xSemaphoreGive(st.lock);
}

uint32_t ss_core_get_events(ss_event_t *out, uint32_t max, const char *class_filter)
{
    if (!out || max == 0) {
        return 0;
    }
    uint32_t count = 0;
    xSemaphoreTake(st.lock, portMAX_DELAY);
    for (uint32_t i = 0; i < st.ring_count && count < max; i++) {
        uint32_t idx = (st.ring_head + SS_EVENT_RING - st.ring_count + i) % SS_EVENT_RING;
        if (class_filter && class_filter[0] != '\0' &&
            strcmp(st.ring[idx].class_name, class_filter) != 0) {
            continue;
        }
        out[count++] = st.ring[idx];
    }
    xSemaphoreGive(st.lock);
    return count;
}
