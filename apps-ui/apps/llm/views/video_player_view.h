#ifndef __VIDEO_PLAYER_VIEW_H__
#define __VIDEO_PLAYER_VIEW_H__

#include "lvgl.h"

/* 创建视频页视图（黑色全屏底 + 动态 RGB565 图像） */
lv_obj_t *lisa_ui_video_view_create(lv_obj_t *parent);

/* 更新显示帧（UI 线程调用；rgb565 缓冲由播放服务持有并保持有效） */
void lisa_ui_video_view_set_frame(const uint16_t *rgb565, int w, int h);

#endif
