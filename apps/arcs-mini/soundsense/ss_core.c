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

#include "cJSON.h"
#include "FreeRTOS.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "lisa_websocket.h"
#include "semphr.h"
#include "task.h"

#include "ss_audio.h"
#include "ss_core.h"

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

    lisa_ws_t *ws;
    SemaphoreHandle_t lock;      /* protects counters + event ring */
    SemaphoreHandle_t connected; /* ON_CONNECTED event */
    SemaphoreHandle_t got_welcome;

    /* stats */
    uint32_t enable_tick;
    uint32_t send_frames, send_drops, result_count, event_count, reconnect_count;
    int32_t last_probs_x1000[SS_CLASS_NUM];

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

static void ss_ws_on_data(lisa_ws_data_t *data)
{
    if (!data || data->type != LISA_WS_TEXT || !data->buf || data->len == 0) {
        return;
    }

    /* NUL-terminate safely (ws text is not guaranteed terminated) */
    char text[512];
    uint32_t n = data->len < sizeof(text) - 1 ? data->len : sizeof(text) - 1;
    memcpy(text, data->buf, n);
    text[n] = '\0';

    cJSON *root = cJSON_Parse(text);
    if (!root) {
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type)) {
        const char *t = type->valuestring;
        if (strcmp(t, "welcome") == 0) {
            if (st.got_welcome) {
                xSemaphoreGive(st.got_welcome);
            }
        } else if (strcmp(t, "result") == 0) {
            ss_handle_result_msg(root);
        } else if (strcmp(t, "event") == 0) {
            ss_handle_event_msg(root);
        } else if (strcmp(t, "error") == 0) {
            cJSON *msg = cJSON_GetObjectItem(root, "message");
            LISA_LOGW(TAG, "server error: %s",
                      cJSON_IsString(msg) ? msg->valuestring : "?");
        }
    }
    cJSON_Delete(root);
}

static void ss_ws_on_event(lisa_ws_event_t *event)
{
    if (!event) {
        return;
    }
    if (event->what == LISA_WS_ON_CONNECTED) {
        if (st.connected) {
            xSemaphoreGive(st.connected);
        }
    } else if (event->what == LISA_WS_ON_DISCONNECTED) {
        if (st.state == SS_STATE_STREAMING || st.state == SS_STATE_CONNECTING) {
            LISA_LOGW(TAG, "disconnected");
            st.state = SS_STATE_RETRY_WAIT;
        }
    }
}

/* ------------------------------------------------------------------ */
/* session lifecycle (management task)                                 */
/* ------------------------------------------------------------------ */

static int ss_parse_url(const char *url, char *scheme, char *host, char *port, char *path)
{
    return sscanf(url, "%7[^:]://%63[^:/]:%7[0-9]/%63s", scheme, host, port, path) >= 3 ? 0 : -1;
}

static int ss_connect_once(void)
{
    static char scheme[8], host[64], port[8], path[64];
    if (ss_parse_url(st.server_url, scheme, host, port, path) != 0) {
        LISA_LOGE(TAG, "bad server url: %s", st.server_url);
        return -1;
    }

    (void)xSemaphoreTake(st.connected, 0);
    (void)xSemaphoreTake(st.got_welcome, 0);

    uint8_t *scheme_u = (uint8_t *)scheme, *host_u = (uint8_t *)host;
    uint8_t *port_u = (uint8_t *)port, *path_u = (uint8_t *)path;
    lisa_ws_request_t req = {
        .scheme = scheme_u,
        .host = host_u,
        .port = port_u,
        .path = path_u,
        .timeout = 8000,
        .user = NULL,
        .on_event = ss_ws_on_event,
        .on_data = ss_ws_on_data,
    };
    st.ws = lisa_ws_init(&req);
    if (!st.ws) {
        return -1;
    }
    if (lisa_ws_connect(st.ws) != LISA_WS_OK) {
        lisa_ws_cleanup(st.ws);
        st.ws = NULL;
        return -1;
    }
    st.state = SS_STATE_CONNECTING;

    if (xSemaphoreTake(st.connected, pdMS_TO_TICKS(10000)) != pdTRUE) {
        LISA_LOGW(TAG, "handshake timeout");
        goto fail;
    }

    char hello[192];
    snprintf(hello, sizeof(hello),
             "{\"type\":\"hello\",\"protocol_version\":\"1.0\",\"sample_rate\":16000,"
             "\"channels\":1,\"format\":\"pcm_s16le\",\"client\":\"arcs-mini\"}");
    if (lisa_ws_send_text(st.ws, (const uint8_t *)hello) != LISA_WS_OK) {
        goto fail;
    }

    if (xSemaphoreTake(st.got_welcome, pdMS_TO_TICKS(10000)) != pdTRUE) {
        LISA_LOGW(TAG, "welcome timeout");
        goto fail;
    }

    st.state = SS_STATE_STREAMING;
    xSemaphoreTake(st.lock, portMAX_DELAY);
    st.reconnect_count++;
    xSemaphoreGive(st.lock);
    LISA_LOGI(TAG, "streaming to %s", st.server_url);
    return 0;

fail:
    lisa_ws_disconnect(st.ws);
    lisa_ws_cleanup(st.ws);
    st.ws = NULL;
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
            /* drain the audio tap and push 50 ms frames */
            static uint8_t chunk[SS_SEND_CHUNK];
            uint32_t n = ss_audio_read(chunk, sizeof(chunk), SS_SEND_PERIOD_MS);
            if (n > 0) {
                if (lisa_ws_send_binary(st.ws, chunk, n) == LISA_WS_OK) {
                    xSemaphoreTake(st.lock, portMAX_DELAY);
                    st.send_frames++;
                    xSemaphoreGive(st.lock);
                    backoff_s = SS_BACKOFF_MIN_S;
                } else {
                    xSemaphoreTake(st.lock, portMAX_DELAY);
                    st.send_drops++;
                    xSemaphoreGive(st.lock);
                }
            }
            continue; /* data-ready semaphore paces the loop */
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
    st.connected = xSemaphoreCreateBinary();
    st.got_welcome = xSemaphoreCreateBinary();
    if (!st.lock || !st.connected || !st.got_welcome) {
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
        if (st.ws && st.state == SS_STATE_STREAMING) {
            lisa_ws_send_text(st.ws, (const uint8_t *)"{\"type\":\"bye\"}");
            lisa_ws_disconnect(st.ws);
            lisa_ws_cleanup(st.ws);
            st.ws = NULL;
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
    if (st.ws && (st.state == SS_STATE_STREAMING || st.state == SS_STATE_CONNECTING)) {
        st.state = SS_STATE_RETRY_WAIT;
        lisa_ws_disconnect(st.ws);
        lisa_ws_cleanup(st.ws);
        st.ws = NULL;
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
