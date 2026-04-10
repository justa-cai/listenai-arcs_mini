/*
 * bt_stack_if.c
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
#include "bt_stack_hal.h"
#include "bt_classic_hal.h"

#if BT_STACK_PRESENT
#include "bt_a2dp.h"
#include "bt_music_hal.h"
#include "bt_hfp.h"
#include "bt_call_hal.h"

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

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */

bool bt_stack_classic_connected(void)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    return stack_env->bt_classic_connected;
}

void bt_stack_bt_scan(uint8_t scan_en)
{
    bt_classic_scan_enable(scan_en);
}

void bt_stack_bt_inquiry(uint8_t disc_mode, uint8_t max_count)
{
    bt_gap_discover_start(disc_mode, max_count, false);
}

void bt_stack_bt_inquiry_stop(void)
{
    bt_gap_discover_stop();
}

void bt_stack_bt_connect(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode)
{
    bt_gap_connect(addr, type, clk_off, page_scan_rep_mode);
}

void bt_stack_bt_connect_cancel(void)
{
    bt_gap_connect_cancel();
}
#endif

