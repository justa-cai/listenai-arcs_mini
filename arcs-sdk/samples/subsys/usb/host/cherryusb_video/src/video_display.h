/*
 * Copyright (c) 2024, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef VIDEO_DISPLAY_H
#define VIDEO_DISPLAY_H

#include <stdint.h>

/* 与 USBH_VIDEO_FORMAT_* 保持一致，避免在 display 模块中引入 CherryUSB 头文件 */
#define VIDEO_DISPLAY_FORMAT_UNCOMPRESSED 0
#define VIDEO_DISPLAY_FORMAT_MJPEG        1

/**
 * 初始化显示子系统（LVGL + ST7789P3 + LVGL 任务）。
 * 必须在 USB 初始化之前调用。
 */
void video_display_init(void);

/**
 * 将一帧图像更新到屏幕对应摄像头区域。
 * 屏幕左右各半：dev_idx=0 → 左半，dev_idx=1 → 右半。
 * 可在 USB 任务上下文（帧回调）中调用，内部通过 workqueue 异步处理。
 *
 * @param dev_idx     设备索引（0 或 1）
 * @param frame_buf   帧数据缓冲区
 * @param frame_size  帧数据字节数
 * @param width       图像宽度（像素）
 * @param height      图像高度（像素）
 * @param format      VIDEO_DISPLAY_FORMAT_UNCOMPRESSED 或 VIDEO_DISPLAY_FORMAT_MJPEG
 */
void video_display_update(uint8_t dev_idx,
                          uint8_t *frame_buf, uint32_t frame_size,
                          uint16_t width, uint16_t height, uint8_t format);

#endif /* VIDEO_DISPLAY_H */
