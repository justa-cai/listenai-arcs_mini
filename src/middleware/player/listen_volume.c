#include "listen_volume.h"
#include "lisa_mutex.h"
#include "app_player.h"
#include "lisa_typedef.h"
#include "kv.h"
#include "lisa_kv.h"

static lisa_mutex_t *s_mutex = NULL;

static int s_vol = 100;
static bool s_mute = false;

void listen_volume_init()
{
	s_mutex = lisa_mutex_create();
	if (!s_mutex) return;
	int r;

	r = lisa_kv_get_int(KV_KEY_USER_VOLUME, &s_vol);
	if (r != 0) {
		s_vol = 70;
	} else {
		if (s_vol > 100) {
			s_vol = 100;
		}

		if (s_vol < 0) {
			s_vol = 0;
		}
	}

    // 设置初始化音量
	listen_set_volume(s_vol);
}

void listen_set_volume(int vol)
{
	if (!s_mutex) return;

	lisa_mutex_lock(s_mutex, LISA_OS_WAIT_FOREVER);
	vol = vol > 100 ? 100 : vol;
	vol = vol < 1 ? 1 : vol;
	app_player_volume(PLAYER_T_CLOUD, vol);
	app_player_volume(PLAYER_T_TONE, vol);
	s_vol = vol;

	lisa_kv_set_int(KV_KEY_USER_VOLUME, s_vol);

	lisa_mutex_unlock(s_mutex);
}

void listen_vol_adjust(int vol)
{
	if (!s_mutex) return;
	int target_vol = s_vol + vol;
	listen_set_volume(target_vol);
}

void listen_vol_mute()
{
	s_mute = true;
	app_player_volume(PLAYER_T_CLOUD, 0);
	app_player_volume(PLAYER_T_TONE, 0);
}

void listen_vol_cancel_mute()
{
	s_mute = false;
	app_player_volume(PLAYER_T_CLOUD, s_vol);
	app_player_volume(PLAYER_T_TONE, s_vol);
}

int listen_get_volume()
{
	return s_vol;
}
