#ifndef __LS_VOICE_SERVER_H__
#define __LS_VOICE_SERVER_H__

struct voice_cloud_chat_config {
    uint8_t full_duplex;
    uint32_t timeout_ms; /* full_duplex timeout */
    uint8_t oneshot;
    char **words;
    uint32_t words_cnt;
};

struct voice_cloud_connect_config {
    char *pid;
    char *sid;
    char *did;
    uint8_t auto_reconn;
    uint32_t reconn_interval_ms;
    uint8_t device_mode;
    char *host;
    char *host_staging;
    char *host_integration;
    char *token_url;
    char *token_url_staging;
    char *token_url_integration;
    char *music_active_url;
    char *music_tranlink_url;
    char *port;
    char *scheme;
};

int voice_cloud_connect(struct voice_cloud_connect_config *config);
int voice_cloud_chat_start(struct voice_cloud_chat_config *config);
int voice_cloud_chat_stop(void);
int voice_cloud_chat_send_audio(uint8_t *data, int len);
int voice_cloud_image_recognition(uint8_t *jpg_image, uint32_t len);
int voice_cloud_audio_recognition_start(void);
int voice_cloud_audio_recognition_stop(void);
int voice_cloud_upload_jpeg_img(const uint8_t *jpeg_data, size_t jpeg_size, char **url_out);
void voice_cloud_jpeg_img_url_free(void *url);
int voice_cloud_tts_synth(const char *txt);
int voice_cloud_is_connected(void);
#endif
