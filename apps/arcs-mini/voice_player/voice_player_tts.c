#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define TAG "voice_player"
#include "lisa_log.h"
#include "lisa_http.h"
#include "sysutils.h"

#include "app_player.h"
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
#include "app_player_focus.h"
#endif
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "voice_player/voice_player_tts.h"

/*
 * TTS 播放执行器：接收云端 URL，通过独立任务串行播放，并维护最近 URL 快照。
 * Pushup MP3 的有限预取用于降低流式资源尚未就绪时的播放器 prepare 失败率。
 */

/* ==================== 私有配置 ==================== */

#define TTS_PLAY_URL_MAX 512
#define TTS_PLAY_QUEUE_SIZE 5
#define TTS_PLAY_TASK_STACK_SIZE 2048
#define TTS_PLAY_TASK_PRIORITY 5

#define PUSHUP_TTS_PREFETCH_READY_BYTES (512U * 1024U)
#define PUSHUP_TTS_PREFETCH_MAX_ATTEMPTS 3
#define PUSHUP_TTS_PREFETCH_RETRY_MS 500
#define PUSHUP_TTS_PREFETCH_HTTP_TIMEOUT_MS 8000

/* ==================== 私有类型与状态 ==================== */

typedef struct {
    uint32_t generation;
    bool stop;
    uint32_t stop_seq;
    bool snapshot_saved;
    bool active_before;
    UBaseType_t queue_depth;
    bool is_replay;
    bool prefetch_mp3;
    char url[TTS_PLAY_URL_MAX];
} tts_play_request_t;

/* 使用具名选项描述 URL 提交行为，避免调用点出现难以区分的连续 bool 参数。 */
typedef struct {
    bool is_replay;
    bool prefetch_mp3;
} tts_submit_options_t;

typedef struct {
    const char *url;
    uint32_t generation;
    size_t len;
    bool ready;
    bool stale;
} pushup_tts_prefetch_ctx_t;

static QueueHandle_t s_tts_play_queue = NULL;
static char s_latest_tts_url[TTS_PLAY_URL_MAX] = {0};
static char s_prepared_tts_url[TTS_PLAY_URL_MAX] __psram_bss__;
static volatile bool s_tts_active = false;
static bool s_latest_replay_prepared = false;
static volatile uint32_t s_tts_generation = 1U;
static uint32_t s_tts_stop_seq = 0U;
/* Set before an async stop is queued and cleared after app_player_stop returns.
 * Wakeup tones use this as a short cross-executor completion barrier. */
static uint32_t s_tts_stop_pending_seq = 0U;

/* Reservation/result outlive asynchronous callbacks; only the play worker
 * touches the underlying track for an owner-scoped cancellation. */
static uint32_t s_owned_id;
static uint32_t s_owned_generation;
static voice_tts_owned_result_t s_owned_result;
static uint32_t s_owned_stop_generation;
static uint32_t s_playing_generation;

