#ifndef __MODEL_VIDEO_PLAYER_H__
#define __MODEL_VIDEO_PLAYER_H__

#include <stdbool.h>

/* 视频播放服务模型层：固件 video_player 服务的薄封装 */

int model_video_player_start(const char *url);
int model_video_player_stop(void);
bool model_video_player_is_playing(void);

#endif
