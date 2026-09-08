#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "ClockManager.h"
#include "system.h"

#define TAG "power.policy"
#include "lisa_log.h"

#include "acomp_logger.h"
#include "app_tone.h"
#include "app_wakeup.h"
#include "battery/battery.h"
#include "battery/battery_ui.h"
#include "power/power_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "voice_player/tone_control/voice_player_tone.h"

#if CONFIG_LISA_SHELL
#include "cmd/cmd.h"
#include "shell.h"
#endif

#include "service_power_policy.h"

#define POWER_POLICY_TASK_STACK_SIZE          3072U
#define POWER_POLICY_TASK_PRIORITY            4U
#define POWER_POLICY_POLL_INTERVAL_MS          1000U
#define POWER_POLICY_IDLE_TIMEOUT_MS           (30U * 1000U)
#define POWER_POLICY_HIBERNATE_TIMEOUT_MS      (5U * 60U * 1000U)
#define POWER_POLICY_SHUTDOWN_AFTER_HIBERNATE_MS (10U * 60U * 1000U)
#define POWER_POLICY_SHUTDOWN_TIMEOUT_MS       (POWER_POLICY_HIBERNATE_TIMEOUT_MS + \
                                                POWER_POLICY_SHUTDOWN_AFTER_HIBERNATE_MS)
#define POWER_POLICY_SHUTDOWN_COUNTDOWN_MS     (10U * 1000U)
#define POWER_POLICY_WARNING_WATCHDOG_MS       (30U * 1000U)

/* Official shutdown profile: only shut down below the low-battery threshold. */
#define POWER_POLICY_SHUTDOWN_BATTERY_THRESHOLD_PCT 30U

#define POWER_POLICY_ACTIVE_CORE_CLOCK         CRM_IpCore_300MHz
#define POWER_POLICY_HIBERNATE_CORE_CLOCK      CRM_IpCore_150MHz

struct power_policy_context {
    SemaphoreHandle_t state_lock;
    SemaphoreHandle_t runtime_lock;
    service_power_policy_state_t state;
    TickType_t last_activity_tick;
    TickType_t warning_start_tick;
    TickType_t countdown_start_tick;
    bool interaction_active;
    bool tts_active;
    bool music_active;
    bool warning_waiting;
    bool countdown_active;
    bool runtime_suspended;
    bool initialized;
};

static struct power_policy_context s_policy;

static const char *power_policy_state_name(service_power_policy_state_t state)
{
    switch (state) {
    case VOICE_MSG_POWER_POLICY_STATE_NORMAL:
        return "normal";
    case VOICE_MSG_POWER_POLICY_STATE_IDLE:
        return "idle";
    case VOICE_MSG_POWER_POLICY_STATE_HIBERNATE:
        return "hibernate";
    case VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING:
        return "shutdown-pending";
    default:
        return "unknown";
    }
}

static int power_policy_set_core_clock(clock_src_core_div_t divider)
{
    int ret;
    int switch_ret;

    taskENTER_CRITICAL();
    ret = (int)HAL_CRM_SetHclkClkSrc(CRM_IpSrcXtalClk);
    if (ret == CSK_DRIVER_OK) {
        ret = CRM_InitCoreSrc(divider);
        switch_ret = (int)HAL_CRM_SetHclkClkSrc(CRM_IpSrcCoreClk);
        if (ret == CSK_DRIVER_OK) {
            ret = switch_ret;
        }
        SystemCoreClockUpdate();
    }
    taskEXIT_CRITICAL();

    return ret;
}

