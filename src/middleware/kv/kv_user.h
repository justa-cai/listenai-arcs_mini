#ifndef __KV_USER_H__
#define __KV_USER_H__

#include "lisa_kv.h"

#define KV_KEY_USER_PID               "user.pid"
#define KV_KEY_USER_SID               "user.sid"
#define KV_KEY_APPID                  "user.appid"
#define KV_KEY_APPKEY                 "user.appkey"
#define KV_KEY_TOKEN                  "user.token"
#define KV_KEY_DEVICE_MODE            "user.device_mode"
#define KV_KEY_USER_VOLUME            "user.volume"
#define KV_KEY_USER_MIN_VOLUME        "user.min_volume"
#define KV_KEY_USER_BRIGHTNESS        "user.brightness"
#ifdef CONFIG_BOARD_ARCS_MINI
#define KV_KEY_USER_MIC_GAIN_DB       "user.mic_gain_db"
#define KV_KEY_USER_AEC_GAIN_DB       "user.aec_gain_db"
#define KV_KEY_USER_SPK_GAIN_DB       "user.spk_gain_db"
#else // !CONFIG_BOARD_ARCS_MINI
#define KV_KEY_USER_MIC_GAIN          "user.mic_gain"
#endif // CONFIG_BOARD_ARCS_MINI
#define KV_KEY_WAKEUP_MODE            "user.wakeup_mode"
#define KV_KEY_NETWORK_MODE           "user.network_mode"
#define KV_KEY_INT_MODE               "user.int_mode"
#define KV_KEY_FULL_DUPLEX_TIMEOUT_MS "user.full_duplex_timeout_ms"
#define KV_KEY_IDLE_EXIT_TIMEOUT_MS   "user.idle_exit_timeout_ms"
#define KV_KEY_USER_ALARM_RING_TEMP_VOLUME "user.alarm_ring_temp_volume"
#define KV_KEY_USER_STANDBY_SLEEP_DELAY_MS "user.standby_sleep_delay_ms"
#define KV_KEY_USER_TTS_TEXT_MS_PER_CHAR "user.tts_text_ms_per_char"
#define KV_KEY_USER_DISABLE_APP_UPDATE      "user.disable_app_update"
#define KV_KEY_USER_DISABLE_WAKEWORD_UPDATE "user.disable_wakeword_update"
#define KV_KEY_USER_DISABLE_TONE_UPDATE     "user.disable_tone_update"
#define KV_KEY_USER_DISABLE_EMOJI_UPDATE    "user.disable_emoji_update"

#define KV_KEY_SD_CID               "sd.card.cid"
#define KV_KEY_SD_STAMP             "sd.card.stamp"
#define KV_KEY_SD_PRESENT           "sd.card.present"

static inline void kv_user_clear_sd_card_sync(void)
{
    (void)lisa_kv_del(KV_KEY_SD_CID);
    (void)lisa_kv_del(KV_KEY_SD_STAMP);
    (void)lisa_kv_del(KV_KEY_SD_PRESENT);
}

#define KV_KEY_USER_DEVICE_ID   "user.device.id"
#endif
