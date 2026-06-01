#include <string.h>
#include <stdio.h>

#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "voice_cloud.h"
#include "voice_camera_preview_state.h"

#include "tone.h"
#include "app_tone.h"
#include "lisa_time.h"
#include "app_player.h"
#include "app_player_focus.h"
#include "voice_player_comm.h"
#include "service_alarm.h"
#include "alarm_ring.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "sysutils.h"
#include "queue.h"

#define TAG "platform"
#include "lisa_log.h"
#define ALARM_TTS_BUF_SIZE 512

/* 异步播放队列相关定义 */
#define ASYNC_PLAY_URL_MAX 256
#define ASYNC_PLAY_QUEUE_SIZE 5
#define ASYNC_PLAY_TASK_STACK_SIZE 2048
#define ASYNC_PLAY_TASK_PRIORITY 5
#define CLOUD_OPEN_INFO_STATUS_BIND 3U

typedef struct {
    uint8_t type;
    uint8_t bypass_tts_gate;
    uint8_t reserved[2];
    app_player_t *player;  /* 播放器实例指针 */
    char url[ASYNC_PLAY_URL_MAX];
} async_play_request_t;

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
static bool s_content_hold_for_tts = false;
/* 拍照预览/上传/result TTS 期间挂起音乐：设备 CPU / 网络 / PSRAM 同时承担相机、
 * 上传和音乐解码会拖慢预览倒计时和帧率，所以拍照流程开始就主动暂停音乐，
 * 直到 phase 回到 NONE 再恢复。 */
static bool s_content_hold_for_photo = false;
static bool s_resume_music_after_voice = false;
static bool s_alarm_session_active = false;
static bool s_resume_music_after_alarm = false;
static TimerHandle_t s_alarm_resume_timer = NULL;
static TimerHandle_t s_cloud_success_tone_timer = NULL;
static TickType_t s_bind_tone_tick = 0;
static voice_msg_camera_preview_state_t s_camera_preview_state = {0};
static bool s_photo_result_tts_gate_active = false;
static bool s_pending_photo_result_tts = false;
static bool s_pending_camera_flow_tts = false;
static char s_pending_photo_result_tts_url[ASYNC_PLAY_URL_MAX] = {0};
static char s_current_tts_url[ASYNC_PLAY_URL_MAX] = {0};
/* TTS 活跃标志：在调用 async_play_url 前置 true，事件回调收到 stop/error/complete
 * 时清零。读取无需获取播放器的 operation_lock，避免 ebus 回调在 prepare 阶段被
 * 长时间阻塞（async_play 线程跑 HTTP recv 时持锁可能达数秒）。 */
static volatile bool s_tts_active = false;
static struct {
    bool tone_active;
    bool tone_finished;
    bool pending_resume;
    bool resume_queued;
    bool focus_behavior_overridden;
    app_player_focus_behavior_t original_focus_behavior;
} s_camera_capture_tts_resume = {0};

static bool voice_player_should_gate_photo_result_tts(void);
static bool voice_player_should_defer_camera_flow_tts(void);
static void voice_player_pending_camera_flow_tts_set(const char *url);
static void voice_player_pending_camera_flow_tts_clear(const char *reason);
static void voice_player_pending_photo_result_tts_store(const char *url);
static void voice_player_pending_photo_result_tts_play_now(const char *reason);
static void voice_player_tts_track_url(const char *url);
static void voice_player_camera_capture_tts_focus_behavior_prepare(void);
static void voice_player_camera_capture_tts_focus_behavior_restore(const char *reason);
static void voice_player_camera_capture_tts_resume_clear(const char *reason);
static void voice_player_camera_capture_tts_restore_if_needed(const char *reason);
static void resume_music_after_voice_if_needed(const char *reason);
static void hold_content_for_photo_flow(const char *reason);
static void release_content_hold_for_photo_flow(const char *reason);

#define ALARM_RESUME_DELAY_MS 300
#define CLOUD_SUCCESS_TONE_DELAY_MS 500
#define CLOUD_UNSTABLE_TONE_GRACE_MS 800

