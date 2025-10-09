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

#include "bt_stack_if.h"

#if BT_STACK_PRESENT
#include "bt_a2dp.h"
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
extern struct ble_rf_api lsip_rf;

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
void bt_stack_classic_enable_cmp(uint16_t status)
{
    uint16_t flags = 0;
    uint16_t uuid[2];
    uint8_t len = GAP_BD_ADDR_LEN;

    //uuid[0] = HID_UUID;
    // start adv
    gap_bdaddr_t peer = {0};

    bt_stack_a2dp_enable(BT_A2DP_SINK);
    CLOGD("bt classic enable cmp, status:%d", status);
}

void bt_stack_classic_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    stack_env->bt_classic_connected = 1;
    
    CLOGD("BT classic connected..................................");

    bt_classic_scan_enable(BT_GAP_SCAN_DIS);

    //bt_a2dp_setup(BT_A2DP_SINK);
    /// save last device address
    //nvds_put(NVS_ID_BT_PEER_ADDRESS, GAP_BD_ADDR_LEN, peer_addr->addr);
}

void bt_stack_classic_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    stack_env->bt_classic_connected = 0;
    uint16_t flags = 0;
    uint8_t disc = 0;
    uint16_t uuid[2];
    uint8_t len = GAP_BD_ADDR_LEN;
    // start adv
    gap_bdaddr_t peer = {0};

    bt_classic_scan_enable(3);

    CLOGD("DISCONNECT BDADDR: 0x%x%x%x%x%x%x, reason:0x%x", peer.addr[0], peer.addr[1], peer.addr[2], \
            peer.addr[3], peer.addr[4], peer.addr[5], reason);
}

void bt_stack_classic_key_req(uint8_t conidx, uint8_t key_type, uint32_t key)
{
    //ble_gap_key_cfm(conidx, 1, 123456);
}

void bt_stack_classic_bond_ind(uint8_t conidx, uint16_t status)
{

}

void bt_stack_classic_discover_ind(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, uint8_t mode, uint32_t cod, struct gap_dev_name *name)
{
    CLOGD("bt classic discover ind");
}

void bt_stack_classic_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data)
{

}


bool bt_stack_classic_connected()
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    return stack_env->bt_classic_connected;
}
#endif

