#include "sys/reboot.h"

#include "PowerManager.h"
#include "arcs_ap.h"
#ifdef CONFIG_BOOT_FEATURES_API
#include "board.h"
#include "uboot_features_api.h"
#endif

void sys_arch_reboot(int type)
{
#ifdef CONFIG_BOOT_FEATURES_API
    if (uboot_features_has(UBOOT_FEATURE_OTA) && uboot_features_has(UBOOT_FEATURE_POWER_GUARD)) {
        /* CMN SW reset 会把 GPIO peripheral 一起复位，POWER_EN 在 stage0
         * 重新接管之前会短暂丢 drive；电池模式下 MOSFET 栅极撑不过这个
         * 窗口就掉电了。用 AON IOMUX 的 force-output 把 POWER_EN 锁到高，
         * AON 域不受 CMN reset 影响，PB3 整个复位过程都是被硬拉高的。*/
        volatile uint32_t *aon_iomux = (volatile uint32_t *)&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + POWER_EN_PIN;
        *aon_iomux &= ~(0x1E00000u);
        *aon_iomux |= 0x1C00000u;

        type = SYS_REBOOT_SOFT;
    }
#endif

    switch (type) {
    case SYS_REBOOT_SOFT:
        __HAL_PMU_WholeChip_RST_ENABLE();
        break;
    case SYS_REBOOT_HARD:
    default:
        sys_platform_sw_full_reset();
        break;
    }
}
