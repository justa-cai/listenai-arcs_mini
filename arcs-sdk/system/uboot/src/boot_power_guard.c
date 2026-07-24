#include "boot_power_guard.h"

#ifdef CONFIG_BOOT_POWER_GUARD

#include <stdint.h>
#include <stdbool.h>

#include "chip.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "PowerManager.h"

#include "boot_config.h"

#define __boot_ramcode__ __attribute__((section(".boot_f.ramcode")))

extern volatile uint32_t SystemCoreClock;

__boot_ramcode__ static GPIO_RegDef *gpio_regs_for(int port)
{
    if (port == CSK_IOMUX_PAD_A) {
        __HAL_CRM_GPIO0_CLK_ENABLE();
        return IP_GPIOA;
    }
    __HAL_CRM_GPIO1_CLK_ENABLE();
    return IP_GPIOB;
}

__boot_ramcode__ static void iomux_to_gpio(int port, int pin)
{
    volatile uint32_t *cmn_reg;
    volatile uint32_t *aon_reg = NULL;

    if (port == CSK_IOMUX_PAD_A) {
        cmn_reg = (volatile uint32_t *)&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + pin;
    } else {
        aon_reg = (volatile uint32_t *)&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + pin;
        cmn_reg = (volatile uint32_t *)&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + pin;
    }

    if (aon_reg != NULL) {
        *aon_reg = (*aon_reg & ~0x1fu) | (CSK_AON_IOMUX_FUNC_NORMAL & 0x1fu);
    }
    *cmn_reg = (*cmn_reg & ~0x1fu) | (CSK_IOMUX_FUNC_DEFAULT & 0x1fu);
}

__boot_ramcode__ static int read_pin(int port, int pin)
{
    GPIO_RegDef *gpio = gpio_regs_for(port);
    iomux_to_gpio(port, pin);
    gpio->REG_CHANNELDIR.all &= ~(1u << pin);
    return (int)((gpio->REG_DATAIN.all >> pin) & 0x1u);
}

__boot_ramcode__ static void write_pin(int port, int pin, int level)
{
    GPIO_RegDef *gpio = gpio_regs_for(port);
    iomux_to_gpio(port, pin);
    if (level) {
        gpio->REG_DATAOUT.all |= (1u << pin);
    } else {
        gpio->REG_DATAOUT.all &= ~(1u << pin);
    }
    gpio->REG_CHANNELDIR.all |= (1u << pin);
}

__boot_ramcode__ static inline uint32_t read_mcycle(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, mcycle" : "=r"(v));
    return v;
}

__boot_ramcode__ static void busy_delay_ms(uint32_t ms)
{
    uint32_t cycles_per_ms = SystemCoreClock / 1000u;
    uint32_t target = ms * cycles_per_ms;
    uint32_t start = read_mcycle();
    while ((uint32_t)(read_mcycle() - start) < target) {
        /* spin */
    }
}

__boot_ramcode__ static bool key_pressed(void)
{
    int level = read_pin(CONFIG_BOOT_POWER_GUARD_KEY_PORT,
                         CONFIG_BOOT_POWER_GUARD_KEY_PIN);
    return level == CONFIG_BOOT_POWER_GUARD_KEY_LEVEL;
}

__boot_ramcode__ static bool usb_plugged(void)
{
    int level = read_pin(CONFIG_BOOT_POWER_GUARD_USB_PORT,
                         CONFIG_BOOT_POWER_GUARD_USB_PIN);
    return level == CONFIG_BOOT_POWER_GUARD_USB_LEVEL;
}

/* 无电池纯 USB 启动时，VBUS → DCDC → VCC → SoC 启动 → guard 入口的链路里
 * USB_DET 可能因为信号爬坡 / 滤波延迟在 guard 第一次采样时还读到 0；
 * 4 次 5ms retry 给信号一点时间稳定。带电池场景 USB 一上来就稳，单次
 * 采样直接命中、不进 retry，无副作用。 */
