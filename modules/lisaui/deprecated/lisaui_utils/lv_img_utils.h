/**
 * @file lv_img_utils.h
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-02-23
 * 
 * @copyright Copyright (c) 2021 - 2024 shenzhen listenai co., ltd.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LV_IMG_UTILS_H__
#define __LV_IMG_UTILS_H__

#ifdef __ZEPHYR__
    #include <zephyr/kernel.h>
    #include "lvgl.h"
#elif CONFIG_LVGL_ENV_SIMULATOR
    #include "lvgl/lvgl.h"
#else
    #include "lvgl.h"
#endif

#include "img_utils.h"

typedef struct _lisaui_ui_res_img_png_t {
    uint32_t res_id;
    lv_img_dsc_t *img;
    const uint8_t *data;
    uint32_t size;
} lisaui_ui_res_img_png_t;

#define UI_RES_IMG_NAME(name, path) \
    static lv_obj_t *lv_obj_icon_##name;\
    lv_img_dsc_t lv_img_dsc_##name;\
    INCBIN(file_##name, path);

#define UI_RES_IMG_PNG(name) &lv_img_dsc_##name, gfile_##name##Data, gfile_##name##Size

// #define LV_UI_RES_IMG_PNG(name) icon_##name
#define LV_OBJ_ICON(name) lv_obj_icon_##name
#define LV_IMG_DSC(name) lv_img_dsc_##name


int lv_img_png_src_init(lv_img_dsc_t *dsc, const void *data, uint32_t size);
int lv_img_png_res_init(lisaui_ui_res_img_png_t *res_img);

int lv_img_gif_src_init(lv_img_dsc_t *dsc, const void *data, uint32_t size);
int lv_img_jpg_src_init(lv_img_dsc_t *dsc, const void *data, uint32_t size);

#endif // __LV_IMG_UTILS_H__