/* 异步播放任务 */
static void async_play_task(void *pvParameters)
{
    async_play_request_t request;

    while (1) {
        /* 从队列中获取播放请求，阻塞等待 */
        if (xQueueReceive(s_async_play_queue, &request, portMAX_DELAY) == pdTRUE) {
            LOGI("async play task received request, type=%u, player=%p, url=%s",
                 (unsigned int)request.type, request.player, request.url);

            if (request.type == ASYNC_PLAY_REQUEST_RESUME) {
                app_player_resume(request.player);
                continue;
            }

            if (request.player == tts_player &&
                !request.bypass_tts_gate &&
                voice_player_should_gate_photo_result_tts()) {
                voice_player_pending_photo_result_tts_store(request.url);
                continue;
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

static bool voice_player_tts_interrupt_needed(void)
{
    app_player_state_t state;

    if (tts_player == NULL) {
        return false;
    }

    state = app_player_get_state(tts_player);
    return state == APP_PLAYER_STATE_PREPARING ||
           state == APP_PLAYER_STATE_PREPARED ||
           state == APP_PLAYER_STATE_PLAYING ||
           state == APP_PLAYER_STATE_PAUSED;
}

static void voice_player_camera_capture_tts_focus_behavior_prepare(void)
{
    app_player_focus_behavior_t behavior = {0};

    if (tts_player == NULL || s_camera_capture_tts_resume.focus_behavior_overridden) {
        return;
    }

    if (app_player_get_focus_behavior(tts_player, &behavior) != APP_PLAYER_OK) {
        LOGW("camera capture tone: failed to read tts focus behavior");
        return;
    }

    s_camera_capture_tts_resume.original_focus_behavior = behavior;
    if (behavior.on_background == APP_PLAYER_FOCUS_LOSS_STOP &&
        behavior.on_focus_lost == APP_PLAYER_FOCUS_LOSS_STOP) {
        return;
    }

    behavior.on_background = APP_PLAYER_FOCUS_LOSS_STOP;
    behavior.on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP;
    if (app_player_set_focus_behavior(tts_player, &behavior) != APP_PLAYER_OK) {
        LOGW("camera capture tone: failed to switch tts focus behavior to stop");
        return;
    }

    s_camera_capture_tts_resume.focus_behavior_overridden = true;
    LOGI("camera capture tone: tts focus behavior switched to stop");
}

static void voice_player_camera_capture_tts_focus_behavior_restore(const char *reason)
{
    if (!s_camera_capture_tts_resume.focus_behavior_overridden || tts_player == NULL) {
        return;
    }

    if (app_player_set_focus_behavior(tts_player,
                                      &s_camera_capture_tts_resume.original_focus_behavior) != APP_PLAYER_OK) {
        LOGW("camera capture tone: failed to restore tts focus behavior (%s)",
             reason ? reason : "unknown");
        return;
    }

    s_camera_capture_tts_resume.focus_behavior_overridden = false;
    LOGI("camera capture tone: restored tts focus behavior (%s)",
         reason ? reason : "unknown");
}

static void voice_player_pending_photo_result_tts_clear(const char *reason)
{
    if (!s_pending_photo_result_tts) {
        return;
    }

    LOGI("drop deferred photo result tts (%s)", reason ? reason : "unknown");
    s_pending_photo_result_tts = false;
    s_pending_photo_result_tts_url[0] = '\0';
}

static bool voice_player_should_gate_photo_result_tts(void)
{
    return voice_camera_preview_state_is_locked(&s_camera_preview_state) ||
           (s_photo_result_tts_gate_active &&
            !voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state));
}

static bool voice_player_should_defer_camera_flow_tts(void)
{
    if (!voice_camera_preview_state_is_mcp_mode(&s_camera_preview_state)) {
        return false;
    }

    return s_camera_preview_state.phase == VOICE_MSG_CAMERA_FLOW_PHASE_PREVIEW ||
           s_camera_capture_tts_resume.tone_active;
}

static void voice_player_pending_camera_flow_tts_set(const char *url)
{
    s_pending_camera_flow_tts = true;
    LOGI("defer latest tts during camera capture flow: %s", url ? url : "unknown");
}

static void voice_player_pending_camera_flow_tts_clear(const char *reason)
{
    if (!s_pending_camera_flow_tts) {
        return;
    }

    LOGI("clear deferred camera flow tts (%s)", reason ? reason : "unknown");
    s_pending_camera_flow_tts = false;
}

static void voice_player_pending_photo_result_tts_store(const char *url)
{
    if (!url || url[0] == '\0') {
        return;
    }

    strncpy(s_pending_photo_result_tts_url, url, sizeof(s_pending_photo_result_tts_url) - 1);
    s_pending_photo_result_tts_url[sizeof(s_pending_photo_result_tts_url) - 1] = '\0';
    s_pending_photo_result_tts = true;

    LOGI("defer photo result tts while camera flow locked: %s",
         s_pending_photo_result_tts_url);
}

static void voice_player_pending_photo_result_tts_play_if_ready(const char *reason)
{
    if (!s_pending_photo_result_tts ||
        !voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state)) {
        return;
    }

    LOGI("play deferred photo result tts (%s): %s",
         reason ? reason : "unknown", s_pending_photo_result_tts_url);
    voice_player_tts_track_url(s_pending_photo_result_tts_url);
    async_play_url(tts_player, s_pending_photo_result_tts_url);
    s_pending_photo_result_tts = false;
    s_pending_photo_result_tts_url[0] = '\0';
}

static void voice_player_pending_photo_result_tts_play_now(const char *reason)
{
    if (!s_pending_photo_result_tts) {
        return;
    }

    LOGI("play deferred photo result tts immediately (%s): %s",
         reason ? reason : "unknown", s_pending_photo_result_tts_url);
    voice_player_tts_track_url(s_pending_photo_result_tts_url);
    async_play_url(tts_player, s_pending_photo_result_tts_url);
    s_pending_photo_result_tts = false;
    s_pending_photo_result_tts_url[0] = '\0';
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

static void voice_player_camera_capture_tts_resume_clear(const char *reason)
{
    if (!s_camera_capture_tts_resume.tone_active &&
        !s_camera_capture_tts_resume.pending_resume &&
        !s_camera_capture_tts_resume.resume_queued &&
        !s_camera_capture_tts_resume.tone_finished &&
        !s_camera_capture_tts_resume.focus_behavior_overridden) {
        return;
    }

    voice_player_camera_capture_tts_focus_behavior_restore(reason);
    LOGI("clear camera capture tts resume state (%s)", reason ? reason : "unknown");
    memset(&s_camera_capture_tts_resume, 0, sizeof(s_camera_capture_tts_resume));
}

static void voice_player_camera_capture_tts_restore_if_needed(const char *reason)
{
    app_player_state_t tts_state;
    char latest_tts_url[ASYNC_PLAY_URL_MAX] = {0};

    if (!s_camera_capture_tts_resume.pending_resume ||
        !s_camera_capture_tts_resume.tone_finished ||
        s_camera_capture_tts_resume.resume_queued) {
        return;
    }

    if (tts_player == NULL) {
        voice_player_camera_capture_tts_resume_clear("invalid resume context");
        return;
    }

    if (s_pending_camera_flow_tts) {
        if (!voice_player_latest_tts_url_copy(latest_tts_url, sizeof(latest_tts_url))) {
            voice_player_pending_camera_flow_tts_clear("missing latest tts url");
            voice_player_camera_capture_tts_resume_clear("no latest tts url");
            return;
        }

        LOGI("play deferred latest tts after camera tone (%s), url=%s",
             reason ? reason : "unknown", latest_tts_url);
        async_play_url_internal(tts_player, latest_tts_url, true);
        voice_player_pending_camera_flow_tts_clear("camera tone restore queued");
        s_camera_capture_tts_resume.resume_queued = true;
        return;
    }

    tts_state = app_player_get_state(tts_player);
    switch (tts_state) {
    case APP_PLAYER_STATE_PLAYING:
        voice_player_camera_capture_tts_resume_clear("tts already resumed");
        return;
    case APP_PLAYER_STATE_PAUSED:
    case APP_PLAYER_STATE_PREPARED:
        LOGI("resume latest tts after camera tone (%s)",
             reason ? reason : "unknown");
        async_resume_player(tts_player);
        s_camera_capture_tts_resume.resume_queued = true;
        return;
    case APP_PLAYER_STATE_STOPPED:
    case APP_PLAYER_STATE_IDLE:
    case APP_PLAYER_STATE_ERROR:
        if (!voice_player_latest_tts_url_copy(latest_tts_url, sizeof(latest_tts_url))) {
            voice_player_camera_capture_tts_resume_clear("no latest tts url");
            return;
        }

        LOGI("replay latest tts after camera tone (%s), url=%s",
             reason ? reason : "unknown", latest_tts_url);
        async_play_url_internal(tts_player, latest_tts_url, true);
        s_camera_capture_tts_resume.resume_queued = true;
        return;
    case APP_PLAYER_STATE_PREPARING:
        LOGI("wait latest tts prepare before restore (%s)",
             reason ? reason : "unknown");
        return;
    default:
        return;
    }
}

void voice_player_notify_camera_capture_tone_start(void)
{
    bool has_latest_tts = false;
    app_player_focus_state_t tts_focus_state = APP_PLAYER_FOCUS_NONE;

    if (tone_player == NULL || tts_player == NULL) {
        return;
    }

    voice_player_camera_capture_tts_resume_clear("new camera tone");
    s_camera_capture_tts_resume.tone_active = true;

    has_latest_tts = voice_player_latest_tts_url_copy(NULL, 0);
    if (!has_latest_tts) {
        LOGI("camera capture tone start without latest tts url");
        return;
    }

    tts_focus_state = app_player_focus_get_state(tts_player);
    if (!s_pending_camera_flow_tts && tts_focus_state == APP_PLAYER_FOCUS_NONE) {
        LOGI("camera capture tone start without active tts focus");
        return;
    }

    voice_player_camera_capture_tts_focus_behavior_prepare();
    s_pending_camera_flow_tts = true;
    s_camera_capture_tts_resume.pending_resume = true;
    if (voice_player_tts_interrupt_needed()) {
        if (app_player_stop(tts_player) == APP_PLAYER_OK) {
            LOGI("camera capture tone: stop active tts before capture tone");
        } else {
            LOGW("camera capture tone: stop active tts failed");
        }
    }
    LOGI("camera capture tone will replay latest tts after tone, focus=%d, deferred=%d",
         tts_focus_state, s_pending_camera_flow_tts);
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

static void voice_player_camera_preview_state_changed(void *unused, uint32_t msg_id, void *data,
                                                      uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!voice_camera_preview_state_parse(&s_camera_preview_state, data, len)) {
        return;
    }

    if (voice_camera_preview_state_is_mcp_mode(&s_camera_preview_state) &&
        s_camera_preview_state.phase != VOICE_MSG_CAMERA_FLOW_PHASE_NONE) {
        s_photo_result_tts_gate_active = true;
    } else if (s_camera_preview_state.phase == VOICE_MSG_CAMERA_FLOW_PHASE_NONE) {
        s_photo_result_tts_gate_active = false;
        release_content_hold_for_photo_flow("camera flow ended (state phase=NONE)");
    }

    if (s_pending_photo_result_tts) {
        if (voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state)) {
            voice_player_pending_photo_result_tts_play_if_ready("camera result tts active");
        } else if (s_camera_preview_state.phase == VOICE_MSG_CAMERA_FLOW_PHASE_NONE) {
            voice_player_pending_photo_result_tts_clear("camera flow ended before result tts");
        }
    }
}

static void voice_player_camera_preview_start(void *unused, uint32_t msg_id, void *data, uint32_t len,
                                              void *user_data)
{
    voice_msg_camera_preview_req_t *req = (voice_msg_camera_preview_req_t *)data;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!req || len < sizeof(*req)) {
        return;
    }

    if (req->mode == VOICE_MSG_CAMERA_PREVIEW_MODE_MCP_PHOTO) {
        s_photo_result_tts_gate_active = true;
        LOGI("enable photo result tts gate on preview start");
        hold_content_for_photo_flow("camera preview start");
    } else {
        s_photo_result_tts_gate_active = false;
        voice_player_pending_camera_flow_tts_clear("non-mcp preview start");
        voice_player_pending_photo_result_tts_clear("non-mcp preview start");
    }
}

static void voice_player_camera_preview_exit(void *unused, uint32_t msg_id, void *data, uint32_t len,
                                             void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (voice_camera_preview_state_is_locked(&s_camera_preview_state)) {
        if (voice_player_tts_interrupt_needed()) {
            if (app_player_stop(tts_player) == APP_PLAYER_OK) {
                LOGI("camera preview canceled, stop active tts");
            } else {
                LOGW("camera preview canceled, stop tts failed");
            }
        }
        voice_player_camera_capture_tts_resume_clear("camera preview canceled");
    }

    s_photo_result_tts_gate_active = false;
    voice_player_pending_camera_flow_tts_clear("camera preview exit");

    if (!voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state)) {
        voice_player_pending_photo_result_tts_clear("camera preview exit");
    }

    release_content_hold_for_photo_flow("camera preview exit");
}

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

