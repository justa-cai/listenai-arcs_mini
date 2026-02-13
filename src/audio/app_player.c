#define TAG "APP_PLAYER"

#include <stdio.h>
#include <string.h>
#include "lisa_mem.h"
#include "lisa_log.h"
#include "lisa_player.h"
#include "app_player.h"
#include "pa_manager.h"
#include "lisa_mutex.h"
#include "lisa_typedef.h"
#include "lisa_semaphore.h"

#define TONE_ID_TAG "mem://"

typedef struct status_cb_map {
	lisa_mutex_t *lock;
	player_status_cb pre_cb;
	uint32_t pre_id;
	player_status_cb cur_cb;
	uint32_t cur_id;
} status_cb_map_t;

typedef struct app_player_item_s {
	/** 播放器句柄 */
	PLAYER_HANDLE hld;
	/** 播放ID */
	uint32_t playid;
	/** 播放器回调 */
	status_cb_map_t cb_map;
	/** 播放器 Prepareing Flag */
	bool is_preparing;
	/** 播放器 Wait Prepare Intercepte Flag */
	bool wait_prepare_intercepted;
	/** 播放器 Force Pause when Preparing */
	bool pause_preparing;
	/** 适配 Preparing 等待 */
	lisa_semaphore_t *preparing_sem;
	/** Player Evt */
	PlayerEvt evt;
} app_player_item_t;

typedef struct app_player_s {
	/** 提示音播放器 */
	app_player_item_t tone_player;
	/** 音乐播放器 */
	app_player_item_t audio_player;
} app_player_t;

static app_player_t *s_app_player = NULL;

#if 1
#define APPLICATION_PLAY_RB_LEFT_SHIFT_BIT 	(12)
#define APPLICATION_PLAY_ONE_FRAME_SIZE 	(512)
#define APPLICATION_PLAY_SAMPLERATE 		(16000)
#define APPLICATION_PLAY_SAMPLERATE_ENUM 	(1) // for samplerate_16000
int lisaplayer_track_samplerate_hook(unsigned char *samplerate_enum, uint16_t *one_frame_size, uint16_t *ringbuffer_shift_bit)
{
	LISA_LOGI(TAG, "Application samplerate: %d, index: %d, one frame size: %d, left shift: %d",
						APPLICATION_PLAY_SAMPLERATE,
						APPLICATION_PLAY_SAMPLERATE_ENUM,
						APPLICATION_PLAY_ONE_FRAME_SIZE,
						APPLICATION_PLAY_RB_LEFT_SHIFT_BIT);

	*samplerate_enum = APPLICATION_PLAY_SAMPLERATE_ENUM;
	*one_frame_size = APPLICATION_PLAY_ONE_FRAME_SIZE;
	*ringbuffer_shift_bit = APPLICATION_PLAY_RB_LEFT_SHIFT_BIT;
	return APPLICATION_PLAY_SAMPLERATE;
}
#endif

static void __player_cache_callback(app_player_item_t *player_item, player_status_cb cb, uint32_t id)
{
	if (!player_item) return;

	lisa_mutex_lock(player_item->cb_map.lock, LISA_OS_WAIT_FOREVER);

	if (player_item->cb_map.pre_cb == NULL || player_item->cb_map.pre_id == 0)
	{
		LISA_LOGV(TAG, "cache callback to pre by %d %p", id, cb);
		player_item->cb_map.pre_cb = cb;
		player_item->cb_map.pre_id = id;
	}
	else
	{
		LISA_LOGV(TAG, "cache callback to cur by %d %p", id, cb);
		player_item->cb_map.cur_cb = cb;
		player_item->cb_map.cur_id = id;
	}

	lisa_mutex_unlock(player_item->cb_map.lock);
}

