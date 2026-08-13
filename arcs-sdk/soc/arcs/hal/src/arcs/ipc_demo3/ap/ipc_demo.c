/*
 * ipc_slave_demo.c
 *
 *  Created on: 2025-11-07
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
#ifdef PSRAM_HEAP
#include "PSRAMManager.h"
#endif
#include "log_print.h"
#include "shell_def.h"

#if IC_BOARD == 1
#include "spiflash.h"
#include "nvs.h"
#endif

#include "rtos_al.h"
#include "ipc.h"
#include "ic_lock.h"
#if CONFIG_PM
#include "pm.h"
#include "vrtc.h"
#endif



#define AMP_CP_START_ADDRESS            0x30010000

extern void luna_demo_init(void);
extern void luna_demo_task_start(void);
extern int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search);

void start_cp(int32_t addr)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = addr;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

static void app_init_task(void *pvParameters)
{
#if CONFIG_PM
    pm_init();
    pm_register_gpio_retention(UART1_IO_TX_PAD, UART1_IO_TX_PIN);
    pm_register_gpio_retention(UART1_IO_RX_PAD, UART1_IO_RX_PIN);

    vrtc_init();
#endif

    luna_demo_task_start();

    rtos_task_delete(NULL);
}

int main(void)
{
    logInit(SHELL_UART1, SHELL_UART1_BAUDRATE);
#ifdef CONFIG_PM_PSRAM
    PSRAM_Initialize(NULL, NULL, 1);
#endif
    ipc_mem_init();
    ic_lock_init();
    ipc_master_init();
    luna_demo_init();

    start_cp(AMP_CP_START_ADDRESS);

    rtos_task_create(app_init_task, "app_init_task",
            APP_INIT_TASK, 1024, NULL, configMAX_PRIORITIES - 2, NULL);

    rtos_start_scheduler();
    return 0;
}