static const char *voice_player_tts_state_name(app_player_state_t state)
{
    switch (state) {
    case APP_PLAYER_STATE_IDLE:
        return "IDLE";
    case APP_PLAYER_STATE_PREPARING:
        return "PREPARING";
    case APP_PLAYER_STATE_PREPARED:
        return "PREPARED";
    case APP_PLAYER_STATE_PLAYING:
        return "PLAYING";
    case APP_PLAYER_STATE_PAUSED:
        return "PAUSED";
    case APP_PLAYER_STATE_STOPPED:
        return "STOPPED";
    case APP_PLAYER_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

/* IDLE/STOPPED 已经没有可停止的轨道，重复调用 stop 只会触发底层无效状态错误。 */
static bool voice_player_tts_state_needs_stop(app_player_state_t state)
{
    return state != APP_PLAYER_STATE_IDLE &&
           state != APP_PLAYER_STATE_STOPPED;
}

static void voice_player_tts_clear_user_stop_marker(void)
{
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    if (tts_player != NULL) {
        app_player_focus_set_user_initiated(tts_player, false);
    }
#endif
}

/* ==================== 内部工具：URL 缓存与判定 ==================== */

/* 判断 URL 主路径是否以 .mp3 结尾，query string 不参与判断。 */
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

/* stop 会推进 generation；旧队列项和正在预取的请求都随即失效。 */
static bool voice_player_tts_request_is_current(const char *url,
                                                uint32_t generation)
{
    bool current;

    if (url == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    current = generation == s_tts_generation &&
              strncmp(s_latest_tts_url, url,
                      sizeof(s_latest_tts_url)) == 0;
    taskEXIT_CRITICAL();

    return current;
}

/* 原子记录 URL 与 generation，确保 stop 前创建的请求一定会失效。 */
static uint32_t voice_player_tts_begin_request(const char *url)
{
    uint32_t generation;

    taskENTER_CRITICAL();
    if (s_owned_generation && s_owned_generation == s_tts_generation) {
        if (s_owned_result == VOICE_TTS_OWNED_PENDING)
            s_owned_result = VOICE_TTS_OWNED_INTERRUPTED;
        ++s_tts_generation;
    }
    strncpy(s_latest_tts_url, url, sizeof(s_latest_tts_url) - 1);
    s_latest_tts_url[sizeof(s_latest_tts_url) - 1] = '\0';
    generation = s_tts_generation;
    s_tts_active = true;
    taskEXIT_CRITICAL();
    LOGI("refresh latest tts url: %s", s_latest_tts_url);
    return generation;
}

static bool voice_player_tts_generation_is_current(uint32_t generation)
{
    bool current;

    taskENTER_CRITICAL();
    current = generation == s_tts_generation;
    taskEXIT_CRITICAL();
    return current;
}

static void voice_player_tts_consume_prepared_url(const char *url)
{
    taskENTER_CRITICAL();
    if (s_latest_replay_prepared &&
        strcmp(s_prepared_tts_url, url) == 0) {
        s_latest_replay_prepared = false;
        s_prepared_tts_url[0] = '\0';
    }
    taskEXIT_CRITICAL();
}

/* ==================== 内部工具：Pushup MP3 预取 ==================== */

static void pushup_tts_http_ignore_data(lisa_http_data_t *data)
{
    (void)data;
}

static void *pushup_tts_http_headers(void)
{
    return (void *)"Accept: audio/mpeg";
}

/* HTTP chunk 回调：累计已接收长度，达到阈值或发现过期 URL 时提前结束。 */
static int pushup_tts_prefetch_on_chunk(lisa_http_data_t *data)
{
    pushup_tts_prefetch_ctx_t *ctx;

    if (data == NULL || data->user == NULL || data->len <= 0) {
        return 0;
    }

    ctx = (pushup_tts_prefetch_ctx_t *)data->user;
    if (!voice_player_tts_request_is_current(ctx->url,
                                             ctx->generation)) {
        ctx->stale = true;
        return 1;
    }

    ctx->len += (size_t)data->len;
    if (ctx->len >= PUSHUP_TTS_PREFETCH_READY_BYTES) {
        ctx->ready = true;
        return 1;
    }

    return 0;
}

/* 单次 HTTP 预取。返回 0 表示可播放，1 表示 URL 已过期，-1 表示本次预取未就绪。 */
static int voice_player_prefetch_pushup_tts_mp3_once(const char *url,
                                                      uint32_t generation,
                                                      size_t *out_len)
{
    pushup_tts_prefetch_ctx_t ctx = {
        .url = url,
        .generation = generation,
    };
    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .headers = (uint8_t *)pushup_tts_http_headers,
        .timeout = PUSHUP_TTS_PREFETCH_HTTP_TIMEOUT_MS,
        .user = &ctx,
        .on_data = pushup_tts_http_ignore_data,
    };
    lisa_http_t *http;
    lisa_http_err_e ret;

    if (url == NULL || out_len == NULL) {
        return -1;
    }

    *out_len = 0;

    http = lisa_http_init(&req);
    if (http == NULL) {
        LOGE("pushup tts mp3 prefetch http init failed");
        return -1;
    }

    ret = lisa_http_perform_chunked_with_cb(http, pushup_tts_prefetch_on_chunk);
    lisa_http_cleanup(http);

    *out_len = ctx.len;

    if (ctx.stale) {
        LOGW("drop stale pushup tts mp3 url during prefetch: %s", url);
        return 1;
    }

    if (!voice_player_tts_request_is_current(url, generation)) {
        LOGW("drop stale pushup tts mp3 url after prefetch: %s", url);
        return 1;
    }

    if (ctx.ready) {
        LOGI("pushup tts mp3 prefetch reached %u bytes, size=%u",
             (unsigned int)PUSHUP_TTS_PREFETCH_READY_BYTES,
             (unsigned int)ctx.len);
        return 0;
    }

    if (ret == LISA_HTTP_OK && ctx.len > 0U) {
        LOGI("pushup tts mp3 prefetch completed before threshold, size=%u",
             (unsigned int)ctx.len);
        return 0;
    }

    LOGW("pushup tts mp3 prefetch failed, ret=%d, size=%u",
         ret,
         (unsigned int)ctx.len);
    return -1;
}

/* 对 pushup MP3 做有限次数预取，避免边下边播导致播放器 prepare 过早失败。 */
static int voice_player_prefetch_pushup_tts_mp3(const char *url,
                                                 uint32_t generation)
{
    size_t latest_len = 0;

    if (!voice_player_url_is_mp3(url)) {
        return 0;
    }

    for (uint8_t attempt = 1; attempt <= PUSHUP_TTS_PREFETCH_MAX_ATTEMPTS; attempt++) {
        int ret;

        if (!voice_player_tts_request_is_current(url, generation)) {
            LOGW("drop stale pushup tts mp3 url before prefetch: %s", url);
            return 1;
        }

        ret = voice_player_prefetch_pushup_tts_mp3_once(
            url, generation, &latest_len);
        if (ret >= 0) {
            return ret;
        }

        LOGW("pushup tts mp3 prefetch attempt %u/%u not ready, size=%u",
             (unsigned int)attempt,
             (unsigned int)PUSHUP_TTS_PREFETCH_MAX_ATTEMPTS,
             (unsigned int)latest_len);

        if (attempt < PUSHUP_TTS_PREFETCH_MAX_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(PUSHUP_TTS_PREFETCH_RETRY_MS));
        }
    }

    LOGW("pushup tts mp3 prefetch not ready after %u attempts, fallback direct url",
         (unsigned int)PUSHUP_TTS_PREFETCH_MAX_ATTEMPTS);
    return -1;
}

/* ==================== 内部工具：异步播放队列 ==================== */

/* TTS 播放工作线程：串行处理 TTS 播放请求，避免在 ebus 回调里阻塞 HTTP prepare。 */
static void voice_player_tts_stop_player(uint32_t stop_seq,
                                         uint32_t generation,
                                         bool snapshot_saved,
                                         bool active_before,
                                         UBaseType_t queue_depth);

static void voice_player_tts_play_task(void *pvParameters)
{
    tts_play_request_t request;

    (void)pvParameters;

    while (1) {
        taskENTER_CRITICAL();
        uint32_t stop_generation = s_owned_stop_generation;
        uint32_t playing_generation = s_playing_generation;
        taskEXIT_CRITICAL();
        if (stop_generation) {
            if (stop_generation == playing_generation &&
                voice_player_tts_state_needs_stop(app_player_get_state(tts_player))) {
                app_player_stop(tts_player);
            }
            taskENTER_CRITICAL();
            if (s_owned_stop_generation == stop_generation) s_owned_stop_generation = 0;
            taskEXIT_CRITICAL();
        }
        if (xQueueReceive(s_tts_play_queue, &request, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (request.stop) {
                voice_player_tts_stop_player(request.stop_seq,
                                             request.generation,
                                             request.snapshot_saved,
                                             request.active_before,
                                             request.queue_depth);
                continue;
            }

            LOGI("async play task received request, type=0, player=%p, prefetch=%u, url=%s",
                 tts_player,
                 (unsigned int)request.prefetch_mp3,
                 request.url);

            if (!voice_player_tts_generation_is_current(
                    request.generation)) {
                LOGI("drop canceled tts request, generation=%u, url=%s",
                     (unsigned int)request.generation, request.url);
                continue;
            }

            if (request.is_replay) {
                voice_player_tts_consume_prepared_url(request.url);
            }

            if (request.prefetch_mp3) {
                int prefetch_ret = voice_player_prefetch_pushup_tts_mp3(
                    request.url, request.generation);
                if (prefetch_ret > 0) {
                    continue;
                }
                if (prefetch_ret < 0) {
                    LOGW("pushup tts mp3 prefetch failed, fallback direct url");
                }
            }

            if (!voice_player_tts_generation_is_current(
                    request.generation)) {
                LOGI("drop canceled tts before play, generation=%u, url=%s",
                     (unsigned int)request.generation, request.url);
                continue;
            }

            /* 在独立任务中执行播放操作，不会阻塞ebus线程 */
            app_player_state_t state_before = app_player_get_state(tts_player);
            app_player_play_opt_t play_opt = {
                .url = request.url,
                .throw_time_ms = 0,
                .request_tag = request.generation,
                .cancel_token = &s_tts_generation,
            };
            taskENTER_CRITICAL();
            s_playing_generation = request.generation;
            taskEXIT_CRITICAL();
            int play_ret = app_player_play_ex(tts_player, &play_opt);
            if (play_ret != 0) {
                taskENTER_CRITICAL();
                if (s_owned_id && s_owned_generation == request.generation &&
                    s_owned_result == VOICE_TTS_OWNED_PENDING) {
                    s_owned_result = VOICE_TTS_OWNED_FAILED;
                    if (s_tts_generation == request.generation) s_tts_active = false;
                }
                taskEXIT_CRITICAL();
            }
            app_player_state_t state_after = app_player_get_state(tts_player);
            LOGI("TTS play request generation=%u ret=%d state=%s(%d)->%s(%d)",
                 (unsigned int)request.generation,
                 play_ret,
                 voice_player_tts_state_name(state_before),
                 state_before,
                 voice_player_tts_state_name(state_after),
                 state_after);
            /* stop 可能恰好发生在 generation 检查与 play 之间。 */
            if (!voice_player_tts_generation_is_current(
                    request.generation)) {
                app_player_state_t stale_state = app_player_get_state(tts_player);
                if (voice_player_tts_state_needs_stop(stale_state)) {
                    LOGI("stop tts started by canceled request, generation=%u state=%s(%d)",
                         (unsigned int)request.generation,
                         voice_player_tts_state_name(stale_state),
                         stale_state);
                    app_player_stop(tts_player);
                } else {
                    LOGI("skip stop for canceled tts request, generation=%u state=%s(%d)",
                         (unsigned int)request.generation,
                         voice_player_tts_state_name(stale_state),
                         stale_state);
                }
            }
        }
    }
}



/* 提交一个 TTS URL；options 明确控制复播和 MP3 预取。 */
static bool voice_player_tts_submit_url(const char *url,
                                        tts_submit_options_t options)
{
    tts_play_request_t request = {0};

    if (s_tts_play_queue == NULL || url == NULL) {
        LOGE("async play queue not ready or url is null");
        return false;
    }

    request.generation = voice_player_tts_begin_request(url);
    request.is_replay = options.is_replay;
    request.prefetch_mp3 = options.prefetch_mp3;
    strncpy(request.url, url, sizeof(request.url) - 1);
    request.url[sizeof(request.url) - 1] = '\0';

    if (xQueueSend(s_tts_play_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        taskENTER_CRITICAL();
        if (request.generation == s_tts_generation) s_tts_active = false;
        taskEXIT_CRITICAL();
        if (options.prefetch_mp3) {
            LOGE("failed to send pushup tts play request to queue");
        } else {
            LOGE("failed to send play request to queue");
        }
        return false;
    } else if (options.prefetch_mp3) {
        LOGI("pushup tts play request sent to async queue");
    } else {
        LOGI("play request sent to async queue, player=%p", tts_player);
    }
    taskENTER_CRITICAL();
    if (request.generation == s_tts_generation) {
        s_tts_active = true;
    }
    taskEXIT_CRITICAL();
    return true;
}

/* ==================== EBUS 与播放器回调 ==================== */

/* TTS URL 到达即进入播放队列；拍照流只快照需要恢复的当前 URL。 */
static void voice_player_tts_msg(void *unused, uint32_t msg_id,
                                 void *data, uint32_t len, void *user_data)
{
    const char *url = (const char *)data;

    (void)unused;
    (void)len;
    (void)user_data;

    if (data == NULL) {
        LOGE("Invalid data");
        return;
    }

    LOGI("Ready to play tts url asynchronously: %s", url);
    voice_player_tts_submit_url(
        url,
        (tts_submit_options_t){
            .is_replay = false,
            /* Pushup MP3 直接交给播放器，避免预取期间被最新 URL 判为 stale。 */
            .prefetch_mp3 = msg_id != VOICE_MSG_CLOUD_PUSHUP_TTS_URL &&
                            voice_player_url_is_mp3(url),
        });
}

/* 将底层 tts_player 事件转成上层 VOICE_MSG_PLAYER_TTS_*，并维护非阻塞活跃标志。 */
static void voice_player_tts_event(app_player_t *player,
                                   app_player_event_t event,
                                   void *user_data)
{
    uint32_t event_tag;

    (void)player;
    (void)user_data;

    LOGI("tts player event: %d", event);

    event_tag = app_player_get_callback_request_tag(player);
    if (event_tag != 0U &&
        !voice_player_tts_generation_is_current(event_tag)) {
        LOGI("ignore stale tts player event: event=%d tag=%u current=%u",
             event,
             (unsigned int)event_tag,
             (unsigned int)s_tts_generation);
        return;
    }

    taskENTER_CRITICAL();
    /* Keep classifying late events as app speech after its result is consumed. */
    bool owned = event_tag && event_tag == s_owned_generation;
    if (owned && s_owned_result == VOICE_TTS_OWNED_PENDING) {
        if (event == APP_PLAYER_EVENT_COMPLETED) s_owned_result = VOICE_TTS_OWNED_COMPLETED;
        else if (event == APP_PLAYER_EVENT_ERROR) s_owned_result = VOICE_TTS_OWNED_FAILED;
        else if (event == APP_PLAYER_EVENT_STOPPED) s_owned_result = VOICE_TTS_OWNED_INTERRUPTED;
        if (s_owned_result != VOICE_TTS_OWNED_PENDING) s_tts_active = false;
    }
    taskEXIT_CRITICAL();
    /* App speech is not a dialogue completion or an alarm completion. */
    if (owned) return;
    switch (event) {
    case APP_PLAYER_EVENT_PLAYING:
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PLAYING, NULL, 0);
        break;
    case APP_PLAYER_EVENT_PAUSED:
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PAUSED, NULL, 0);
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        s_tts_active = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_COMPLETED, NULL, 0);
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        break;
    case APP_PLAYER_EVENT_STOPPED:
        s_tts_active = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        break;
    case APP_PLAYER_EVENT_ERROR:
        s_tts_active = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_ERROR, NULL, 0);
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        break;
    default:
        break;
    }
}

