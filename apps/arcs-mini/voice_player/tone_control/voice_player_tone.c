#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#define TAG "voice_player_tone"
#include "lisa_log.h"

#include "app_player.h"
#include "voice_intent_mgr.h"
#include "voice_msg.h"
#include "voice_player_comm.h"

#include "tone_control/voice_player_tone.h"

/*
 * 提示音执行器：普通请求按 FIFO 串行播放；唤醒请求清空队列并抢占当前提示音。
 * tone 资源 ID 到 URL 的映射由应用层 tone/ 资源索引负责，本模块只处理 URL。
 *
 * 调度规则：
 * 1. 播放器空闲时，请求直接开始播放。
 * 2. 播放器忙时，非唤醒请求进入固定长度的 FIFO 队列。
 * 3. 唤醒请求不排队等待：清空旧队列并替换当前提示音。
 * 4. 当前已经是唤醒提示音时，忽略新的唤醒请求，避免连续唤醒堆积。
 *
 * s_tone_ctx 的访问统一受互斥锁保护。业务线程只投递命令，公共 tone 任务
 * 与异步播放器回调通过同一把锁维护状态一致性。
 */

/* ==================== 私有配置、类型与状态 ==================== */

#define TONE_PENDING_QUEUE_SIZE 5U
#define TONE_COMMAND_QUEUE_SIZE (TONE_PENDING_QUEUE_SIZE + 1U)
#define TONE_TASK_STACK_SIZE 2048
#define TONE_TASK_PRIORITY 7

typedef enum {
    TONE_STATE_IDLE = 0,          /* 当前没有本模块管理的提示音。 */
    TONE_STATE_PLAYING_NORMAL,    /* 正在播放，不持有 PROMPT_TONE intent。 */
    TONE_STATE_PLAYING_PROMPT,    /* 正在播放，并持有 PROMPT_TONE intent。 */
} tone_state_t;

/* 队列中的最小播放单元。队列只保存 URL 指针，不复制 URL 内容。 */
typedef struct {
    const char *url;                     /* 调用方需保证播放完成前 URL 有效。 */
    voice_player_tone_source_t source;   /* 用于唤醒调度及完成事件来源识别。 */
    bool prompt;                         /* 是否需要管理 PROMPT_TONE intent。 */
} tone_request_t;

typedef struct {
    tone_request_t pending[TONE_PENDING_QUEUE_SIZE]; /* 固定长度环形等待队列。 */
    tone_request_t current;                          /* 当前正在播放的请求。 */
    uint8_t pending_head;                            /* 下一条待播放请求的位置。 */
    uint8_t pending_count;                           /* 当前有效等待请求数量。 */
    tone_state_t state;                              /* 本模块维护的逻辑播放状态。 */
    SemaphoreHandle_t lock;                          /* 保护本结构的全部成员。 */
} tone_context_t;

static tone_context_t s_tone_ctx;
static QueueHandle_t s_tone_command_queue;

/* ==================== 并发边界 ==================== */

/*
 * 请求提交由公共 tone 任务获取锁；播放器事件和主动停止沿用原有锁边界。
 * 业务线程只入队，不直接进入可能阻塞的 app_player_play()。
 */
static bool tone_context_acquire(void)
{
    if (s_tone_ctx.lock == NULL) {
        LOGW("tone executor is not initialized");
        return false;
    }

    return xSemaphoreTake(s_tone_ctx.lock, portMAX_DELAY) == pdTRUE;
}

static void tone_context_release(void)
{
    xSemaphoreGive(s_tone_ctx.lock);
}

/* ==================== 持锁区工具：环形队列 ==================== */

/*
 * 本区域到“请求调度”区域的工具均要求调用方已经持有 s_tone_ctx.lock，
 * 工具内部不会再次加锁，避免嵌套加锁和分支遗漏解锁。
 */

/*
 * 将普通请求追加到队尾。调用前必须确认队列未满，并持有 s_tone_ctx.lock。
 * pending_head + pending_count 指向环形队列的下一个空槽位。
 */
static void tone_queue_push(const tone_request_t *request)
{
    uint8_t tail = (uint8_t)((s_tone_ctx.pending_head +
                              s_tone_ctx.pending_count) %
                             TONE_PENDING_QUEUE_SIZE);

    s_tone_ctx.pending[tail] = *request;
    s_tone_ctx.pending_count++;
}

