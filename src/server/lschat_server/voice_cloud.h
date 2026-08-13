#ifndef __LS_VOICE_SERVER_H__
#define __LS_VOICE_SERVER_H__

typedef enum {
    VOICE_CLOUD_STATE_CONNECTED = 0,
    VOICE_CLOUD_STATE_CONNECTING,
    VOICE_CLOUD_STATE_NO_NETWORK,
    VOICE_CLOUD_STATE_NO_INTERNET,
    VOICE_CLOUD_STATE_TOKEN_FAILED,
    VOICE_CLOUD_STATE_CONNECT_FAILED,
} voice_cloud_state_t;

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
int voice_cloud_disconnect(void);
int voice_cloud_chat_start(struct voice_cloud_chat_config *config);
int voice_cloud_chat_stop(void);
int voice_cloud_chat_stop_local(void);
int voice_cloud_cancel_current_response(void);
int voice_cloud_chat_send_audio(uint8_t *data, int len);
int voice_cloud_image_recognition(uint8_t *jpg_image, uint32_t len);
void voice_cloud_image_recognition_drop_pending_result(void);
int voice_cloud_audio_recognition_start(void);
int voice_cloud_audio_recognition_stop(void);
int voice_cloud_upload_audio_pause(void);
int voice_cloud_upload_audio_resume(void);
int voice_cloud_upload_jpeg_img(const uint8_t *jpeg_data, size_t jpeg_size, char **url_out);
void voice_cloud_jpeg_img_url_free(void *url);
int voice_cloud_tts_synth(const char *txt);
int voice_cloud_is_connected(void);
voice_cloud_state_t voice_cloud_get_state(void);
int voice_cloud_is_device_unbound(void);
int voice_cloud_is_session_active(void);
int voice_cloud_is_uploading_audio(void);
#endif
