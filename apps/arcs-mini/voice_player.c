#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

#define TAG "voice_player"
#include "lisa_log.h"
#include "lisa_time.h"
#include "sysutils.h"
#include "lisa_http.h"
#include "sysheap.h"

#include "alarm_ring.h"

#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "voice_cloud.h"
#include "tone.h"
#include "app_tone.h"
#include "app_player.h"
#include "voice_player_comm.h"
#include "voice_player.h"
#include "voice_music_list.h"
#include "voice_intent_mgr.h"
#include "voice_intent/voice_intent_music.h"
#include "voice_intent/voice_intent_photo_flow.h"
#include "service_sd_music.h"
#define ALARM_TTS_BUF_SIZE 512

/* 异步播放队列相关定义 */
#define ASYNC_PLAY_URL_MAX 256
#define ASYNC_PLAY_QUEUE_SIZE 5
#define ASYNC_PLAY_TASK_STACK_SIZE 2048
#define ASYNC_PLAY_TASK_PRIORITY 5
#define PUSHUP_TTS_READY_POLL_MS 1000
#define PUSHUP_TTS_READY_STABLE_COUNT 2
#define PUSHUP_TTS_READY_MAX_ATTEMPTS 21
#define PUSHUP_TTS_HTTP_TIMEOUT_MS 15000
#define PUSHUP_TTS_DOWNLOAD_MAX_BYTES (3U * 1024U * 1024U)
#define CLOUD_OPEN_INFO_STATUS_BIND 3U

/* 提示音排队播放：短时间内多次请求 tone 时，按顺序逐个播完，避免打架 */
#define TONE_PENDING_QUEUE_SIZE 5

typedef struct {
    uint8_t type;
    uint8_t bypass_tts_gate;
    uint8_t wait_mp3_ready;
    uint8_t reserved;
    app_player_t *player;  /* 播放器实例指针 */
    char url[ASYNC_PLAY_URL_MAX];
} async_play_request_t;

typedef struct {
    const char *url;
    bool prompt_tone;
} tone_play_request_t;

typedef enum {
    ASYNC_PLAY_REQUEST_URL = 0,
    ASYNC_PLAY_REQUEST_RESUME,
} async_play_request_type_t;

static QueueHandle_t s_async_play_queue = NULL;
static TaskHandle_t s_async_play_task = NULL;

static bool s_disconnect_tone_played = false;
static bool s_cloud_unstable_pending = false;
static TickType_t s_cloud_unstable_pending_tick = 0;
static bool s_cloud_reconnect_pending = false;
static bool s_cloud_success_tone_played = false;
static bool s_suppress_next_cloud_success_tone = false;
static TimerHandle_t s_cloud_success_tone_timer = NULL;
static TickType_t s_bind_tone_tick = 0;
static TickType_t s_boot_tick = 0;
static char s_current_tts_url[ASYNC_PLAY_URL_MAX] = {0};
/* TTS 活跃标志：在调用 async_play_url 前置 true，事件回调收到 stop/error/complete
 * 时清零。读取无需获取播放器的 operation_lock，避免 ebus 回调在 prepare 阶段被
 * 长时间阻塞（async_play 线程跑 HTTP recv 时持锁可能达数秒）。 */
static volatile bool s_tts_active = false;

/* 提示音排队播放状态 */
static tone_play_request_t s_tone_pending_queue[TONE_PENDING_QUEUE_SIZE];
static uint8_t s_tone_pending_head = 0;
static uint8_t s_tone_pending_count = 0;
static bool s_tone_playing = false;
static bool s_current_tone_prompt = false;

static void voice_player_tts_track_url(const char *url);
static void voice_player_flush_tone_queue(void);

#define CLOUD_SUCCESS_TONE_DELAY_MS 2000
#define CLOUD_UNSTABLE_TONE_GRACE_MS 800

typedef struct {
    uint8_t *buf;
    size_t len;
    bool overflow;
} pushup_tts_download_ctx_t;

static bool voice_player_url_is_mp3(const char *url)
{
    const char *query;
    size_t len;

    if (url == NULL) {
        return false;
    }

    query = strchr(url, '?');
    len = query ? (size_t)(query - url) : strlen(url);

    return len >= 4 &&
           url[len - 4] == '.' &&
           url[len - 3] == 'm' &&
           url[len - 2] == 'p' &&
           url[len - 1] == '3';
}

static bool voice_player_tts_url_is_latest(const char *url)
{
    bool latest;

    if (url == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    latest = strncmp(s_current_tts_url, url, sizeof(s_current_tts_url)) == 0;
    taskEXIT_CRITICAL();

    return latest;
}

static void pushup_tts_http_ignore_data(lisa_http_data_t *data)
{
    (void)data;
}

static void *pushup_tts_http_headers(void)
{
    return (void *)"Accept: audio/mpeg";
}

static int pushup_tts_download_on_chunk(lisa_http_data_t *data)
{
    pushup_tts_download_ctx_t *ctx;
    size_t need;

    if (data == NULL || data->user == NULL || data->len <= 0) {
        return 0;
    }

    ctx = (pushup_tts_download_ctx_t *)data->user;
    need = ctx->len + (size_t)data->len;
    if (need > PUSHUP_TTS_DOWNLOAD_MAX_BYTES) {
        ctx->overflow = true;
        return -1;
    }

    memcpy(ctx->buf + ctx->len, data->buf, (size_t)data->len);
    ctx->len = need;

    return 0;
}

static int voice_player_download_pushup_tts_mp3(const char *url, uint8_t *buf, size_t *out_len)
{
    pushup_tts_download_ctx_t ctx = {
        .buf = buf,
    };
    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .headers = (uint8_t *)pushup_tts_http_headers,
        .timeout = PUSHUP_TTS_HTTP_TIMEOUT_MS,
        .user = &ctx,
        .on_data = pushup_tts_http_ignore_data,
    };
    lisa_http_t *http;
    lisa_http_err_e ret;

    if (url == NULL || buf == NULL || out_len == NULL) {
        return -1;
    }

    *out_len = 0;

    http = lisa_http_init(&req);
    if (http == NULL) {
        LOGE("pushup tts mp3 http init failed");
        return -1;
    }

    ret = lisa_http_perform_chunked_with_cb(http, pushup_tts_download_on_chunk);
    lisa_http_cleanup(http);

    if (ret != LISA_HTTP_OK || ctx.overflow || ctx.len == 0) {
        LOGW("pushup tts mp3 download failed, ret=%d, len=%u, overflow=%d",
             ret,
             (unsigned int)ctx.len,
             ctx.overflow ? 1 : 0);
        return -1;
    }

    *out_len = ctx.len;
    return 0;
}

