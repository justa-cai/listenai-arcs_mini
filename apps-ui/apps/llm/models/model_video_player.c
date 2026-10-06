#define LOG_TAG "model_video_player"

#include "model_video_player.h"

#include <string.h>

#include "lisa_ui.h"

/*
 * 视频播放模型层：转发到固件侧 video_player 服务。
 * 设备平台直接链接服务实现；模拟器平台提供空实现以便 UI 联调。
 */

#ifdef LISA_UI_PLATFORM_ARCS
#include "video_player.h"

int model_video_player_start(const char *url) {
    return video_player_start(url);
}

int model_video_player_stop(void) {
    return video_player_stop();
}

bool model_video_player_is_playing(void) {
    return video_player_is_playing();
}

#else /* 模拟器 */

static bool s_sim_playing;

int model_video_player_start(const char *url) {
    (void)url;
    s_sim_playing = true;
    return 0;
}

int model_video_player_stop(void) {
    s_sim_playing = false;
    return 0;
}

bool model_video_player_is_playing(void) {
    return s_sim_playing;
}

#endif