/* 初始化 TTS 播放队列和工作线程。 */
static int voice_player_tts_play_queue_init(void)
{
    BaseType_t ret;

    if (s_tts_play_queue != NULL) {
        return 0;
    }

    s_tts_play_queue = xQueueCreate(TTS_PLAY_QUEUE_SIZE, sizeof(tts_play_request_t));
    if (s_tts_play_queue == NULL) {
        LOGE("failed to create async play queue");
        return -1;
    }

    ret = xTaskCreate(
        voice_player_tts_play_task,
        "async_play",
        TTS_PLAY_TASK_STACK_SIZE,
        NULL,
        TTS_PLAY_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        LOGE("failed to create async play task");
        vQueueDelete(s_tts_play_queue);
        s_tts_play_queue = NULL;
        return -1;
    }

    LOGI("async play task created successfully");
    return 0;
}

/*
 * 硬停止顺序：先推进 generation 作废所有旧请求，再清空队列，最后同步
 * 停止底层播放器。snapshot=true 时只保留显式复播快照。
 */
static void voice_player_tts_prepare_stop(bool snapshot,
                                          uint32_t *stop_seq,
                                          uint32_t *generation,
                                          bool *snapshot_saved,
                                          bool *active_before,
                                          UBaseType_t *queue_depth)
{
    *snapshot_saved = false;
    *queue_depth = 0;

    taskENTER_CRITICAL();
    *stop_seq = ++s_tts_stop_seq;
    if (*stop_seq == 0U) {
        *stop_seq = 1U;
        s_tts_stop_seq = *stop_seq;
    }
    *active_before = s_tts_active;
    if (snapshot && s_tts_active && s_latest_tts_url[0] != '\0' &&
        !(s_owned_id && s_owned_generation == s_tts_generation)) {
        strncpy(s_prepared_tts_url, s_latest_tts_url,
                sizeof(s_prepared_tts_url) - 1);
        s_prepared_tts_url[sizeof(s_prepared_tts_url) - 1] = '\0';
        s_latest_replay_prepared = true;
        *snapshot_saved = true;
    } else {
        s_prepared_tts_url[0] = '\0';
        s_latest_replay_prepared = false;
    }

    if (s_owned_id && s_owned_generation == s_tts_generation &&
        s_owned_result == VOICE_TTS_OWNED_PENDING)
        s_owned_result = VOICE_TTS_OWNED_INTERRUPTED;
    *generation = ++s_tts_generation;
    s_latest_tts_url[0] = '\0';
    s_tts_active = false;
    taskEXIT_CRITICAL();

    /* Generation invalidation happens before queueing stop.  Mark the SDK
     * player cancelled as well, so PREPARED cannot race into PLAYING. */
    if (tts_player != NULL) {
        (void)app_player_cancel_pending(tts_player);
    }

    if (s_tts_play_queue != NULL) {
        xQueueReset(s_tts_play_queue);
        *queue_depth = uxQueueMessagesWaiting(s_tts_play_queue);
    }
}