static int voice_player_wait_pushup_tts_mp3_ready(const char *url)
{
    uint8_t *buf = NULL;
    size_t latest_len = 0;
    bool has_download = false;
    uint8_t stable_count = 0;

    buf = (uint8_t *)psram_malloc(PUSHUP_TTS_DOWNLOAD_MAX_BYTES);
    if (buf == NULL) {
        LOGE("pushup tts mp3 psram malloc failed, size=%u",
             (unsigned int)PUSHUP_TTS_DOWNLOAD_MAX_BYTES);
        return -1;
    }

    for (uint8_t attempt = 1; attempt <= PUSHUP_TTS_READY_MAX_ATTEMPTS; attempt++) {
        size_t len = 0;

        if (!voice_player_tts_url_is_latest(url)) {
            LOGW("drop stale pushup tts mp3 url: %s", url);
            psram_free(buf);
            return 1;
        }

        if (voice_player_download_pushup_tts_mp3(url, buf, &len) != 0) {
            LOGW("pushup tts mp3 ready attempt %u/%u failed",
                 (unsigned int)attempt,
                 (unsigned int)PUSHUP_TTS_READY_MAX_ATTEMPTS);
            psram_free(buf);
            return -1;
        }

        if (has_download && len == latest_len) {
            stable_count++;
        } else {
            stable_count = 1;
        }

        latest_len = len;
        has_download = true;

        LOGI("pushup tts mp3 ready attempt %u/%u, size=%u, stable=%u/%u",
             (unsigned int)attempt,
             (unsigned int)PUSHUP_TTS_READY_MAX_ATTEMPTS,
             (unsigned int)latest_len,
             (unsigned int)stable_count,
             (unsigned int)PUSHUP_TTS_READY_STABLE_COUNT);

        if (stable_count >= PUSHUP_TTS_READY_STABLE_COUNT) {
            break;
        }

        if (attempt < PUSHUP_TTS_READY_MAX_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(PUSHUP_TTS_READY_POLL_MS));
        }
    }

    if (!has_download) {
        psram_free(buf);
        return -1;
    }

    if (!voice_player_tts_url_is_latest(url)) {
        LOGW("drop stale pushup tts mp3 url before play: %s", url);
        psram_free(buf);
        return 1;
    }

    if (stable_count < PUSHUP_TTS_READY_STABLE_COUNT) {
        LOGW("pushup tts mp3 still growing after %u ms, fallback direct url, latest size=%u",
             (unsigned int)((PUSHUP_TTS_READY_MAX_ATTEMPTS - 1U) * PUSHUP_TTS_READY_POLL_MS),
             (unsigned int)latest_len);
        psram_free(buf);
        return -1;
    }

    LOGI("pushup tts mp3 ready, size=%u, play direct url", (unsigned int)latest_len);
    psram_free(buf);
    return 0;
}

/* 异步播放任务 */
static void async_play_task(void *pvParameters)
{
    async_play_request_t request;

    while (1) {
        /* 从队列中获取播放请求，阻塞等待 */
        if (xQueueReceive(s_async_play_queue, &request, portMAX_DELAY) == pdTRUE) {
            LOGI("async play task received request, type=%u, player=%p, wait_mp3=%u, url=%s",
                 (unsigned int)request.type,
                 request.player,
                 (unsigned int)request.wait_mp3_ready,
                 request.url);

            if (request.type == ASYNC_PLAY_REQUEST_RESUME) {
                app_player_resume(request.player);
                continue;
            }

            if (request.player == tts_player &&
                !request.bypass_tts_gate &&
                voice_intent_photo_flow_should_gate_tts()) {
                continue;
            }

            if (request.player == tts_player &&
                request.wait_mp3_ready &&
                voice_player_url_is_mp3(request.url)) {
                int ready_ret = voice_player_wait_pushup_tts_mp3_ready(request.url);
                if (ready_ret > 0) {
                    continue;
                }
                if (ready_ret < 0) {
                    LOGW("pushup tts mp3 ready workaround failed, fallback to direct url");
                }
            }

            /* 在独立任务中执行播放操作，不会阻塞ebus线程 */
            app_player_play(request.player, request.url);
        }
    }
}

