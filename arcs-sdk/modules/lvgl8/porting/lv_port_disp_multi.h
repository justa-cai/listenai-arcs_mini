/**
 * @file lv_port_disp_multi.h
 */

#include "lisa_device.h"
#if 1

#ifndef LV_PORT_DISP_MULTI_H
#define LV_PORT_DISP_MULTI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lisa_display.h"

lv_disp_t *lv_port_disp_register(lisa_device_t *display_dev);

#ifdef __cplusplus
}
#endif

#endif

#endif
