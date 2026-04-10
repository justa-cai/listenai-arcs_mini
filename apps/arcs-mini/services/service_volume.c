#include <stdbool.h>
#include <stdint.h>

#define TAG "volume"

#include "lisa_log.h"
#include "lisa_kv.h"
#include "kv.h"
#include "service_volume.h"
#include "voice_player_comm.h"

#define DEFAULT_VOLUME 50

static int service_volume_clamp(int volume)
{
    if (volume < 10) {
        volume = 10;
    }
    if (volume > 100) {
        volume = 100;
    }

    return volume;
}

static void service_volume_apply(int volume)
{
    int failed_count = 0;

    volume = service_volume_clamp(volume);

    if (tone_player != NULL && app_player_set_volume(tone_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (tts_player != NULL && app_player_set_volume(tts_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (music_player != NULL && app_player_set_volume(music_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (alert_player != NULL && app_player_set_volume(alert_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (failed_count > 0) {
        LOGW("Failed to apply volume for %d player(s)", failed_count);
    }
}

void service_volume_init(void)
{
    service_volume_restore_from_kv();

    LOGI("Volume service initialized");
}

void service_volume_set(int volume)
{
    volume = service_volume_clamp(volume);

    lisa_kv_set_int(KV_KEY_USER_VOLUME, volume);
    service_volume_apply(volume);

    LOGI("Volume set to %d", volume);
}

void service_volume_set_temp(int volume)
{
    volume = service_volume_clamp(volume);
    service_volume_apply(volume);

    LOGI("Volume temporarily set to %d", volume);
}

void service_volume_restore_from_kv(void)
{
    int volume = service_volume_get();

    service_volume_apply(volume);
    LOGI("Volume restored from kv to %d", volume);
}

int service_volume_get(void)
{
    int volume = DEFAULT_VOLUME;

    if (lisa_kv_get_int(KV_KEY_USER_VOLUME, &volume) != 0) {
        volume = DEFAULT_VOLUME;
    }

    return service_volume_clamp(volume);
}

void service_volume_adjust(int delta)
{
    int current_vol = service_volume_get();
    int target_vol = current_vol + delta;

    service_volume_set(target_vol);
}