/* 初始化异步播放队列和任务 */
static int async_play_init(void)
{
    if (s_async_play_queue != NULL) {
        return 0; /* 已经初始化 */
    }

    /* 创建播放请求队列 */
    s_async_play_queue = xQueueCreate(ASYNC_PLAY_QUEUE_SIZE, sizeof(async_play_request_t));
    if (s_async_play_queue == NULL) {
        LOGE("failed to create async play queue");
        return -1;
    }

    /* 创建异步播放任务 */
    BaseType_t ret = xTaskCreate(
        async_play_task,
        "async_play",
        ASYNC_PLAY_TASK_STACK_SIZE,
        NULL,
        ASYNC_PLAY_TASK_PRIORITY,
        &s_async_play_task
    );

    if (ret != pdPASS) {
        LOGE("failed to create async play task");
        vQueueDelete(s_async_play_queue);
        s_async_play_queue = NULL;
        return -1;
    }

    LOGI("async play task created successfully");
    return 0;
}

/* 异步播放接口，发送播放请求到队列 */
static void async_play_url_internal(app_player_t *player, const char *url, bool bypass_tts_gate)
{
    if (s_async_play_queue == NULL || url == NULL) {
        LOGE("async play queue not ready or url is null");
        return;
    }

    async_play_request_t request = {0};
    request.type = ASYNC_PLAY_REQUEST_URL;
    request.bypass_tts_gate = bypass_tts_gate ? 1U : 0U;
    request.player = player;
    strncpy(request.url, url, sizeof(request.url) - 1);
    request.url[sizeof(request.url) - 1] = '\0';

    if (player == tts_player) {
        s_tts_active = true;
    }

    /* 发送到队列，最多等待100ms */
    if (xQueueSend(s_async_play_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("failed to send play request to queue");
    } else {
        LOGI("play request sent to async queue, player=%p", player);
    }
}

static void async_play_url(app_player_t *player, const char *url)
{
    async_play_url_internal(player, url, false);
}

static void async_play_tts_mp3_ready_url(const char *url)
{
    if (s_async_play_queue == NULL || url == NULL) {
        LOGE("async play queue not ready or url is null");
        return;
    }

    async_play_request_t request = {0};
    request.type = ASYNC_PLAY_REQUEST_URL;
    request.wait_mp3_ready = 1U;
    request.player = tts_player;
    strncpy(request.url, url, sizeof(request.url) - 1);
    request.url[sizeof(request.url) - 1] = '\0';

    s_tts_active = true;

    if (xQueueSend(s_async_play_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("failed to send tts mp3 ready request to queue");
    } else {
        LOGI("tts mp3 ready request sent to async queue");
    }
}

static void voice_player_queue_pushup_tts_ready_check(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        LOGW("pushup tts ready check skipped: empty url");
        return;
    }

    LOGW("queue pushup tts ready check every %u ms: %s",
         (unsigned int)PUSHUP_TTS_READY_POLL_MS,
         url);
    async_play_tts_mp3_ready_url(url);
}

static void async_resume_player(app_player_t *player)
{
    if (s_async_play_queue == NULL || player == NULL) {
        LOGE("async play queue not ready or player is null");
        return;
    }

    async_play_request_t request = {0};
    request.type = ASYNC_PLAY_REQUEST_RESUME;
    request.player = player;

    if (player == tts_player) {
        s_tts_active = true;
    }

    if (xQueueSend(s_async_play_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("failed to send resume request to queue");
    } else {
        LOGI("resume request sent to async queue, player=%p", player);
    }
}

static void voice_player_tts_track_url(const char *url)
{
    if (!url || url[0] == '\0') {
        return;
    }

    taskENTER_CRITICAL();
    strncpy(s_current_tts_url, url, sizeof(s_current_tts_url) - 1);
    s_current_tts_url[sizeof(s_current_tts_url) - 1] = '\0';
    taskEXIT_CRITICAL();
    LOGI("refresh latest tts url: %s", s_current_tts_url);
}

void voice_player_replay_tts_url(const char *url)
{
    if (!url || url[0] == '\0') {
        LOGW("replay tts url: empty url");
        return;
    }
    LOGI("replay tts url: %s", url);
    async_play_url_internal(tts_player, url, true);
}

void voice_player_play_tts_url_async(const char *url)
{
    if (!url || url[0] == '\0') {
        LOGW("play tts url async: empty url");
        return;
    }
    voice_player_tts_track_url(url);
    async_play_url_internal(tts_player, url, true);
}

bool voice_player_latest_tts_url_copy(char *url_buf, size_t buf_len)
{
    bool has_url = false;

    taskENTER_CRITICAL();
    has_url = s_current_tts_url[0] != '\0';
    if (has_url && url_buf != NULL && buf_len > 0) {
        strncpy(url_buf, s_current_tts_url, buf_len - 1);
        url_buf[buf_len - 1] = '\0';
    }
    taskEXIT_CRITICAL();

    return has_url;
}

bool voice_player_tts_is_active(void)
{
    return s_tts_active;
}

/* 闹钟播放阶段管理 */
typedef enum {
    ALARM_PLAY_PHASE_TONE_FIRST = 0,  // 首次播放默认提示音
    ALARM_PLAY_PHASE_TTS,              // 播放云端TTS
    ALARM_PLAY_PHASE_TONE_LOOP,        // 循环播放默认提示音
} alarm_play_phase_t;

static struct {
    alarm_play_phase_t phase;
    char text[ALARM_TTS_BUF_SIZE];
} s_alarm_play_ctx __psram_bss__;

static uint32_t xorshift32(void)
{
    static uint32_t state = 0;
    if (state == 0) {
        state = lisa_rand32() | 1;
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static char *voice_player_get_wakeup_tone_url(void)
{
    static bool s_wakeup_tone_inited = false;
    static uint16_t s_wakeup_tone_ids[5] = {0};
    static uint8_t s_wakeup_tone_cnt = 0;
    static bool s_last_wakeup_tone_valid = false;
    static uint16_t s_last_wakeup_tone_id = TONE_ID_0;


    if (!s_wakeup_tone_inited) {
        for (uint16_t tone_id = TONE_ID_0; tone_id <= TONE_ID_4; tone_id++) {
            if (app_tone_get_url(tone_id) != NULL) {
                s_wakeup_tone_ids[s_wakeup_tone_cnt++] = tone_id;
            }
        }

        if (s_wakeup_tone_cnt == 0) {
            s_wakeup_tone_ids[s_wakeup_tone_cnt++] = TONE_ID_0;
        }

        s_wakeup_tone_inited = true;
        LOGI("wakeup tone candidates count: %u", (unsigned int)s_wakeup_tone_cnt);
    }

    uint16_t tone_id = s_wakeup_tone_ids[0];
    if (s_wakeup_tone_cnt > 1) {
        uint8_t attempts = 0;
        do {
            tone_id = s_wakeup_tone_ids[xorshift32() % s_wakeup_tone_cnt];
        } while (s_last_wakeup_tone_valid && tone_id == s_last_wakeup_tone_id && ++attempts < 8);
    }

    LOGI("Selected wakeup tone id: %u", tone_id);

    char *tone_url = app_tone_get_url(tone_id);
    if (tone_url == NULL) {
        tone_id = TONE_ID_0;
        tone_url = app_tone_get_url(tone_id);
    }

    if (tone_url != NULL) {
        s_last_wakeup_tone_id = tone_id;
        s_last_wakeup_tone_valid = true;
    }

    return tone_url;
}

static int voice_player_prompt_tone_enter(void)
{
    if (voice_intent_contains(INTENT_PROMPT_TONE)) {
        return 0;
    }

    return voice_intent_push(INTENT_PROMPT_TONE);
}

static void voice_player_prompt_tone_exit(void)
{
    if (voice_intent_contains(INTENT_PROMPT_TONE)) {
        voice_intent_pop(INTENT_PROMPT_TONE);
    }
}

static bool voice_player_start_tone_request(const tone_play_request_t *request)
{
    bool entered_prompt = false;

    if (request == NULL || request->url == NULL) {
        LOGW("tone request is null, skip");
        return false;
    }

    if (request->prompt_tone && !voice_intent_contains(INTENT_PROMPT_TONE)) {
        if (voice_player_prompt_tone_enter() == 0) {
            entered_prompt = true;
        } else {
            LOGW("failed to enter PROMPT_TONE intent, play without music resume focus");
        }
    }

    s_tone_playing = true;
    s_current_tone_prompt = request->prompt_tone && voice_intent_contains(INTENT_PROMPT_TONE);

    if (app_player_play(tone_player, request->url) != APP_PLAYER_OK) {
        LOGW("failed to play tone url");
        s_tone_playing = false;
        s_current_tone_prompt = false;
        if (entered_prompt) {
            voice_player_prompt_tone_exit();
        }
        return false;
    }

    return true;
}

static void voice_player_start_next_queued_tone(void)
{
    while (s_tone_pending_count > 0) {
        tone_play_request_t request = s_tone_pending_queue[s_tone_pending_head];
        s_tone_pending_head = (s_tone_pending_head + 1) % TONE_PENDING_QUEUE_SIZE;
        s_tone_pending_count--;

        if (voice_player_start_tone_request(&request)) {
            return;
        }
    }

    s_tone_playing = false;
    s_current_tone_prompt = false;
}

static bool voice_player_play_tone_url_internal(const char *url, bool prompt_tone)
{
    if (url == NULL) {
        LOGW("tone url is null, skip");
        return false;
    }

    if (s_tone_playing) {
        if (s_tone_pending_count < TONE_PENDING_QUEUE_SIZE) {
            s_tone_pending_queue[(s_tone_pending_head + s_tone_pending_count) % TONE_PENDING_QUEUE_SIZE] =
                (tone_play_request_t){
                    .url = url,
                    .prompt_tone = prompt_tone,
                };
            s_tone_pending_count++;
            LOGD("tone queued, pending: %u", (unsigned int)s_tone_pending_count);
            return true;
        } else {
            LOGW("tone queue full, drop");
            return false;
        }
    }

    return voice_player_start_tone_request(&(tone_play_request_t){
        .url = url,
        .prompt_tone = prompt_tone,
    });
}

void voice_player_play_tone_url(const char *url)
{
    (void)voice_player_play_tone_url_internal(url, false);
}

void voice_player_play_prompt_tone_url(const char *url)
{
    (void)voice_player_play_tone_url_internal(url, true);
}

static void voice_player_flush_tone_queue(void)
{
    s_tone_pending_head = 0;
    s_tone_pending_count = 0;
    s_tone_playing = false;
    s_current_tone_prompt = false;
    voice_player_prompt_tone_exit();
}

static void voice_player_play_wakeup_tone(void)
{
    char *tone_url = voice_player_get_wakeup_tone_url();
    if (tone_url == NULL) {
        LOGE("wakeup tone url is null");
        return;
    }

    voice_player_play_tone_url(tone_url);
}

static void voice_player_play_network_success_tone(void)
{
    static TickType_t s_network_success_tone_tick = 0;
    TickType_t now = xTaskGetTickCount();

    if (s_network_success_tone_tick != 0 &&
        (now - s_network_success_tone_tick) < pdMS_TO_TICKS(3000U)) {
        return;
    }

    voice_player_play_tone_url(app_tone_get_url(TONE_ID_59));
    s_network_success_tone_tick = now;
}

static void cloud_success_tone_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    voice_player_play_network_success_tone();
}

static void voice_player_cancel_cloud_connect_success_tone(void)
{
    if (s_cloud_success_tone_timer != NULL &&
        xTimerIsTimerActive(s_cloud_success_tone_timer) != pdFALSE &&
        xTimerStop(s_cloud_success_tone_timer, 0) != pdPASS) {
        LOGW("failed to stop cloud success tone timer");
    }

    s_cloud_success_tone_played = false;
}

static void voice_player_schedule_cloud_connect_success_tone(void)
{
    if (s_cloud_success_tone_timer == NULL) {
        voice_player_play_network_success_tone();
        return;
    }

    if (xTimerReset(s_cloud_success_tone_timer, 0) != pdPASS) {
        LOGW("failed to start cloud success tone timer");
        voice_player_play_network_success_tone();
    }
}

static void voice_player_play_bind_tone(void)
{
    TickType_t now = xTaskGetTickCount();

    if (s_bind_tone_tick != 0 &&
        (now - s_bind_tone_tick) < pdMS_TO_TICKS(3000U)) {
        return;
    }

    voice_player_play_tone_url(app_tone_get_url(TONE_ID_71));
    s_bind_tone_tick = now;
}

static void voice_player_handle_cloud_connect_success(bool force_play)
{
    s_disconnect_tone_played = false;
    s_cloud_unstable_pending = false;
    s_cloud_unstable_pending_tick = 0;

    if (!force_play && !s_cloud_reconnect_pending) {
        return;
    }

    if (s_cloud_success_tone_played) {
        s_cloud_reconnect_pending = false;
        return;
    }

    voice_player_schedule_cloud_connect_success_tone();
    s_cloud_success_tone_played = true;
    s_cloud_reconnect_pending = false;
}

static void voice_player_skip_cloud_connect_success_tone(void)
{
    voice_player_cancel_cloud_connect_success_tone();
    s_disconnect_tone_played = false;
    s_cloud_unstable_pending = false;
    s_cloud_unstable_pending_tick = 0;
    s_cloud_reconnect_pending = false;
    s_cloud_success_tone_played = false;
}

static bool voice_player_is_cloud_unbound_active(void)
{
    return voice_cloud_is_connected() &&
           voice_cloud_get_state() == VOICE_CLOUD_STATE_CONNECT_FAILED;
}

static bool voice_player_should_skip_disconnect_tone(const sys_network_status_t *network_status,
                                                     bool network_status_ok)
{
    if (network_status_ok && network_status->switching) {
        return true;
    }

    /*
     * Keep disconnect tones quiet in explicit provisioning flow:
     * saved APs have been cleared (e.g. user multi-click to enter netcfg).
     */
    if (!sys_wifi_has_ap()) {
        s_disconnect_tone_played = false;
        s_cloud_unstable_pending = false;
        s_cloud_unstable_pending_tick = 0;
        s_cloud_reconnect_pending = false;
        s_cloud_success_tone_played = false;
        LOGI("skip network disconnect tone because no saved AP");
        return true;
    }

    return false;
}

static bool voice_player_has_network_link(const sys_network_status_t *network_status,
                                          bool network_status_ok)
{
    if (!network_status_ok) {
        return false;
    }

    if (network_status->active_bearer == SYS_NETWORK_BEARER_WIFI) {
        return network_status->wifi_connected;
    }

    if (network_status->active_bearer == SYS_NETWORK_BEARER_MODEM) {
        return network_status->modem_connected;
    }

    return false;
}

static bool voice_player_should_play_cloud_unstable_tone(const sys_network_status_t *network_status,
                                                         bool network_status_ok)
{
    if (!voice_player_has_network_link(network_status, network_status_ok)) {
        return false;
    }

    /*
     * Cloud disconnected while bearer link is still alive but probe already
     * reports internet unavailable: this is the "network unstable" case.
     */
    return !network_status->connected;
}

static void voice_player_reset_cloud_unstable_pending(void)
{
    s_cloud_unstable_pending = false;
    s_cloud_unstable_pending_tick = 0;
}

static void voice_player_try_play_cloud_unstable_tone(const sys_network_status_t *network_status,
                                                      bool network_status_ok)
{
    TickType_t now;

    if (s_disconnect_tone_played) {
        return;
    }

    if (!voice_player_should_play_cloud_unstable_tone(network_status, network_status_ok)) {
        voice_player_reset_cloud_unstable_pending();
        return;
    }

    now = xTaskGetTickCount();
    if (!s_cloud_unstable_pending) {
        s_cloud_unstable_pending = true;
        s_cloud_unstable_pending_tick = now;
        return;
    }

    if ((now - s_cloud_unstable_pending_tick) < pdMS_TO_TICKS(CLOUD_UNSTABLE_TONE_GRACE_MS)) {
        return;
    }

    voice_player_play_tone_url(app_tone_get_url(TONE_ID_65));
    s_disconnect_tone_played = true;
    voice_player_reset_cloud_unstable_pending();
}

void voice_player_play_msg(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{

    struct app_datas *app_datas = get_app_datas();
    sys_network_status_t network_status = {0};
    bool network_status_ok = sys_network_get_status(&network_status) == 0;

    if (app_datas == NULL) {
        LOGW("Invalid app_datas");
        return;
    }
    switch (msg_id) {
    case VOICE_MSG_WAKEUP_BUTTON_START: {
        if (service_sd_music_is_syncing()) {
            LOGI("ignore button wakeup tone during SD music sync");
            return;
        }

        if ((app_datas->voice_work_mode & VOICE_WORK_MODE_BUTTON_WAKEUP) == 0) {
            return;
        }

        if (!app_datas->can_wakeup) {
            return;
        }

        if (voice_cloud_is_connected()) {
            voice_player_play_wakeup_tone();
        } else if (!network_status_ok || !network_status.connected) {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_64));
        } else if (app_datas->auth_failed) {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_105));
        } else {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_85));
        }
    } break;
    case VOICE_MSG_WAKEUP_KEYWORD: {
        if (service_sd_music_is_syncing()) {
            LOGI("ignore keyword wakeup tone during SD music sync");
            return;
        }

        if ((app_datas->voice_work_mode & VOICE_WORK_MODE_VOICE_WAKEUP) == 0) {
            return;
        }

        if (!app_datas->can_wakeup) {
            return;
        }

        if (voice_cloud_is_connected()) {
            voice_player_play_wakeup_tone();
        } else if (!network_status_ok || !network_status.connected) {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_64));
        } else if (app_datas->auth_failed) {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_105));
        } else {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_85));
        }
    } break;
    case VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS: {
        app_datas->auth_failed = 0;
        voice_player_handle_cloud_connect_success(true);
    } break;
    case VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED: {
        voice_player_cancel_cloud_connect_success_tone();
        app_datas->auth_failed = 1;
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_105));
    } break;
    case VOICE_MSG_CLOUD_OPEN_INFO: {
        uint32_t status = 0;

        if (data == NULL || len != sizeof(status)) {
            break;
        }

        memcpy(&status, data, sizeof(status));
        if (status == CLOUD_OPEN_INFO_STATUS_BIND) {
            voice_player_cancel_cloud_connect_success_tone();
            if (voice_player_is_cloud_unbound_active()) {
                voice_player_flush_tone_queue();
                app_player_stop(tone_player);
                app_player_stop(tts_player);
                break;
            }
            voice_player_play_bind_tone();
        }
    } break;
    case VOICE_MSG_BLE_AUTH_INFO_DONE: {
        s_suppress_next_cloud_success_tone =
            (voice_cloud_get_state() == VOICE_CLOUD_STATE_CONNECT_FAILED);
    } break;
    case VOICE_MSG_CLOUD_CONNECTED: {
        if (s_suppress_next_cloud_success_tone) {
            s_suppress_next_cloud_success_tone = false;
            voice_player_skip_cloud_connect_success_tone();
            break;
        }
        voice_player_handle_cloud_connect_success(false);
    } break;
    case VOICE_MSG_CLOUD_DISCONNECTED: {
        voice_player_cancel_cloud_connect_success_tone();
        if (voice_player_should_skip_disconnect_tone(&network_status, network_status_ok)) {
            break;
        }
        s_cloud_reconnect_pending = true;
        s_cloud_success_tone_played = false;
        voice_player_try_play_cloud_unstable_tone(&network_status, network_status_ok);
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_CONNECTED: {
        voice_player_reset_cloud_unstable_pending();
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL: {
        if (voice_player_should_skip_disconnect_tone(&network_status, network_status_ok)) {
            break;
        }
        voice_player_try_play_cloud_unstable_tone(&network_status, network_status_ok);
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS: {
        voice_player_reset_cloud_unstable_pending();
        if (s_disconnect_tone_played) {
            s_disconnect_tone_played = false;
            voice_player_play_network_success_tone();
        }
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED: {
        voice_player_reset_cloud_unstable_pending();
        if (voice_player_should_skip_disconnect_tone(&network_status, network_status_ok)) {
            break;
        }

        if (!s_disconnect_tone_played) {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_60));
            s_disconnect_tone_played = true;
        }
    } break;
    case VOICE_MSG_BLE_CONNECT_DONE: {
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_72));
    } break;
    case VOICE_MSG_CLOUD_TTS_URL: {
        if (data == NULL) {
            LOGE("Invalid data");
            break;
        }

        LOGI("Ready to play tts url asynchronously: %s", (char *)data);
        voice_player_tts_track_url((char *)data);
        if (voice_intent_photo_flow_should_gate_tts()) {
            break;
        }
        /* 使用异步播放接口，避免在ebus回调中阻塞HTTP下载 */
        async_play_url(tts_player, (char *)data);

    } break;
    case VOICE_MSG_CLOUD_PUSHUP_TTS_URL: {
        if (data == NULL) {
            LOGE("Invalid data");
            break;
        }

        voice_player_cancel_cloud_connect_success_tone();
        if (voice_player_is_cloud_unbound_active()) {
            voice_player_flush_tone_queue();
            app_player_stop(tone_player);
            app_player_stop(tts_player);
        }

        LOGI("Ready to play tts url asynchronously: %s", (char *)data);
        voice_player_tts_track_url((char *)data);
        if (voice_intent_photo_flow_should_gate_tts()) {
            break;
        }
        /* 临时规避：pushup MP3 可能还在云端生成，下载到连续两次大小一致后再播。 */
        voice_player_queue_pushup_tts_ready_check((char *)data);

    } break;
    case VOICE_MSG_CLOUD_MCP_CALL_RESP:
        break;
    default:
        break;
    }
}

