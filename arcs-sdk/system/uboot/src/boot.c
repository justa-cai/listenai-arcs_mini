#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_BOOT_APP_CORE_AUTO
#include "boot_app_target.h"
#endif 
#include "boot_config.h"
#include "boot_default_app.h"
#ifdef CONFIG_BOOT_FACTORY_PARAMS
#include "boot_factory_params.h"
#endif
#include "boot_gpio_check.h"
#include "boot_power_guard.h"
#include "boot_reset_cause.h"
#include "boot_stage_gate.h"
#ifdef CONFIG_BOOT_OTA_PACKAGE
#include "boot_control_store.h"
#endif
#include "chip.h"
#include "ClockManager.h"
#include "clock_config.h"
#include "Driver_WDT.h"

#ifdef CONFIG_BOOT_FEATURES
#include "uboot_features.h"
/* Pull boot_features.c.o from boot_app archive. */
extern const struct uboot_features_desc boot_features_desc;
__attribute__((used))
static const struct uboot_features_desc *const boot_features_desc_anchor = &boot_features_desc;
#endif

#define __boot_text__    __attribute__((section(".boot.text")))
#define __boot_ramcode__ __attribute__((section(".boot_f.ramcode")))
#define __noreturn__     __attribute__((noreturn))

#define FALLBACK_DEFAULT_ECLIC_BASE    0x0C000000UL
#define FALLBACK_DEFAULT_SYSTIMER_BASE 0x02000000UL

extern volatile uint32_t SystemCoreClock;
extern volatile IRegion_Info_Type SystemIRegionInfo;

static uint32_t boot_rand_state = 0xACE1U;

__boot_ramcode__ static void boot_jump_to_cp(uint32_t addr);

#ifdef CONFIG_BOOT_FACTORY_PARAMS
/* A failed/previous recovery attempt can leave transient routing bits in the
 * AON register.  They must not divert a valid factory image into boot_s. */
__boot_ramcode__ static void boot_factory_clear_transient_state(uint32_t *boot_info_raw)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits;

    if (boot_info_raw == NULL) {
        return;
    }

    bits.raw = *boot_info_raw;
    bits.info.reboot_cnt = 0U;
    bits.info.recover_reason = RECOVER_REASON_NONE;
    bits.info.ota_pending = 0U;
    bits.info.shutdown_req = 0U;
    bits.info.charging_wait = 0U;
    bits.info.resume_normal_boot = 0U;
    bits.info.handshake_timeout = 0U;
    bits.info.boot_wdt = 0U;
    bits.info.req = 0U;
    *boot_info_raw = bits.raw;
}
#endif

__boot_ramcode__ __attribute__((weak)) int soc_cpu_id_get(void)
{
    return CONFIG_HARTID;
}

/* -------------------------------------------------------------------------
 * boot_f 阶段裸 UART 日志（不依赖 syslog）。
 * 仅在 CONFIG_BOOT_EARLY_LOG=y 时编译进来，用于排障早期启动流程
 * （app 地址解析、stage_gate 决策）。
 * ------------------------------------------------------------------------- */
#ifdef CONFIG_BOOT_EARLY_LOG

#define BFLOG_UART ((volatile UART_RegDef *)UART0_BASE)

__boot_ramcode__ static void bflog_putc(char c)
{
    while (!BFLOG_UART->REG_STATUS.bit.TX_FIFO_SPACE) { }
    BFLOG_UART->REG_RXTX_BUFFER.all = (uint32_t)(uint8_t)c;
}

__boot_ramcode__ static void bflog_puts(const char *s)
{
    while (*s) {
        bflog_putc(*s++);
    }
}

__boot_ramcode__ static void bflog_put_hex(uint32_t v)
{
    bflog_putc('0');
    bflog_putc('x');
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t d = (v >> i) & 0xF;
        bflog_putc(d < 10 ? '0' + d : 'A' + d - 10);
    }
}

__boot_ramcode__ static void bflog_flush(void)
{
    while (BFLOG_UART->REG_STATUS.bit.TX_FIFO_SPACE < 16) { }
}

__boot_ramcode__ static void bflog_init(void)
{
#ifndef CONFIG_BOOT_EARLY_CLOCK_INIT
    BootClock_Init();
    __FENCE_I();
#endif

    __HAL_CRM_UART0_CLK_ENABLE();
    HAL_CRM_SetUart0ClkDiv(96, 625);

    volatile UART_RegDef *u = BFLOG_UART;
    u->REG_IRQ_MASK.all = 0;
    u->REG_CTRL.all = 0;
    u->REG_CMD_SET.bit.TX_FIFO_RESET = 1;
    u->REG_CMD_SET.bit.RX_FIFO_RESET = 1;
    u->REG_CTRL.bit.DATA_BITS = 1;
    u->REG_CTRL.bit.ENABLE = 1;
    u->REG_STATUS.all = 1;

    /* PA3 -> UART0 TX (func ALTER2 = 2) */
    volatile uint32_t *pa3 = &(IP_CMN_IOMUX->REG_PAD_GPIOA_00.all) + 3;
    *pa3 = (*pa3 & ~0x1fU) | 2U;
}

