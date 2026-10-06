#include <stdbool.h>
#include <stdint.h>

#define TAG "voice_wakeup_tone"
#include "lisa_log.h"
#include "lisa_time.h"

#include "app_datas.h"
#include "app_wakeup.h"
#include "app_tone.h"
#ifdef CONFIG_MIDDLEWARE_SD_MUSIC
#include "service_sd_music.h"
#endif
#include "sys_network_manager.h"
#include "tone.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "tone_control/voice_player_tone.h"
#include "tone_control/voice_player_wakeup_tone.h"

/*
 * 唤醒提示音策略：云端可用时随机播放本地唤醒音，否则按网络、鉴权状态
 * 播放对应故障提示音。该模块只消费唤醒消息，不负责唤醒流程本身。
 */

/* ==================== 私有配置、类型与状态 ==================== */

#define WAKEUP_TONE_CANDIDATE_COUNT 5U

#define WAKEUP_NETWORK_UNAVAILABLE_TONE_ID TONE_ID_64
#define WAKEUP_CLOUD_CONNECT_FAILED_TONE_ID TONE_ID_85
#define WAKEUP_AUTH_FAILED_TONE_ID TONE_ID_105

typedef struct {
    /* 当前资源包中实际存在的唤醒提示音 ID。 */
    uint16_t candidates[WAKEUP_TONE_CANDIDATE_COUNT];
    /* 上一次播放的提示音 ID，用于避免连续播放相同提示音。 */
    uint16_t last_tone_id;
    /* xorshift32 的内部状态，首次使用时由平台随机数初始化。 */
    uint32_t random_state;
    /* candidates 数组中的有效元素数量。 */
    uint8_t candidate_count;
    /* 候选提示音列表是否已经从资源包加载。 */
    bool initialized;
    /* last_tone_id 是否已经记录过一次有效的播放结果。 */
    bool last_tone_valid;
} wakeup_tone_context_t;

static wakeup_tone_context_t s_wakeup_tone;

/* ==================== 内部工具：候选音选择 ==================== */

/* xorshift32 伪随机序列；状态为 0 时使用平台随机数初始化。 */
static uint32_t xorshift32(void)
{
    if (s_wakeup_tone.random_state == 0U) {
        s_wakeup_tone.random_state = lisa_rand32() | 1U;
    }

    s_wakeup_tone.random_state ^= s_wakeup_tone.random_state << 13;
    s_wakeup_tone.random_state ^= s_wakeup_tone.random_state >> 17;
    s_wakeup_tone.random_state ^= s_wakeup_tone.random_state << 5;
    return s_wakeup_tone.random_state;
}

static void voice_player_wakeup_load_candidates(void)
{
    uint16_t tone_id;

    if (s_wakeup_tone.initialized) {
        return;
    }

    /* 资源包可能裁剪部分唤醒音，只将实际存在的 TONE_ID_0~4 加入候选集。 */
    for (tone_id = TONE_ID_0; tone_id <= TONE_ID_4; tone_id++) {
        if (app_tone_get_url(tone_id) != NULL) {
            s_wakeup_tone.candidates[s_wakeup_tone.candidate_count++] = tone_id;
        }
    }

    /* 保留默认 ID，后续播放时仍会再次检查 URL，避免候选集为空时越界。 */
    if (s_wakeup_tone.candidate_count == 0U) {
        s_wakeup_tone.candidates[s_wakeup_tone.candidate_count++] = TONE_ID_0;
    }

    s_wakeup_tone.initialized = true;
    LOGI("wakeup tone candidates count: %u",
         (unsigned int)s_wakeup_tone.candidate_count);
}

static uint16_t voice_player_wakeup_select_tone(void)
{
    uint16_t tone_id;
    uint8_t attempts = 0U;

    voice_player_wakeup_load_candidates();
    tone_id = s_wakeup_tone.candidates[0];

    /* 多个候选音时尽量避免连续两次播放同一个，重试次数用于防御异常随机序列。 */
    if (s_wakeup_tone.candidate_count > 1U) {
        do {
            uint8_t index = (uint8_t)(xorshift32() %
                                      s_wakeup_tone.candidate_count);
            tone_id = s_wakeup_tone.candidates[index];
        } while (s_wakeup_tone.last_tone_valid &&
                 tone_id == s_wakeup_tone.last_tone_id &&
                 ++attempts < 8U);
    }

    s_wakeup_tone.last_tone_id = tone_id;
    s_wakeup_tone.last_tone_valid = true;
    LOGI("selected wakeup tone id: %u", (unsigned int)tone_id);
    return tone_id;
}

