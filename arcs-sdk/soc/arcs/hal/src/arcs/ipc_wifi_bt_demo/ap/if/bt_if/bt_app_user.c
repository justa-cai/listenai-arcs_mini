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

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"

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
#if (ADV_USER_DATA)
const uint8_t adv_user_data[] = {0x02, 0x01, 0x05, 0x05, 0xFF, 0x66, 0x79, 0x30, 0x02, 0x12, 0x09, 0x43, 0x4d, 0x43, 0x43, 0x5f, 0x56, 0x6f, 0x69, 0x63, 0x65, 0x5f, 0x52, 0x65, 0x6d, 0x6f, 0x74, 0x65};
const uint8_t adv_user_data_size = sizeof(adv_user_data)/sizeof(adv_user_data[0]);
#endif
extern app_handler_by_user_cb_t app_bt_user_handler;

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
void app_user_bt_handler_init(void)
{
    memset(&app_bt_user_handler, 0x00, sizeof(app_bt_user_handler));
}