static void __player_broadcast_status(app_player_item_t *player_item, PlayerEvt st)
{
	if (!player_item) return;

	lisa_mutex_lock(player_item->cb_map.lock, LISA_OS_WAIT_FOREVER);

	if (player_item->cb_map.pre_cb != NULL)
	{
		LISA_LOGV(TAG, "pre callback with %d %p", player_item->cb_map.pre_id, player_item->cb_map.pre_cb);
		player_item->cb_map.pre_cb(st);

		if (st == PLAYER_EVT_STOPED || st == PLAYER_EVT_PLAYBACK_COMPLETE || st == PLAYER_EVT_ERROR)
		{
			LISA_LOGV(TAG, "move cur: %d %p to pre: %d %p",
								player_item->cb_map.cur_id,
								player_item->cb_map.cur_cb,
								player_item->cb_map.pre_id,
								player_item->cb_map.pre_cb);
			player_item->cb_map.pre_cb = player_item->cb_map.cur_cb;
			player_item->cb_map.pre_id = player_item->cb_map.cur_id;
			player_item->cb_map.cur_cb = NULL;
			player_item->cb_map.cur_id = 0;
		}
	}
	else
	{
		LISA_LOGE(TAG, "pre callback null");
	}

	lisa_mutex_unlock(player_item->cb_map.lock);
}

/**
 * @brief   提示音播放器状态回调
 * @param   evt     播放器事件
 * @param   arg1    参数1
 * @param   arg2    参数2
 * @param   id
 * @return
 */
static int _tone_player_callback(PlayerEvt evt, int arg1, int arg2, int id)
{
	app_player_item_t *player = &(s_app_player->tone_player);
	bool ignore_play = false;
	player->is_preparing = false;
	if (player->wait_prepare_intercepted) {
		ignore_play = true;
		lisa_semaphore_give(player->preparing_sem);
	}
	LISA_LOGI(TAG, "tone player evt %d", evt);
	switch (evt) {
		case PLAYER_EVT_PREPARED: {
			if (!ignore_play) {
				if (!player->pause_preparing) {
					if (lisa_player_play(player->hld) == PLAYER_OP_FAIL) {
						__player_broadcast_status(player, PLAYER_EVT_ERROR);
					}
				} else {
					LISA_LOGD(TAG, "tone player has pause when preparing");
					__player_broadcast_status(player, PLAYER_EVT_PAUSED);
				}
			}
			return 0;
		} break;
		case PLAYER_EVT_PAUSED:
		case PLAYER_EVT_STOPED:
		case PLAYER_EVT_PLAYBACK_COMPLETE:
		case PLAYER_EVT_ERROR:
			if (player->evt == PLAYER_EVT_STOPED && evt == PLAYER_EVT_ERROR) {
				// 打断时(Stop), 先来Stop情况下, 再来Error, 不需要关闭PA
			} else {
				pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "tone_player_end");
			}
		case PLAYER_EVT_PLAYING: {
			__player_broadcast_status(player, evt);

			if (evt == PLAYER_EVT_ERROR) {
				lisa_player_reset(player->hld);
			}
		} break;
		default:
			break;
	}
	player->evt = evt;
	return 0;
}

/**
 * @brief   音乐播放器状态回调
 * @param   evt     播放器事件
 * @param   arg1    参数1
 * @param   arg2    参数2
 * @param   id
 * @return
 */
static int _audio_player_callback(PlayerEvt evt, int arg1, int arg2, int id)
{
	app_player_item_t *player = &(s_app_player->audio_player);
	bool ignore_play = false;
	player->is_preparing = false;
	if (player->wait_prepare_intercepted) {
		ignore_play = true;
		lisa_semaphore_give(player->preparing_sem);
	}

	LISA_LOGI(TAG, "audio player evt %d", evt);
	switch (evt) {
		case PLAYER_EVT_PREPARED: {
			if (!ignore_play) {
				if (!player->pause_preparing) {
					lisa_player_play(player->hld);
				} else {
					LISA_LOGD(TAG, "audio player has pause when preparing");
					__player_broadcast_status(player, PLAYER_EVT_PAUSED);
				}
			}
			return 0;
		} break;
		case PLAYER_EVT_PAUSED:
		case PLAYER_EVT_STOPED:
		case PLAYER_EVT_PLAYBACK_COMPLETE:
		case PLAYER_EVT_ERROR:
			pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "audio_player_end");
		case PLAYER_EVT_PLAYING: {
			__player_broadcast_status(player, evt);

			if (evt == PLAYER_EVT_ERROR) {
				lisa_player_reset(player->hld);
			}
		} break;
		default:
			break;
	}
	player->evt = evt;
	return 0;
}