static void voice_player_tts_stop_player(uint32_t stop_seq,
                                         uint32_t generation,
                                         bool snapshot_saved,
                                         bool active_before,
                                         UBaseType_t queue_depth)
{
    app_player_state_t state_before;
    app_player_state_t state_after;

    state_before = app_player_get_state(tts_player);
    LOGI("TTS stop begin seq=%u generation=%u snapshot=%u active=%u state=%s(%d) queue=%u",
         (unsigned int)stop_seq,
         (unsigned int)generation,
         (unsigned int)snapshot_saved,
         (unsigned int)active_before,
         voice_player_tts_state_name(state_before),
         state_before,
         (unsigned int)queue_depth);

    int stop_ret = 0;
    if (voice_player_tts_state_needs_stop(state_before)) {
        stop_ret = app_player_stop(tts_player);
    } else {
        LOGI("skip duplicate tts stop seq=%u state=%s(%d)",
             (unsigned int)stop_seq,
             voice_player_tts_state_name(state_before),
             state_before);
    }
    state_after = app_player_get_state(tts_player);
    voice_player_tts_clear_user_stop_marker();
    taskENTER_CRITICAL();
    if (s_tts_stop_pending_seq == stop_seq) {
        s_tts_stop_pending_seq = 0U;
    }
    taskEXIT_CRITICAL();
    LOGI("TTS stop end seq=%u ret=%d state=%s(%d)->%s(%d)",
         (unsigned int)stop_seq,
         stop_ret,
         voice_player_tts_state_name(state_before),
         state_before,
         voice_player_tts_state_name(state_after),
         state_after);


    LOGI("hard stop tts, generation=%u, snapshot=%u",
         (unsigned int)generation, (unsigned int)snapshot_saved);
}