static void voice_player_restart_music_intent(void)
{
    if (voice_intent_contains(INTENT_MUSIC)) {
        voice_intent_pop(INTENT_MUSIC);
    }
    if (voice_intent_contains(INTENT_VOICE_SESSION)) {
        LOGI("force finish voice session before music playback");
        voice_intent_pop(INTENT_VOICE_SESSION);
    }
    voice_intent_music_set_user_paused(false);
    voice_intent_push(INTENT_MUSIC);
}

/* 云端歌曲列表到达（MCP kuwo / audio_url 等工具）。
 * 替换当前在线歌单为收到的曲目，从头开始播放，并通知 UI 显示第一首歌名。 */
static void voice_player_audio_item(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct voice_msg_audio_items *msg_items = (struct voice_msg_audio_items *)data;
    if (msg_items == NULL || msg_items->cnt == 0) {
        LOGE("Invalid data");
        return;
    }

    int count = (msg_items->cnt > 50) ? 50 : (int)msg_items->cnt;

    music_item_t *tracks = lisa_mem_alloc(count * sizeof(music_item_t));
    if (tracks == NULL) {
        LOGE("Failed to alloc tracks");
        return;
    }
    memset(tracks, 0, count * sizeof(music_item_t));

    for (int i = 0; i < count; i++) {
        memcpy(tracks[i].mid, msg_items->items[i].id, sizeof(msg_items->items[i].id));
        memcpy(tracks[i].m_name, msg_items->items[i].name, sizeof(msg_items->items[i].name));
        /* play_audio_link 链路会预填充 url，直接复制即可跳过 URL 解析 */
        if (msg_items->items[i].url[0]) {
            memcpy(tracks[i].m_url, msg_items->items[i].url, sizeof(msg_items->items[i].url));
        }
    }

    char first_track_name[AUIDO_OUT_NAME_LEN] = {0};
    if (tracks[0].m_name[0]) {
        strncpy(first_track_name, tracks[0].m_name, sizeof(first_track_name) - 1);
    }

    if (voice_music_list_set(MUSIC_LIST_ONLINE, tracks, count) != 0) {
        LOGE("Failed to set online music list");
        lisa_mem_free(tracks);
        return;
    }
    voice_music_list_set_active(MUSIC_LIST_ONLINE);
    voice_music_list_set_current_index(0);
    lisa_mem_free(tracks);

    /* apps-ui 显示歌曲名称*/
    if (first_track_name[0]) {
        voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, first_track_name, strlen(first_track_name) + 1);
    }

    voice_player_restart_music_intent();
}