/* 仅丢弃等待项，不影响 current。调用方持有 s_tone_ctx.lock。 */
static void tone_queue_clear(void)
{
    s_tone_ctx.pending_head = 0U;
    s_tone_ctx.pending_count = 0U;
}

/* ==================== 内部工具：prompt intent 管理 ==================== */

/*
 * PROMPT_TONE intent 用于暂停并在提示音结束后恢复被抢占的业务意图，
 * 例如播放相机快门音时临时暂停后台音乐。
 */
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

/* ==================== 内部工具：播放生命周期 ==================== */

/*
 * 启动一条请求并同步更新 current/state。调用方持有 s_tone_ctx.lock。
 * entered_prompt 只记录本次调用是否压入 intent，播放启动失败时据此回滚。
 */
static bool tone_start(const tone_request_t *request)
{
    bool entered_prompt = false;

    if (request == NULL || request->url == NULL) {
        LOGW("tone request is null, skip");
        return false;
    }

    if (request->prompt && !voice_intent_contains(INTENT_PROMPT_TONE)) {
        if (voice_player_prompt_tone_enter() == 0) {
            entered_prompt = true;
        } else {
            LOGW("failed to enter PROMPT_TONE intent, play without music resume focus");
        }
    }

    s_tone_ctx.state = request->prompt && voice_intent_contains(INTENT_PROMPT_TONE)
                           ? TONE_STATE_PLAYING_PROMPT
                           : TONE_STATE_PLAYING_NORMAL;
    s_tone_ctx.current = *request;

    if (app_player_play(tone_player, request->url) != APP_PLAYER_OK) {
        LOGW("failed to play tone url");
        s_tone_ctx.state = TONE_STATE_IDLE;
        s_tone_ctx.current = (tone_request_t){0};
        if (entered_prompt) {
            voice_player_prompt_tone_exit();
        }
        return false;
    }

    return true;
}

/*
 * 从队头依次取请求播放。某个 URL 启动失败时继续尝试下一条，避免一条
 * 无效请求阻塞整个队列。调用方持有 s_tone_ctx.lock。
 */
static void tone_start_next(void)
{
    while (s_tone_ctx.pending_count > 0U) {
        tone_request_t request = s_tone_ctx.pending[s_tone_ctx.pending_head];

        s_tone_ctx.pending_head =
            (uint8_t)((s_tone_ctx.pending_head + 1U) % TONE_PENDING_QUEUE_SIZE);
        s_tone_ctx.pending_count--;

        if (tone_start(&request)) {
            return;
        }
    }

    s_tone_ctx.state = TONE_STATE_IDLE;
    s_tone_ctx.current = (tone_request_t){0};
}

/* 强制结束本模块的整条播放链，并释放可能持有的 prompt intent。 */
static void tone_reset(void)
{
    tone_queue_clear();
    s_tone_ctx.state = TONE_STATE_IDLE;
    s_tone_ctx.current = (tone_request_t){0};
    voice_player_prompt_tone_exit();
}

/* ==================== 内部工具：请求调度 ==================== */

/*
 * app_player_play() 会同步替换当前 URL，但旧播放的 STOPPED/COMPLETED 事件
 * 通过异步回调队列稍后送达。此时底层已在准备或播放新 URL，旧终止事件应忽略。
 *
 * 例如普通 tone 被唤醒 tone 替换后，普通 tone 的 STOPPED 可能晚于新 URL
 * 启动到达；若继续处理该事件，会错误清空刚开始播放的唤醒 tone。
 */
static bool tone_event_is_stale(app_player_event_t event)
{
    app_player_state_t player_state;

    if (event != APP_PLAYER_EVENT_COMPLETED &&
        event != APP_PLAYER_EVENT_STOPPED &&
        event != APP_PLAYER_EVENT_ERROR) {
        return false;
    }

    player_state = app_player_get_state(tone_player);
    return player_state == APP_PLAYER_STATE_PREPARING ||
           player_state == APP_PLAYER_STATE_PREPARED ||
           player_state == APP_PLAYER_STATE_PLAYING ||
           player_state == APP_PLAYER_STATE_PAUSED;
}

/*
 * 唤醒是用户主动操作，旧提示音及其等待项都不再重要，因此直接替换。
 * app_player_play() 负责停止底层当前 URL；本函数负责重置上层逻辑状态。
 * 调用方持有 s_tone_ctx.lock。
 */
