/*
 * ipc_demo.c
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
#include "ipc.h"
#include "net_al.h"
#include "spiflash.h"
#include "ic_lock.h"
#if CONFIG_PM
#include "pm_impl.h"
#endif
static void app_init_task(void *pvParameters)
{
#if IC_BOARD == 1
    ls_rf_cali_proc();
#endif
    ls_crypto_init();

    ls_wifi_init();

#if CONFIG_PM
    pm_init();
#endif
#if (defined(CONFIG_PM) || SYS_PSM)
    vrtc_init();
#endif
    rtos_task_delete(NULL);
}

#define AMP_CP_START_ADDRESS            0x30100000


extern uint8_t _sshram[], _eshram[];

void start_cp(int32_t addr)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = addr;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

int main(void)
{
    struct ipc_slave_cb_tag ipc_cb = {
            .ipc_wifi_tx = net_ipc_send,
            .ipc_wifi_rx_cfm = net_ipc_rx_cfm,
    };

    logInit(SHELL_UART1, SHELL_UART1_BAUDRATE);
#ifdef PSRAM_HEAP
    PSRAM_Initialize(NULL, NULL, 1);
#endif
    ipc_mem_init(1);
    ic_lock_init();
    //start_cp(AMP_CP_START_ADDRESS);

    ipc_slave_init(&ipc_cb);

    memset(_sshram, 0, (_eshram - _sshram));
    start_cp(AMP_CP_START_ADDRESS);
#if 0
#if IC_BOARD == 1
    ls_rf_cali_proc();
#endif
    ls_crypto_init();

    ls_wifi_init();
#endif

    rtos_task_create(app_init_task, "app_init_task",
            APP_INIT_TASK, 1024, NULL, configMAX_PRIORITIES-2, NULL);

    rtos_start_scheduler();
    return 0;
}