int service_power_policy_set_runtime_suspended(bool suspended)
{
    int ret = 0;
    int step_ret;

    if (s_policy.runtime_lock == NULL) {
        return -1;
    }

    xSemaphoreTake(s_policy.runtime_lock, portMAX_DELAY);
    if (s_policy.runtime_suspended == suspended) {
        xSemaphoreGive(s_policy.runtime_lock);
        return 0;
    }

    if (suspended) {
        LISA_LOGI(TAG, "Runtime suspend: audio begin");
        step_ret = app_wakeup_audio_standby_suspend();
        LISA_LOGI(TAG, "Runtime suspend: audio end ret=%d", step_ret);
        if (step_ret != 0) {
            LISA_LOGW(TAG, "Audio hibernate suspend reported: %d", step_ret);
            ret = step_ret;
        }

#if CONFIG_ACOMP_LOGGER
        step_ret = acomp_logger_stop();
        if (step_ret != 0) {
            LISA_LOGW(TAG, "ACOMP logger stop reported: %d", step_ret);
            if (ret == 0) {
                ret = step_ret;
            }
        }
#endif

#if CONFIG_WIFI
        step_ret = sys_wifi_set_standby_power_save(true);
        if (step_ret != 0) {
            LISA_LOGW(TAG, "WiFi hibernate power-save reported: %d", step_ret);
            if (ret == 0) {
                ret = step_ret;
            }
        }
#endif

#if CONFIG_BATTERY_COLLECTION
        battery_ui_set_sampling_suspended(true);
#endif

        step_ret = power_policy_set_core_clock(POWER_POLICY_HIBERNATE_CORE_CLOCK);
        if (step_ret != CSK_DRIVER_OK) {
            LISA_LOGW(TAG, "150 MHz core clock request reported: %d", step_ret);
            if (ret == 0) {
                ret = step_ret;
            }
        }

        s_policy.runtime_suspended = true;
        xSemaphoreGive(s_policy.runtime_lock);
        LISA_LOGI(TAG, "Runtime hibernate active: audio/logger/battery reduced, WiFi DTIM power-save, core 150 MHz");
        return ret;
    }

    LISA_LOGI(TAG, "Runtime resume: core clock begin");
    step_ret = power_policy_set_core_clock(POWER_POLICY_ACTIVE_CORE_CLOCK);
    LISA_LOGI(TAG, "Runtime resume: core clock end ret=%d", step_ret);
    if (step_ret != CSK_DRIVER_OK) {
        LISA_LOGW(TAG, "300 MHz core clock restore reported: %d", step_ret);
        ret = step_ret;
    }

    LISA_LOGI(TAG, "Runtime resume: audio begin");
    step_ret = app_wakeup_audio_standby_resume();
    LISA_LOGI(TAG, "Runtime resume: audio end ret=%d", step_ret);
    if (step_ret != 0) {
        LISA_LOGW(TAG, "Audio hibernate resume reported: %d", step_ret);
        if (ret == 0) {
            ret = step_ret;
        }
    }

#if CONFIG_WIFI
    step_ret = sys_wifi_set_standby_power_save(false);
    if (step_ret != 0) {
        LISA_LOGW(TAG, "WiFi hibernate power-save exit reported: %d", step_ret);
        if (ret == 0) {
            ret = step_ret;
        }
    }
#endif

#if CONFIG_ACOMP_LOGGER
    step_ret = acomp_logger_start();
    if (step_ret != 0) {
        LISA_LOGW(TAG, "ACOMP logger restart reported: %d", step_ret);
        if (ret == 0) {
            ret = step_ret;
        }
    }
#endif

#if CONFIG_BATTERY_COLLECTION
    battery_ui_set_sampling_suspended(false);
#endif

    s_policy.runtime_suspended = false;
    xSemaphoreGive(s_policy.runtime_lock);
    LISA_LOGI(TAG, "Runtime hibernate exited: core/audio/network monitoring restored");
    return ret;
}

static void power_policy_publish_state(service_power_policy_state_t state,
                                       service_power_policy_reason_t reason)
{
    voice_msg_power_policy_state_event_t event = {
        .state = (uint8_t)state,
        .reason = (uint8_t)reason,
    };

    voice_msg_pub(VOICE_MSG_POWER_POLICY_STATE_CHANGED, &event, sizeof(event));
}

