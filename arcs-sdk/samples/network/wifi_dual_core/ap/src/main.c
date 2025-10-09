

#include "FreeRTOS.h"
#include "task.h"

#include "arcs_ap.h"

#include "memap.h"
#include "lisa_log.h"

#include "ipc.h"
#include "net_al.h"
#include "wifi_api.h"
#include "vrtc.h"
#include "rf_cali.h"
#include "ic_lock.h"
#include "ls_misc.h"

#include <sys/times.h>
#include <stdio.h>
#include <string.h>

static struct wifi_ops ops = {
    .get_mac = NULL,
    .temp_update = ls_temp_por_update,
};

extern void ls_crypto_init(void);

clock_t _times_r(struct _reent *reent, struct tms *buf) {
    (void)reent;
    (void)buf;
    return (clock_t)-1; // Standard return value for unimplemented function
}

static void user_wifi_pre_init(void) {
    struct ipc_slave_cb_tag ipc_cb = {
        .ipc_wifi_tx = net_ipc_send,
        .ipc_wifi_rx_cfm = net_ipc_rx_cfm,
    };

    ipc_mem_init(1);
    ic_lock_init();
    ipc_slave_init(&ipc_cb);
    extern uint8_t _sshram[], _eshram[];
    memset(_sshram, 0, (_eshram - _sshram));
}

static void user_wifi_init(void)
{
    ls_crypto_init();

    log_print_level_set(CLOG_LEVEL_DEBUG);
    ls_rf_cali_proc();
    log_print_level_set(CLOG_LEVEL_INFO);

    wifi_ops_register(&ops);
    wifi_init();

    vrtc_init();
}


int main(int argc, char **argv)
{
    user_wifi_pre_init();

#if CONFIG_ARCS_AP_CORE
    LOGI("boot cp from address: 0x%x", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif

    vTaskDelay(pdMS_TO_TICKS(500));
    user_wifi_init();


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

    } 
    return 0;
}
