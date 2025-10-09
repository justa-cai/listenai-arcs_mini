#ifndef __LISA_EVS_SDK_AUDIO_H__
#define __LISA_EVS_SDK_AUDIO_H__

/**
 * @brief 播放状态
 */
typedef enum
{
	LISA_EVS_PLAYSTATE_IDLE = 0,
	LISA_EVS_PLAYSTATE_PLAYING,
	LISA_EVS_PLAYSTATE_PAUSED,
	LISA_EVS_PLAYSTATE_STOPED,
	LISA_EVS_PLAYSTATE_FINISHED,
	LISA_EVS_PLAYSTATE_ERROR
} lisa_evs_play_state_e;

/**
 * @brief 资源类型
 */
typedef enum
{
	LISA_EVS_RES_TTS = 0,
	LISA_EVS_RES_PLAYBACK,
	LISA_EVS_RES_RING,
} lisa_evs_play_res_type_e;

/**
 * @brief 播放模块信息
 */
typedef struct lisa_evs_playback_state_s
{
	uint8_t version[12];
	uint8_t resource_id[65];
	lisa_evs_play_state_e state;
	long offset;
} lisa_evs_playback_state_t;

/**
 * @brief 上传iFLYOS的播放相关信息
 */
typedef struct lisa_evs_play_info_s
{
	/// 资源类型
	lisa_evs_play_res_type_e type;
	/// 播放状态
	lisa_evs_play_state_e state;
	/// 资源id
	uint8_t *resource_id;
	/// 播放进度，非必填
	long offset;
	/// 错误码，非必填
	uint8_t *failure_code;
} lisa_evs_play_info_t;

#endif // LISA_EVS_SDK_AUDIO_H
