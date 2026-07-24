#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#define TAG "button.factory_reset"

#include "app_player.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "power/power_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "voice_player_comm.h"
#include "voice_player/tone_control/voice_player_tone.h"

#include "button_factory_reset.h"

#define FACTORY_RESET_TONE_POLL_MS 20U
#define FACTORY_RESET_TONE_WAIT_MS 10000U

static bool factory_reset_tone_is_active(void)
{
    switch (app_player_get_state(tone_player)) {
    case APP_PLAYER_STATE_PREPARING:
    case APP_PLAYER_STATE_PREPARED:
    case APP_PLAYER_STATE_PLAYING:
    case APP_PLAYER_STATE_PAUSED:
        return true;
    default:
        return false;
    }
}

void button_factory_reset(void)
{
    const char *tone_url;

    LOGI("factory reset running...");
    if (lisa_kv_clear() != 0) {
        LISA_LOGW(TAG, "Failed to clear KV storage");
    }
    if (sys_wifi_clear_saved_aps() != 0) {
        LISA_LOGW(TAG, "Failed to reset WiFi state");
    }
    LOGI("factory reset done.");

    voice_player_tone_stop();
    tone_url = app_tone_get_url(TONE_ID_103);
    if (tone_url != NULL &&
        app_player_play(tone_player, tone_url) == APP_PLAYER_OK) {
        TickType_t wait_start = xTaskGetTickCount();

        while (factory_reset_tone_is_active() &&
               xTaskGetTickCount() - wait_start <
                   pdMS_TO_TICKS(FACTORY_RESET_TONE_WAIT_MS)) {
            vTaskDelay(pdMS_TO_TICKS(FACTORY_RESET_TONE_POLL_MS));
        }
    } else {
        LISA_LOGW(TAG, "Failed to play factory reset tone");
    }

    LOGI("Factory reset completed, reboot now.");
    power_reboot_soft();
}