/* 用户主动播放控制（MCP ls.playback_control 工具驱动）。
 *
 * PLAY/PAUSE 只改变播放状态，不改变 MUSIC intent（仍在栈上）。
 * PAUSE 通过 s_user_paused 标记阻止后续自动 resume。
 * NEXT/PREVIOUS/REPLAY 切换曲目，同时清除 s_user_paused。
 * STOP 直接 pop MUSIC → on_exit → stop 播放器。 */
static void voice_player_play_control(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    switch (msg_id) {

    /* ---- 播放状态控制 ---- */
    case VOICE_MSG_PLAY_CONTROL_PLAY: {
        voice_intent_music_set_user_paused(false);
        app_player_resume(music_player);
    } break;
    case VOICE_MSG_PLAY_CONTROL_PAUSE: {
        voice_intent_music_set_user_paused(true);
        app_player_pause(music_player);
        voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, NULL, 0);
    } break;

    /* ---- 曲目切换 ---- */
    case VOICE_MSG_PLAY_CONTROL_NEXT: {
        voice_intent_music_set_user_paused(false);
        music_item_t next_track;
        if (voice_music_list_get_next(&next_track) == 0) {
            if (voice_player_play_music_url(next_track.m_url) == APP_PLAYER_OK &&
                next_track.m_name[0]) {
                voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, next_track.m_name, strlen(next_track.m_name) + 1);
            }
        }
    } break;
    case VOICE_MSG_PLAY_CONTROL_PREVIOUS: {
        voice_intent_music_set_user_paused(false);
        music_item_t prev_track;
        if (voice_music_list_get_prev(&prev_track) == 0) {
            if (voice_player_play_music_url(prev_track.m_url) == APP_PLAYER_OK &&
                prev_track.m_name[0]) {
                voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, prev_track.m_name, strlen(prev_track.m_name) + 1);
            }
        }
    } break;
    case VOICE_MSG_PLAY_CONTROL_REPLAY: {
        voice_intent_music_set_user_paused(false);
        music_item_t curr_track;
        if (voice_music_list_get_current(&curr_track) == 0) {
            if (voice_player_play_music_url(curr_track.m_url) == APP_PLAYER_OK &&
                curr_track.m_name[0]) {
                voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, curr_track.m_name, strlen(curr_track.m_name) + 1);
            }
        }
    } break;

    /* ---- 停止 ---- */
    case VOICE_MSG_PLAY_CONTROL_STOP: {
        voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, NULL, 0);
        voice_intent_pop(INTENT_MUSIC);
    } break;
    default:
        break;
    }
}

