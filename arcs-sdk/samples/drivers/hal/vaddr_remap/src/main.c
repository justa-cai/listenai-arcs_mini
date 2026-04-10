/*
 * vaddr_remap sample — loader
 *
 * Demonstrates using the vaddr_remap HAL driver to map Region A to a Flash
 * offset, then boot firmware at that virtual address on AP or CP.
 *
 * Flash layout (configured in sample.yaml images):
 *   0x00000  loader (this program, linked at 0x30000000 XIP)
 *   0x20000  fw_a   (linked at Region A: 0x08000000)
 */

#include <stdio.h>

#include "arcs_ap.h"
#include "vaddr_remap.h"

#define SAMPLE_REGION    VADDR_REGION_A
#define FW_FLASH_OFFSET  CONFIG_VADDR_REMAP_SAMPLE_FW_FLASH_OFFSET

typedef void (*entry_fn)(void);

static void setup_flash_mapping(void)
{
    vaddr_remap_init(VADDR_TARGET_FLASH);
    vaddr_remap_map(SAMPLE_REGION, FW_FLASH_OFFSET);
    vaddr_remap_apply();
}

int main(void)
{
    entry_fn fw_entry = (entry_fn)VADDR_REGION_A_BASE;

    printf("\r\n=== vaddr_remap sample: %s ===\r\n\r\n",
           CONFIG_VADDR_REMAP_SAMPLE_BOOT_VARIANT_NAME);

    setup_flash_mapping();

#if CONFIG_VADDR_REMAP_SAMPLE_BOOT_FW_IN_CP
    printf("Boot CP from Region A (0x%08lX)\r\n", (unsigned long)VADDR_REGION_A_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = VADDR_REGION_A_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#else
    printf("Boot AP from Region A (0x%08lX)\r\n", (unsigned long)VADDR_REGION_A_BASE);
    fw_entry();
#endif

    while (1) {
        __asm__ volatile("wfi");
    }
}
