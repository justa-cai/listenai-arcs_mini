/*
 * cli_demo.c
 *
 *  Created on: 2024-10-16
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <stdbool.h>          // standard boolean definitions
#include <stdint.h>           // standard integer functions
#include <string.h>

#include "arcs_ap.h"
#ifdef GPIO_BASED_DEBUG
#include "IOMuxManager.h"
#endif
#include "log_print.h"
#include "shell_def.h"
#include "rf_drv.h"
#include "rf_cali.h"
#ifdef SYS_PSM
#include "vrtc.h"
#endif
#if IC_BOARD == 1
#include "spiflash.h"
#include "nvs.h"
#endif

#include "rtos_al.h"
#include "atcmd.h"
#include "ls_wifi_type.h"
#include "ls_bt_type.h"
//#include "atcmd_bt_if.h"
#include "wifi_api.h"
#include "ls_event.h"
#include "cli_main.h"
#include "net_al.h"
#include "net_ip.h"

/**
 ****************************************************************************************
 * @addtogroup DRIVERS
 * @{
 *
 *
 * ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/*
 * MAIN FUNCTION
 ****************************************************************************************
 */
#if IC_BOARD == 1 && (RFCALI_BT_EN == 1 || RFCALI_WF_EN == 1)
    P_RF_CALI_PARAMS cali_params_ptr = &rf_cali.params;
#endif
extern uint8_t _sshram[], _eshram[];
extern int bt_demo_init(void);
extern int bt_event_cb(void *arg, event_module_t event_module,int event_id, void *event_data);
extern int wifi_cli_exec_sta_auto_conn(void);
int wifi_event_cb(void *arg, event_module_t event_module,
                  int event_id, void *event_data)
{
    event_ap_sta_add_param_t *sta_add_param;
    event_ap_sta_del_param_t *sta_del_param;
    event_connect_fail_param_t *conn_fail_evt;
    event_disconnect_param_t *disc_evt;

    switch (event_id)
    {
        case EVENT_WIFI_INIT_DONE:
        CLOGI("event <%d %d>  wifi init done\n", event_module, event_id);

        //if sta_autoconn flag and ssid/pwd setted in flash, try to auto connect ap
        if (wifi_cli_exec_sta_auto_conn() == 0)
        {
            CLOGI("sta mode auto connect\n");
            break;
        }

        break;
        case EVENT_WIFI_CONNECTED:
        net_if_t *net_if;
        CLOGI("event <%d %d>  connected \n", event_module, event_id);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip)
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_GOT_IP:
        CLOGI("event <%d %d>  IP obtained \n", event_module, event_id);
        break;
        case EVENT_WIFI_STA_DHCP_FAIL:
        CLOGI("event <%d %d>  DHCP FAILED \n", event_module, event_id);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_DISCONNECT:
        disc_evt = (event_disconnect_param_t *)event_data;
        CLOGI("event <%d %d>  disconnected:%d \n", event_module, event_id, disc_evt->reason_code);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;
        case EVENT_WIFI_SCAN_DONE:
        CLOGI("event <%d %d>  scan done \n", event_module, event_id);
        break;
        case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        CLOGI("event <%d %d>  connect fail:%d \n", event_module, event_id, conn_fail_evt->reason_code);
        break;
        case EVENT_WIFI_AP_STARTED:
        CLOGI("event <%d %d>  ap_started \n", event_module, event_id);
        net_if_up(net_if_get(WIFI_VIF_AP_IDX));
        // start DHCPS
        ls_dhcps_start(WIFI_VIF_AP_IDX);
        break;
        case EVENT_WIFI_AP_STA_ADD:
        sta_add_param = (event_ap_sta_add_param_t *)event_data;
        CLOGI("event <%d %d>  ap_sta_add:%d\n", event_module, event_id, sta_add_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STA_DEL:
        sta_del_param = (event_ap_sta_del_param_t *)event_data;
        CLOGI("event <%d %d>  ap_sta_del:%d \n", event_module, event_id, sta_del_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STOPPED:
        CLOGI("event <%d %d>  ap_stopped \n", event_module, event_id);
        ls_dhcps_stop();
        net_if_down(net_if_get(WIFI_VIF_AP_IDX));
        break;
        default:
        CLOGI("rx event <%d %d>\n", event_module, event_id);
        break;
    }


    return LS_OK;
}


#if CFG_NVS
#define NVDS_FLASH_ADDRESS   (CMN_FLASH_REGION + 0x180000) //offset 1280KB
#define NVDS_FLASH_SIZE      (0x8000) //32KB
struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

int arcs_nvs_init(void)
{
    struct flash_pages_info info;

#ifdef CFG_FLASH_IF
    flash_if_init(&arcs_flash_dev, 0, 0);
#else
    flash_init(&arcs_flash_dev, 0, 0);
#endif

    //flash_write_protection_set(&arcs_flash_dev, false);
    //flash_erase(&arcs_flash_dev, NVDS_FLASH_ADDRESS, NVDS_FLASH_SIZE);
    //flash_write_protection_set(&arcs_flash_dev, true);

    //init nvs module
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE/info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

int main(void)
{
#ifdef CFG_ATCMD
    atcmd_init();
#endif
    // shell_init(cli_shell_process);
    //logInit(1, 1000000);

#if CFG_NVS
    arcs_nvs_init();
#endif

    ls_wifi_init();

    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_BT,   EVENT_ID_ALL, bt_event_cb,   NULL);


    bt_demo_init();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    return 0;
}

