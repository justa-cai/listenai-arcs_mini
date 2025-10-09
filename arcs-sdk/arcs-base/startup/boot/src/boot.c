#include "chip.h"
#include "memap.h"
#include "PSRAMManager.h"
#include "ClockManager.h"

#include "boot_log.h"

#include <stdint.h>

extern uint32_t __scat_copy_base, __scat_copy_last, __scat_zero_base, __scat_zero_last;
extern uint32_t __psram_scat_copy_base, __psram_scat_copy_last, __psram_scat_zero_base, __psram_scat_zero_last;

typedef struct {
    uint32_t len, *vma, *lma;
} scat_copy_item_t;
typedef struct {
    uint32_t len, *vma;
} scat_zero_item_t;

static __attribute__((section(".init"))) void _scatload(uint32_t *dst, const uint32_t *src, uint32_t cnt)
{
    while (cnt--) {
        *dst++ = *src++;
    }
}

static __attribute__((section(".init"))) void _scatfill(uint32_t *dst, uint32_t fill, int cnt)
{
    while (cnt--) {
        *dst++ = fill;
    }
}

static __attribute__((section(".init"))) void scatload(void)
{
    for (scat_copy_item_t *item = (void *)&__scat_copy_base; (uint32_t *)item < &__scat_copy_last; item++) {
        if (item->vma && item->len >= sizeof(uint32_t) && item->vma != item->lma) {
            _scatload(item->vma, item->lma, item->len >> 2);
        }
    }

    for (scat_zero_item_t *item = (void *)&__scat_zero_base; (uint32_t *)item < &__scat_zero_last; item++) {
        if (item->vma && item->len >= sizeof(uint32_t)) {
            _scatfill(item->vma, 0, item->len >> 2);
        }
    }
}

static void scatload_psram(void)
{
    for (scat_copy_item_t *item = (void *)&__psram_scat_copy_base; (uint32_t *)item < &__psram_scat_copy_last; item++) {
        if (item->vma && item->len >= sizeof(uint32_t) && item->vma != item->lma) {
            _scatload(item->vma, item->lma, item->len >> 2);
        }
    }

    for (scat_zero_item_t *item = (void *)&__psram_scat_zero_base; (uint32_t *)item < &__psram_scat_zero_last; item++) {
        if (item->vma && item->len >= sizeof(uint32_t)) {
            _scatfill(item->vma, 0, item->len >> 2);
        }
    }
}

void irq_default_handler(void)
{

}

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
    if (mcfg_info & MCFG_INFO_IREGION_EXIST) { // IRegion Info present
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

static const uint32_t psram_size[] = {0, 32, 0, 64, 0, 128, 512, 256};

void boot_run(void)
{
    __disable_irq();

    scatload();

    _get_iregion_info(&SystemIRegionInfo);

    extern void BootClock_Init();
    BootClock_Init();
    __FENCE_I();

    bootlog_init(CONFIG_BOOT_UART_PORT, CONFIG_BOOT_UART_BAUDRATE);

    bootlog_dbg("main  clk: %d Hz\n", CRM_GetSrcFreq(CRM_IpSrcCoreClk));
    bootlog_dbg("psram clk: %d Hz\n", CRM_GetPsramFreq());

    uint32_t rdly = 18, wdly = 22;
    int r;

    r = PSRAM_Initialize(&rdly, &wdly, 1);
    if (r != 0) {
        bootlog_err("psram error, code: %d\n", r);
        while(1);
    }

    uint32_t density = ((IP_PSRAM_CTRL->REG_MR2.all & 0xff) >> 0) & 0x07;
    if (density < sizeof(psram_size)/ sizeof(psram_size[0])) {
        bootlog_dbg("psram inf, size: %d Mbit, w: %d, r: %d\n", psram_size[density], wdly, rdly);
    } else {
        bootlog_dbg("psram inf, size: unknown, w: %d, r: %d\n", wdly, rdly);
    }

    for (int i = 0; i < IRQ_MAX; i++) {
        disable_IRQ(i);
        clear_IRQ(i);
    }

#if defined CONFIG_BOOT_CP_ENTRY
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = (uint32_t)CONFIG_BOOT_CP_ENTRY;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    bootlog_dbg("cp: 0x%x\n", CONFIG_BOOT_CP_ENTRY);
#endif

#if defined CONFIG_BOOT_AP_ENTRY
    if (CONFIG_BOOT_AP_ENTRY) {
        void (*ap_core_start)(void) = (void (*)(void))CONFIG_BOOT_AP_ENTRY;
        ap_core_start();
        bootlog_dbg("ap: 0x%x\n", CONFIG_BOOT_AP_ENTRY);
    }
#endif

    bootlog_inf("ap: none\n");

    while (1) {
        __WFI();
    }
}
