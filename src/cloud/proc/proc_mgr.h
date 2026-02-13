#ifndef __PROC_MGR_H__
#define __PROC_MGR_H__
#include "app_client.h"
#include "app_cloud.h"

typedef enum {
	MUSIC_REPLAY,
	MUSIC_RESUME_PLAY,
	MUSIC_PLAY_NEXT,
	MUSIC_PLAY_PREV,
} MUSIC_CONTRL_EVENT;

// Forward declarations
typedef struct audioplayer_s audioplayer_t;

typedef void (*tts_url_callback_t)(const char *url);

void app_proc_init(struct app_client_s *app_client, struct app_cloud_s *cloud);

void app_proc_msg(const char* msg, int len);

int started_wait(int timeout);
void started_reset(void);

/**
 * @brief Get the audio player instance
 * @return audioplayer_t* Audio player instance or NULL if not initialized
 */
audioplayer_t *get_audio_player(void);

/**
 * @brief Enter audio idle state
 */
void enter_audio_idle(void);

/**
 * @brief Process music control message
 * @param evt Music control event
 */
void music_control_msg(MUSIC_CONTRL_EVENT evt);

void register_tts_url_callback(tts_url_callback_t cb);
void proc_mgr_expect_tts_url(bool expect);

#endif // __PROC_MGR_H__
