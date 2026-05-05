/**
 * @file lv_port_indev_multi.h
 */

#if 1

#ifndef LV_PORT_INDEV_MULTI_H
#define LV_PORT_INDEV_MULTI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_touch.h"

lv_indev_t *lv_port_indev_register(lisa_device_t *touch_dev, lisa_device_t *display_dev, lv_disp_t *target_disp);

#ifdef __cplusplus
}
#endif

#endif

#endif
