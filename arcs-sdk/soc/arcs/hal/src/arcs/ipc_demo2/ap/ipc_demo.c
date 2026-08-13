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
#include "IOMuxManager.h"
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
#ifdef PSRAM_HEAP
#include "PSRAMManager.h"
#endif

#include "rtos_al.h"
#include "atcmd.h"
#include "ls_wifi_type.h"
#include "wifi_api.h"
#include "ls_event.h"
#include "cli_main.h"
#include "ipc.h"
#include "ipc_slave_wifi.h"
#include "net_al.h"
#include "spiflash.h"
#include "ic_lock.h"
#if CONFIG_PM
#include "pm.h"
#endif

extern void ls_wifi_init(void);
extern void ls_crypto_init(void);
extern int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search);
#if CONFIG_PM_PSRAM
extern int32_t _sstack;
#endif
static void app_init_task(void *pvParameters)
{
#if CONFIG_PM
    pm_init();
    pm_register_gpio_retention(UART1_IO_TX_PAD, UART1_IO_TX_PIN);
    pm_register_gpio_retention(UART1_IO_RX_PAD, UART1_IO_RX_PIN);
#if CONFIG_PM_PSRAM
    pm_register_snapshot_region(0x00100000, (int32_t)&_sstack - 0x00100000, 0);
#endif
    vrtc_init();
#endif

#if IC_BOARD == 1
    ls_rf_cali_proc();
#endif
    ls_crypto_init();

    ls_wifi_init();

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
    struct ipc_slave_wifi_ops ipc_wifi_ops = {
            .tx = net_ipc_send,
            .rx_cfm = net_ipc_rx_cfm,
    };

    logInit(SHELL_UART1, SHELL_UART1_BAUDRATE);

#if defined(CONFIG_PM_PSRAM) || defined(PSRAM_HEAP)
    PSRAM_Initialize(NULL, NULL, 1);
#endif
    ipc_mem_init();
    ic_lock_init();
    //start_cp(AMP_CP_START_ADDRESS);

    ipc_slave_init();
    ipc_slave_wifi_init(&ipc_wifi_ops);

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
