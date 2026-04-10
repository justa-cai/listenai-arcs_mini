/**
 * vaddr_remap firmware — single entry driven by prj_fw_*.conf
 */

#define LOG_TAG "vaddr_fw"

#include <lisa_log.h>
#include "arcs_ap.h"

int main(void)
{
    LOGI("========================================");
    LOGI("  >>> Firmware %s running! <<<", CONFIG_VADDR_REMAP_FW_VARIANT_NAME);
    LOGI("  Region base: 0x%08lX", (unsigned long)CONFIG_MEM_FLASH_BASE);
    LOGI("  Flash offset: 0x%05lX", (unsigned long)CONFIG_VADDR_REMAP_FW_FLASH_OFFSET);
    LOGI("========================================");

    while (1) {
        __asm__ volatile("wfi");
    }
}