static void voice_player_play_wakeup_tone(void)
{
    char *tone_url = voice_player_get_wakeup_tone_url();
    if (tone_url == NULL) {
        LOGE("wakeup tone url is null");
        return;
    }

    app_player_play(tone_player, tone_url);
}

static void voice_player_play_network_success_tone(void)
{
    static TickType_t s_network_success_tone_tick = 0;
    TickType_t now = xTaskGetTickCount();

    if (s_network_success_tone_tick != 0 &&
        (now - s_network_success_tone_tick) < pdMS_TO_TICKS(3000U)) {
        return;
    }

    app_player_play(tone_player, app_tone_get_url(TONE_ID_59));
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

    app_player_play(tone_player, app_tone_get_url(TONE_ID_71));
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

    app_player_play(tone_player, app_tone_get_url(TONE_ID_65));
    s_disconnect_tone_played = true;
    voice_player_reset_cloud_unstable_pending();
}

static void hold_content_until_tts_playing(void)
{
    if (!s_content_hold_for_tts && !s_content_hold_for_photo) {
        return;
    }

    bool tts_playing = (app_player_get_state(tts_player) == APP_PLAYER_STATE_PLAYING);
    bool content_playing = (app_player_get_state(music_player) == APP_PLAYER_STATE_PLAYING);
    if (!tts_playing && content_playing) {
        LOGI("re-pause content while waiting tts start");
        app_player_pause(music_player);
    }
}

