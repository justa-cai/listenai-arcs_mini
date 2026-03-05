#ifndef __MODEL_VOICE_H__
#define __MODEL_VOICE_H__

#include <math.h>
#include <stdbool.h>

#ifdef CONFIG_OTA
#include "ota_manager.h"
#endif

typedef enum {
    MODEL_VOICE_WAKEUP_MODE_BUTTON = 0,
    MODEL_VOICE_WAKEUP_MODE_VOICE_SINGLE,
    MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI,
    MODEL_VOICE_WAKEUP_MODE_MAX
} model_voice_wakeup_mode_t;

int model_voice_off(void);
int model_voice_on(void);

struct model_voice_cb {
    void (*on_tts_stoped)(void *arg);
    void (*on_tts_playing)(void *arg);
    void (*on_emoji)(void *arg, const char *emoji_name);
    void (*on_mcp_emoji)(void *arg, const char *emoji_name);
    void (*on_mcp_loading)(void *arg, bool is_loading, const char *loading_text);
    void (*on_connected)(void *arg);
    void (*on_disconnected)(void *arg);
    void (*on_start)(void *arg);
    void (*on_finished)(void *arg);
    void (*on_tts_text_start)(void *arg);
    void (*on_tts_text_update)(const char *text, void *arg);
    void (*on_tts_text_end)(void *arg);
    void (*on_iat_text_start)(void *arg);
    void (*on_iat_text_update)(const char *text, void *arg);
    void (*on_iat_text_end)(void *arg);
    void (*on_image_rec)(void *arg);
    void (*on_image_preview)(void *arg);
    void (*on_image_url)(void *arg, const char *url);
    void (*on_info_show)(void *arg);
    void (*on_show_qrcode)(void *arg);
    void (*on_standby_texts_changed)(void *arg);
    void (*on_standby_text_update)(void *arg, const char *text, bool is_cloud_text);
#ifdef CONFIG_OTA
    void (*on_ota_state_change)(const ota_state_t *state, void *arg);
#endif
    void (*on_wakeup_mode_changed)(void *arg, model_voice_wakeup_mode_t mode);
};

int model_voice_init(void);
int model_voice_cb_register(const struct model_voice_cb *cb, void *arg);
int model_voice_cb_unregister(const struct model_voice_cb *cb);
const char *model_voice_role_name_get(void);
const char *model_voice_role_propmt_get(void);
uint8_t model_voice_cloud_is_connected(void);
uint8_t model_voice_cloud_is_running(void);
uint8_t model_voice_tts_is_playing(void);
const char *model_voice_last_iat_text_get(void);

model_voice_wakeup_mode_t model_voice_wakeup_mode_get(void);
int model_voice_wakeup_mode_set(model_voice_wakeup_mode_t mode);
const char *model_voice_wakeup_mode_name_get(model_voice_wakeup_mode_t mode);
int model_voice_img_recognition(uint8_t *rgb565, uint32_t len, int width, int height);

uint32_t model_voice_standby_text_count_get(void);
uint32_t model_voice_standby_text_interval_ms_get(void);
const char *model_voice_standby_text_get(uint32_t index);

#endif
