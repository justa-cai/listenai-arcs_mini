/**
 * @file app_setting_view_wakeup_config.h
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-15
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_SETTING_VIEW_WAKEUP_CONFIG_H__
#define __APP_SETTING_VIEW_WAKEUP_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_app_common.h"

typedef enum {
    LISAUI_APP_SETTING_WAKEUP_MODE_KEY = 0,
    LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_SINGLE,
    LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_MULTI,
} lisaui_app_setting_wakeup_mode_t;

typedef enum {
    LISAUI_SETTING_WAKEUP_OP_GET = 0,
    LISAUI_SETTING_WAKEUP_OP_SET,
} lisaui_setting_wakeup_mode_op_t;

typedef lisaui_err_t (*lisaui_app_setting_set_wakeup_mode_cb_t)(lisaui_setting_wakeup_mode_op_t operation, void *param);

lisaui_err_t lisaui_app_setting_register_set_wakeup_mode_handler(lisaui_app_setting_set_wakeup_mode_cb_t handler);

lv_obj_t *private_lisaui_app_view_create_wakeup_config(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // __APP_SETTING_VIEW_WAKEUP_CONFIG_H__