static void hold_content_for_photo_flow(const char *reason)
{
    app_player_state_t music_state = app_player_get_state(music_player);
    app_player_focus_state_t music_focus_state = app_player_focus_get_state(music_player);

    s_content_hold_for_photo = true;

    if (music_state == APP_PLAYER_STATE_PLAYING) {
        LOGI("pause content for photo flow: %s", reason ? reason : "unknown");
        s_resume_music_after_voice = true;
        app_player_pause(music_player);
        return;
    }

    if (music_state == APP_PLAYER_STATE_PAUSED && music_focus_state != APP_PLAYER_FOCUS_NONE) {
        LOGI("keep content paused for photo flow: %s, focus_state=%d",
             reason ? reason : "unknown", music_focus_state);
        s_resume_music_after_voice = true;
    }
}

static void release_content_hold_for_photo_flow(const char *reason)
{
    if (!s_content_hold_for_photo) {
        return;
    }

    s_content_hold_for_photo = false;
    LOGI("release photo flow hold: %s", reason ? reason : "unknown");
    resume_music_after_voice_if_needed(reason);
}

static void hold_content_for_voice_session(const char *reason)
{
    app_player_state_t music_state = app_player_get_state(music_player);
    app_player_focus_state_t music_focus_state = app_player_focus_get_state(music_player);

    s_content_hold_for_tts = true;

    if (music_state == APP_PLAYER_STATE_PLAYING) {
        LOGI("pause content for voice session: %s", reason ? reason : "unknown");
        s_resume_music_after_voice = true;
        app_player_pause(music_player);
        return;
    }

    if (music_state == APP_PLAYER_STATE_PAUSED && music_focus_state != APP_PLAYER_FOCUS_NONE) {
        LOGI("keep content paused for voice session: %s, focus_state=%d",
             reason ? reason : "unknown", music_focus_state);
        s_resume_music_after_voice = true;
    }
}

