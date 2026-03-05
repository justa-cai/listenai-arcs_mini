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
#include "ipc_slave.h"
#include "net_al.h"

extern uint8_t _sshram[], _eshram[];

int main(void)
{
    struct ipc_slave_cb_tag ipc_cb = {
            .ipc_wifi_tx = net_ipc_send,
            .ipc_wifi_rx_cfm = net_ipc_rx_cfm,
    };
    memset(_sshram, 0, (_eshram - _sshram));
    ipc_slave_init(&ipc_cb);

    logInit(SHELL_UART1, SHELL_UART1_BAUDRATE);
#if IC_BOARD == 1
    ls_rf_cali_proc();
#endif

    ls_crypto_init();

    ls_wifi_init();

#ifdef SYS_PSM
    vrtc_init();
#endif

    rtos_start_scheduler();

    return 0;
}