static bool tone_preempt_with_wakeup(const tone_request_t *request)
{
    bool interrupted_prompt =
        s_tone_ctx.state == TONE_STATE_PLAYING_PROMPT;
    bool started;

    /* 清队列后先清 current，让 tone_start() 完整建立新的唤醒状态。 */
    tone_queue_clear();
    s_tone_ctx.state = TONE_STATE_IDLE;
    s_tone_ctx.current = (tone_request_t){0};

    /* app_player_play() 内部会先同步停止当前 tone，再播放新的唤醒提示音。 */
    started = tone_start(request);

    /* 被打断的是 prompt tone 时，唤醒启动后即可释放原来的 prompt intent。 */
    if (interrupted_prompt && s_tone_ctx.state != TONE_STATE_PLAYING_PROMPT) {
        voice_player_prompt_tone_exit();
    }

    return started;
}

static bool tone_process_request(const tone_request_t *request)
{
    bool accepted = false;

    if (request == NULL || request->url == NULL) {
        LOGW("tone url is null, skip");
        return false;
    }

    if (!tone_context_acquire()) {
        return false;
    }

    /* 唤醒来源是唯一不遵循普通 FIFO 的请求类型。 */
    if (request->source == VOICE_PLAYER_TONE_SOURCE_WAKEUP) {
        /* 唤醒提示音 single-flight：播放期间的重复唤醒不排队。 */
        if (s_tone_ctx.state != TONE_STATE_IDLE &&
            s_tone_ctx.current.source == VOICE_PLAYER_TONE_SOURCE_WAKEUP) {
            LOGD("ignore wakeup tone while wakeup tone is active");
        } else if (s_tone_ctx.state == TONE_STATE_IDLE) {
            tone_queue_clear();
            accepted = tone_start(request);
        } else {
            LOGI("wakeup tone preempts current tone and clears pending queue");
            accepted = tone_preempt_with_wakeup(request);
        }
    } else if (s_tone_ctx.state != TONE_STATE_IDLE) {
        /* 非唤醒请求播放期间只追加到队尾，不打断 current。 */
        if (s_tone_ctx.pending_count == TONE_PENDING_QUEUE_SIZE) {
            LOGW("tone queue full, drop");
        } else {
            tone_queue_push(request);
            LOGD("tone queued, source: %u, pending: %u",
                 (unsigned int)request->source,
                 (unsigned int)s_tone_ctx.pending_count);
            accepted = true;
        }
    } else {
        /* 空闲路径无需先入队，直接建立 current 并启动播放器。 */
        accepted = tone_start(request);
    }

    tone_context_release();
    return accepted;
}

/* 所有 tone 来源共用同一个提交线程。原有 pending[]、完成回调和 stop
 * 逻辑保持不变；这里只隔离业务线程与可能同步阻塞的首次播放器调用。 */