static void clear_music_resume_after_voice(const char *reason)
{
    if (!s_resume_music_after_voice) {
        return;
    }

    LOGI("clear music resume after voice: %s", reason ? reason : "unknown");
    s_resume_music_after_voice = false;
}

static void clear_music_resume_after_alarm(const char *reason)
{
    if (!s_resume_music_after_alarm) {
        return;
    }

    LOGI("clear music resume after alarm: %s", reason ? reason : "unknown");
    s_resume_music_after_alarm = false;
}

static void resume_music_after_voice_if_needed(const char *reason)
{
    if (!s_resume_music_after_voice) {
        return;
    }

    if (s_alarm_session_active) {
        LOGI("skip music resume after voice (%s), alarm session is active", reason ? reason : "unknown");
        return;
    }

    if (s_content_hold_for_tts) {
        LOGI("skip music resume after voice (%s), still waiting for tts", reason ? reason : "unknown");
        return;
    }

    if (s_content_hold_for_photo) {
        LOGI("skip music resume after voice (%s), photo flow active", reason ? reason : "unknown");
        return;
    }

    if (app_player_get_state(tts_player) == APP_PLAYER_STATE_PLAYING) {
        LOGI("skip music resume after voice (%s), tts still playing", reason ? reason : "unknown");
        return;
    }

    app_player_state_t music_state = app_player_get_state(music_player);
    if (music_state == APP_PLAYER_STATE_PAUSED) {
        LOGI("resume music after voice: %s", reason ? reason : "unknown");
        if (app_player_resume(music_player) == APP_PLAYER_OK) {
            s_resume_music_after_voice = false;
        }
        return;
    }

    if (music_state == APP_PLAYER_STATE_PLAYING) {
        LOGI("music already resumed before voice end: %s", reason ? reason : "unknown");
    } else {
        LOGI("skip music resume after voice (%s), music state=%d", reason ? reason : "unknown", music_state);
    }

    s_resume_music_after_voice = false;
}

static void hold_content_for_alarm_session(const char *reason)
{
    app_player_state_t music_state = app_player_get_state(music_player);
    app_player_focus_state_t music_focus_state = app_player_focus_get_state(music_player);

    s_alarm_session_active = true;

    if (music_state == APP_PLAYER_STATE_PLAYING) {
        LOGI("pause content for alarm session: %s", reason ? reason : "unknown");
        s_resume_music_after_alarm = true;
        app_player_pause(music_player);
        return;
    }

    if (music_state == APP_PLAYER_STATE_PAUSED && (s_resume_music_after_voice || music_focus_state != APP_PLAYER_FOCUS_NONE)) {
        LOGI("inherit paused content for alarm session: %s, focus_state=%d",
             reason ? reason : "unknown", music_focus_state);
        s_resume_music_after_alarm = true;
        return;
    }

    LOGI("alarm session starts without resumable music: %s, state=%d, focus_state=%d",
         reason ? reason : "unknown", music_state, music_focus_state);
}

static void resume_music_after_alarm_if_needed(const char *reason)
{
    if (!s_resume_music_after_alarm) {
        return;
    }

    if (s_alarm_session_active || alarm_ring_is_active()) {
        LOGI("skip music resume after alarm (%s), alarm still active", reason ? reason : "unknown");
        return;
    }

    if (s_content_hold_for_tts) {
        LOGI("skip music resume after alarm (%s), voice session is holding content",
             reason ? reason : "unknown");
        return;
    }

    if (app_player_get_state(tts_player) == APP_PLAYER_STATE_PLAYING) {
        LOGI("skip music resume after alarm (%s), tts still playing", reason ? reason : "unknown");
        return;
    }

    app_player_state_t music_state = app_player_get_state(music_player);
    if (music_state == APP_PLAYER_STATE_PAUSED) {
        LOGI("resume music after alarm: %s", reason ? reason : "unknown");
        if (app_player_resume(music_player) == APP_PLAYER_OK) {
            s_resume_music_after_alarm = false;
        }
        return;
    }

    if (music_state == APP_PLAYER_STATE_PLAYING) {
        LOGI("music already resumed before alarm end: %s", reason ? reason : "unknown");
    } else {
        LOGI("skip music resume after alarm (%s), music state=%d", reason ? reason : "unknown", music_state);
    }

    s_resume_music_after_alarm = false;
}

static void alarm_resume_timer_stop(void)
{
    if (s_alarm_resume_timer == NULL) {
        return;
    }

    if (xTimerIsTimerActive(s_alarm_resume_timer) != pdFALSE) {
        if (xTimerStop(s_alarm_resume_timer, 0) != pdPASS) {
            LOGW("failed to stop alarm resume timer");
        }
    }
}

static void alarm_resume_timer_start(void)
{
    if (s_alarm_resume_timer == NULL) {
        return;
    }

    if (xTimerIsTimerActive(s_alarm_resume_timer) != pdFALSE) {
        if (xTimerStop(s_alarm_resume_timer, 0) != pdPASS) {
            LOGW("failed to stop alarm resume timer before restart");
        }
    }

    if (xTimerChangePeriod(s_alarm_resume_timer, pdMS_TO_TICKS(ALARM_RESUME_DELAY_MS), 0) != pdPASS) {
        LOGW("failed to start alarm resume timer");
    }
}