#define BFLOG(msg)    bflog_puts(msg)
#define BFLOG_HEX(v)  bflog_put_hex((uint32_t)(v))
#define BFLOG_FLUSH() bflog_flush()
#else
#define BFLOG(msg)    ((void)0)
#define BFLOG_HEX(v)  ((void)0)
#define BFLOG_FLUSH() ((void)0)
#endif /* CONFIG_BOOT_EARLY_LOG */

__boot_text__ void irq_default_handler(void)
{
}

__boot_ramcode__ static int boot_rand(void)
{
    boot_rand_state ^= boot_rand_state << 13;
    boot_rand_state ^= boot_rand_state >> 17;
    boot_rand_state ^= boot_rand_state << 5;
    return (int)(boot_rand_state & 0x7FFFFFFFU);
}

__boot_ramcode__ void *memset(void *dst, int value, size_t len)
{
    uint8_t *ptr = (uint8_t *)dst;

    while (len--) {
        *ptr++ = (uint8_t)value;
    }

    return dst;
}

__boot_ramcode__ void *memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in = (const uint8_t *)src;

    while (len--) {
        *out++ = *in++;
    }

    return dst;
}

__boot_ramcode__ int memcmp(const void *lhs, const void *rhs, size_t len)
{
    const uint8_t *left = (const uint8_t *)lhs;
    const uint8_t *right = (const uint8_t *)rhs;

    while (len--) {
        if (*left != *right) {
            return *left - *right;
        }
        left++;
        right++;
    }

    return 0;
}

__boot_ramcode__ size_t strlen(const char *str)
{
    const char *cursor = str;

    while (*cursor != '\0') {
        cursor++;
    }

    return (size_t)(cursor - str);
}

__boot_ramcode__ int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }

    return (unsigned char)*lhs - (unsigned char)*rhs;
}

__boot_ramcode__ int strncmp(const char *lhs, const char *rhs, size_t len)
{
    while (len > 0 && *lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
        len--;
    }

    if (len == 0) {
        return 0;
    }

    return (unsigned char)*lhs - (unsigned char)*rhs;
}

__boot_ramcode__ char *strchr(const char *str, int ch)
{
    char target = (char)ch;

    while (*str != '\0') {
        if (*str == target) {
            return (char *)str;
        }
        str++;
    }

    if (target == '\0') {
        return (char *)str;
    }

    return NULL;
}

__boot_ramcode__ char *strstr(const char *haystack, const char *needle)
{
    size_t needle_len = strlen(needle);

    if (needle_len == 0U) {
        return (char *)haystack;
    }

    while (*haystack != '\0') {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }

    return NULL;
}

__boot_ramcode__ __noreturn__ void _exit(int status)
{
    (void)status;

    while (1) {
        __asm__ volatile("wfi");
    }
}

__boot_ramcode__ static void boot_get_iregion_info(volatile IRegion_Info_Type *iregion)
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
        return;
    }

    iregion->eclic_base = FALLBACK_DEFAULT_ECLIC_BASE;
    iregion->systimer_base = FALLBACK_DEFAULT_SYSTIMER_BASE;
}


#ifdef CONFIG_BOOT_OTA_PACKAGE
/* boot_ota_request_has_pending 调用链被 relocate 到 .psram.text，stage0
 * 在 scatload_psram 之前调它会跳到未初始化 PSRAM。本地最小实现：跳过
 * CRC，只校验两层 magic/version。boot_control_store_clear 只清内层
 * record，必须查内层 magic 才能区分"pending"和"已清"。 */
__boot_ramcode__ static bool stage0_ota_has_pending(void)
{
    extern void boot_flash_read(uint8_t *src, uint8_t *dst, uint32_t size);
    struct {
        uint32_t store_magic;
        uint32_t store_version;
        uint32_t record_magic;
        uint32_t record_version;
    } hdr = {0};

    boot_flash_read((uint8_t *)CONFIG_BOOT_CONTROL_STORE_BASE_ADDR,
                    (uint8_t *)&hdr, sizeof(hdr));

    return hdr.store_magic == BOOT_CONTROL_STORE_MAGIC
        && hdr.store_version == BOOT_CONTROL_STORE_VERSION
        && hdr.record_magic == BOOT_OTA_REQUEST_MAGIC
        && hdr.record_version == BOOT_OTA_REQUEST_VERSION;
}
#endif