void app_player_init()
{
	if (s_app_player) return;

	LISA_LOGD(TAG, "App Player init [in]");
	s_app_player = lisa_mem_calloc(1, sizeof(app_player_t));
	LISA_ASSERT(s_app_player, "no mem");

	lisa_player_set_loglev(2);

	printk("Lisa Player Version %s\n", lisa_player_get_version());

	// 创建提示音播放器
	s_app_player->tone_player.hld = lisa_player_create("toneplayer", 0);
	s_app_player->tone_player.cb_map.lock = lisa_mutex_create();
	s_app_player->tone_player.cb_map.pre_cb = NULL;
	s_app_player->tone_player.cb_map.pre_id = 0;
	s_app_player->tone_player.cb_map.cur_cb = NULL;
	s_app_player->tone_player.cb_map.cur_id = 0;
	s_app_player->tone_player.is_preparing = false;
	s_app_player->tone_player.wait_prepare_intercepted = false;
	s_app_player->tone_player.pause_preparing = false;
	s_app_player->tone_player.preparing_sem = lisa_semaphore_create(1);
	s_app_player->tone_player.evt = PLAYER_EVT_ERROR;
	LISA_ASSERT(s_app_player->tone_player.preparing_sem, "creat sem fail");
	// 设置提示音播放器回调函数
	lisa_player_set_callback(s_app_player->tone_player.hld, _tone_player_callback);

	// 创建音乐播放器
	s_app_player->audio_player.hld = lisa_player_create("audioplayer", 0);
	s_app_player->audio_player.cb_map.lock = lisa_mutex_create();
	s_app_player->audio_player.cb_map.pre_cb = NULL;
	s_app_player->audio_player.cb_map.pre_id = 0;
	s_app_player->audio_player.cb_map.cur_cb = NULL;
	s_app_player->audio_player.cb_map.cur_id = 0;
	s_app_player->audio_player.is_preparing = false;
	s_app_player->audio_player.wait_prepare_intercepted = false;
	s_app_player->audio_player.pause_preparing = false;
	s_app_player->audio_player.preparing_sem = lisa_semaphore_create(1);
	s_app_player->audio_player.evt = PLAYER_EVT_ERROR;
	LISA_ASSERT(s_app_player->audio_player.preparing_sem, "creat sem fail");
	// 设置音乐播放器回调函数
	lisa_player_set_callback(s_app_player->audio_player.hld, _audio_player_callback);

	LISA_LOGD(TAG, "App Player init [out]");
}

/**
 * @brief   检查 Preparing 状态
 * @param   player_item      Player Pointer
 * @param   name             Player Name
 * @return  true    Continue
 * @return  false   Intercepte
 */
static bool __player_prepare_check(app_player_item_t *player_item, const char *tips)
{
	if (player_item->is_preparing) {
		// is_preparing 判断完后打印后, 可能引起时间片由player回调线程调度
		LISA_LOGD(TAG, "wait %s prepare complete...", tips);
		lisa_player_pre_close(player_item->hld);
		// 回调线程执行后, is_preparing 会被置为false
		// 因此此处信号量可能会一直等待
		if (player_item->is_preparing) {
			player_item->wait_prepare_intercepted = true;
			lisa_semaphore_take(player_item->preparing_sem, LISA_OS_WAIT_FOREVER);
			player_item->wait_prepare_intercepted = false;
		}
		LISA_LOGD(TAG, "pre %s prepare complete", tips);
		PlayerState st = lisa_player_get_state(player_item->hld);
		if (st == PLAYER_ST_READY_TO_PLAY || st == PLAYER_ST_PREPARED ||
			st == PLAYER_ST_PLAYING || st == PLAYER_ST_PAUSED) {
			lisa_player_stop_sync(player_item->hld);
			lisa_player_reset(player_item->hld);
		} else {
			LISA_LOGW(TAG, "skip sync stop/reset, state=%d", st);
			lisa_player_stop(player_item->hld);
		}
		LISA_LOGD(TAG, "pre %s status check end", tips);
		return false;
	}
	return true;
}