static void alarm_resume_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (alarm_ring_is_active()) {
        LOGI("alarm resume timer fired, but alarm is still active");
        return;
    }

    s_alarm_session_active = false;
    resume_music_after_alarm_if_needed("alarm stopped");
}

static void voice_player_cloud_session_interrupt(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LOGI("voice session interrupted, stop current tts");
    s_content_hold_for_tts = false;
    /* 不清 s_resume_music_after_voice：interrupt 仅替换"当前 TTS / 当前会话"
     * （拍照、闹钟、log_upload、cloud 网络抖动重启），并不取消"先前因唤醒被
     *  暂停的音乐应当恢复"这一意图。清掉会让拍照/闹钟流结束后音乐永远回不来。 */
    if (voice_player_tts_interrupt_needed()) {
        if (app_player_stop(tts_player) == APP_PLAYER_OK) {
            LOGI("voice session interrupt: stopped active tts");
        } else {
            LOGW("voice session interrupt: stop tts failed");
        }
    } else {
        LOGI("voice session interrupt ignored, tts inactive");
    }
}

static void voice_player_alarm_trigger(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    alarm_resume_timer_stop();
    hold_content_for_alarm_session("alarm trigger");
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
        if ((app_datas->voice_work_mode & VOICE_WORK_MODE_BUTTON_WAKEUP) == 0) {
            return;
        }

        if (!app_datas->can_wakeup) {
            return;
        }

        if (app_datas->voice_cloud_connected == 1) {
            voice_player_play_wakeup_tone();
        } else if (!network_status_ok || !network_status.connected) {
            app_player_play(tone_player, app_tone_get_url(TONE_ID_64));
        } else if (app_datas->auth_failed) {
            app_player_play(tone_player, app_tone_get_url(TONE_ID_105));
        } else {
            app_player_play(tone_player, app_tone_get_url(TONE_ID_85));
        }
    } break;
    case VOICE_MSG_WAKEUP_KEYWORD: {
        if ((app_datas->voice_work_mode & VOICE_WORK_MODE_VOICE_WAKEUP) == 0) {
            return;
        }

        if (!app_datas->can_wakeup) {
            return;
        }

        if (app_datas->voice_cloud_connected == 1) {
            voice_player_play_wakeup_tone();
        } else if (!network_status_ok || !network_status.connected) {
            app_player_play(tone_player, app_tone_get_url(TONE_ID_64));
        } else if (app_datas->auth_failed) {
            app_player_play(tone_player, app_tone_get_url(TONE_ID_105));
        } else {
            app_player_play(tone_player, app_tone_get_url(TONE_ID_85));
        }
    } break;
    case VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS: {
        app_datas->auth_failed = 0;
        voice_player_handle_cloud_connect_success(true);
    } break;
    case VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED: {
        voice_player_cancel_cloud_connect_success_tone();
        app_datas->auth_failed = 1;
        app_player_play(tone_player, app_tone_get_url(TONE_ID_105));
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
            app_player_play(tone_player, app_tone_get_url(TONE_ID_60));
            s_disconnect_tone_played = true;
        }
    } break;
    case VOICE_MSG_BLE_CONNECT_DONE: {
        app_player_play(tone_player, app_tone_get_url(TONE_ID_72));
    } break;
    case VOICE_MSG_CLOUD_TTS_URL: {
        if (data == NULL) {
            LOGE("Invalid data");
            break;
        }

        LOGI("Ready to play tts url asynchronously: %s", (char *)data);
        voice_player_tts_track_url((char *)data);
        if (voice_player_should_defer_camera_flow_tts()) {
            voice_player_pending_camera_flow_tts_set((char *)data);
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
            app_player_stop(tone_player);
            app_player_stop(tts_player);
        }

        LOGI("Ready to play tts url asynchronously: %s", (char *)data);
        voice_player_tts_track_url((char *)data);
        if (voice_player_should_defer_camera_flow_tts()) {
            voice_player_pending_camera_flow_tts_set((char *)data);
            break;
        }
        if (voice_player_should_gate_photo_result_tts()) {
            voice_player_pending_photo_result_tts_store((char *)data);
            break;
        }
        /* 使用异步播放接口，避免在ebus回调中阻塞HTTP下载 */
        async_play_url(tts_player, (char *)data);

    } break;
    case VOICE_MSG_CLOUD_SESSION_STARTING: {
        hold_content_for_voice_session("cloud session starting");
    } break;
    case VOICE_MSG_CLOUD_IAT_UPDATE: {
        if (data == NULL || len == 0 || ((char *)data)[0] == '\0') {
            break;
        }

        bool tts_playing = (app_player_get_state(tts_player) == APP_PLAYER_STATE_PLAYING);
        bool content_playing = (app_player_get_state(music_player) == APP_PLAYER_STATE_PLAYING);
        if (!tts_playing && !content_playing) {
            break;
        }

        s_content_hold_for_tts = true;

        if (tts_playing) {
            LOGI("stop tts due to valid iat update");
            app_player_stop(tts_player);
        }

        if (content_playing) {
            LOGI("pause content due to valid iat update");
            s_resume_music_after_voice = true;
            app_player_pause(music_player);
        }
    } break;
    case VOICE_MSG_CLOUD_SESSION_FINISHED: {
        s_content_hold_for_tts = false;
        resume_music_after_voice_if_needed("cloud session finished");
        resume_music_after_alarm_if_needed("cloud session finished");
    } break;
    case VOICE_MSG_CLOUD_MCP_CALL_RESP: {
        /*
         * 兜底：拍照上传成功后若未进入 RESULT_TTS 阶段，直接释放延迟 TTS，
         * 避免卡在 processing 导致不复播。
         */
        if (s_photo_result_tts_gate_active &&
            s_pending_photo_result_tts &&
            !voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state)) {
            s_photo_result_tts_gate_active = false;
            voice_player_pending_camera_flow_tts_clear("mcp call response");
            voice_player_pending_photo_result_tts_play_now("mcp call response");
        }
    } break;
    default:
        break;
    }
}

