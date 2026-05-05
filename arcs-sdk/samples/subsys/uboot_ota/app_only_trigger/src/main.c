#include <stdio.h>

#include "FreeRTOS.h"
#include "sys/reboot.h"
#include "task.h"
#include "uboot_ota_api.h"

#define OTA_LOG_DRAIN_DELAY_MS 200U
#define OTA_PAYLOAD_FLASH_OFFSET 0x00600000UL

#if defined(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH) && \
    !defined(CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE)
#error "CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE is required for flash OTA trigger"
#endif

#if defined(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH) && \
    (CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE == 0)
#error "FLASH OTA package size must be greater than zero"
#endif

static void flush_ota_log(void)
{
    /* Give the UART path time to flush before the software reset fires. */
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(OTA_LOG_DRAIN_DELAY_MS));
}

#if defined(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH)
static int trigger_from_flash(void)
{
    printf("APP-only OTA source: flash\n");
    return uboot_ota_start_from_flash(
        OTA_PAYLOAD_FLASH_OFFSET,
        CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE
    );
}
#endif

#if defined(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF)
static int trigger_from_tf(void)
{
    printf("APP-only OTA source: tf path=%s\n", CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF_PATH);
    return uboot_ota_start_from_tf(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF_PATH);
}
#endif

int main(void)
{
    int ret;

    printf("APP-only OTA trigger sample running\n");
    flush_ota_log();

#if defined(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH)
    /* Assume the OTA package has already been staged in the flash OTA area. */
    ret = trigger_from_flash();
#elif defined(CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF)
    /* Assume the OTA package has already been staged at the TF card OTA path. */
    ret = trigger_from_tf();
#else
#error "Select one OTA trigger source"
#endif

    if (ret != 0) {
        printf("APP-only OTA request save failed\n");
        return -1;
    }

    printf("APP-only OTA request saved, rebooting\n");
    flush_ota_log();
    sys_reboot(SYS_REBOOT_SOFT);
}
