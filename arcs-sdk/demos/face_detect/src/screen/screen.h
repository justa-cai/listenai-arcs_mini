/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <float.h>
#include <lvgl.h>

#include "acomp_fd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建屏幕组件
 *
 * @return lv_obj_t* 创建的屏幕对象
 *
 */
lv_obj_t *screen_create(void);

/**
 * @brief 更新人脸图片或人脸检测结果到屏幕
 *
 * @param info[in] 人脸检测结果信息
 * @param img_data[in] 人脸图像数据
 * @param img_width[in] 图像宽度
 * @param img_height[in] 图像高度
 * @param img_len[in] 图像长度
 *
 * @return void
 *
 */
void screen_update_fd_info(acomp_fd_result_info_t *info, uint8_t *img_data, int img_width, int img_height, int img_len);

/**
 * @brief 更新已注册的人脸个数到屏幕
 *
 * @param count[in] 已注册的人脸个数
 *
 * @return void
 *
 */
void screen_update_registered(uint32_t count);

/**
 * @brief 更新人脸特征比较结果到屏幕
 *
 * @param index[in] 已注册的人脸索引
 * @param pass[in] 是否通过
 * @param score[in] 比较得分
 *
 * @return void
 *
 */
void screen_update_compare(uint32_t index, bool pass, float score);

#ifdef __cplusplus
}
#endif