static void voice_player_audio_item(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct voice_msg_audio_items *items = (struct voice_msg_audio_items *)data;
    if (items == NULL) {
        LOGE("Invalid data");
        return;
    }

    if (items->cnt == 0) {
        LOGE("Invalid cnt");
        return;
    }

    voice_player_play_array(items->items, items->cnt);
}

static void voice_player_play_control(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    switch (msg_id) {
    case VOICE_MSG_PLAY_CONTROL_PLAY: {
        clear_music_resume_after_voice("play control play");
        clear_music_resume_after_alarm("play control play");
        app_player_resume(music_player);
    } break;
    case VOICE_MSG_PLAY_CONTROL_PAUSE: {
        clear_music_resume_after_voice("play control pause");
        clear_music_resume_after_alarm("play control pause");
        app_player_pause(music_player);
    } break;
    case VOICE_MSG_PLAY_CONTROL_NEXT: {
        clear_music_resume_after_voice("play control next");
        clear_music_resume_after_alarm("play control next");
        voice_player_play_next();
    } break;
    case VOICE_MSG_PLAY_CONTROL_PREVIOUS: {
        clear_music_resume_after_voice("play control previous");
        clear_music_resume_after_alarm("play control previous");
        voice_player_play_prev();
    } break;
    case VOICE_MSG_PLAY_CONTROL_REPLAY: {
        clear_music_resume_after_voice("play control replay");
        clear_music_resume_after_alarm("play control replay");
        voice_player_replay_current();
    } break;
    default:
        break;
    }
}

static void on_tts_event(app_player_t *player, app_player_event_t event, void *user_data)
{
    bool suppress_stop_event = false;

    LOGI("tts player event: %d", event);

    if (event == APP_PLAYER_EVENT_PLAYING) {
        voice_player_pending_camera_flow_tts_clear("tts playing");
        if (s_camera_capture_tts_resume.pending_resume) {
            voice_player_camera_capture_tts_resume_clear("tts resumed after camera tone");
        }
        s_content_hold_for_tts = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PLAYING, NULL, 0);
    } else if (event == APP_PLAYER_EVENT_PAUSED) {
        if (s_camera_capture_tts_resume.pending_resume &&
            !s_camera_capture_tts_resume.tone_active) {
            s_camera_capture_tts_resume.resume_queued = false;
            s_camera_capture_tts_resume.tone_finished = true;
            voice_player_camera_capture_tts_restore_if_needed("tts paused after camera tone");
        }
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PAUSED, NULL, 0);
    } else if (event == APP_PLAYER_EVENT_COMPLETED) {
        s_tts_active = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        resume_music_after_voice_if_needed("tts completed");
        if (alarm_ring_is_active()) {
            alarm_ring_notify_playback_complete();
        }
    } else if (event == APP_PLAYER_EVENT_STOPPED || event == APP_PLAYER_EVENT_ERROR) {
        s_tts_active = false;
        if (s_camera_capture_tts_resume.pending_resume) {
            suppress_stop_event = true;
            if (!s_camera_capture_tts_resume.tone_active) {
                s_camera_capture_tts_resume.resume_queued = false;
                s_camera_capture_tts_resume.tone_finished = true;
                voice_player_camera_capture_tts_restore_if_needed(
                    event == APP_PLAYER_EVENT_STOPPED ? "tts stopped after camera tone"
                                                      : "tts error after camera tone");
            }
        }

        if (!suppress_stop_event) {
            voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        }
        resume_music_after_voice_if_needed(event == APP_PLAYER_EVENT_STOPPED ? "tts stopped" : "tts error");
    }
}

static bool on_music_focus_change(app_player_t *player,
                                  app_player_focus_state_t state,
                                  app_player_t *by_which,
                                  void *user_data)
{
    if (state == APP_PLAYER_FOCUS_FOREGROUND) {
        if (s_content_hold_for_tts) {
            LOGI("hold music auto resume while voice session is active");
            return true;
        }

        if (s_content_hold_for_photo) {
            LOGI("hold music auto resume while photo flow is active");
            return true;
        }

        hold_content_until_tts_playing();
    }

    return false;
}