static void voice_player_tone_task(void *arg)
{
    tone_request_t request;

    (void)arg;

    while (1) {
        if (xQueueReceive(s_tone_command_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        (void)tone_process_request(&request);
    }
}

static int voice_player_tone_task_init(void)
{
    BaseType_t ret;

    if (s_tone_command_queue != NULL) {
        return 0;
    }

    s_tone_command_queue = xQueueCreate(TONE_COMMAND_QUEUE_SIZE,
                                        sizeof(tone_request_t));
    if (s_tone_command_queue == NULL) {
        LOGE("failed to create tone command queue");
        return -1;
    }

    ret = xTaskCreate(voice_player_tone_task,
                      "tone_play",
                      TONE_TASK_STACK_SIZE,
                      NULL,
                      TONE_TASK_PRIORITY,
                      NULL);
    if (ret != pdPASS) {
        LOGE("failed to create tone task");
        vQueueDelete(s_tone_command_queue);
        s_tone_command_queue = NULL;
        return -1;
    }

    return 0;
}

static bool voice_player_tone_submit(const char *url,
                                     bool prompt,
                                     voice_player_tone_source_t source)
{
    tone_request_t request = {
        .url = url,
        .source = source,
        .prompt = prompt,
    };

    if (url == NULL) {
        LOGW("tone url is null, skip");
        return false;
    }
    if (s_tone_command_queue == NULL) {
        LOGW("tone executor is not initialized");
        return false;
    }

    if (source == VOICE_PLAYER_TONE_SOURCE_WAKEUP) {
        /* 唤醒仍保持最高调度权：清除尚未处理的提交命令并插到队首；
         * 已进入原 pending[] 的请求由 tone_process_request() 一并清理。 */
        xQueueReset(s_tone_command_queue);
        if (xQueueSendToFront(s_tone_command_queue, &request, 0) != pdTRUE) {
            LOGE("failed to queue wakeup tone");
            return false;
        }
    } else if (xQueueSend(s_tone_command_queue, &request, 0) != pdTRUE) {
        LOGW("tone command queue full, drop source: %u", (unsigned int)source);
        return false;
    }

    return true;
}

/* ==================== 播放器事件回调 ==================== */

static void voice_player_tone_event(app_player_t *player,
                                    app_player_event_t event,
                                    void *user_data)
{
    voice_player_tone_completed_t completed = {0};
    bool notify_complete = false;

    (void)player;
    (void)user_data;

    if (!tone_context_acquire()) {
        return;
    }

    if (tone_event_is_stale(event)) {
        LOGD("ignore stale tone terminal event: %d", event);
    } else {
        switch (event) {
        case APP_PLAYER_EVENT_COMPLETED: {
            bool completed_prompt =
                s_tone_ctx.state == TONE_STATE_PLAYING_PROMPT;

            /* start_next 会覆盖 current，因此先保存本次完成请求的来源。 */
            completed.source = s_tone_ctx.current.source;
            s_tone_ctx.state = TONE_STATE_IDLE;
            s_tone_ctx.current = (tone_request_t){0};
            tone_start_next();

            /*
             * 先启动下一条再退出 intent：连续 prompt tone 可复用同一个 intent，
             * 避免两条提示音之间短暂恢复后台音乐。
             */
            if (completed_prompt &&
                s_tone_ctx.state != TONE_STATE_PLAYING_PROMPT) {
                voice_player_prompt_tone_exit();
            }
            notify_complete = true;
            break;
        }
        case APP_PLAYER_EVENT_STOPPED:
        case APP_PLAYER_EVENT_ERROR:
            /* 非自然结束不续播，避免强停后队列中的旧提示音继续响起。 */
            tone_reset();
            break;
        default:
            break;
        }
    }

    tone_context_release();

    /* 消息订阅者可能再次提交 tone，必须在释放互斥锁后发布，避免重入死锁。 */
    if (notify_complete) {
        voice_msg_pub(VOICE_MSG_PLAYER_TONE_COMPLETED,
                      &completed,
                      sizeof(completed));
    }
}

/* ==================== 对外 API ==================== */

int voice_player_tone_init(void)
{
    if (s_tone_ctx.lock == NULL) {
        s_tone_ctx.lock = xSemaphoreCreateMutex();
        if (s_tone_ctx.lock == NULL) {
            LOGE("failed to create tone state lock");
            return -1;
        }
    }

    if (voice_player_tone_task_init() != 0) {
        return -1;
    }

    if (app_player_register_callback(tone_player,
                                     voice_player_tone_event,
                                     NULL) != APP_PLAYER_OK) {
        LOGE("failed to register tone player callback");
        return -1;
    }

    return 0;
}

void voice_player_tone_stop(void)
{
    if (!tone_context_acquire()) {
        return;
    }

    /* 先清理逻辑状态，再停止底层；稍后到达的 STOPPED 回调只会重复清理。 */
    tone_reset();
    app_player_stop(tone_player);
    tone_context_release();
}

/* 以下包装接口只负责补充调度元数据，实际播放统一经过 tone_submit。 */

/* 默认来源：空闲时直接播放，忙时按 FIFO 排队。 */
void voice_player_play_tone_url(const char *url)
{
    (void)voice_player_tone_submit(url,
                                   false,
                                   VOICE_PLAYER_TONE_SOURCE_DEFAULT);
}

/* prompt 来源仍按 FIFO 调度，但播放期间额外持有 PROMPT_TONE intent。 */
void voice_player_play_prompt_tone_url(const char *url)
{
    (void)voice_player_tone_submit(url,
                                   true,
                                   VOICE_PLAYER_TONE_SOURCE_DEFAULT);
}

/* 唤醒来源采用清队列、抢占当前 tone、重复唤醒不排队的特殊策略。 */
void voice_player_play_wakeup_tone_url(const char *url)
{
    (void)voice_player_tone_submit(url,
                                   true,
                                   VOICE_PLAYER_TONE_SOURCE_WAKEUP);
}

/* 闹钟仍按 FIFO 调度，独立来源用于完成事件匹配闹钟播放阶段。 */
void voice_player_play_alarm_tone_url(const char *url)
{
    (void)voice_player_tone_submit(url,
                                   false,
                                   VOICE_PLAYER_TONE_SOURCE_ALARM);
}
