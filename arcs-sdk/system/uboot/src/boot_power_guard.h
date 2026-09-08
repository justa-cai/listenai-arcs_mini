#ifndef __BOOT_POWER_GUARD_H__
#define __BOOT_POWER_GUARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_BOOT_POWER_GUARD

/* 在 boot_f 中尽早调用。sysrst_status = REG_SYSRST_STATUS 快照，
 * boot_info_raw = &REG_AON_DIG_RSVD4（可读写）。
 *
 * POR：PB+USB 组合直接写 recover_reason=HARD_REQ 进 recovery；
 *      否则自旋等 PB 长按，满足后 latch 返回。
 * 非 POR：shutdown_req=1 清标志 + 释放 latch + 进假关机自旋；
 *      其它保险拉高 latch 立即返回。
 * factory_mode：冷启动已插 USB 时直接进入产测；电池供电仍执行长按策略。*/
void boot_power_guard_run(uint32_t sysrst_status, uint32_t *boot_info_raw,
                          bool factory_mode);

/* 读 POWER_KEY 当前电平，给 stage1 用（比如充电提示界面里轮询判定开机）。*/
bool boot_power_guard_key_pressed(void);

/* 读 USB_DET 当前电平。*/
bool boot_power_guard_usb_plugged(void);

/* 在 stage1 触发 CMN SW reset 之前调用。CMN reset 期间 GPIO 外设会被
 * 复位，latch (PWR_LOCK) 短暂失去 drive；电池模式下 MOSFET 栅极撑不过
 * 这个窗口，VCC 跌落后整个设备掉电。这里把 latch 切到 AON IOMUX
 * force-output（AON 不被 CMN reset 影响），让 latch 在复位全程被硬拉
 * 到 active 电平。USB 维持 VCC 的场景调用也无副作用。 */
void boot_power_guard_lock_latch_for_reset(void);

#else /* !CONFIG_BOOT_POWER_GUARD */

static inline void boot_power_guard_run(uint32_t sysrst_status, uint32_t *boot_info_raw,
                                        bool factory_mode)
{
    (void)sysrst_status;
    (void)boot_info_raw;
    (void)factory_mode;
}

static inline bool boot_power_guard_key_pressed(void)
{
    return false;
}

static inline bool boot_power_guard_usb_plugged(void)
{
    return false;
}

static inline void boot_power_guard_lock_latch_for_reset(void)
{
}

#endif /* CONFIG_BOOT_POWER_GUARD */

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_POWER_GUARD_H__ */
