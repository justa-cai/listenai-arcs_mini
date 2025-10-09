/**
 * @file app_wifi.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_APP_WIFI_H__
#define __LISAUI_APP_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../app_common.h"
#include "lisaui_app_common.h"
#include "wifi_type.h"

typedef enum {
    LISAUI_WIFI_STATE_AP_SCAN_FINISH = 0,
    LISAUI_WIFI_STATE_STA_CONNECT_SUCCESS,
    LISAUI_WIFI_STATE_STA_CONNECT_FAILED

} LISAUI_WIFI_STATE_e;

lisaui_err_t lisaui_app_update_state(LISAUI_WIFI_STATE_e state, lisaui_wifi_list_t *hotspot_list);
lisaui_err_t lisaui_app_add_wifi_list_item(lv_obj_t *list, wifi_metadata_t *wifi_info);
lisaui_err_t app_wifi_init(void);

typedef lisaui_err_t (*app_wifi_event_handler_t)(lisaui_wifi_item_t type, lisaui_wifi_op_t op, void *params);
lisaui_err_t lisaui_app_wifi_register_event_handler(app_wifi_event_handler_t handler);


#ifdef __cplusplus
}
#endif

#endif // __LISAUI_APP_WIFI_H__
