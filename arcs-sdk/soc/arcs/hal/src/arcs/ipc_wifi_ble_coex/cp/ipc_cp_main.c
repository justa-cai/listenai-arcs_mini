
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "chip.h"
#include "rtos_al.h"
#include "wlif.h"
#include "log_print.h"
#include "shell_def.h"
#include "ipc.h"
#include "ipc_master_wifi.h"
#include "ipc_master_bt.h"
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
#include "bt_ipc_api.h"
#include "atcmd.h"
#include "arcs_ap.h"

#include "bt_stack_hal.h"
#include "aud_os_task.h"
#include "bt_os_task.h"
//#include "aud_if.h"
#include "os_task_init.h"

#define TIMER_23BITS_MASK  ((1<<24)-1)
#define TIMER_GET_23BITS_TIME_STAMP(_time) ((_time) & TIMER_23BITS_MASK)

#ifndef NULL
#define NULL                        (void *)0
#endif


#define AMP_CP_START_ADDRESS            0x30180000

extern int bt_event_cb(void *arg, event_module_t event_module,int event_id, void *event_data);
extern void bt_ipc_host_c2h_handler(const uint8_t *data, uint16_t len);
extern uint8_t os_task_init(uint8_t *args);

extern os_task_cb_t *aud_if_get_cb(void);
extern os_task_cb_t *aud_pro_if_get_cb(void);
extern void aud_pro_os_init(os_task_cb_t *cb);
typedef int32_t (*hci_ipc_send_t)(const uint8_t *buf, uint16_t len);
extern void hci_ipc_register(hci_ipc_send_t c2h, hci_ipc_send_t h2c);
extern int32_t ipc_master_bt_h2c_send(const uint8_t *data, uint16_t len);
extern uint8_t bt_send_schedule_notify(void);

void bt_disable_GINT()
{
    disable_GINT();;
}

void bt_enable_GINT()
{
    enable_GINT();
}

typedef void (*timer_cb) (void);
typedef void (*os_timer_cb) (void *time_id);

/// timer environment structure
typedef struct timer_env_
{
    /// timer id
    void *timer_id;

    /// callback to call when timer expires
    timer_cb timeout_cb;
    /// Callback to call periodically
    timer_cb periodic_cb;
} timer_env_t;

/// timer environment structure
timer_env_t timer_env;

void time_cb(void *time_id)
{
    if(timer_env.timeout_cb != NULL)
        timer_env.timeout_cb();
    bt_send_schedule_notify();
}

void timer_init(void)
{
    timer_env.timer_id = NULL;

    timer_env.periodic_cb = NULL;
    timer_env.timeout_cb  = NULL;
}

void timer_set_timeout(uint32_t to, timer_cb cb)
{
    if(timer_env.timer_id != NULL)
        btos_timer_cancel(timer_env.timer_id);
    timer_env.timer_id = NULL;
    if(to != 0 && cb != NULL)
    {
        timer_env.timeout_cb = cb;
        timer_env.timer_id = btos_timer_creat(0, to, (TimerCallbackFunction_t)time_cb);
    }
}

uint32_t timer_get_time(void)
{
    uint32_t time_us;
    uint32_t sec = 0, usec = 0;
    btos_get_time(&sec, &usec);

    time_us = sec*1000000 + usec;
    return time_us;
}

uint32_t timer_get_time_ms(void)
{
    uint32_t time_ms;
    uint32_t sec = 0, msec = 0;
    btos_get_time_ms(&sec, &msec);

    time_ms = sec*1000 + msec;
    return time_ms;
}

extern void *btos_malloc(uint32_t size);
ls_err_t cp_btos_malloc_api(void **buffer_ptr, uint32_t size)
{
	if(buffer_ptr == NULL)
	{
		return LS_FAIL;
	}
    *buffer_ptr = btos_malloc(size);

    CLOGI("cp_btos_malloc_api 0x%x", *buffer_ptr);

    return LS_OK;
}

extern void btos_free(void *ptr);
ls_err_t cp_btos_free_api(void **ptr)
{
    btos_free(*ptr);

    return LS_OK;
}


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
        CLOGI("event <%d %d>  connect fail:%d max retry reach %d \n", event_module, event_id, conn_fail_evt->status_code, conn_fail_evt->max_retry_reach);
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

int bt_demo_init(void)
{
    //patch_func_ptr = patch_func_ptr_default;

    /*
     ************************************************************************************
     * Platform initialization
     ************************************************************************************
     */

    //logInit(1, 1000000);
    CLOGD("Enter %s \r\n", __func__);

    // Initialize UART component

    /// os task init
    //app_os_init(app_if_get_cb());
    bt_os_init((os_task_cb_t *)bt_stack_if_get_cb());
    //aud_os_init((os_task_cb_t *)aud_if_get_cb());
    //aud_pro_os_init((os_task_cb_t *)aud_pro_if_get_cb());
    os_task_init(NULL);

    return 0;
}

static void app_init_task(void *pvParameters)
{

#if CFG_NVS
    CLOGD("Init NVS");
    arcs_nvs_init();
#endif
    CLOGD("Done");
#ifdef CFG_AMP_IPC_WIFI_CHAN
    wlif_start();
#endif

    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, wifi_event_cb, NULL);
    ls_event_register_cb(EVENT_BT,   EVENT_ID_ALL, bt_event_cb,   NULL);
#if CONFIG_PM
    vrtc_init();
#endif

#ifdef CFG_ATCMD
    atcmd_init();
#endif

    bt_demo_init();

    shell_init(cli_shell_process);

    rtos_task_delete(NULL);
}


/*
 * MAIN FUNCTION
 ****************************************************************************************
 */
int main(void)
{
    struct ipc_master_wifi_ops ipc_wifi_ops = {
            .tx_data_cfm = wlif_tx_cfm,
            .rx_data = wlif_rx_buf_forward,
    };

    logInit(SHELL_UART0, SHELL_UART0_BAUDRATE);
    ic_lock_init();
    ipc_master_init();
    ipc_master_wifi_init(&ipc_wifi_ops);
    ipc_master_bt_init();
    hci_ipc_register(NULL, ipc_master_bt_h2c_send);

    rtos_task_create(app_init_task, "app_init_task",
            APP_INIT_TASK, 256, NULL, configMAX_PRIORITIES-1, NULL);

    rtos_start_scheduler();

    /* Will only get here if there was insufficient memory to create the idle task. */
    for( ;; );

    return 0;
}