static void on_tone_event(app_player_t *player, app_player_event_t event, void *user_data)
{
    if (s_camera_capture_tts_resume.tone_active &&
        (event == APP_PLAYER_EVENT_COMPLETED ||
         event == APP_PLAYER_EVENT_STOPPED ||
         event == APP_PLAYER_EVENT_ERROR)) {
        s_camera_capture_tts_resume.tone_active = false;
        s_camera_capture_tts_resume.tone_finished = true;
        s_camera_capture_tts_resume.resume_queued = false;
        voice_player_camera_capture_tts_focus_behavior_restore(
            event == APP_PLAYER_EVENT_COMPLETED ? "camera tone completed"
                                                : (event == APP_PLAYER_EVENT_STOPPED
                                                       ? "camera tone stopped"
                                                       : "camera tone error"));
        voice_player_camera_capture_tts_restore_if_needed(
            event == APP_PLAYER_EVENT_COMPLETED ? "camera tone completed"
                                                : (event == APP_PLAYER_EVENT_STOPPED
                                                       ? "camera tone stopped"
                                                       : "camera tone error"));
        voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_TONE_FINISHED, NULL, 0);
    }

    if (event == APP_PLAYER_EVENT_COMPLETED && alarm_ring_is_active()) {
        alarm_ring_notify_playback_complete();
    }
}

static void voice_alarm_ring_play_once(const char *text)
{
    struct app_datas *app_datas = get_app_datas();

    switch (s_alarm_play_ctx.phase) {
        case ALARM_PLAY_PHASE_TONE_FIRST:
            /* 第一阶段：播放默认闹钟提示音（一次） */
            LISA_LOGI(TAG, "alarm play phase: TONE_FIRST");
            app_player_play(tone_player, app_tone_get_url(TONE_ID_94));
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
                app_player_play(tone_player, app_tone_get_url(TONE_ID_94));
                break;
            }

            char *tts_text = lisa_mem_alloc(ALARM_TTS_BUF_SIZE);
            if (tts_text == NULL) {
                LISA_LOGW(TAG, "alarm TTS alloc failed, fallback to tone loop");
                s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_LOOP;
                app_player_play(tone_player, app_tone_get_url(TONE_ID_94));
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
            app_player_play(tone_player, app_tone_get_url(TONE_ID_94));
            break;
    }
}

static void voice_alarm_ring_force_stop(void)
{
    app_player_stop(tone_player);
    app_player_stop(tts_player);

    /* 重置闹钟播放状态机，确保下次触发时从头开始 */
    s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_FIRST;
    memset(s_alarm_play_ctx.text, 0, sizeof(s_alarm_play_ctx.text));

    alarm_resume_timer_start();
}

static void voice_player_mcp_chat_exit(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_player_mcp_chat_exit, stop content player");
    s_content_hold_for_tts = false;
    clear_music_resume_after_voice("mcp chat exit");
    app_player_stop(music_player);
}

void voice_player_ready(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_LOGI(TAG, "voice_player_ready");

    /* 初始化异步播放队列和任务 */
    if (async_play_init() != 0) {
        LOGE("async play init failed");
    }

    sys_network_status_t network_status = {0};
    bool wifi_mode = (sys_network_get_status(&network_status) == 0) &&
                     (network_status.mode == SYS_NETWORK_MODE_WIFI_PREFERRED);

    if (wifi_mode && !sys_wifi_has_ap()) {
        app_player_play(tone_player, app_tone_get_url(TONE_ID_70));
    }


    if (s_alarm_resume_timer == NULL) {
        s_alarm_resume_timer =
            xTimerCreate("alarm.resume", pdMS_TO_TICKS(ALARM_RESUME_DELAY_MS), pdFALSE, NULL, alarm_resume_timer_cb);
        if (s_alarm_resume_timer == NULL) {
            LOGE("failed to create alarm resume timer");
        }
    }

    if (s_cloud_success_tone_timer == NULL) {
        s_cloud_success_tone_timer = xTimerCreate("cloud.success", pdMS_TO_TICKS(CLOUD_SUCCESS_TONE_DELAY_MS),
                                                  pdFALSE, NULL, cloud_success_tone_timer_cb);
        if (s_cloud_success_tone_timer == NULL) {
            LOGE("failed to create cloud success tone timer");
        }
    }

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
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CALL_RESP, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_AUDIO_ITEM, voice_player_audio_item, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_START, voice_player_camera_preview_start, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, voice_player_camera_preview_state_changed, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, voice_player_camera_preview_exit, NULL);

    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PLAY, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PAUSE, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_NEXT, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PREVIOUS, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_REPLAY, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, voice_player_mcp_chat_exit, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, voice_player_cloud_session_interrupt, NULL);
    voice_msg_sub(VOICE_MSG_BLE_CONNECT_DONE, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_ALARM_TRIGGER, voice_player_alarm_trigger, NULL);

    alarm_ring_init(voice_alarm_ring_play_once, voice_alarm_ring_force_stop);
    app_player_register_callback(tts_player, on_tts_event, NULL);
    app_player_register_callback(tone_player, on_tone_event, NULL);
    app_player_register_focus_cb(music_player, on_music_focus_change, NULL);
}

static int voice_player_init(void)
{
    LISA_LOGI(TAG, "voice_player_init");

    voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_player_ready, NULL);

    return 0;
}

SYS_INIT(voice_player_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);