static void voice_alarm_ring_play_once(const char *text)
{
    struct app_datas *app_datas = get_app_datas();

    switch (s_alarm_play_ctx.phase) {
        case ALARM_PLAY_PHASE_TONE_FIRST:
            /* 第一阶段：播放默认闹钟提示音（一次） */
            LISA_LOGI(TAG, "alarm play phase: TONE_FIRST");
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_94));
            s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TTS;
            if (text) {
                strncpy(s_alarm_play_ctx.text, text, sizeof(s_alarm_play_ctx.text) - 1);
            }
            break;

        case ALARM_PLAY_PHASE_TTS:
            /* 第二阶段：播放云端TTS（一次） */
            LISA_LOGI(TAG, "alarm play phase: TTS");
            if (app_datas == NULL || app_datas->voice_cloud_connected == 0 ||
                !s_alarm_play_ctx.text || s_alarm_play_ctx.text[0] == 0) {
                /* 云端未连接或文本为空，跳过TTS，直接进入循环播放提示音 */
                LISA_LOGI(TAG, "alarm TTS skipped: no cloud or no text");
                s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_LOOP;
                voice_player_play_tone_url(app_tone_get_url(TONE_ID_94));
                break;
            }

            char *tts_text = lisa_mem_alloc(ALARM_TTS_BUF_SIZE);
            if (tts_text == NULL) {
                LISA_LOGW(TAG, "alarm TTS alloc failed, fallback to tone loop");
                s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_LOOP;
                voice_player_play_tone_url(app_tone_get_url(TONE_ID_94));
                break;
            }

            snprintf(tts_text, ALARM_TTS_BUF_SIZE, "你有一个 %s 的提醒, 请不要忘记哦!", s_alarm_play_ctx.text);
            voice_cloud_tts_synth(tts_text);
            lisa_mem_free(tts_text);
            s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_LOOP;
            break;

        case ALARM_PLAY_PHASE_TONE_LOOP:
            /* 第三阶段：循环播放默认闹钟提示音 */
            LISA_LOGI(TAG, "alarm play phase: TONE_LOOP");
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_94));
            break;
    }
}

