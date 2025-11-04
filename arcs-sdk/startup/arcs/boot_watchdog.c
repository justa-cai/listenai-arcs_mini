#include "stdint.h"
#include "stdio.h"
#include "chip.h"
#include "Driver_WDT.h"
#include "wdt_reg.h"
#include "arcs_ap.h"

struct boot_info {
    uint32_t reboot_cnt: 8;
    uint32_t recover_reason: 8;
    uint32_t reserved: 14;
    uint32_t boot_wdt: 1;
    uint32_t req: 1;
};

static void boot_watchdog_handler(void)
{
    ECLIC_DisableIRQ(IRQ_AP_WDT_VECTOR);
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    info->boot_wdt = 1;
    __asm__ volatile("fence.i");
    printf("boot_watchdog_handler\n");

    volatile int i = 0;
    while(i++ < 10000);

    /* 为防止看门狗无法复位, 冗余软件复位 */
    info->req = 1;
    IP_CMN_SYS->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;
    IP_CMN_SYS->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
    IP_CMN_SYS->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 1;
    __COMPILER_BARRIER();
    IP_CMN_SYS->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

int boot_watchdog_init(void)
{
    uint8_t clk_src = hal_driver_wdt_clk_src_32k;
    uint8_t int_time = hal_driver_wdt_int_time_19;
    uint8_t rst_time = hal_driver_wdt_rst_time_14;
    uint32_t cfg = 0;

    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_CLKSEL_Msk, clk_src);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_INTTIME_Msk, int_time);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_RSTTIME_Msk, rst_time);

    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_RSTEN_Msk, 0x1);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_INTEN_Msk, 0x1);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_EN_Msk, 0x1);

    WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;
    wdt_hw->REG_WREN.all = 0x5AA5;
    wdt_hw->REG_CTRL.all = cfg;

    register_ISR(IRQ_AP_WDT_VECTOR, boot_watchdog_handler, NULL);
    ECLIC_EnableIRQ(IRQ_AP_WDT_VECTOR);
    return 0;
}

int boot_watchdog_feed(void)
{
    WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;
    wdt_hw->REG_WREN.all = 0x5AA5;
    wdt_hw->REG_RESTART.all = 0xCAFE;
    return 0;
}


int boot_watchdog_enable(int flag)
{
    WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;
    wdt_hw->REG_WREN.all = 0x5AA5;
    wdt_hw->REG_CTRL.all = __RV_INSERT_FIELD(wdt_hw->REG_CTRL.all, WDT_CTRL_EN_Msk, flag);
    return 0;
}