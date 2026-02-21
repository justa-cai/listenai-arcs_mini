#ifndef __APPLICATION_CLIENT_H__
#define __APPLICATION_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef MY_CLOUD
struct jk_cloud;
#endif

typedef struct app_client_s {
	short *buffer;
	uint32_t buffer_size;
	struct listen_audiomgr_s *audio_mgr;
	struct short_player_s *short_player;
	struct audioplayer_s *audio_player;
	struct tts_player_s *tts_player;
	struct sound_player_s *sound_player;
	struct play_mode_s *play_mode;
#ifdef MY_CLOUD
	struct jk_cloud *cloud;
#else
	struct app_cloud_s *cloud;
#endif
} app_client_t;

app_client_t *app_client_create();

app_client_t *app_client_get_instance();

/**
 * @brief 	送录音音频
 * @param	audio	录音数据
 * @param	len		录音数据长度
 */
void app_client_record(const char *audio, int len);

/**
 * @brief 	本地唤醒 + 识别 + 超时
 * @param  	data	识别JSON消息
 * @param  	len     消息长度
 */
void app_client_wakeup(const uint8_t *data, uint32_t len);

/**
 * @brief 	ESR 超时
 */
void app_client_algo_timeout();

/**
 * @brief 	停止 Client
 */
void app_client_stop();

#endif