static void voice_alarm_ring_force_stop(void)
{
    voice_player_flush_tone_queue();
    app_player_stop(tone_player);
    app_player_stop(tts_player);

    /* 重置闹钟播放状态机，确保下次触发时从头开始 */
    s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_FIRST;
    memset(s_alarm_play_ctx.text, 0, sizeof(s_alarm_play_ctx.text));
}


static void on_tts_event(app_player_t *player, app_player_event_t event, void *user_data)
{
    LOGI("tts player event: %d", event);

    switch (event) {
    case APP_PLAYER_EVENT_PLAYING:
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PLAYING, NULL, 0);
        break;
    case APP_PLAYER_EVENT_PAUSED:
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PAUSED, NULL, 0);
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        s_tts_active = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        if (alarm_ring_is_active()) {
            alarm_ring_notify_playback_complete();
        }
        break;
    case APP_PLAYER_EVENT_STOPPED:
    case APP_PLAYER_EVENT_ERROR:
        s_tts_active = false;
        /* 始终发布 STOPED，on_tts_stoped 中通过 voice_intent_top() 判断是否 pop，
         * preemption 时 VOICE_SESSION 不在栈顶，不会被误弹出。 */
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        break;
    default:
        break;
    }
}


static void on_tone_event(app_player_t *player, app_player_event_t event, void *user_data)
{
    switch (event) {
    case APP_PLAYER_EVENT_COMPLETED: {
        bool completed_prompt = s_current_tone_prompt;

        s_current_tone_prompt = false;
        voice_player_start_next_queued_tone();

        if (completed_prompt && !s_current_tone_prompt) {
            voice_player_prompt_tone_exit();
        }
        if (alarm_ring_is_active()) {
            alarm_ring_notify_playback_complete();
        }
        break;
    }
    case APP_PLAYER_EVENT_STOPPED:
    case APP_PLAYER_EVENT_ERROR:
        voice_player_flush_tone_queue();
        break;
    default:
        break;
    }
}

