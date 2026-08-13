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

#include "bt_stack_hal.h"

#include "bt_stack_cfg.h"
#include "bt_ble_if.h"
#include "bt_app_if.h"
#include "atcmd_bt_if.h"

/*
 * MACROS
 ****************************************************************************************
 */


/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
struct plf_sys_config bt_stack_plf_cfg =
{
    .hcit_feat = PLF_BUILD_FEAT_HCIT,
    .core_feat = PLF_BUILD_FEAT_CORE,
    .stack_feat = PLF_BUILD_FEAT_STACK,
    .msg_heap = 5*1024,
    .env_heap = 10*1024,
    .big_buf = 1024,
    .small_buff = 128,
    .big_nb = 4,
    .small_nb = 12,
};

ble_gap_cfg_t bt_stack_dev_cfg = {
    .addr = {{0x44, 0x55, 0x66, 0x03, 0x23, 0x20}, 0},
    .privacy_cfg = 0,
    .name_len = sizeof(DEVICE_NAME),
    .name = DEVICE_NAME,
    .appearance = GAP_APP_GENERIC_MEDIA_PLAYER, // hid_keyboard
    .iocap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
    .auth = GAP_SEC_NOT_ENC,
    .pairing_mode = GAPM_PAIRING_LEGACY,
    .renew_dur = 0x96, //update time 150s.
    .att_cfg = GAP_ATT_CLI_DIS_AUTO_FEAT_EN_MASK|GAP_ATT_CLI_DIS_AUTO_EATT_MASK|GAP_ATT_CLI_DIS_AUTO_MTU_EXCH_MASK,
};

#if BT_STACK_PRESENT
bt_gap_cfg_t bt_stack_classic_dev_cfg = {
    .cod = GAP_APP_HANDSFREE,
    .discover_mode = GAPM_GEN_DISCOVERABLE,
    .connect_mode = GAPM_CONNECTABLE,
    .iscan_interval = 1280,
    .pscan_interval = 1280,
};
#endif

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern struct ble_rf_api lsip_rf;
extern const ble_gap_cb_t bt_stack_gap_cb;

#if BT_STACK_PRESENT
extern const bt_gap_cb_t bt_stack_classic_gap_cb;
#endif

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
void bt_stack_gen_addr(ble_gap_cfg_t *bt_cfg)
{
    uint8_t len = GAP_BD_ADDR_LEN;

    if(bt_stack_nvs_get(NVS_ID_BD_ADDRESS, &len, bt_cfg->addr.addr) != NVDS_OK)
    {
        bt_cfg->addr.addr[0] = (uint8_t)rand();
        bt_cfg->addr.addr[1] = (uint8_t)rand();
        bt_cfg->addr.addr[2] = (uint8_t)rand();
        bt_stack_nvs_set(NVS_ID_BD_ADDRESS, GAP_BD_ADDR_LEN, bt_cfg->addr.addr);
        if(bt_stack_nvs_set(NVS_ID_BD_ADDRESS, GAP_BD_ADDR_LEN, bt_cfg->addr.addr)==NVDS_OK)
        {
            ble_gap_set_loc_pub_addr(bt_cfg->addr.addr);
        }
    }
    CLOGD("bt reset cmp: %02x:%02x:%02x:%02x:%02x:%02x",
                        bt_cfg->addr.addr[5],bt_cfg->addr.addr[4],bt_cfg->addr.addr[3],
                        bt_cfg->addr.addr[2],bt_cfg->addr.addr[1],bt_cfg->addr.addr[0]);
}
void bt_stack_cfg_enable(uint16_t status)
{

    ble_gap_cb_t  *bt_gap_cb = (ble_gap_cb_t  *)&bt_stack_gap_cb;
    ble_gap_cfg_t *bt_cfg = &bt_stack_dev_cfg;

    ///gen bt addr
    bt_stack_gen_addr(bt_cfg);
    ble_gap_enable(bt_cfg, bt_gap_cb);
#if BT_STACK_PRESENT
    {
        bt_gap_cfg_t * bt_cfg = (bt_gap_cfg_t *)&bt_stack_classic_dev_cfg;
        bt_gap_cb_t  * bt_gap_cb = (bt_gap_cb_t  *)&bt_stack_classic_gap_cb;

        bt_gap_enable(bt_cfg, bt_gap_cb);
    }
#else///wait ble and bt enable cmp.
    #if WHITE_LIST_ADD
    bt_stack_ble_add_paired_to_wlist();
    #endif
    #if RESOVLE_LIST_ADD
    ble_gap_add_paired_rpa_to_rlist();
    #endif
#endif
}
void bt_stack_reset_cmp(uint16_t status)
{
	app_user_bt_handler_init();
    bt_stack_cfg_enable(status);
}