static bool power_policy_transition(service_power_policy_state_t state,
                                    service_power_policy_reason_t reason)
{
    service_power_policy_state_t previous;

    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    previous = s_policy.state;
    if (previous == state) {
        xSemaphoreGive(s_policy.state_lock);
        return false;
    }

    s_policy.state = state;
    if (previous == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING &&
        state != VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
        s_policy.warning_waiting = false;
        s_policy.countdown_active = false;
    }
    xSemaphoreGive(s_policy.state_lock);

    if (state == VOICE_MSG_POWER_POLICY_STATE_NORMAL ||
        state == VOICE_MSG_POWER_POLICY_STATE_IDLE ||
        state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
        (void)service_power_policy_set_runtime_suspended(false);
    }

    LISA_LOGI(TAG, "State %s -> %s, reason=%u",
              power_policy_state_name(previous), power_policy_state_name(state),
              (unsigned int)reason);
    power_policy_publish_state(state, reason);
    return true;
}

static void power_policy_mark_activity(service_power_policy_reason_t reason)
{
    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    s_policy.last_activity_tick = xTaskGetTickCount();
    xSemaphoreGive(s_policy.state_lock);

    (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_NORMAL, reason);
}

service_power_policy_state_t service_power_policy_get_state(void)
{
    service_power_policy_state_t state = VOICE_MSG_POWER_POLICY_STATE_NORMAL;

    if (s_policy.state_lock == NULL) {
        return state;
    }

    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    state = s_policy.state;
    xSemaphoreGive(s_policy.state_lock);
    return state;
}

bool service_power_policy_handle_function_click(void)
{
    service_power_policy_state_t state = service_power_policy_get_state();

    if (state != VOICE_MSG_POWER_POLICY_STATE_HIBERNATE &&
        state != VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
        return false;
    }

    if (state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
        voice_player_tone_stop();
    }
    power_policy_mark_activity(VOICE_MSG_POWER_POLICY_REASON_BUTTON);
    LISA_LOGI(TAG, "First function-key click consumed to restore normal mode");
    return true;
}

static void power_policy_start_shutdown_warning(void)
{
    const char *url;
    bool queued;
    TickType_t now = xTaskGetTickCount();

    LISA_LOGI(TAG, "Shutdown warning: runtime resume begin");
    (void)service_power_policy_set_runtime_suspended(false);
    LISA_LOGI(TAG, "Shutdown warning: runtime resume end");

    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    s_policy.warning_waiting = true;
    s_policy.warning_start_tick = now;
    s_policy.countdown_active = false;
    xSemaphoreGive(s_policy.state_lock);

    url = app_tone_get_url(TONE_ID_95);
    LISA_LOGI(TAG, "Shutdown warning: tone id=%u url=%s",
              (unsigned int)TONE_ID_95, url != NULL ? url : "(null)");
    queued = voice_player_play_power_shutdown_tone_url(url);
    LISA_LOGI(TAG, "Shutdown warning: tone queued=%u", (unsigned int)queued);
    if (!queued) {
        LISA_LOGW(TAG, "Low-battery shutdown prompt unavailable, starting silent countdown");
        xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
        s_policy.warning_waiting = false;
        s_policy.countdown_active = true;
        s_policy.countdown_start_tick = now;
        xSemaphoreGive(s_policy.state_lock);
    }
}

static void power_policy_on_wakeup(void *unused, uint32_t msg_id, void *data,
                                   uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    power_policy_mark_activity(VOICE_MSG_POWER_POLICY_REASON_VOICE_WAKE);
}

static void power_policy_on_session(void *unused, uint32_t msg_id, void *data,
                                    uint32_t len, void *user_data)
{
    bool active = msg_id == VOICE_MSG_CLOUD_SESSION_STARTING;
    bool was_active;

    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    was_active = s_policy.interaction_active;
    s_policy.interaction_active = active;
    if (active || was_active) {
        s_policy.last_activity_tick = xTaskGetTickCount();
    }
    xSemaphoreGive(s_policy.state_lock);
    if (active) {
        (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_NORMAL,
                                      VOICE_MSG_POWER_POLICY_REASON_INTERACTION);
    }
}

