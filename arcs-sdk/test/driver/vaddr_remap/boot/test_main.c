/*
 * vaddr_remap boot test — single entry driven by prj_boot_*.conf
 *
 * Maps the selected virtual region to the configured Flash offset, then
 * jumps directly to the firmware linked at that region.
 */

#include <stdio.h>

#include "arcs_ap.h"
#include "vaddr_remap.h"

#define BOOT_REGION      ((vaddr_region_t)CONFIG_VADDR_REMAP_BOOT_REGION_INDEX)
#define FW_FLASH_OFFSET  CONFIG_VADDR_REMAP_FW_FLASH_OFFSET

static const char *const boot_variant_name = CONFIG_VADDR_REMAP_BOOT_VARIANT_NAME;
static const char *const region_name[] = { "A", "B", "C", "D" };

static const uint32_t region_vaddr[VADDR_REGION_MAX] = {
    [VADDR_REGION_A] = VADDR_REGION_A_BASE,
    [VADDR_REGION_B] = VADDR_REGION_B_BASE,
    [VADDR_REGION_C] = VADDR_REGION_C_BASE,
    [VADDR_REGION_D] = VADDR_REGION_D_BASE,
};

typedef void (*entry_fn)(void);

static void setup_flash_mapping(void)
{
    vaddr_remap_init(VADDR_TARGET_FLASH);
    vaddr_remap_map(BOOT_REGION, FW_FLASH_OFFSET);
    vaddr_remap_apply();
}

int main(void)
{
    entry_fn fw_entry = (entry_fn)region_vaddr[BOOT_REGION];

    printf("\r\n=== vaddr_remap Boot Test: Region %s (variant %s) ===\r\n\r\n",
           region_name[BOOT_REGION],
           boot_variant_name);

    setup_flash_mapping();

#if CONFIG_VADDR_REMAP_BOOT_FW_IN_CP
    printf("Boot CP from Region %s (0x%08lX)\r\n",
           region_name[BOOT_REGION],
           (unsigned long)region_vaddr[BOOT_REGION]);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = region_vaddr[BOOT_REGION];
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#else
    fw_entry();
#endif

    while (1) {
        __asm__ volatile("wfi");
    }
}
