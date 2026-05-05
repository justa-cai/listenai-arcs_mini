#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "sys/reboot.h"
#include "task.h"
#include "uboot_recovery_api.h"

#define UPGRADE_VERIFY_MARKER "FLASH OTA upgraded firmware running (AP+CP verified)"
#define UPGRADE_RECOVERY_REQUEST_MARKER "FLASH OTA upgraded firmware requesting soft recovery"
#define UPGRADE_RECOVERY_ARMED_MARKER "FLASH OTA upgraded firmware recovery armed, rebooting"

static bool cp_payload_is_valid(void)
{
    const char expected[] = OTA_CP_VERIFY_MAGIC;
    const void *cp_payload_addr = (const void *)OTA_CP_VERIFY_ADDR;

    return memcmp(cp_payload_addr, expected, sizeof(expected) - 1U) == 0;
}

int main(void)
{
    if (!cp_payload_is_valid()) {
        printf("FLASH OTA CP payload verify failed\n");
        return -1;
    }

    printf("%s\n", UPGRADE_VERIFY_MARKER);
    vTaskDelay(pdMS_TO_TICKS(200));

    printf("%s\n", UPGRADE_RECOVERY_REQUEST_MARKER);
    if (uboot_recovery_request(UBOOT_RECOVERY_MODE_SOFT) != 0) {
        printf("FLASH OTA soft recovery request failed\n");
        return -1;
    }

    printf("%s\n", UPGRADE_RECOVERY_ARMED_MARKER);
    vTaskDelay(pdMS_TO_TICKS(200));
    sys_reboot(SYS_REBOOT_SOFT);
}
