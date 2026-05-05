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
#include "pm.h"
#include "amp_shared.h"


#define PM_GPIO_DBG         1

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


#define PM_UART_IDLE_TIME  1000000
#define PM_GPIO_IDLE_TIME  1000000
#define PM_RAM_RETENTION_BIT_MASK  0xFFFF



#if !defined(CFG_AMP_IPC) || defined(CFG_AMP_IPC_SLAVE)
/* In dual-core applications, the last core to enter sleep is the primary core;
 * in single-core applications, only the primary core exists.
**/
#define  IS_PM_CORE_PRIMARY  1
#else
#define  IS_PM_CORE_PRIMARY  0
#endif

#if (CONFIG_CORE_NUM == 1) || (IS_PM_CORE_PRIMARY == 0)
#define PM_GPIO_ACTIVE_DET      1
#if CONFIG_PM_UART_WAKEUP
#undef PM_GPIO_ACTIVE_DET
#define PM_GPIO_ACTIVE_DET      1
#endif
#endif

#if (CONFIG_CORE_NUM == 2)
#if (BOOT_HARTID == 0)
#define  PM_CORE_CUR         0
#define  PM_CORE_PEER        1
#else
#define  PM_CORE_CUR         1
#define  PM_CORE_PEER        0
#endif
#else
#if (CONFIG_CORE_NUM == 1)
#define  PM_CORE_CUR         0
#else
#error "Invalid CONFIG_CORE_NUM value; please verify"
#endif
#endif

#if (BOOT_HARTID == 0)
#define  RV_ILM_BASE         0x00080000
#define  RV_ILM_LEN          0x00004000
#else
#define  RV_ILM_BASE         0x00280000
#define  RV_ILM_LEN          0x00004000
#endif

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_FLASH       (0xBB)
#define WAKEUP_ACT_JUMP_NONE        (0xFF)

enum
{
    PM_CORE_STATE_ACTIVE = 0,
    PM_CORE_STATE_IDLE,
    PM_CORE_STATE_STARTUP,
    PM_CORE_STATE_MAX
};

enum {
    PM_HANDLE_TYPE_DEV  = 0,
    PM_HANDLE_TYPE_COMM = 1,
    PM_HANDLE_TYPE_MAX
};

struct _pm_handler {
    uint32_t id; /*It's the identity and also the priority*/
    pm_handler_ops_t ops;
    struct _pm_handler *next;
};

typedef struct _pm_handler pm_handler_t;

struct pm_env_t
{
    pm_handler_t *handle[PM_HANDLE_TYPE_MAX];
    uint32_t lock_bits;
    #if PM_GPIO_ACTIVE_DET
    uint32_t wakeup_time;
    #endif
    uint64_t startup_time;
    volatile pm_config_t *config;
    volatile struct pm_core_context *core_ctx;
    volatile pm_sleep_config_t *sleep_cfg;
    volatile uint32_t *last_wakeup_cause;
    pm_gpio_retention_t gpio_retention[PM_GPIO_RETENTION_MAX];
};

extern struct pm_env_t pm_env;

int32_t pm_impl_init(void);

int32_t pm_internal_register(int32_t type, int32_t id, pm_handler_ops_t *ops);

int32_t pm_internal_unregister(int32_t type, int32_t id);

int32_t pm_device_register(int32_t dev_id, pm_handler_ops_t *ops);

int32_t pm_device_unregister(int32_t dev_id);
#endif  /* _PM_IMPL_H_ */