static void power_policy_on_player(void *unused, uint32_t msg_id, void *data,
                                   uint32_t len, void *user_data)
{
    bool active;
    bool was_active;

    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    if (msg_id == VOICE_MSG_PLAYER_TTS_PLAYING ||
        msg_id == VOICE_MSG_PLAYER_TTS_STOPED) {
        active = msg_id == VOICE_MSG_PLAYER_TTS_PLAYING;
        was_active = s_policy.tts_active;
        s_policy.tts_active = active;
    } else {
        active = msg_id == VOICE_MSG_PLAYER_MUSIC_PLAYING;
        was_active = s_policy.music_active;
        s_policy.music_active = active;
    }
    if (active || was_active) {
        s_policy.last_activity_tick = xTaskGetTickCount();
    }
    xSemaphoreGive(s_policy.state_lock);

    if (active) {
        (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_NORMAL,
                                      VOICE_MSG_POWER_POLICY_REASON_INTERACTION);
    }
}

static void power_policy_on_button(void *unused, uint32_t msg_id, void *data,
                                   uint32_t len, void *user_data)
{
    const voice_msg_button_evt_t *event = data;
    service_power_policy_state_t state;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (event == NULL || len < sizeof(*event)) {
        return;
    }

    state = service_power_policy_get_state();
    if (state == VOICE_MSG_POWER_POLICY_STATE_HIBERNATE ||
        state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
        return;
    }

    power_policy_mark_activity(VOICE_MSG_POWER_POLICY_REASON_BUTTON);
}

static void power_policy_on_tone_completed(void *unused, uint32_t msg_id,
                                           void *data, uint32_t len,
                                           void *user_data)
{
    const voice_player_tone_completed_t *completed = data;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (completed == NULL || len < sizeof(*completed) ||
        completed->source != VOICE_PLAYER_TONE_SOURCE_POWER_SHUTDOWN) {
        return;
    }

    xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
    if (s_policy.state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
        s_policy.warning_waiting = false;
        s_policy.countdown_active = true;
        s_policy.countdown_start_tick = xTaskGetTickCount();
        LISA_LOGI(TAG, "Low-battery prompt completed, silent shutdown countdown started");
    }
    xSemaphoreGive(s_policy.state_lock);
}