__boot_ramcode__ static bool usb_plugged_stable(void)
{
    if (usb_plugged()) {
        return true;
    }
    for (int i = 0; i < 4; i++) {
        busy_delay_ms(5);
        if (usb_plugged()) {
            return true;
        }
    }
    return false;
}

__boot_ramcode__ static void latch_set(int on)
{
    int level = on ? CONFIG_BOOT_POWER_GUARD_LATCH_ACTIVE_LEVEL
                   : !CONFIG_BOOT_POWER_GUARD_LATCH_ACTIVE_LEVEL;
    write_pin(CONFIG_BOOT_POWER_GUARD_LATCH_PORT,
              CONFIG_BOOT_POWER_GUARD_LATCH_PIN, level);
}

__boot_ramcode__ static void led_set(int on)
{
#ifdef CONFIG_BOOT_POWER_GUARD_LED
    int level = on ? CONFIG_BOOT_POWER_GUARD_LED_ACTIVE_LEVEL
                   : !CONFIG_BOOT_POWER_GUARD_LED_ACTIVE_LEVEL;
    write_pin(CONFIG_BOOT_POWER_GUARD_LED_PORT,
              CONFIG_BOOT_POWER_GUARD_LED_PIN, level);
#else
    (void)on;
#endif
}

/* W1C 清 REG_SYSRST_STATUS。AON_SW_RESET 不会重新置 POR_STATUS=1
 * （只有 VCC 真掉重新上电才置），清了之后 app 走 sys_platform_sw_full_reset
 * 重启回来 POR=0，guard 走非 POR 分支 normal boot。*/
__boot_ramcode__ static void clear_sysrst_status(void)
{
    uint32_t cause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
    IP_AON_CTRL->REG_SYSRST_STATUS.all = cause;
}

__boot_ramcode__ static bool wait_pb_long_press(void)
{
    /* 入口先把 latch 拉低，PB 不达标时无 VCC 维持源头，MOS 立即关断 */
    latch_set(0);

    while (1) {
        if (key_pressed()) {
            /* 进内循环时采样 USB。若入口 USB=0（电池供电路径启动的标志），
             * 计时中 USB 变 1 表示 "按 PB 启动后插 USB" 的 recovery 意图，
             * 立即退出不必等满 3 秒。入口 USB=1（USB 已在维持 VCC）则只
             * 关心 PB 计时，USB 状态变化不触发 recovery。usb_plugged_stable
             * 内部多采几次，避免抖动让"USB 一直插着"误判成"USB 后到"。 */
            bool usb_at_entry = usb_plugged_stable();
            bool usb_arrived = false;
            uint32_t elapsed = 0;
            bool released = false;

            while (elapsed < CONFIG_BOOT_POWER_GUARD_HOLD_TIME_MS) {
                if (!key_pressed()) {
                    released = true;
                    break;
                }
                if (!usb_at_entry && usb_plugged()) {
                    usb_arrived = true;
                    break;
                }
                busy_delay_ms(CONFIG_BOOT_POWER_GUARD_SAMPLE_INTERVAL_MS);
                elapsed += CONFIG_BOOT_POWER_GUARD_SAMPLE_INTERVAL_MS;
            }

            if (!released) {
                /* recovery 路径（usb_arrived）故意不 latch：recovery 依赖 USB
                 * 供电，此时 USB 就是唯一的 VCC 源，用户拔线 VCC 立即掉 → 自动
                 * 真关机。normal boot 路径必须 latch(1)，因为可能走纯电池路径，
                 * 没 latch 时 PB 松开 VCC 就没了。*/
                if (!usb_arrived) {
                    latch_set(1);
                }
                return usb_arrived;
            }
        }

        busy_delay_ms(CONFIG_BOOT_POWER_GUARD_SAMPLE_INTERVAL_MS);
    }
}

