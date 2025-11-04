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
#include "wifi_api.h"
#include "ls_event.h"
#include "cli_main.h"
#include "net_al.h"
#include "net_ip.h"

#include "spiflash.h"
#include "wf_soc_drv.h"
#include "ota_config.h"
#include "ota.h"
#include "spiflash.h"
#include "ipc.h"
#ifdef CFG_AMP_IPC_MRPC_SERVER
#include "mrpc.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
#include "mrpc_flash_if_api_server.h"
#endif

#include "ic_lock.h"

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
extern uint8_t _sshram[], _eshram[];
extern const ls_ota_header_t cp_main_header;
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
        CLOGI("event <%d %d>  wifi init done, OTA version=0x%x\n", event_module, event_id, cp_main_header.version.version);

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
#define NVDS_FLASH_ADDRESS   0x1c000000  //(CMN_FLASH_REGION + 0x140000) //offset 1280KB
#define NVDS_FLASH_SIZE      OTA_ZONE_USER_SIZE // (0x8000) //32KB

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

    flash_if_init(&arcs_flash_dev, 0, 0);

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
#ifdef CFG_AMP_IPC_MRPC_SERVER
    struct mrpc_server_env *mrpc_server;
#endif
    memset(_sshram, 0, (_eshram - _sshram));

#if SHRINK==1
    memset(sram_start, 0, (sram_end - sram_start));
    memcpy(__text2_start__, _etext, (_etext2 - _etext));
    memcpy(__text3_start__, _etext2, (_etext3 - _etext2));
#endif
    logInit(SHELL_UART0, SHELL_UART0_BAUDRATE);
    ic_lock_init();
    ipc_slave_init(NULL);
    shell_init(cli_shell_process);

#if CFG_NVS
    arcs_nvs_init();
#endif
#if IC_BOARD == 1
    ls_rf_cali_proc();
#endif

    ls_crypto_init();
#ifdef CONFIG_TRACE
    vTraceEnable(TRC_START);
#endif

    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL);

    ls_wifi_init();

#ifdef CFG_AMP_IPC_MRPC_SERVER
    mrpc_server = mrpc_server_init(IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_CMN_SRV);
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_FLASH_IF, mrpc_msg_flash_if_handlers, MRPC_MSG_ID_FLASH_IF_MAX);
#endif
#endif

#ifdef SYS_PSM
    vrtc_init();
#endif

    rtos_start_scheduler();
    return 0;
}

extern void _start();

/// sign data size: CRC32 - 4, SHA256 - 32, ECSDA256 - 128, RSA2048 - 512
SIGN_DATA const uint32_t sign_data[512/4] =
{
        0
};


OTA_HEADER const ls_ota_header_t cp_main_header = {
        .valid_flag = 0xffffffff,
        .version = {.vendor_id = OTA_VENDOR_ID,
                    .device_id = OTA_DEVICE_ID,
                    .flash_id  = 1,
                    .zone_id   = OTA_ZONE_ID_CP,
                    .rom_ver   = 0x0100,
                    .version   = 0x01010001,
                    .date      = BUILD_DATE},
        .flags   = OTA_MODE,
        .address = &cp_main_header,
        .entry   = _start,
};
