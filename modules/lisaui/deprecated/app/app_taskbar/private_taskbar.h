/**
 * @file private_taskbar.h
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-04-15
 * 
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __PRIVATE_APP_TASKBAR_H__
#define __PRIVATE_APP_TASKBAR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_app_common.h"
#include "assets/assets_res.h"

lv_obj_t *_lisaui_taskbar_create_battery(lv_obj_t *parent);
lv_obj_t *_lisaui_taskbar_create_back_btn(lv_obj_t *parent);
lv_obj_t *_lisaui_taskbar_create_date_label(lv_obj_t *parent);
lv_obj_t *_lisaui_taskbar_create_time_label(lv_obj_t *parent);
lv_obj_t *_lisaui_taskbar_create_operate_menu(lv_obj_t *parent);

void _lisaui_taskbar_battery_update_cb(lv_timer_t *timer);
lisaui_err_t lisaui_app_common_app_enter_common_handler(lisaui_app_t *app);

#ifdef __cplusplus
}
#endif

#endif // __PRIVATE_APP_TASKBAR_H__