void voice_player_ready(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_LOGI(TAG, "voice_player_ready");

    s_boot_tick = xTaskGetTickCount();

    /* 初始化交互意图栈（含 ebus 订阅 + 各 intent 钩子注册） */
    voice_intent_mgr_init();

    /* 初始化异步播放队列和任务 */
    if (async_play_init() != 0) {
        LOGE("async play init failed");
    }

    sys_network_status_t network_status = {0};
    bool wifi_mode = (sys_network_get_status(&network_status) == 0) &&
                     (network_status.mode == SYS_NETWORK_MODE_WIFI);

    if (wifi_mode && !sys_wifi_has_ap()) {
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_70));
    }


    if (s_cloud_success_tone_timer == NULL) {
        s_cloud_success_tone_timer = xTimerCreate("cloud.success", pdMS_TO_TICKS(CLOUD_SUCCESS_TONE_DELAY_MS),
                                                  pdFALSE, NULL, cloud_success_tone_timer_cb);
        if (s_cloud_success_tone_timer == NULL) {
            LOGE("failed to create cloud success tone timer");
        }
    }

    // 提示音播放 控制 tone_player
    voice_msg_sub(VOICE_MSG_WAKEUP_BUTTON_START, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_OPEN_INFO, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_CONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_BLE_AUTH_INFO_DONE, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CALL_RESP, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_BLE_CONNECT_DONE, voice_player_play_msg, NULL);

    // 播放歌曲（通过 mcp_tool_kuwo.c / mcp_tool_audio_url.c 工具）控制 music_player
    voice_msg_sub(VOICE_MSG_CLOUD_AUDIO_ITEM, voice_player_audio_item, NULL);

    // 用户主动切歌（通过 mcp_tool_play_ctrl.c 工具） 控制 music_player
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PLAY, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PAUSE, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_NEXT, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PREVIOUS, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_REPLAY, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_STOP, voice_player_play_control, NULL);

    alarm_ring_init(voice_alarm_ring_play_once, voice_alarm_ring_force_stop);
    
    app_player_register_callback(tts_player, on_tts_event, NULL);
    app_player_register_callback(tone_player, on_tone_event, NULL);
}

static int voice_player_init(void)
{
    LISA_LOGI(TAG, "voice_player_init");

    voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_player_ready, NULL);

    return 0;
}

SYS_INIT(voice_player_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);
