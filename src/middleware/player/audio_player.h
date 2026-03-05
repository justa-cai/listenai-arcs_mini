#ifndef __LISTENAI_AUDIO_PLAYER_H__
#define __LISTENAI_AUDIO_PLAYER_H__

#include "listen_audiomgr.h"
#include "play_mode.h"
#include <stdbool.h>
#include <audio_out.h>
#include "lisa_semaphore.h"
#include <stdint.h>
#include "lisa_player.h"

typedef enum {
	AUDIO_PLAY,
	AUDIO_PAUSE,
	AUDIO_RESUME,
	AUDIO_STOP,
} audio_play_cmd;

typedef struct {
	void (*on_focus_state)(focus_state_e state, int by_which);
	void (*on_play_state)(uint16_t state);
} audio_player_callback_t;

/**
 * @brief 音频播放器配置（用于新焦点管理）
 */
typedef struct audio_player_config_s {
	int id;
	char *name;
	int priority;
	int capture_ids[MAX_CAPTURE_COUNT];
	int capture_count;
} audio_player_config_t;

typedef struct audioplayer_s {
	bool (*on_directive)(struct audioplayer_s *handle, audio_play_cmd cmd, audio_out_t *item, uint32_t item_size);
	void (*pause)(struct audioplayer_s *handle);
	void (*resumeByVoice)(struct audioplayer_s *handle);
	void (*next)(struct audioplayer_s *handle);
	void (*prev)(struct audioplayer_s *handle);
	void (*stop)(struct audioplayer_s *handle, bool play_after_stop);
	void (*replay)(struct audioplayer_s *handle);

	int m_id;
	char *m_name;
	bool m_is_need_play_after_op;
	bool m_is_pause_called;
	focus_state_e m_focus_state;
	PlayerEvt m_player_state;
	listen_audiomgr_t *m_audio_mgr;
	lisa_semaphore_t *pause_sema;
	play_mode_t *m_play_mode;
	audio_player_callback_t *m_callback;
} audioplayer_t;

/**
 * @brief 创建音频播放器（使用新焦点管理）
 */
audioplayer_t *listen_audioplayer_create(audio_player_config_t *config, listen_audiomgr_t *audio_mgr);

/**
 * @brief 销毁音频播放器
 */
void listen_audioplayer_destroy(audioplayer_t *handle);

/**
 * @brief 恢复播放
 */
void listen_audioplayer_resume(audioplayer_t *handle);

/**
 * @brief 暂停播放
 */
void listen_audioplayer_pause(audioplayer_t *handle);

/**
 * @brief 获取播放状态
 */
PlayerEvt listen_audioplayer_get_state(audioplayer_t *handle);

/**
 * @brief 用户停止播放
 */
void audio_player_stop_by_user(void);

/**
 * @brief 添加回调
 */
void listen_audioplayer_add_callback(audioplayer_t *handle, audio_player_callback_t *callback);

/**
 * @brief 切换播放模式
 */
int listen_audioplayer_switch_mode(audioplayer_t *handle, PLAY_MODE_E mode);

#endif
