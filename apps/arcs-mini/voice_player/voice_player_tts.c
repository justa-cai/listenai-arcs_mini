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
static uint32_t s_tts_generation = 1U;

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
    strncpy(s_latest_tts_url, url, sizeof(s_latest_tts_url) - 1);
    s_latest_tts_url[sizeof(s_latest_tts_url) - 1] = '\0';
    generation = s_tts_generation;
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
static void voice_player_tts_play_task(void *pvParameters)
{
    tts_play_request_t request;

    (void)pvParameters;

    while (1) {
        if (xQueueReceive(s_tts_play_queue, &request, portMAX_DELAY) == pdTRUE) {
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
            app_player_play(tts_player, request.url);
            /* stop 可能恰好发生在 generation 检查与 play 之间。 */
            if (!voice_player_tts_generation_is_current(
                    request.generation)) {
                LOGI("stop tts started by canceled request, generation=%u",
                     (unsigned int)request.generation);
                app_player_stop(tts_player);
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
    (void)msg_id;
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
            .prefetch_mp3 = voice_player_url_is_mp3(url),
        });
}

/* 将底层 tts_player 事件转成上层 VOICE_MSG_PLAYER_TTS_*，并维护非阻塞活跃标志。 */
static void voice_player_tts_event(app_player_t *player,
                                   app_player_event_t event,
                                   void *user_data)
{
    (void)player;
    (void)user_data;

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
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_COMPLETED, NULL, 0);
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        break;
    case APP_PLAYER_EVENT_STOPPED:
    case APP_PLAYER_EVENT_ERROR:
        s_tts_active = false;
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
static bool voice_player_tts_hard_stop(bool snapshot)
{
    bool snapshot_saved = false;
    uint32_t generation;

    taskENTER_CRITICAL();
    if (snapshot && s_tts_active && s_latest_tts_url[0] != '\0') {
        strncpy(s_prepared_tts_url, s_latest_tts_url,
                sizeof(s_prepared_tts_url) - 1);
        s_prepared_tts_url[sizeof(s_prepared_tts_url) - 1] = '\0';
        s_latest_replay_prepared = true;
        snapshot_saved = true;
    } else {
        s_prepared_tts_url[0] = '\0';
        s_latest_replay_prepared = false;
    }

    generation = ++s_tts_generation;
    s_latest_tts_url[0] = '\0';
    s_tts_active = false;
    taskEXIT_CRITICAL();

    if (s_tts_play_queue != NULL) {
        xQueueReset(s_tts_play_queue);
    }

    app_player_stop(tts_player);


    LOGI("hard stop tts, generation=%u, snapshot=%u",
         (unsigned int)generation, (unsigned int)snapshot_saved);
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