__boot_ramcode__ static void mark_recovery(uint32_t *boot_info_raw)
{
    if (boot_info_raw == NULL) {
        return;
    }
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {.raw = *boot_info_raw};

    if (bits.info.recover_reason == RECOVER_REASON_NONE) {
        bits.info.recover_reason = RECOVER_REASON_HARD_REQ;
    }
    /* gate 的 should_enter_second_stage 只看 req/ota_pending/persistent，
     * 不看 recover_reason，所以必须设 req=1 才能让 gate 把我们路由到
     * boot_s。main_task 再据 recover_reason 决定 recovery UI。*/
    bits.info.req = 1;
    *boot_info_raw = bits.raw;
}

__boot_ramcode__ void boot_power_guard_run(uint32_t sysrst_status, uint32_t *boot_info_raw)
{
    /* 芯片默认电平下 LED 一上电就亮。guard 期间先关掉，等决定启动 app 再点亮 */
    led_set(0);

    /* shutdown_req 是 app 软关机的明确意图，谁先谁后都要优先处理；
     * 靠 POR_STATUS 判冷启动不保险（CMN SW reset 在部分芯片上也会
     * 置位 POR），要是掉进冷启动分支，USB 供电 + PB 按下触发
     * wait_pb_long_press 里 USB-arrived 的判定，就会被误标成 recovery。*/
    if (boot_info_raw != NULL) {
        union {
            struct boot_info info;
            uint32_t raw;
        } bits = {.raw = *boot_info_raw};

        if (bits.info.shutdown_req != 0u) {
            bits.info.shutdown_req = 0u;
            *boot_info_raw = bits.raw;

            latch_set(0);
            /* app 触发软关机时 PB 大概率还被按着（长按触发关机，
             * 从 app 走到 sw reset 不过几百毫秒，用户通常没松开），
             * 先等 PB 松开再进长按计时，避免把这次遗留按压直接
             * 数满 3 秒又把机器开回去。同时能把复位瞬间 IOMUX
             * 切换可能造成的误判"按下"窗口一起滤掉。*/
            while (key_pressed()) {
                busy_delay_ms(CONFIG_BOOT_POWER_GUARD_SAMPLE_INTERVAL_MS);
            }
            if (!wait_pb_long_press()) {
                led_set(1);
            }
            /* 以防 POR 也置了位，顺手清掉，让后面 gate 不把它当
             * 冷启动状态误处理。*/
            clear_sysrst_status();
            return;
        }
    }

    bool is_cold = (sysrst_status & (1u << PMU_RST_POR)) != 0u;

    /* CMN SW reset 在本芯片上会同时置位 POR，所以仅靠 POR 判冷启动不可靠。
     * app 明确表达过意图（recovery / ota / wdt 逃生 / stage1 已确认开机）
     * 时，即便 POR=1 也当 warm reset 处理，不要把"关机态插 USB"这条路径
     * 误触发。 */
    bool has_app_intent = false;
    if (boot_info_raw != NULL) {
        union {
            struct boot_info info;
            uint32_t raw;
        } bits = {.raw = *boot_info_raw};
        has_app_intent = bits.info.req || bits.info.ota_pending ||
                         bits.info.boot_wdt || bits.info.handshake_timeout ||
                         bits.info.resume_normal_boot ||
                         bits.info.recover_reason != RECOVER_REASON_NONE;
    }

    if (!is_cold || has_app_intent) {
        /* 保险拉高，防软复位瞬间 GPIO 回到 input */
        latch_set(1);
        led_set(1);
        /* resume_normal_boot 是一次性意图标志，消费掉避免下一次复位又触发 */
        if (boot_info_raw != NULL) {
            union {
                struct boot_info info;
                uint32_t raw;
            } bits = {.raw = *boot_info_raw};
            if (bits.info.resume_normal_boot) {
                bits.info.resume_normal_boot = 0;
                *boot_info_raw = bits.raw;
            }
        }
        return;
    }

    /* 冷启动入口的 USB 检测统一用 usb_plugged_stable：纯 USB 无电池
     * 启动时 VBUS/VCC 上升过程会让 USB_DET 单次采样可能漏判，导致
     * PB+USB recovery 手势 / USB charging_wait 都识别不到。 */
    bool usb_at_entry = usb_plugged_stable();

    /* 冷启动入口 USB 插着 + PB 按下：用户预先按 PB 再插 USB（老版本的
     * 保底 recovery 手势）。无电池场景 USB 先上电，guard 看到的就是
     * USB+PB 同时，wait_pb_long_press 的 usb_arrived 判定跑不到——直接
     * 在入口判掉，走 recovery 路径（不 latch，USB 拔掉自动关）。 */
    if (boot_info_raw != NULL && usb_at_entry && key_pressed()) {
        mark_recovery(boot_info_raw);
        clear_sysrst_status();
        return;
    }

#ifdef CONFIG_BOOT_POWER_GUARD_LONG_PRESS_STARTUP
    /* 冷启动 + USB 插着 + PB 未按：用户刚插 USB（关机态插线），让 stage1
     * 显示「正在充电 / 长按电源键开机」3 秒再经 shutdown_req 路径回到
     * 假关机自旋。guard 自己做不到显示（需要 FreeRTOS + display 驱动），
     * 所以先 latch(0) 保证此时拔 USB 立即真关机，设 charging_wait=1 让
     * gate 把我们路由到 stage1 接手。 */
    if (boot_info_raw != NULL && usb_at_entry && !key_pressed()) {
        union {
            struct boot_info info;
            uint32_t raw;
        } bits = {.raw = *boot_info_raw};
        bits.info.charging_wait = 1;
        *boot_info_raw = bits.raw;
        latch_set(0);
        clear_sysrst_status();
        return;
    }

    /* 冷启动统一走 wait_pb_long_press：
     * - 内循环 usb_at_entry=1（USB 供电启动）→ 单纯等 PB 3 秒 → normal
     * - 内循环 usb_at_entry=0（电池供电 + PB 启动）→ 若计时期间插 USB，
     *   立即退出判 recovery；否则满 3 秒 normal。*/
    if (wait_pb_long_press()) {
        mark_recovery(boot_info_raw);
    } else {
        led_set(1);
    }
#else
    /* 普通冷启动直接开机，不再要求 PB 长按，也不再进入 charging_wait。 */
    latch_set(1);
    led_set(1);
#endif
    clear_sysrst_status();
}

