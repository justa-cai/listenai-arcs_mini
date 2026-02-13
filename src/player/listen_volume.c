#include "listen_volume.h"
#include "lisa_mutex.h"
#include "app_player.h"
#include "lisa_typedef.h"
#include "kv.h"
#include "lisa_kv.h"
#include "listen_mic_gain.h"

#define SPK_PCT_CLAMP(pct) ((pct) < 1 ? 1 : ((pct) > 100 ? 100 : (pct)))

static lisa_mutex_t *s_mutex = NULL;

static bool s_mute = false;

void listen_volume_init(void)
{
    int vol;

    s_mutex = lisa_mutex_create();
    if (!s_mutex) {
        return;
    }

    vol = listen_get_volume();

    // 设置初始化音量
    app_player_volume(PLAYER_T_CLOUD, vol);
    app_player_volume(PLAYER_T_TONE, vol);

    // 基于音量调整aec增益
    listen_update_aec_by_volume(vol);
}

void listen_set_volume(int vol)
{
    if (!s_mutex) {
        return;
    }

    lisa_mutex_lock(s_mutex, LISA_OS_WAIT_FOREVER);

    vol = SPK_PCT_CLAMP(vol);
    app_player_volume(PLAYER_T_CLOUD, vol);
    app_player_volume(PLAYER_T_TONE, vol);

    lisa_kv_set_int(KV_KEY_USER_VOLUME, vol);

    lisa_mutex_unlock(s_mutex);

    // 基于音量调整aec增益
    listen_update_aec_by_volume(vol);
}

int listen_get_volume(void)
{
    int vol = 0;
    if (lisa_kv_get_int(KV_KEY_USER_VOLUME, &vol) != 0) {
        vol = CONFIG_DEFAULT_VOLUME;
    }
    return SPK_PCT_CLAMP(vol);
}

void listen_vol_adjust(int adj)
{
    if (!s_mutex) {
        return;
    }
    int target_vol = listen_get_volume() + adj;
    listen_set_volume(target_vol);
}

void listen_vol_mute(void)
{
    s_mute = true;
    app_player_volume(PLAYER_T_CLOUD, 0);
    app_player_volume(PLAYER_T_TONE, 0);
}

void listen_vol_cancel_mute(void)
{
    int vol = listen_get_volume();
    s_mute = false;
    app_player_volume(PLAYER_T_CLOUD, vol);
    app_player_volume(PLAYER_T_TONE, vol);
}
