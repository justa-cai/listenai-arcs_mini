#define TAG "voice_player"
#include "lisa_log.h"

#include "sys_init.h"
#include "voice_msg.h"

#include "voice_intent_mgr.h"
#include "voice_player/voice_player_alarm.h"
#include "voice_player/voice_player_music.h"
#include "voice_player/voice_player_tts.h"
#include "voice_player/tone_control/voice_player_network_tone.h"
#include "voice_player/tone_control/voice_player_tone.h"
#include "voice_player/tone_control/voice_player_wakeup_tone.h"

/*
 * voice_player 组合根：只负责按依赖顺序初始化各播放子模块。
 * 具体播放策略分别由 voice_player/ 下的业务模块实现。
 */

/* ==================== 平台消息入口 ==================== */

/* 平台资源就绪后按依赖顺序初始化各子模块。 */
static void voice_player_ready(void *unused, uint32_t msg_id,
                               void *data, uint32_t len,
                               void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LOGI("voice_player_ready");

    if (voice_intent_mgr_init() != 0) {
        LOGE("voice intent manager init failed");
    }
    if (voice_player_music_init() != 0) {
        LOGE("music player init failed");
    }
    if (voice_player_tts_init() != 0) {
        LOGE("tts player init failed");
    }
    if (voice_player_alarm_init() != 0) {
        LOGE("alarm player init failed");
    }
    if (voice_player_tone_init() != 0) {
        LOGE("tone player init failed");
    }
    if (voice_player_network_tone_init() != 0) {
        LOGE("network tone policy init failed");
    }
    if (voice_player_wakeup_tone_init() != 0) {
        LOGE("wakeup tone policy init failed");
    }
}

/* ==================== 系统初始化入口 ==================== */

static int voice_player_init(void)
{
    LOGI("voice_player_init");

    if (voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_player_ready, NULL) != 0) {
        LOGE("failed to subscribe platform ready message");
        return -1;
    }

    return 0;
}

SYS_INIT(voice_player_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);