__boot_ramcode__ bool boot_power_guard_key_pressed(void)
{
    return key_pressed();
}

__boot_ramcode__ bool boot_power_guard_usb_plugged(void)
{
    return usb_plugged();
}

/* AON IOMUX 的 force-output 控制位（bit[24:22]：bit24 open force out、
 * bit23 output value、bit22 force enable）。CMN reset 不复位 AON 域，
 * 所以这条配置能撑过整个复位窗口；与 soc/arcs/common/sys_reboot.c 的
 * 写法保持一致。stage0 接管后 IOMuxManager_PinConfigure 会自动清掉
 * 这几位（IOMuxManager.c:227 的 ~0x1E00000 路径），无需手工还原。 */
#define BOOT_LATCH_AON_FORCE_MASK   0x01E00000u
#define BOOT_LATCH_AON_FORCE_HIGH   0x01C00000u
#define BOOT_LATCH_AON_FORCE_LOW    (0x01000000u | 0x00400000u)

__boot_ramcode__ void boot_power_guard_lock_latch_for_reset(void)
{
#if CONFIG_BOOT_POWER_GUARD_LATCH_PORT == CSK_IOMUX_PAD_B
    volatile uint32_t *aon_iomux =
        (volatile uint32_t *)&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all
        + CONFIG_BOOT_POWER_GUARD_LATCH_PIN;

    *aon_iomux &= ~BOOT_LATCH_AON_FORCE_MASK;
#if CONFIG_BOOT_POWER_GUARD_LATCH_ACTIVE_LEVEL
    *aon_iomux |= BOOT_LATCH_AON_FORCE_HIGH;
#else
    *aon_iomux |= BOOT_LATCH_AON_FORCE_LOW;
#endif
#endif
}

#endif /* CONFIG_BOOT_POWER_GUARD */
