#ifndef __LISAUI_ASSETS_H__
#define __LISAUI_ASSETS_H__

#include "lvgl.h"

#define LISA_UI_ARRAY_SIZE(name) (sizeof(name) / sizeof(name[0]))

#define LISA_UI_ASSETS_IMG_DSC(name) lv_img_dsc_##name

#define LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(name)        lv_img_dsc_t LISA_UI_ASSETS_IMG_DSC(name)[]
#define LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(name, size) extern lv_img_dsc_t LISA_UI_ASSETS_IMG_DSC(name)[size]

#define LISA_UI_ASSETS_IMG_DSC_LIST_GET(name)  LISA_UI_ASSETS_IMG_DSC(name)
#define LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(name) LISA_UI_ARRAY_SIZE(LISA_UI_ASSETS_IMG_DSC(name))

LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_wifi, 5);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_charging, 10);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_power, 10);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_angry, 16);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_blink, 5);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_eye, 12);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_hug, 25);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_love, 23);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_puzzled, 19);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_sad, 19);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_sleepy, 41);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_wait, 8);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_wakeup, 28);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_battery, 22);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_happy, 21);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_cute, 24);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_interactive, 1);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_alarm, 1);
LISA_UI_ASSETS_IMG_DSC_LIST_DECLARE(img_png_music, 1);

void lisa_ui_assets_init(void);

#endif /* __LISAUI_ASSETS_H__ */
