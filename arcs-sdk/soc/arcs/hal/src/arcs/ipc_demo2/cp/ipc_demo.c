
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "chip.h"
#include "rtos_al.h"
#include "wlif.h"
#include "log_print.h"
#include "shell_def.h"
#include "ipc.h"
#include "ls_wifi_type.h"
#include "wifi_api.h"
#include "ls_event.h"
#include "cli_main.h"
#include "spiflash.h"
#include "nvs.h"
#include "net_al.h"
#include "net_ip.h"
#include "flash_if.h"
#include "ic_lock.h"

#define AMP_CP_START_ADDRESS            0x30100000


void start_cp(int32_t addr)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = addr;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

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
        wlif_netif_up(WIFI_VIF_STA_IDX, VIF_STA);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
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
        CLOGI("event <%d %d>  disconnected:%d max retry reach %d \n", event_module, event_id, disc_evt->reason_code, disc_evt->max_retry_reach);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        wlif_netif_down(WIFI_VIF_STA_IDX);
        break;
        case EVENT_WIFI_SCAN_DONE:
        CLOGI("event <%d %d>  scan done \n", event_module, event_id);
        break;
        case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        CLOGI("event <%d %d>  connect fail:%d max retry reach %d \n", event_module, event_id, conn_fail_evt->reason_code, conn_fail_evt->max_retry_reach);
        break;
        case EVENT_WIFI_AP_STARTED:
        CLOGI("event <%d %d>  ap_started \n", event_module, event_id);
        wlif_netif_up(WIFI_VIF_AP_IDX, VIF_AP);
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
        wlif_netif_down(WIFI_VIF_AP_IDX);
        break;
        default:
        CLOGI("rx event <%d %d>\n", event_module, event_id);
        break;
    }

    return LS_OK;
}
#if CFG_NVS
static FLASH_DEV remote_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xff,  // 0 means divider=2 //0xff,  //0xff means divider=1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
};

#define NVDS_FLASH_ADDRESS   (CMN_FLASH_REGION + 0x300000) //offset 3072KB
#define NVDS_FLASH_SIZE      (0x8000) //32KB
struct nvs_fs arcs_nvs_fs;
int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&remote_flash_dev, 0, 0);

    //flash_write_protection_set(&remote_flash_dev, false);
    //flash_erase(&remote_flash_dev, NVDS_FLASH_ADDRESS, NVDS_FLASH_SIZE);
    //flash_write_protection_set(&remote_flash_dev, true);

    //init nvs module
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS;
    arcs_nvs_fs.flash_device = &remote_flash_dev;
    flash_get_page_info_by_offs(&remote_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE/info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

static void app_init_task(void *pvParameters)
{
    CLOGD("Init NVS");
#if CFG_NVS
    arcs_nvs_init();
#endif
    CLOGD("Done");
    ipc_wifi_init();
    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL);

    shell_init(cli_shell_process);

    rtos_task_delete(NULL);
}


/*
 * MAIN FUNCTION
 ****************************************************************************************
 */
int main(void)
{
    struct ipc_master_cb_tag ipc_cb = {
            .wifi_tx_data_cfm = wlif_tx_cfm,
            .wifi_rx_data_ind = wlif_rx_buf_forward,
            .indication_handler = ipc_indication_handler
    };

    logInit(SHELL_UART0, SHELL_UART0_BAUDRATE);
    ic_lock_init();
    ipc_master_init(&ipc_cb);

    rtos_task_create(app_init_task, "app_init_task",
            APP_INIT_TASK, 256, NULL, configMAX_PRIORITIES-1, NULL);

    rtos_start_scheduler();

    /* Will only get here if there was insufficient memory to create the idle task. */
    for( ;; );

    return 0;
}