/* ==================== 内部工具：唤醒状态判定与播放 ==================== */

static void voice_player_wakeup_play_random_tone(void)
{
    uint16_t tone_id = voice_player_wakeup_select_tone();
    const char *url = app_tone_get_url(tone_id);

    if (url == NULL && tone_id != TONE_ID_0) {
        url = app_tone_get_url(TONE_ID_0);
    }

    if (url == NULL) {
        LOGE("wakeup tone url is null");
        return;
    }

    voice_player_play_wakeup_tone_url(url);
}

static void voice_player_wakeup_handle(uint32_t required_mode)
{
    struct app_datas *app_data = get_app_datas();
    sys_network_status_t status = {0};
    bool status_ok;

    if (app_data == NULL) {
        LOGW("invalid app_datas");
        return;
    }

    /*
     * SD 卡插入后的扫描、文件列表上报及同步结果展示期间，UI 处于专用同步流程。
     * 此时忽略唤醒提示音，避免声音打断文件列表上传过程或造成用户误以为已进入语音交互。
     */
#ifdef CONFIG_MIDDLEWARE_SD_MUSIC
    if (service_sd_music_is_syncing()) {
        LOGI("ignore wakeup tone during SD music sync");
        return;
    }
#endif

    /* required_mode 区分按键/关键词来源；can_wakeup 是两种唤醒方式共用的总开关。 */
    if ((app_data->voice_work_mode & required_mode) == 0U ||
        !app_data->can_wakeup) {
        return;
    }

    /* 云端可用时播放正常唤醒音，否则按失败层级给出更准确的提示。 */
    if (voice_cloud_is_connected()) {
        voice_player_wakeup_play_random_tone();
        return;
    }

    status_ok = sys_network_get_status(&status) == 0;
    if (!status_ok || !status.connected) {
        voice_player_play_wakeup_tone_url(
            app_tone_get_url(WAKEUP_NETWORK_UNAVAILABLE_TONE_ID));
    } else if (app_data->auth_failed) {
        voice_player_play_wakeup_tone_url(
            app_tone_get_url(WAKEUP_AUTH_FAILED_TONE_ID));
    } else {
        voice_player_play_wakeup_tone_url(
            app_tone_get_url(WAKEUP_CLOUD_CONNECT_FAILED_TONE_ID));
    }
}

/* ==================== EBUS 消息回调 ==================== */

static void voice_player_wakeup_on_message(void *unused, uint32_t msg_id,
                                          void *data, uint32_t len,
                                          void *user_data)
{
    int power_ret;
    uint32_t required_mode;

    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    /* 两类唤醒共享播放策略，仅使用不同的工作模式 bit 做入口门控。 */
    switch (msg_id) {
    case VOICE_MSG_WAKEUP_BUTTON_START:
        required_mode = VOICE_WORK_MODE_BUTTON_WAKEUP;
        break;
    case VOICE_MSG_WAKEUP_KEYWORD:
        required_mode = VOICE_WORK_MODE_VOICE_WAKEUP;
        break;
    default:
        return;
    }

    /*
     * Button-triggered wake events do not pass through the wake-word callback,
     * so restore the DAC here as a final guard before playing any wake tone.
     */
    power_ret = app_wakeup_audio_standby_resume();
    if (power_ret != 0) {
        LISA_LOGW(TAG, "Audio standby resume before wake tone failed: %d", power_ret);
    }

    voice_player_wakeup_handle(required_mode);
}

/* ==================== 对外 API ==================== */

int voice_player_wakeup_tone_init(void)
{
    int ret = 0;

    if (voice_msg_sub(VOICE_MSG_WAKEUP_BUTTON_START,
                      voice_player_wakeup_on_message,
                      NULL) != 0) {
        ret = -1;
    }
    if (voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD,
                      voice_player_wakeup_on_message,
                      NULL) != 0) {
        ret = -1;
    }

    return ret;
}