static void power_policy_task(void *arg)
{
    (void)arg;

    while (1) {
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed;
        TickType_t warning_elapsed;
        TickType_t countdown_elapsed;
        service_power_policy_state_t state;
        bool busy;
        bool warning_waiting;
        bool countdown_active;
        bool external_power = battery_usb_plugged_stable_get();
        uint8_t battery_pct = 0U;

        xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
        state = s_policy.state;
        busy = s_policy.interaction_active || s_policy.tts_active || s_policy.music_active;
        elapsed = now - s_policy.last_activity_tick;
        warning_waiting = s_policy.warning_waiting;
        warning_elapsed = now - s_policy.warning_start_tick;
        countdown_active = s_policy.countdown_active;
        countdown_elapsed = now - s_policy.countdown_start_tick;
        xSemaphoreGive(s_policy.state_lock);

        if ((state == VOICE_MSG_POWER_POLICY_STATE_HIBERNATE ||
             state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) &&
            external_power) {
            if (state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
                voice_player_tone_stop();
            }
            (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_IDLE,
                                          VOICE_MSG_POWER_POLICY_REASON_EXTERNAL_POWER);
            vTaskDelay(pdMS_TO_TICKS(POWER_POLICY_POLL_INTERVAL_MS));
            continue;
        }

        if (state == VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING) {
            if (warning_waiting &&
                warning_elapsed >= pdMS_TO_TICKS(POWER_POLICY_WARNING_WATCHDOG_MS)) {
                LISA_LOGW(TAG, "Shutdown prompt completion timed out, starting silent countdown");
                xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
                s_policy.warning_waiting = false;
                s_policy.countdown_active = true;
                s_policy.countdown_start_tick = now;
                xSemaphoreGive(s_policy.state_lock);
            } else if (countdown_active &&
                       countdown_elapsed >= pdMS_TO_TICKS(POWER_POLICY_SHUTDOWN_COUNTDOWN_MS)) {
                if (power_is_usb_plugged()) {
                    voice_player_tone_stop();
                    (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_IDLE,
                                                  VOICE_MSG_POWER_POLICY_REASON_EXTERNAL_POWER);
                } else {
                    LISA_LOGI(TAG, "Low-battery shutdown countdown elapsed");
                    power_shutdown();
                }
            }

            vTaskDelay(pdMS_TO_TICKS(POWER_POLICY_POLL_INTERVAL_MS));
            continue;
        }

        if (state == VOICE_MSG_POWER_POLICY_STATE_HIBERNATE &&
            !busy && !external_power &&
            elapsed >= pdMS_TO_TICKS(POWER_POLICY_SHUTDOWN_TIMEOUT_MS)) {
            battery_pct = battery_get_pct_raw();
            LISA_LOGI(TAG,
                      "Shutdown condition reached: elapsed=%ums battery=%u%% external=%u busy=%u",
                      (unsigned int)(elapsed * portTICK_PERIOD_MS),
                      (unsigned int)battery_pct,
                      (unsigned int)external_power,
                      (unsigned int)busy);
            if (battery_pct <= POWER_POLICY_SHUTDOWN_BATTERY_THRESHOLD_PCT &&
                power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING,
                                        VOICE_MSG_POWER_POLICY_REASON_LOW_BATTERY)) {
                power_policy_start_shutdown_warning();
            }
        } else if (!busy && !external_power &&
                   elapsed >= pdMS_TO_TICKS(POWER_POLICY_HIBERNATE_TIMEOUT_MS)) {
            (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_HIBERNATE,
                                          VOICE_MSG_POWER_POLICY_REASON_HIBERNATE_TIMEOUT);
        } else if (!busy &&
                   elapsed >= pdMS_TO_TICKS(POWER_POLICY_IDLE_TIMEOUT_MS)) {
            (void)power_policy_transition(VOICE_MSG_POWER_POLICY_STATE_IDLE,
                                          VOICE_MSG_POWER_POLICY_REASON_IDLE_TIMEOUT);
        }

        vTaskDelay(pdMS_TO_TICKS(POWER_POLICY_POLL_INTERVAL_MS));
    }
}

int service_power_policy_init(void)
{
    BaseType_t task_ret;
    int ret = 0;

    if (s_policy.initialized) {
        return 0;
    }

    s_policy.state_lock = xSemaphoreCreateMutex();
    s_policy.runtime_lock = xSemaphoreCreateMutex();
    if (s_policy.state_lock == NULL || s_policy.runtime_lock == NULL) {
        LISA_LOGE(TAG, "Failed to create power policy locks");
        return -1;
    }

    s_policy.state = VOICE_MSG_POWER_POLICY_STATE_NORMAL;
    s_policy.last_activity_tick = xTaskGetTickCount();

    ret |= voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, power_policy_on_wakeup, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, power_policy_on_session, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, power_policy_on_session, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAYER_TTS_PLAYING, power_policy_on_player, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAYER_TTS_STOPED, power_policy_on_player, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAYER_MUSIC_PLAYING, power_policy_on_player, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAYER_MUSIC_STOPPED, power_policy_on_player, NULL);
    ret |= voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, power_policy_on_button, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAYER_TONE_COMPLETED,
                         power_policy_on_tone_completed, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to subscribe power policy messages: %d", ret);
        return ret;
    }

    task_ret = xTaskCreate(power_policy_task, "power_policy",
                           POWER_POLICY_TASK_STACK_SIZE, NULL,
                           POWER_POLICY_TASK_PRIORITY, NULL);
    if (task_ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create power policy task");
        return -1;
    }

    s_policy.initialized = true;
    power_policy_publish_state(s_policy.state, VOICE_MSG_POWER_POLICY_REASON_INIT);
    LISA_LOGI(TAG,
              "Initialized: idle=30s, hibernate=5min, shutdown=15min total "
              "(hibernate+10min), "
              "low-battery threshold=%u%%",
              (unsigned int)POWER_POLICY_SHUTDOWN_BATTERY_THRESHOLD_PCT);
    return 0;
}