static bool voice_player_tts_hard_stop(bool snapshot)
{
    uint32_t stop_seq;
    uint32_t generation;
    bool snapshot_saved;
    bool active_before;
    UBaseType_t queue_depth;

    voice_player_tts_prepare_stop(snapshot, &stop_seq, &generation,
                                  &snapshot_saved, &active_before, &queue_depth);
    voice_player_tts_stop_player(stop_seq, generation, snapshot_saved,
                                 active_before, queue_depth);
    return snapshot_saved;
}

/* ==================== 对外 API ==================== */

/* 初始化 TTS 子模块：创建播放队列、订阅 TTS URL 消息、注册播放器事件回调。 */
int voice_player_tts_init(void)
{
    if (voice_player_tts_play_queue_init() != 0) {
        return -1;
    }

    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, voice_player_tts_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, voice_player_tts_msg, NULL);
    app_player_register_callback(tts_player, voice_player_tts_event, NULL);

    return 0;
}

void voice_player_tts_stop(void)
{
    voice_player_tts_hard_stop(false);
}

bool voice_player_tts_snapshot_and_stop(void)
{
    return voice_player_tts_hard_stop(true);
}

static bool voice_player_tts_schedule_stop_async(bool snapshot)
{
    tts_play_request_t stop_request = {0};
    uint32_t stop_seq;
    uint32_t generation;
    bool snapshot_saved;
    bool active_before;
    UBaseType_t queue_depth;

    if (s_tts_play_queue == NULL) {
        LOGE("async tts stop queue is not ready, fallback to sync stop");
        return voice_player_tts_hard_stop(snapshot);
    }

    voice_player_tts_prepare_stop(snapshot, &stop_seq, &generation,
                                  &snapshot_saved, &active_before,
                                  &queue_depth);

    taskENTER_CRITICAL();
    s_tts_stop_pending_seq = stop_seq;
    taskEXIT_CRITICAL();

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    /* The stop command is handled by async_play. Mark it as user initiated
     * before the queue wakes the worker, so focus callbacks do not issue a
     * competing stop while the command is pending. */
    if (active_before) {
        app_player_focus_set_user_initiated(tts_player, true);
    }
#endif

    stop_request.generation = generation;
    stop_request.stop = true;
    stop_request.stop_seq = stop_seq;
    stop_request.snapshot_saved = snapshot_saved;
    stop_request.active_before = active_before;
    stop_request.queue_depth = queue_depth;

    if (xQueueSend(s_tts_play_queue, &stop_request, 0) != pdTRUE) {
        taskENTER_CRITICAL();
        if (s_tts_stop_pending_seq == stop_seq) {
            s_tts_stop_pending_seq = 0U;
        }
        taskEXIT_CRITICAL();
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
        if (active_before) {
            app_player_focus_set_user_initiated(tts_player, false);
        }
#endif
        LOGE("failed to queue async tts stop seq=%u",
             (unsigned int)stop_seq);
        return snapshot_saved;
    }

    LOGI("queued async tts stop seq=%u generation=%u snapshot=%u",
         (unsigned int)stop_seq,
         (unsigned int)generation,
         (unsigned int)snapshot_saved);
    return snapshot_saved;
}

