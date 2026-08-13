/****************************************************************************************
 *
 * @file pm.h
 *
 * @brief power management
 *
 * Copyright (C) ListenAI 2024
 *
 *
 *
 ****************************************************************************************
 */

#ifndef _PM_H_
#define _PM_H_

#include <stdint.h>

#define PM_GPIO_PIN_MAX              10
#define PM_GPIO_RETENTION_MAX        4
#define PM_RAM_REGIN_RETENTION_MAX   4
#define PM_SNAPSHOT_MAX_REGION       4

typedef enum {
    PM_MODE_ACTIVE = 0,
    PM_MODE_LIGHT_SLEEP,
    PM_MODE_DEEP_SLEEP,
    PM_MODE_MAX
} pm_mode_t;

typedef enum {
    PM_CLOCK_LEVEL0,  /*maximum clock frequency*/
    PM_CLOCK_LEVEL1,
    PM_CLOCK_LEVEL2,
    PM_CLOCK_LEVEL_COUNT,
} pm_clock_level_t;

typedef enum {
    PM_WAKEUP_TIMER = 0,
    PM_WAKEUP_RTC,
    PM_WAKEUP_BT,
    PM_WAKEUP_WIFI,
    PM_WAKEUP_GPIO,
    PM_WAKEUP_MAX
} pm_wakeup_source_t;

typedef enum {
    PM_GPIO_MODE_LOW  = 0,
    PM_GPIO_MODE_HIGH,
} pm_gpio_wakeup_mode_t;

enum {
    PM_DBG_OFF = 0,
    PM_DBG_CRT,
    PM_DBG_INF,
    PM_DBG_VRB,
    PM_DBG_MAX
};

typedef struct {
    uint32_t gpio_idx;
    uint32_t val;
} pm_gpio_retention_t;

enum {
    PM_SNAPSHOT_OK = 0,
    PM_SNAPSHOT_ERR_STATE = -1,
    PM_SNAPSHOT_ERR_ARG = -2,
    PM_SNAPSHOT_ERR_FULL = -3,
    PM_SNAPSHOT_ERR_OVERLAP = -4,
    PM_SNAPSHOT_ERR_NOSPACE = -5,
    PM_SNAPSHOT_ERR_MAGIC = -6,
    PM_SNAPSHOT_ERR_CHECK = -7,
    PM_SNAPSHOT_ERR_INVALID = -8,
    PM_SNAPSHOT_ERR_RESERVED = -9,
};

enum {
    PM_SNAPSHOT_REGION_VALID      = (1U << 0),
    PM_SNAPSHOT_REGION_RESTORE_EN = (1U << 1),
};

typedef struct {
    uint32_t dst_addr;
    uint32_t size;
    uint32_t psram_addr;
    uint32_t checksum;
    uint32_t flags;
} pm_image_desc_t;

typedef struct {
    uint32_t magic;
    uint32_t region_num;
    uint32_t total_size;
    pm_image_desc_t region[PM_SNAPSHOT_MAX_REGION];
} pm_image_header_t;

typedef struct {
    pm_mode_t mode;
    pm_clock_level_t  clock_level;
    uint16_t  auto_mode;
    uint16_t  dbg_level;
} pm_config_t;

typedef struct {
    uint32_t wakeup_src_mask;
    uint32_t time_us;
    uint32_t gpio_mask;
    uint32_t gpio_level;
    uint32_t retention_bits;
} pm_sleep_config_t;

typedef enum {
    PM_LOCK_NONE = 0,
    PM_LOCK_WIFI,
    PM_LOCK_BT,
    PM_LOCK_FLASH,
    PM_LOCK_APP,
    PM_LOCK_MAX = 32
} pm_lock_t;

typedef enum {
    PM_DEV_ID_WIFI = 0,
    PM_DEV_ID_UART,
} pm_dev_id_t;

typedef enum {
    PM_HOOK_ID_0 = 0,
    PM_HOOK_ID_1,
} pm_hook_id_t;

typedef int32_t (*pm_handler_func_t)(uint32_t sleep_time_us, void *arg);

typedef struct {
    int32_t (*check_idle)(pm_mode_t mode);
    pm_handler_func_t on_enter;
    pm_handler_func_t on_exit;
    pm_handler_func_t on_wake;
} pm_handler_ops_t;


struct pm_reg_info
{
    volatile uint32_t *addr;
    uint32_t value;
};


/**
 * @brief 配置并切换到指定低功耗模式。
 * @param config  配置结构体指针
 * @return 0 成功，<0 错误码
 */
int32_t pm_set_config(pm_config_t *config);

int32_t pm_init(void);

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

int32_t pm_device_register(int32_t pm_dev_id_t, pm_handler_ops_t *ops);

int32_t pm_device_unregister(int32_t pm_dev_id_t);

int32_t pm_hook_register(pm_hook_id_t hook_id, pm_handler_func_t enter, pm_handler_func_t exit);

int32_t pm_hook_unregister(pm_hook_id_t hook_id);

int32_t pm_enable_gpio_wakeup(uint32_t mask, uint32_t level);

int32_t pm_disable_gpio_wakeup(uint32_t mask);

int32_t pm_register_gpio_retention(uint32_t pad, uint32_t gpio);

int32_t pm_enable_timer_wakeup(uint32_t time_in_us);

int32_t pm_disable_timer_wakeup(void);

int32_t pm_get_sleep_config(pm_sleep_config_t *sleep_config);

int32_t pm_set_sleep_config(pm_sleep_config_t *sleep_config);

int32_t pm_register_snapshot_region(uint32_t dst_addr, uint32_t size, uint32_t flags);

uint64_t pm_get_startup_time(void);

void pm_force_ap_off(void);

void pm_force_ap_on(void);

void pm_wakeup_other_core(void);

#endif  /* _PM_IMPL_H_ */
