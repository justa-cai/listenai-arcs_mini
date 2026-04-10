/*
 * bt_app_if.c
 *
 *  bt stack interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "log_print.h"
#include "nvs.h"

//#include "plf.h"
#include "bt_config.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"

#include "bt_stack_cfg.h"
#include "bt_ble_if.h"
#include "bt_app_if.h"
#include "bt_os_task.h"

#include "bt_stack_hal.h"
#include "bt_app_hal.h"
#include "lisa_bluetooth.h"
#include "ble_adv_data.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"
#include "bt_ble_hal.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern app_handler_by_user_cb_t app_bt_user_handler;

static const uint8_t default_adv_data[] = {
    BLE_AD_FLAGS(GAP_AD_TYPE_FLAGS_GENERAL | GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED),
    BLE_AD_COMPLETE_NAME(2, 'P', 'G'),
};

static const uint8_t default_scan_rsp_data[] = {
    BLE_AD_UUID16_COMPLETE(2, 0x12, 0x18),
};

__attribute__((weak))
const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(default_adv_data);
    return default_adv_data;
}

__attribute__((weak))
const uint8_t* lisa_bt_get_scan_rsp_data(uint8_t *len)
{
    *len = sizeof(default_scan_rsp_data);
    return default_scan_rsp_data;
}

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
 
/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

uint8_t app_ble_netcfg_bles_send_notify_user_handler(ble_net_cfg_info_t *netcfg_info)
{
    uint8_t status = 0;
    status = ble_netcfg_bles_send_notify(netcfg_info->conidx, netcfg_info->op, netcfg_info->status);

    return status;
}

void app_user_bt_handler_init(void)
{
    memset(&app_bt_user_handler, 0x00, sizeof(app_bt_user_handler));
    app_bt_user_handler.cb_app_ble_netcfg_bles_send_notify_handler = app_ble_netcfg_bles_send_notify_user_handler;
}

/* Classic BT callbacks (bt_a2dp_cb, bt_hfp_cb, bt_avrcp_cb, GAP callbacks)
 * are provided by bt_classic_user.c - do NOT duplicate here.
 */

__attribute__((weak))
void atcmd_ble_enc_clear_send(void)
{
}