void voice_player_tts_stop_async(void)
{
    (void)voice_player_tts_schedule_stop_async(false);
}

bool voice_player_tts_snapshot_and_stop_async(void)
{
    return voice_player_tts_schedule_stop_async(true);
}

bool voice_player_tts_wait_stop_complete(uint32_t timeout_ms)
{
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    TickType_t deadline;

    if (timeout_ticks == 0U) {
        timeout_ticks = 1U;
    }
    deadline = xTaskGetTickCount() + timeout_ticks;

    while (true) {
        bool pending;

        taskENTER_CRITICAL();
        pending = s_tts_stop_pending_seq != 0U;
        taskEXIT_CRITICAL();
        if (!pending) {
            return true;
        }

        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            return false;
        }
        vTaskDelay(1);
    }
}

void voice_player_tts_discard_prepared_replay(void)
{
    taskENTER_CRITICAL();
    s_latest_replay_prepared = false;
    s_prepared_tts_url[0] = '\0';
    taskEXIT_CRITICAL();
}

/* 提交复播后保留快照，直到工作线程消费复播请求。 */
bool voice_player_tts_replay_prepared(void)
{
    bool can_replay;

    taskENTER_CRITICAL();
    can_replay = s_latest_replay_prepared &&
                 s_prepared_tts_url[0] != '\0';
    if (!can_replay) {
        s_prepared_tts_url[0] = '\0';
        s_latest_replay_prepared = false;
    }
    taskEXIT_CRITICAL();

    if (!can_replay) {
        return false;
    }

    LOGI("replay prepared tts url: %s", s_prepared_tts_url);
    if (!voice_player_tts_submit_url(
            s_prepared_tts_url,
            (tts_submit_options_t){
                .is_replay = true,
                .prefetch_mp3 = voice_player_url_is_mp3(
                    s_prepared_tts_url),
            })) {
        voice_player_tts_discard_prepared_replay();
        return false;
    }
    return true;
}

