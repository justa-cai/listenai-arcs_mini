/****************************************************************************************
 *
 * @file pm_impl.h
 *
 * @brief power management
 *
 * Copyright (C) ListenAI 2024
 *
 *
 *
 ****************************************************************************************
 */

#ifndef _PM_IMPL_H_
#define _PM_IMPL_H_

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_PM_KEEP_ALIVE_PERIOD      30000
#define PM_GPIO_DBG         0

#if PM_GPIO_DBG
#define pm_dbg(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define pm_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define PM_SET_GPIOB(pin, level) (((union CORE_IOMUX_REG_PAD_GPIOB_00*)(0x48100000UL + (pin<<2)))->bit.PAD_GPIOB_00_OUT_REG = level)
#else
#define pm_dbg(fmt, ...)
#define pm_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define PM_SET_GPIOB(pin, level)
#endif
#if (BOOT_HARTID == 0)
#define PM_SLEEP_PIN   0
#else
#define PM_SLEEP_PIN   1
#endif

#if !defined(CFG_AMP_IPC) || defined(CFG_AMP_IPC_MASTER)
#define PM_GPIO_ACTIVE_DET      1
#if CONFIG_PM_UART_WAKEUP
#undef PM_GPIO_ACTIVE_DET
#define PM_GPIO_ACTIVE_DET      1
#endif
#endif

#define PM_WAKEUP_RTC     (1U << 0)
#define PM_WAKEUP_GPIO    (1U << 1)
#define PM_WAKEUP_TIMER   (1U << 2)
#define PM_WAKEUP_UART    (1U << 3)


#define PM_GPIO_PIN_MAX   10
#define PM_UART_IDLE_TIME  1000000
#define PM_GPIO_IDLE_TIME  1000000
#define PM_RAM_RETENTION_BIT_MASK  0xFFFF


typedef enum {
    PM_MODE_ACTIVE = 0,
    PM_MODE_LIGHT_SLEEP,   // Light sleep
    PM_MODE_DEEP_SLEEP,    // Deep sleep
    PM_MODE_MAX
} pm_mode_t;

typedef enum {
    PM_CLOCK_LEVEL0,  /*maximum clock frequency*/
    PM_CLOCK_LEVEL1,
    PM_CLOCK_LEVEL2,
    PM_CLOCK_LEVEL_COUNT,
} pm_clock_level_t;
typedef struct {
    pm_mode_t   mode;           // 要进入的低功耗模式
    uint32_t    timeout_ms;     // 最大睡眠时长（ms，0 表示无限制）
    uint32_t    wakeup_source;  // PM_WAKEUP_* 位掩码
    uint32_t    gpio_pin_mask;  // 当使用 GPIO 唤醒时的引脚编号
    uint32_t    retention_bits; // 位掩码
    int32_t     clock_level;
    bool        auto_mode;
    bool        keep_alive;
#if CONFIG_PM_DEBUG
    uint8_t     dbg_level;
#endif
} pm_config_t;

typedef struct {
    pm_mode_t   current_mode;
    uint32_t    active_locks;          //对应 pm_lock_t 位掩码
    uint32_t    wakeup_source;         //唤醒源
} pm_status_t;

typedef enum {
    PM_LOCK_NONE = 0,
    PM_LOCK_WIFI,
    PM_LOCK_BT,
    PM_LOCK_FLASH,
    PM_LOCK_APP,
	PM_LOCK_MAX = 32
} pm_lock_t;

enum {
    PM_DBG_OFF,
    PM_DBG_CRT,
    PM_DBG_INF,
    PM_DBG_VRB,
    PM_DBG_MAX
};

/**
 * @brief 外设低功耗操作集合，每个外设模块需实现并注册到 Power Manager。
 */
typedef struct {
    const char *name;
    uint32_t flags;
    void *next;
    /**
     * @brief 系统准备进入低功耗关闭外设电源。
     * @param mode 当前将要进入的低功耗模式（LIGHT/DEEP）
     * @return 0 -允许继续轮询，<0 -拒绝本轮 sleep
     */
    int32_t (*pm_suspend)(pm_mode_t mode);

    /**
     * @brief 判断外设在指定模式下是否允许睡眠（skip 判断）。
     *        Power Manager 会在综合所有模块后决定是否进入 sleep。
     * @param mode 当前将要进入的低功耗模式
     * @return >0=允许该外设进入 sleep，0=不允许（会跳过本轮 sleep），<0=错误
     */
    int32_t (*pm_check_idle)(pm_mode_t mode);

    /**
     * @brief 从低功耗恢复后调用，用于重启或重配置外设。
     * @param mode 刚退出的低功耗模式
     */
    int32_t (*pm_resume)(uint32_t cause);
} pm_peripheral_dev_t;
struct pm_reg_info
{
    volatile uint32_t *addr;
    uint32_t value;
};

/**
 * @brief 初始化电源管理模块（在系统启动时调用）。
 */
int32_t pm_init(void);

/**
 * @brief 外设启动时注册PM接口
 */
int32_t pm_peripheral_register(pm_peripheral_dev_t *pm_dev);

/**
 * @brief 外设注销PM接口
 */
int32_t pm_peripheral_unregister(pm_peripheral_dev_t *dev_ops);

/**
 * @brief 配置并切换到指定低功耗模式。
 * @param config  配置结构体指针
 * @return 0 成功，<0 错误码
 */
int32_t pm_set_config(pm_config_t *config);

/**
 * @brief 获取当前电源管理状态。
 * @param status  输出状态结构体指针
 * @return 0 成功，<0 错误码
 */
int32_t pm_get_status(pm_status_t *status);

/**
 * @brief 申请一个功耗锁，防止进入某些睡眠模式。
 * @param lock  要申请的锁（可按位或组合）。
 * @return 0 成功，<0 错误码
 */
int32_t pm_lock_acquire(pm_lock_t lock);

/**
 * @brief 释放一个功耗锁。
 * @param lock  要释放的锁。
 * @return 0 成功，<0 错误码
 */
int32_t pm_lock_release(pm_lock_t lock);

int32_t pm_init(void);

int32_t pm_enable_keep_alive(bool enable);
#endif  /* _PM_IMPL_H_ */

