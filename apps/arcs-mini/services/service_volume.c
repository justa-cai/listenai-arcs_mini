#include <stdbool.h>
#include <stdint.h>

#define TAG "volume"

#include "lisa_log.h"
#include "lisa_kv.h"
#include "kv.h"
#include "player_mgr.h"

#define DEFAULT_VOLUME 50

void service_volume_set(int volume);

void service_volume_init(void)
{
    int volume = DEFAULT_VOLUME;
    if (lisa_kv_get_int(KV_KEY_USER_VOLUME, &volume) == 0) {
        service_volume_set(volume);
    }

    LOGI("Volume service initialized");
}

void service_volume_set(int volume)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 100) {
        volume = 100;
    }

    lisa_kv_set_int(KV_KEY_USER_VOLUME, volume);
    player_mgr_set_volume(volume);

    LOGI("Volume set to %d", volume);
}

int service_volume_get(void)
{
    int volume = DEFAULT_VOLUME;

    if (lisa_kv_get_int(KV_KEY_USER_VOLUME, &volume) != 0) {
        volume = DEFAULT_VOLUME;
    }

    return volume;
}

void service_volume_adjust(int delta)
{
    int current_vol = service_volume_get();
    int target_vol = current_vol + delta;

    service_volume_set(target_vol);
}