/* 返回 TTS 子模块维护的非阻塞活跃标志。 */
bool voice_player_tts_is_active(void)
{
    return s_tts_active;
}

bool voice_player_tts_claim(uint32_t owner)
{
    bool accepted = false;
    taskENTER_CRITICAL();
    if (owner && s_tts_play_queue && !s_tts_active && !s_owned_id &&
        !s_tts_stop_pending_seq && !s_owned_stop_generation) {
        s_owned_id = owner;
        s_owned_generation = ++s_tts_generation;
        s_owned_result = VOICE_TTS_OWNED_PENDING;
        s_tts_active = true; /* Includes synthesis, before a URL exists. */
        s_latest_tts_url[0] = 0;
        accepted = true;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

bool voice_player_tts_play_owned(uint32_t owner, const char *url)
{
    if (!url || !url[0] || strlen(url) >= TTS_PLAY_URL_MAX) return false;
    tts_play_request_t request = {0};
    strncpy(request.url, url, sizeof(request.url) - 1);
    taskENTER_CRITICAL();
    bool current = s_owned_id == owner && s_owned_generation == s_tts_generation &&
                   s_owned_result == VOICE_TTS_OWNED_PENDING;
    request.generation = s_owned_generation;
    /* Queue copies are bounded and nonblocking; stop cannot interleave with
     * validation and enqueueing an owned request. */
    bool accepted = current && xQueueSend(s_tts_play_queue, &request, 0) == pdTRUE;
    taskEXIT_CRITICAL();
    return accepted;
}

void voice_player_tts_cancel_owned(uint32_t owner)
{
    taskENTER_CRITICAL();
    if (owner && s_owned_id == owner && s_owned_result == VOICE_TTS_OWNED_PENDING) {
        s_owned_result = VOICE_TTS_OWNED_INTERRUPTED;
        if (s_owned_generation == s_tts_generation) {
            s_owned_stop_generation = s_owned_generation;
            ++s_tts_generation; /* Invalidates only this owner's queued play. */
            s_tts_active = false;
        }
    }
    taskEXIT_CRITICAL();
}

voice_tts_owned_result_t voice_player_tts_owned_result(uint32_t owner)
{
    taskENTER_CRITICAL();
    voice_tts_owned_result_t result = s_owned_id == owner ? s_owned_result : VOICE_TTS_OWNED_INTERRUPTED;
    taskEXIT_CRITICAL();
    return result;
}

void voice_player_tts_release_owned(uint32_t owner)
{
    taskENTER_CRITICAL();
    if (s_owned_id == owner && s_owned_result != VOICE_TTS_OWNED_PENDING) s_owned_id = 0;
    taskEXIT_CRITICAL();
}
