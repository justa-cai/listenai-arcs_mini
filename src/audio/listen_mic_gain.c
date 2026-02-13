#include "listen_volume.h"
#include "lisa_mutex.h"
#include "app_player.h"
#include "lisa_typedef.h"
#include "kv.h"
#include <math.h>
#include "lisa_kv.h"
#include "comm_service.h"

#define TAG "gain"
#include "lisa_log.h"

static lisa_mutex_t *s_mutex = NULL;
static bool s_mute = false;

#define LISTEN_ADC_PDM_GAIN_A_DEFAULT (30)
#define LISTEN_ADC_PDM_GAIN_A_MAX_DB  (36)
#define LISTEN_ADC_PDM_GAIN_A_MIN_DB  (-12)

#define LISTEN_ADC_PDM_GAIN_D_MAX_DB (42)
#define LISTEN_ADC_PDM_GAIN_D_MIN_DB (-83)

#define MIC_PCT_CLAMP(pct) ((pct) < 0 ? 0 : ((pct) > 100 ? 100 : (pct)))

static int a_gain_percent_to_db(int percent)
{
    float step;
    float db;
    int idb;
    if (percent >= 50) {
        step = (float)(LISTEN_ADC_PDM_GAIN_A_MAX_DB - LISTEN_ADC_PDM_GAIN_A_DEFAULT) / 50;
        db = LISTEN_ADC_PDM_GAIN_A_DEFAULT + (percent - 50) * step;
    } else {
        step = (float)(LISTEN_ADC_PDM_GAIN_A_MIN_DB - LISTEN_ADC_PDM_GAIN_A_DEFAULT) / 50;
        db = LISTEN_ADC_PDM_GAIN_A_DEFAULT - (percent - 50) * step;
    }

    idb = round(db);

    return idb;
}

int listen_mic_gain_get(void)
{
    int gain = 0;
    if (lisa_kv_get_int(KV_KEY_USER_MIC_GAIN, &gain) != 0) {
        gain = CONFIG_DEFAULT_MIC_GAIN;
    }
    return MIC_PCT_CLAMP(gain);
}

static int listen_aec_gain_get(void)
{
    int gain = 0;
    if (lisa_kv_get_int(KV_KEY_USER_AEC_GAIN, &gain) != 0) {
        gain = CONFIG_DEFAULT_AEC_GAIN;
    }
    return MIC_PCT_CLAMP(gain);
}

void listen_mic_gain_set(int gain)
{
    int8_t v_db_mic, v_db_aec;
    int aec_gain = listen_aec_gain_get();

    gain = MIC_PCT_CLAMP(gain);

    v_db_aec = a_gain_percent_to_db(aec_gain);
    v_db_mic = a_gain_percent_to_db(gain);
    lis_ivw_gain_set(v_db_mic, v_db_aec, 0, 0);

    lisa_kv_set_int(KV_KEY_USER_MIC_GAIN, gain);

    LISA_LOGI(TAG, "Updated MIC gain, mic_gain=%d%% (%d dB), aec_gain=%d%% (%d dB)", gain, v_db_mic, aec_gain,
              v_db_aec);
}

void listen_update_aec_by_volume(int volume)
{
    int8_t v_db_mic, v_db_aec;
    int mic_gain, aec_gain;

    // 计算 AEC 增益
    aec_gain = (volume >= 90) ? 0 : (volume >= 80) ? 2 : 4;
    v_db_aec = a_gain_percent_to_db(aec_gain);

    // 获取麦克风增益
    mic_gain = listen_mic_gain_get();
    v_db_mic = a_gain_percent_to_db(mic_gain);

    // 设置麦克风增益和AEC增益
    lis_ivw_gain_set(v_db_mic, v_db_aec, 0, 0);

    LISA_LOGI(TAG, "Updated AEC gain by volume: volume=%d%%, mic_gain=%d%% (%d dB), aec_gain=%d%% (%d dB)", volume,
              mic_gain, v_db_mic, aec_gain, v_db_aec);
}

void listen_mic_gain_set_debug(void)
{
    int target_mic_gain = listen_mic_gain_get();
    int target_aec_gain = listen_aec_gain_get();
    int target_volume = listen_get_volume();

    int8_t v_db_mic = a_gain_percent_to_db(target_mic_gain);
    int8_t v_db_aec = a_gain_percent_to_db(target_aec_gain);

    // 设置音量
    app_player_volume(PLAYER_T_CLOUD, target_volume);
    app_player_volume(PLAYER_T_TONE, target_volume);

    // 设置麦克风增益和AEC增益
    lis_ivw_gain_set(v_db_mic, v_db_aec, 0, 0);

    LISA_LOGI(TAG, "========== DEBUG GAIN SET ==========");
    LISA_LOGI(TAG, "volume:   %d%% [0,100]", target_volume);
    LISA_LOGI(TAG, "mic_gain: %d%% [0,100]", target_mic_gain);
    LISA_LOGI(TAG, "mic_gain_db: %d db", v_db_mic);
    LISA_LOGI(TAG, "aec_gain: %d%% [0,100]", target_aec_gain);
    LISA_LOGI(TAG, "aec_gain_db: %d db", v_db_aec);
    LISA_LOGI(TAG, "====================================");
}

void listen_mic_mute(bool is_mute)
{
    s_mute = is_mute;
    if (s_mute) {
        lis_ivw_idle();
    } else {
        lis_ivw_run();
    }
}

int listen_mic_mute_get(void)
{
    return s_mute;
}

void listen_mic_gain_init(void)
{
    int8_t v_db_mic, v_db_aec;
    int mic_gain, aec_gain;

    mic_gain = listen_mic_gain_get();
    aec_gain = listen_aec_gain_get();

    v_db_mic = a_gain_percent_to_db(mic_gain);
    v_db_aec = a_gain_percent_to_db(aec_gain);
    lis_ivw_gain_set(v_db_mic, v_db_aec, 0, 0);

    LISA_LOGI(TAG, "Inited with mic_gain=%d%% (%d dB), aec_gain=%d%% (%d dB)", mic_gain, v_db_mic, aec_gain, v_db_aec);
}
