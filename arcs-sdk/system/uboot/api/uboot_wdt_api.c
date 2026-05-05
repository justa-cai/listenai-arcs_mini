#include "uboot_wdt_api.h"

#include <stddef.h>
#include <stdint.h>

#ifndef UBOOT_WDT_API_HOST_TEST
#include "arcs_ap.h"
#endif

#define UBOOT_WDT_UNLOCK_KEY  0x5AA5u
#define UBOOT_WDT_RESTART_KEY 0xCAFEu

#define UBOOT_WDT_CTRL_EN_Msk      0x1u
#define UBOOT_WDT_CTRL_CLKSEL_Pos  1u
#define UBOOT_WDT_CTRL_CLKSEL_Msk  0x2u
#define UBOOT_WDT_CTRL_INTEN_Msk   0x4u
#define UBOOT_WDT_CTRL_RSTEN_Msk   0x8u
#define UBOOT_WDT_CTRL_INTTIME_Pos 4u
#define UBOOT_WDT_CTRL_RSTTIME_Pos 8u

#define UBOOT_WDT_CLKSEL_32K  0u
#define UBOOT_WDT_INTTIME_17  8u
#define UBOOT_WDT_RSTTIME_14  7u

#define UBOOT_WDT_ENABLE_CTRL_VALUE                           \
    ((UBOOT_WDT_RSTTIME_14 << UBOOT_WDT_CTRL_RSTTIME_Pos) |  \
     (UBOOT_WDT_INTTIME_17 << UBOOT_WDT_CTRL_INTTIME_Pos) |  \
     (UBOOT_WDT_CLKSEL_32K << UBOOT_WDT_CTRL_CLKSEL_Pos) |   \
     UBOOT_WDT_CTRL_RSTEN_Msk | UBOOT_WDT_CTRL_INTEN_Msk |   \
     UBOOT_WDT_CTRL_EN_Msk)

__attribute__((weak)) volatile uint32_t *uboot_wdt_api_wren_reg(void)
{
#ifdef UBOOT_WDT_API_HOST_TEST
    return NULL;
#else
    return &IP_AP_WDT->REG_WREN.all;
#endif
}

__attribute__((weak)) volatile uint32_t *uboot_wdt_api_ctrl_reg(void)
{
#ifdef UBOOT_WDT_API_HOST_TEST
    return NULL;
#else
    return &IP_AP_WDT->REG_CTRL.all;
#endif
}

__attribute__((weak)) volatile uint32_t *uboot_wdt_api_restart_reg(void)
{
#ifdef UBOOT_WDT_API_HOST_TEST
    return NULL;
#else
    return &IP_AP_WDT->REG_RESTART.all;
#endif
}

int uboot_wdt_enable(void)
{
    volatile uint32_t *wren = uboot_wdt_api_wren_reg();
    volatile uint32_t *ctrl = uboot_wdt_api_ctrl_reg();

    if (wren == NULL || ctrl == NULL) {
        return -1;
    }

    *wren = UBOOT_WDT_UNLOCK_KEY;
    *ctrl = UBOOT_WDT_ENABLE_CTRL_VALUE;
    return 0;
}

int uboot_wdt_disable(void)
{
    volatile uint32_t *wren = uboot_wdt_api_wren_reg();
    volatile uint32_t *ctrl = uboot_wdt_api_ctrl_reg();

    if (wren == NULL || ctrl == NULL) {
        return -1;
    }

    *wren = UBOOT_WDT_UNLOCK_KEY;
    *ctrl &= ~UBOOT_WDT_CTRL_EN_Msk;
    return 0;
}

int uboot_wdt_feed(void)
{
    volatile uint32_t *wren = uboot_wdt_api_wren_reg();
    volatile uint32_t *restart = uboot_wdt_api_restart_reg();

    if (wren == NULL || restart == NULL) {
        return -1;
    }

    *wren = UBOOT_WDT_UNLOCK_KEY;
    *restart = UBOOT_WDT_RESTART_KEY;
    return 0;
}
