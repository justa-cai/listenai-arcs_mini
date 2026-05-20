#include <string.h>
#include <stdio.h>

#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"

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
#define ASYNC_PLAY_QUEUE_SIZE 5
#define ASYNC_PLAY_TASK_STACK_SIZE 2048
#define ASYNC_PLAY_TASK_PRIORITY 5

typedef struct {
    app_player_t *player;  /* 播放器实例指针 */
    char url[256];
} async_play_request_t;

static QueueHandle_t s_async_play_queue = NULL;
static TaskHandle_t s_async_play_task = NULL;

static bool s_disconnect_tone_played = false;
static bool s_cloud_reconnect_pending = false;
static bool s_cloud_success_tone_played = false;
static bool s_content_hold_for_tts = false;
static bool s_resume_music_after_voice = false;
static bool s_alarm_session_active = false;
static bool s_resume_music_after_alarm = false;
static TimerHandle_t s_alarm_resume_timer = NULL;

#define ALARM_RESUME_DELAY_MS 300

/* 异步播放任务 */
static void async_play_task(void *pvParameters)
{
    async_play_request_t request;

    while (1) {
        /* 从队列中获取播放请求，阻塞等待 */
        if (xQueueReceive(s_async_play_queue, &request, portMAX_DELAY) == pdTRUE) {
            LOGI("async play task received request, player=%p, url=%s",
                 request.player, request.url);

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
static void async_play_url(app_player_t *player, const char *url)
{
    if (s_async_play_queue == NULL || url == NULL) {
        LOGE("async play queue not ready or url is null");
        return;
    }

    async_play_request_t request;
    request.player = player;
    strncpy(request.url, url, sizeof(request.url) - 1);
    request.url[sizeof(request.url) - 1] = '\0';

    /* 发送到队列，最多等待100ms */
    if (xQueueSend(s_async_play_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("failed to send play request to queue");
    } else {
        LOGI("play request sent to async queue, player=%p", player);
    }
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

static void voice_player_handle_cloud_connect_success(bool force_play)
{
    s_disconnect_tone_played = false;

    if (!force_play && !s_cloud_reconnect_pending) {
        return;
    }

    if (s_cloud_success_tone_played) {
        s_cloud_reconnect_pending = false;
        return;
    }

    voice_player_play_network_success_tone();
    s_cloud_success_tone_played = true;
    s_cloud_reconnect_pending = false;
}

static bool voice_player_should_skip_disconnect_tone(const sys_network_status_t *network_status,
                                                     bool network_status_ok)
{
    if (network_status_ok && network_status->switching) {
        return true;
    }

    if (sys_wifi_get_user_force_provision()) {
        s_disconnect_tone_played = false;
        s_cloud_reconnect_pending = false;
        s_cloud_success_tone_played = false;
        LOGI("skip network disconnect tone during user WiFi provisioning flow");
        return true;
    }

    if (sys_wifi_get_force_provision()) {
        s_disconnect_tone_played = false;
        s_cloud_reconnect_pending = false;
        s_cloud_success_tone_played = false;
        LOGI("skip network disconnect tone during WiFi force provisioning");
        return true;
    }

    return false;
}

static void hold_content_until_tts_playing(void)
{
    if (!s_content_hold_for_tts) {
        return;
    }

    bool tts_playing = (app_player_get_state(tts_player) == APP_PLAYER_STATE_PLAYING);
    bool content_playing = (app_player_get_state(music_player) == APP_PLAYER_STATE_PLAYING);
    if (!tts_playing && content_playing) {
        LOGI("re-pause content while waiting tts start");
        app_player_pause(music_player);
    }
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

    LOGI("voice session interrupted by alarm");
    s_content_hold_for_tts = false;
    clear_music_resume_after_voice("alarm interrupt");
    app_player_stop(tts_player);
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
        app_datas->auth_failed = 1;
        app_player_play(tone_player, app_tone_get_url(TONE_ID_105));
    } break;
    case VOICE_MSG_CLOUD_CONNECTED: {
        voice_player_handle_cloud_connect_success(false);
    } break;
    case VOICE_MSG_CLOUD_DISCONNECTED: {
        if (voice_player_should_skip_disconnect_tone(&network_status, network_status_ok)) {
            break;
        }
        s_cloud_reconnect_pending = true;
        s_cloud_success_tone_played = false;
        if (!s_disconnect_tone_played) {
            if (network_status_ok && network_status.connected) {
                app_player_play(tone_player, app_tone_get_url(TONE_ID_65));
            } else {
                app_player_play(tone_player, app_tone_get_url(TONE_ID_60));
            }
            s_disconnect_tone_played = true;
        }
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_CONNECTED: {
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS: {
        if (s_disconnect_tone_played) {
            s_disconnect_tone_played = false;
            voice_player_play_network_success_tone();
        }
    } break;
    case VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED: {
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
    case VOICE_MSG_CLOUD_TTS_URL:
    case VOICE_MSG_CLOUD_PUSHUP_TTS_URL: {
        if (data == NULL) {
            LOGE("Invalid data");
            break;
        }

        LOGI("Ready to play tts url asynchronously: %s", (char *)data);
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
    LOGI("tts player event: %d", event);

    if (event == APP_PLAYER_EVENT_PLAYING) {
        s_content_hold_for_tts = false;
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PLAYING, NULL, 0);
    } else if (event == APP_PLAYER_EVENT_PAUSED) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PAUSED, NULL, 0);
    } else if (event == APP_PLAYER_EVENT_COMPLETED) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        resume_music_after_voice_if_needed("tts completed");
        if (alarm_ring_is_active()) {
            alarm_ring_notify_playback_complete();
        }
    } else if (event == APP_PLAYER_EVENT_STOPPED || event == APP_PLAYER_EVENT_ERROR) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
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

        hold_content_until_tts_playing();
    }

    return false;
}

static void on_tone_event(app_player_t *player, app_player_event_t event, void *user_data)
{
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

    if (s_alarm_resume_timer == NULL) {
        s_alarm_resume_timer =
            xTimerCreate("alarm.resume", pdMS_TO_TICKS(ALARM_RESUME_DELAY_MS), pdFALSE, NULL, alarm_resume_timer_cb);
        if (s_alarm_resume_timer == NULL) {
            LOGE("failed to create alarm resume timer");
        }
    }

    voice_msg_sub(VOICE_MSG_WAKEUP_BUTTON_START, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_CONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_AUDIO_ITEM, voice_player_audio_item, NULL);

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
