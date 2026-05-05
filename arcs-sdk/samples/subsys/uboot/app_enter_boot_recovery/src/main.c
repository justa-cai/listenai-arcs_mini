#include <stdio.h>

#include "FreeRTOS.h"
#include "sys/reboot.h"
#include "task.h"
#include "uboot_recovery_api.h"

#define RECOVERY_LOG_DRAIN_DELAY_MS 200U

static void flush_recovery_log(void)
{
    /* Give the UART log path time to drain before the software reset fires. */
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(RECOVERY_LOG_DRAIN_DELAY_MS));
}

int main(void)
{
    printf("APP enter boot recovery sample running\n");
    flush_recovery_log();

    if (uboot_recovery_request(UBOOT_RECOVERY_MODE_SOFT) != 0) {
        printf("APP request boot recovery mode failed\n");
        return -1;
    }

    printf("APP requested boot recovery mode, rebooting\n");
    flush_recovery_log();
    sys_reboot(SYS_REBOOT_SOFT);
}
