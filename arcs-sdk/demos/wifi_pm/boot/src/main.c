#include <stdarg.h>
#include "chip.h"

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_NONE        (0xFF)

#define FALLBACK_DEFAULT_ECLIC_BASE    0x0C000000UL
#define FALLBACK_DEFAULT_SYSTIMER_BASE 0x02000000UL

volatile IRegion_Info_Type SystemIRegionInfo;

static void _get_iregion_info(volatile IRegion_Info_Type *iregion)
{
    unsigned long mcfg_info;
    if (iregion == NULL) {
        return;
    }
    mcfg_info = __RV_CSR_READ(CSR_MCFG_INFO);
    if (mcfg_info & MCFG_INFO_IREGION_EXIST) {
        iregion->iregion_base = (__RV_CSR_READ(CSR_MIRGB_INFO) >> 10) << 10;
        iregion->eclic_base = iregion->iregion_base + IREGION_ECLIC_OFS;
        iregion->systimer_base = iregion->iregion_base + IREGION_TIMER_OFS;
        iregion->smp_base = iregion->iregion_base + IREGION_SMP_OFS;
        iregion->idu_base = iregion->iregion_base + IREGION_IDU_OFS;
    } else {
        iregion->eclic_base = FALLBACK_DEFAULT_ECLIC_BASE;
        iregion->systimer_base = FALLBACK_DEFAULT_SYSTIMER_BASE;
    }
}

int cloglvl = 0;
void logDbg(const char *fmt, ...) { (void)fmt; }

#if defined(CONFIG_PM) && (CONFIG_PM == 1)
void ap_startup_check(void)
{
    if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_NONE)
    {
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = 0;
        goto WFI_LOOP;
    }
    else if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_RAM)
    {
        if ((IP_AON_CTRL->REG_WAKEUP_ISR.all == 0) && (IP_AON_CTRL->REG_AON_DIG_RSVD2.all == WAKEUP_ACT_JUMP_RAM))
        {
            /* UART wakeup may reach here without REG_WAKEUP_ISR being set */
            IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD3.all;
            IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
        }
        goto WFI_LOOP;
    }

    return;

WFI_LOOP:
    do {
        __WFI();
    } while(1);
}
#endif

int main(void)
{
#if PSRAM_SEC
    logInit(0, 115200);
    PSRAM_Initialize(NULL, NULL, 1);
#endif

    extern void BootClock_Init();
    BootClock_Init();

    for (int i = 0; i < IRQ_MAX; i++) {
        disable_IRQ(i);
        clear_IRQ(i);
    }

    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = CONFIG_BOOT_CP_ENTRY;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    do {
        __WFI();
    } while (1);
}