__boot_ramcode__ static void boot_prepare_handoff(void)
{
    DisableDCache();
    DisableICache();
    MInvalDCache();
    __FENCE_I();
    __RWMB();
}

__boot_ramcode__ static void boot_jump_to_cp(uint32_t addr)
{
    __asm__ volatile("la sp, __StackTop\n\t" : : : "memory");
    __RV_CSR_CLEAR(CSR_MSTATUS, MSTATUS_MIE);
    boot_prepare_handoff();
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = addr;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    while (1) {
        __asm__ volatile("wfi");
    }
}

__boot_ramcode__ static void boot_reset_route_init(void)
{
    IP_AP_CFG->REG_SW_RESET.bit.APWDT2APSOC_RST_EN = 1;
    IP_AP_CFG->REG_SW_RESET.bit.APWDT2AP_RST_EN = 1;

    IP_SYSCTRL->REG_SW_RESET_CP1.bit.AP2SOC_RST_CMN_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNWDT2CMN_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNWDT2CP_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNWDT2AP_RST_EN = 1;
}

#ifdef CONFIG_BOOT_WDT
__boot_ramcode__ static void boot_ap_wdt_start(void)
{
    uint8_t clk_src = hal_driver_wdt_clk_src_32k;
    uint8_t int_time = hal_driver_wdt_int_time_17;
    uint8_t rst_time = hal_driver_wdt_rst_time_14;
    uint32_t cfg = 0;
    WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;

    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_CLKSEL_Msk, clk_src);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_INTTIME_Msk, int_time);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_RSTTIME_Msk, rst_time);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_RSTEN_Msk, 0x1);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_INTEN_Msk, 0x1);
    cfg = __RV_INSERT_FIELD(cfg, WDT_CTRL_EN_Msk, 0x1);

    wdt_hw->REG_WREN.all = 0x5AA5;
    wdt_hw->REG_CTRL.all = cfg;
}

__boot_ramcode__ static void boot_ap_wdt_stop(void)
{
    WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;

    wdt_hw->REG_WREN.all = 0x5AA5;
    wdt_hw->REG_CTRL.bit.EN = 0;
}
#endif