void app_player_play(player_t type, char *url, player_status_cb cb)
{
	app_player_play_by_throw(type, url, 0, cb);
}

void app_player_play_by_throw(player_t type, char *url, int throw_time_ms, player_status_cb cb)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	pa_manager_refresh(PA_MGR_ON, LS_PA_FOREVER, "player play");
	PlayerErr ret = PLAYER_OK;

	if (type == PLAYER_T_TONE) {
		app_player_item_t *player = &s_app_player->tone_player;

		if(__player_prepare_check(player, "tone_play")) {
			// Reset Tone Player
			app_player_reset(PLAYER_T_TONE);
		}

		player->playid++;
		__player_cache_callback(player, cb, player->playid);

		if (cb) cb(APP_PLAYER_PREPARING);

		if ((strncmp(TONE_ID_TAG, url, strlen(TONE_ID_TAG)) == 0) ||
			(strncmp("http://", url, 7) == 0) ||
			(strncmp("https://", url, 8) == 0)) {
			LISA_LOGI(TAG, "play audio %s", url);
			lisa_player_throw_low_energy(player->hld, throw_time_ms);
			player->is_preparing = true;
			ret = lisa_player_seturl(player->hld, url);
			if (ret != PLAYER_OK) {
				player->is_preparing = false;
			}
		}

		player->pause_preparing = false;
	} else if (type == PLAYER_T_CLOUD) {
		app_player_item_t *player = &s_app_player->audio_player;

		if(__player_prepare_check(player, "audio_play")) {
			// Reset Cloud Player
			app_player_reset(PLAYER_T_CLOUD);
		}

		player->playid++;
		__player_cache_callback(player, cb, player->playid);

		if (cb) cb(APP_PLAYER_PREPARING);

		if (strncmp("http", url, 4) == 0) {
			lisa_player_throw_low_energy(player->hld, throw_time_ms);
			player->is_preparing = true;
			ret = lisa_player_seturl(player->hld, url);
			if (ret != PLAYER_OK) {
				player->is_preparing = false;
			}
		}

		player->pause_preparing = false;
	}
}

void app_player_pause(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "player pause");

	app_player_item_t *player = NULL;
	if (type == PLAYER_T_TONE) {
		player = &s_app_player->tone_player;
	} else if (type == PLAYER_T_CLOUD) {
		player = &s_app_player->audio_player;
	}

	if (player)
	{
		if (player->is_preparing) {
			player->pause_preparing = true;
		} else {
			lisa_player_pause(player->hld);
		}
	}
}

void app_player_resume(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	pa_manager_refresh(PA_MGR_ON, LS_PA_FOREVER, "player resume");

	app_player_item_t *player = NULL;
	if (type == PLAYER_T_TONE) {
		player = &s_app_player->tone_player;
	} else if (type == PLAYER_T_CLOUD) {
		player = &s_app_player->audio_player;
	}

	if (player)
	{
		if (player->pause_preparing) {
			player->pause_preparing = false;
			lisa_player_play(player->hld);
		} else {
			lisa_player_resume(player->hld);
		}
	}
}

void app_player_resume_sync(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	pa_manager_refresh(PA_MGR_ON, LS_PA_FOREVER, "player resume sync");

	app_player_item_t *player = NULL;
	if (type == PLAYER_T_TONE) {
		player = &s_app_player->tone_player;
	} else if (type == PLAYER_T_CLOUD) {
		player = &s_app_player->audio_player;
	}

	if (player)
	{
		if (player->pause_preparing) {
			player->pause_preparing = false;
			lisa_player_play(player->hld);
		} else {
			lisa_player_resume_sync(player->hld);
		}
	}
}

void app_player_stop(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	app_player_item_t *player = NULL;
	char *tips = "none";
	if (type == PLAYER_T_TONE) {
		player = &s_app_player->tone_player;
		tips = "tone_stop";
	} else if (type == PLAYER_T_CLOUD) {
		player = &s_app_player->audio_player;
		tips = "audio_stop";
	}

	pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, tips);

	if (player)
	{
		if (__player_prepare_check(player, tips)) {
			lisa_player_stop(player->hld);
		}
	}
}

