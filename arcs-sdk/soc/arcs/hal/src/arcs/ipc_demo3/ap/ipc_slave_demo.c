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
#ifdef GPIO_BASED_DEBUG
#include "IOMuxManager.h"
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



#define AMP_CP_START_ADDRESS            0x30010000



void start_cp(int32_t addr)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = addr;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

int main(void)
{

    logInit(SHELL_UART1, SHELL_UART1_BAUDRATE);
#ifdef PSRAM_HEAP
    PSRAM_Initialize(NULL, NULL, 1);
#endif
    ipc_mem_init(0);
    ic_lock_init();
    ipc_slave_init(NULL);

    start_cp(AMP_CP_START_ADDRESS);

    rtos_start_scheduler();
    return 0;
}
