#ifndef __JK_CLOUD_H__
#define __JK_CLOUD_H__

#include <stdbool.h>
#include <stdint.h>
#include "jk_asr.h"
#include "jk_llm.h"
#include "jk_tts.h"

typedef enum {
    JK_CLOUD_STATE_DISCONNECTED = 0,
    JK_CLOUD_STATE_CONNECTING,
    JK_CLOUD_STATE_CONNECTED,
} jk_cloud_state_e;

struct app_client_s;

typedef struct jk_pcm_player jk_pcm_player_t;

struct recognizer_s;
typedef struct jk_cloud {
    jk_asr_t *asr;
    jk_llm_t *llm;
    jk_tts_t *tts;
    jk_pcm_player_t *pcm_player;
    struct app_client_s *m_client;
    struct recognizer_s *m_rec;
    jk_cloud_state_e state;
    bool m_wifi_conn;
    bool m_ntp_conn;
    bool asr_connected;
    bool llm_connected;
    bool tts_connected;
    char *host;
    bool is_recording;
    uint32_t drop_frame_count;
    char accumulated_text[1024];
} jk_cloud_t;

struct app_client_s;
jk_cloud_t *jk_cloud_create(struct app_client_s *client);
void jk_cloud_destroy(jk_cloud_t *cloud);

void jk_cloud_process_wifi_connected(jk_cloud_t *cloud);
void jk_cloud_process_wifi_disconnected(jk_cloud_t *cloud);

bool jk_cloud_is_connected(void);
bool jk_cloud_is_wifi_connected(void);

void jk_cloud_audio(jk_cloud_t *cloud, const char *audio, uint32_t len);
void jk_cloud_wakeup(jk_cloud_t *cloud);
jk_cloud_t *jk_cloud_get_instance(void);

void jk_cloud_tts(const char *text);
void jk_cloud_txt(const char *txt);

void jk_cloud_connect(void);
void jk_cloud_disconnect(void);

void jk_cloud_ntp_ok(void *arg);

int jk_cloud_img_recognition(uint16_t *rgb565_datas, uint32_t width, uint32_t height);

#endif