__boot_text__ __noreturn__ void boot_f(void)
{
    extern void scatload_boot_f(void);
    extern int32_t PSRAM_Initialize(uint32_t *read_delay, uint32_t *write_delay, uint8_t search);
#ifdef CONFIG_BOOT_SECOND_STAGE
    extern void boot_s(void);
#endif

    __RV_CSR_CLEAR(CSR_MSTATUS, MSTATUS_MIE);
    scatload_boot_f();

    IP_GPIOA->REG_INTREN.all = 0;
    IP_GPIOB->REG_INTREN.all = 0;

    SystemCoreClock = 240000000;
    boot_get_iregion_info(&SystemIRegionInfo);

    /* 清 ROMCODE 残留的 ECLIC 中断使能，避免 App soc_init 内
     * __enable_irq 后触发无 handler 的残留 IRQ。对齐 solutions/boot。 */
    for (int i = 0; i < SOC_INT_MAX; i++) {
        ECLIC_DisableIRQ((IRQn_Type)i);
    }

#ifdef CONFIG_BOOT_EARLY_CLOCK_INIT
    BootClock_Init();
    SystemCoreClock = CRM_GetSrcFreq(CRM_IpSrcCoreClk);
    __FENCE_I();
#endif

#ifdef CONFIG_BOOT_EARLY_LOG
    bflog_init();
    BFLOG("[bf] start clk=");
    BFLOG_HEX(SystemCoreClock);
    BFLOG("\r\n");
    BFLOG_FLUSH();
#endif

#ifdef CONFIG_BOOT_EARLY_PSRAM_INIT
    {
        uint32_t rdly = 18;
        uint32_t wdly = 22;

        if (PSRAM_Initialize(&rdly, &wdly, 1) != 0) {
            BFLOG("[bf] psr FAIL\r\n");
            BFLOG_FLUSH();
            __builtin_trap();
        }
        BFLOG("[bf] psr ok\r\n");
    }
#endif

    boot_reset_route_init();

    uint32_t app_addr = boot_default_app_addr_get();
    bool factory_mode = false;
#ifdef CONFIG_BOOT_FACTORY_PARAMS
    app_addr = boot_factory_app_addr_get(app_addr);
    factory_mode = app_addr == CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FACTORY_TEST_OFFSET;
#endif

#ifdef CONFIG_BOOT_SECOND_STAGE
    {
        uint32_t *boot_info_raw = (uint32_t *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
        uint32_t sysrst_status = IP_AON_CTRL->REG_SYSRST_STATUS.all;

#ifdef CONFIG_BOOT_POWER_GUARD
        if (factory_mode) {
            boot_factory_clear_transient_state(boot_info_raw);
        }
        boot_power_guard_run(sysrst_status, boot_info_raw, factory_mode);
#endif

#ifdef CONFIG_BOOT_FACTORY_PARAMS
        if (factory_mode) {
#ifndef CONFIG_BOOT_POWER_GUARD
            boot_factory_clear_transient_state(boot_info_raw);
#endif
            BFLOG("[bf] ->factory cp @");
            BFLOG_HEX(app_addr);
            BFLOG("\r\n");
            BFLOG_FLUSH();
            boot_jump_to_cp(app_addr);
        }
#endif

        const struct boot_config *boot_cfg = boot_config_load_valid(NULL);

        boot_reset_cause_apply(boot_info_raw, sysrst_status);
        bool app_valid = boot_default_app_image_is_valid(
            (const uint8_t *)(uintptr_t)app_addr, app_addr);
        boot_default_app_prepare_recovery(boot_info_raw, app_valid);

        BFLOG("[bf] A=");
        BFLOG_HEX(app_addr);
        BFLOG(" V=");
        BFLOG(app_valid ? "1" : "0");
        BFLOG(" rst=");
        BFLOG_HEX(sysrst_status);
        BFLOG(" raw=");
        BFLOG_HEX(*boot_info_raw);
        BFLOG("\r\n");
        BFLOG_FLUSH();

        bool gate_wants_recovery =
            boot_stage_gate_should_enter_second_stage(boot_info_raw, boot_cfg);
        bool gpio_force_recovery = boot_gpio_check_power_key_triggered();

#ifdef CONFIG_BOOT_OTA_PACKAGE
        /* 上一轮 OTA 没跑完就掉电 / 被硬复位：RSVD4 里的 req / ota_pending /
         * mode 都可能被清掉，但 flash 里挂着的 OTA 请求记录是 lifecycle 成
         * 功 / 放弃前都不会动的持久证据。见到它就走 boot_s，让
         * boot_ota_try_handle_update 把残局续跑完。 */
        if (!gate_wants_recovery && stage0_ota_has_pending()) {
            gate_wants_recovery = true;
        }
#endif

        if (gpio_force_recovery && !gate_wants_recovery) {
            union {
                struct boot_info info;
                uint32_t raw;
            } bits = {.raw = *boot_info_raw};
            if (bits.info.recover_reason == RECOVER_REASON_NONE) {
                bits.info.recover_reason = RECOVER_REASON_HARD_REQ;
                *boot_info_raw = bits.raw;
            }
        }

        if (gate_wants_recovery || gpio_force_recovery) {
            BFLOG("[bf] ->bs raw=");
            BFLOG_HEX(*boot_info_raw);
            BFLOG("\r\n");
            BFLOG_FLUSH();
#ifdef CONFIG_BOOT_WDT
            boot_ap_wdt_stop();
#endif
            boot_prepare_handoff();
            boot_s();
        }
    }
#endif

#ifdef CONFIG_BOOT_WDT
    boot_ap_wdt_start();
#endif

#if CONFIG_BOOT_APP_CORE_AUTO
    switch (boot_app_detect_target(app_addr)) {
    case BOOT_APP_TARGET_CP:
        BFLOG("[bf] ->cp @");
        BFLOG_HEX(app_addr);
        BFLOG("\r\n");
        BFLOG_FLUSH();
        boot_jump_to_cp(app_addr);
        break;
    case BOOT_APP_TARGET_AP:
        BFLOG("[bf] ->ap @");
        BFLOG_HEX(app_addr);
        BFLOG("\r\n");
        BFLOG_FLUSH();
        boot_prepare_handoff();
        ((void (*)(void))app_addr)();
        break;
    default:
#if CONFIG_BOOT_APP_CORE_AUTO_FALLBACK_CP
        BFLOG("[bf] ->cp(fb) @");
        BFLOG_HEX(app_addr);
        BFLOG("\r\n");
        BFLOG_FLUSH();
        boot_jump_to_cp(app_addr);
#else
        BFLOG("[bf] ->ap(fb) @");
        BFLOG_HEX(app_addr);
        BFLOG("\r\n");
        BFLOG_FLUSH();
        boot_prepare_handoff();
        ((void (*)(void))app_addr)();
#endif
        break;
    }
#elif CONFIG_BOOT_APP_CORE_CP
    BFLOG("[bf] ->cp @");
    BFLOG_HEX(app_addr);
    BFLOG("\r\n");
    BFLOG_FLUSH();
    boot_jump_to_cp(app_addr);
#else
    BFLOG("[bf] ->ap @");
    BFLOG_HEX(app_addr);
    BFLOG("\r\n");
    BFLOG_FLUSH();
    boot_prepare_handoff();
    ((void (*)(void))app_addr)();
#endif

    while (1) {
        __asm__ volatile("wfi");
    }
}