#if CONFIG_LISA_SHELL
static int power_policy_shell_cmd(int argc, char **argv)
{
    app_wakeup_audio_diag_t audio_diag;
    const char *url;
    bool queued;
    service_power_policy_state_t state;
    TickType_t elapsed;

    if (argc < 2) {
        printf("usage: power_policy status|tone|cycle\n");
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) {
        app_wakeup_audio_diag_get(&audio_diag);
        xSemaphoreTake(s_policy.state_lock, portMAX_DELAY);
        state = s_policy.state;
        elapsed = xTaskGetTickCount() - s_policy.last_activity_tick;
        xSemaphoreGive(s_policy.state_lock);
        printf("state=%s elapsed_ms=%u external=%u battery=%u runtime_suspended=%u\n",
               power_policy_state_name(state),
               (unsigned int)(elapsed * portTICK_PERIOD_MS),
               (unsigned int)battery_usb_plugged_stable_get(),
               (unsigned int)battery_get_pct_raw(),
               (unsigned int)s_policy.runtime_suspended);
        printf("audio wake=%u record=%u input_fail=%u output_buf=%u output_bytes=%u "
               "cloud_frames=%u non_silent=%u send_fail=%u peak=%u\n",
               (unsigned int)audio_diag.wake_count,
               (unsigned int)audio_diag.record_events_since_wake,
               (unsigned int)audio_diag.input_submit_failures_since_wake,
               (unsigned int)audio_diag.output_buffers_since_wake,
               (unsigned int)audio_diag.output_bytes_since_wake,
               (unsigned int)audio_diag.cloud_frames_since_wake,
               (unsigned int)audio_diag.cloud_non_silent_frames_since_wake,
               (unsigned int)audio_diag.cloud_send_failures_since_wake,
               (unsigned int)audio_diag.last_cloud_peak);
        printf("audio_power suspend=%u resume=%u resume_fail=%u adc_low=%u "
               "ref_off=%u playback_off=%u interaction=%u tts=%u music=%u\n",
               (unsigned int)audio_diag.suspend_count,
               (unsigned int)audio_diag.resume_count,
               (unsigned int)audio_diag.resume_failures,
               (unsigned int)audio_diag.adc_low_power,
               (unsigned int)audio_diag.reference_channel_off,
               (unsigned int)audio_diag.playback_off,
               (unsigned int)s_policy.interaction_active,
               (unsigned int)s_policy.tts_active,
               (unsigned int)s_policy.music_active);
        return 0;
    }

    if (strcmp(argv[1], "cycle") == 0) {
        power_policy_mark_activity(VOICE_MSG_POWER_POLICY_REASON_INTERACTION);
        printf("power_policy audio cycle: suspend\n");
        (void)service_power_policy_set_runtime_suspended(true);
        vTaskDelay(pdMS_TO_TICKS(1000U));
        printf("power_policy audio cycle: resume\n");
        (void)service_power_policy_set_runtime_suspended(false);
    } else if (strcmp(argv[1], "tone") == 0) {
        power_policy_mark_activity(VOICE_MSG_POWER_POLICY_REASON_INTERACTION);
    } else {
        printf("usage: power_policy status|tone|cycle\n");
        return -1;
    }

    url = app_tone_get_url(TONE_ID_95);
    queued = voice_player_play_power_shutdown_tone_url(url);
    printf("power_policy tone: url=%s queued=%u\n",
           url != NULL ? url : "(null)", (unsigned int)queued);
    return queued ? 0 : -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 power_policy, power_policy_shell_cmd,
                 power policy diagnostic commands);
#endif