void app_player_stop_sync(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	app_player_item_t *player = NULL;
	char *tips = "none";
	if (type == PLAYER_T_TONE) {
		player = &s_app_player->tone_player;
		tips = "tone_stop_sync";
	} else if (type == PLAYER_T_CLOUD) {
		player = &s_app_player->audio_player;
		tips = "audio_stop_sync";
	}

	pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, tips);

	if (__player_prepare_check(player, tips)) {
		PlayerState st = lisa_player_get_state(player->hld);
		int ret = PLAYER_OK;
		if (st == PLAYER_ST_READY_TO_PLAY || st == PLAYER_ST_PREPARED ||
			st == PLAYER_ST_PLAYING || st == PLAYER_ST_PAUSED) {
			ret = lisa_player_stop_sync(player->hld);
		} else {
			LISA_LOGW(TAG, "skip sync stop, state=%d", st);
			ret = lisa_player_stop(player->hld);
		}
		if (ret != PLAYER_OK)
		{
			__player_broadcast_status(player, PLAYER_EVT_STOPED);
		}
	} else {
		__player_broadcast_status(player, PLAYER_EVT_STOPED);
	}
}

void app_player_seek(player_t type, uint32_t seek_ms)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	if (type == PLAYER_T_TONE) {
		lisa_player_seek(s_app_player->tone_player.hld, seek_ms);
	} else if (type == PLAYER_T_CLOUD) {
		lisa_player_seek(s_app_player->audio_player.hld, seek_ms);
	}
}

uint32_t app_player_position(player_t type)
{
	if (!s_app_player) return 0;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return 0;

	if (type == PLAYER_T_TONE) {
		return lisa_player_get_pos(s_app_player->tone_player.hld);
	} else if (type == PLAYER_T_CLOUD) {
		return lisa_player_get_pos(s_app_player->audio_player.hld);
	}
	return 0;
}

uint32_t app_player_duration(player_t type)
{
	if (!s_app_player) return 0;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return 0;

	if (type == PLAYER_T_TONE) {
		return lisa_player_get_duration(s_app_player->tone_player.hld);
	} else if (type == PLAYER_T_CLOUD) {
		return lisa_player_get_duration(s_app_player->audio_player.hld);
	}
	return 0;
}

void app_player_volume(player_t type, uint8_t volume)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	app_player_item_t *player = NULL;
	char *tips = "none";
	if (type == PLAYER_T_TONE) {
		player = &s_app_player->tone_player;
		tips = "tone";
	} else if (type == PLAYER_T_CLOUD) {
		player = &s_app_player->audio_player;
		tips = "cloud";
	}

	if (player)
	{
		int regular_vol = volume;
		LISA_LOGD(TAG, "%s vol %d -> %d with [%d, %d]",
							tips, volume, regular_vol);
		lisa_player_set_vol(player->hld, regular_vol);
	}
}

void app_player_reset(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	if (type == PLAYER_T_TONE) {
		lisa_player_reset(s_app_player->tone_player.hld);
	} else if (type == PLAYER_T_CLOUD) {
		lisa_player_reset(s_app_player->audio_player.hld);
	}
}

void app_player_close(player_t type)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	if (type == PLAYER_T_TONE) {
		lisa_player_pre_close(s_app_player->tone_player.hld);
	} else if (type == PLAYER_T_CLOUD) {
		lisa_player_pre_close(s_app_player->audio_player.hld);
	}
}

void app_player_set_data_hook(player_t type, app_player_data_hook hook, int flag)
{
	if (!s_app_player) return;
	if (!(s_app_player->tone_player.hld) || !(s_app_player->audio_player.hld)) return;

	extern PlayerErr lisa_player_hook_data(PLAYER_HANDLE h, app_player_data_hook hook, int type);
	if (type == PLAYER_T_TONE) {
		lisa_player_hook_data(s_app_player->tone_player.hld, hook, flag);
	} else if (type == PLAYER_T_CLOUD) {
		lisa_player_hook_data(s_app_player->audio_player.hld, hook, flag);
	}
}