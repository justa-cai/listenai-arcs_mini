#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "Driver_GPIO.h"

#include "sysheap.h"
#include "cJSON.h"
#include "arcs_flash_if.h"
#include "sdmmc_init.h"
#include "user_fs.h"
#include "lisa_kv.h"

#include "user_wifi.h"
#include "lisa_log.h"
#include "get_tests.h"
#include <sys/times.h>

clock_t _times_r(struct _reent *reent, struct tms *buf) {
    (void)reent;
    (void)buf;
    return (clock_t)-1; // Standard return value for unimplemented function
}

int main(int argc, char **argv)
{
    user_wifi_pre_init();

    // arcs flash should be initialized after ic_lock_init and ipc_master_init
    arcs_flash_init();

    user_wifi_start();

    http_test_init();

    sdmmc_hard_init();
    user_fs_init();
    lisa_kv_init();

    user_shell_start();

    while (1) {
        extern bool g_wifi_connected;
        if (g_wifi_connected) {
            cpr_test();
            // curl_test();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));

    }
_ERR1:
    LOGE("Application start failed");
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(100));
    }    
    return 0;
}
