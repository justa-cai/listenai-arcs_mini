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
#include "PSRAMManager.h"


#ifndef CONFIG_PM_DEBUG
#define CONFIG_PM_DEBUG       0
#endif

#ifndef PM_TRACE_GPIO_ON
#define PM_TRACE_GPIO_ON      CONFIG_PM_DEBUG
#endif
#ifndef PM_TRACE_HB_ON
#define PM_TRACE_HB_ON        CONFIG_PM_DEBUG
#endif

/* derived: any path that physically drives the AON trace pads */
#define PM_TRACE_PAD_ON       (CONFIG_PM_DEBUG && (PM_TRACE_GPIO_ON || PM_TRACE_HB_ON))

#if PM_TRACE_PAD_ON
#define PM_SET_GPIOB(pin, level) (((union CORE_IOMUX_REG_PAD_GPIOB_00*)(0x48100000UL + (pin<<2)))->bit.PAD_GPIOB_00_OUT_REG = level)
#define PM_TOGGLE_GPIOB(pin)     (((union CORE_IOMUX_REG_PAD_GPIOB_00*)(0x48100000UL + (pin<<2)))->bit.PAD_GPIOB_00_OUT_REG ^= 1)
#else
#define PM_SET_GPIOB(pin, level)
#define PM_TOGGLE_GPIOB(pin)
#endif

#if CONFIG_PM_DEBUG
#define pm_dbg(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define pm_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#else
#define pm_dbg(fmt, ...)
#define pm_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#endif

#if (BOOT_HARTID == 0)
#define PM_TRACE_HB_PIN   0
#else
#define PM_TRACE_HB_PIN   1
#endif

#ifndef PM_TRACE_GPIO_HARTID
#define PM_TRACE_GPIO_HARTID   0   /* only this core drives the shared trace pins */
#endif
#ifndef PM_TRACE_GPIO0
#define PM_TRACE_GPIO0   7
#endif
#ifndef PM_TRACE_GPIO1
#define PM_TRACE_GPIO1   8
#endif
#ifndef PM_TRACE_GPIO2
#define PM_TRACE_GPIO2   9
#endif

#define PM_STAGE_MAGIC        0x5A5A0000U   /* tags a valid stage vs a leftover entry addr */
#define PM_STAGE_MASK         0x000000FFU
#define PM_STAGE_CODE_MASK    0x0000007FU

#if (BOOT_HARTID == 0)
#define PM_STAGE_REG     IP_AON_CTRL->REG_AON_DIG_RSVD1.all
#else
#define PM_STAGE_REG     IP_AON_CTRL->REG_AON_DIG_RSVD3.all
#endif

/*
 * stage byte = direction | (gpio_code << 4) | seq
 *   bit7      direction: 0 = sleep path, 1 = wakeup path
 *   bit[6:4]  3-bit GPIO code, driven on the trace pins
 *   bit[3:0]  fine-grained sub-step, visible only in the AON register
 *
 * Sleep and wakeup each get their own GPIO code space 1..7; ACTIVE drives 0,
 * separating the two runs in a logic analyzer capture. A frozen GPIO value
 * alone is ambiguous between sleep-N and wake-N - disambiguate by the capture
 * history (which side of the 0 / power-gap it is on) or the supply current;
 * the AON register keeps bit7, so post-mortem reads are unambiguous.
 */
#define PM_STG_WK            0x80   /* direction bit: wakeup path */

enum pm_stage {
    PM_STG_ACTIVE        = 0x00,              /* back in the RTOS - GPIO 0    */
    /* ---- sleep path, GPIO codes 1..7 --------------------------------------*/
    PM_STG_SLEEP_ENTER   = 0x10,              /* pm_light_sleep() entered     */
    PM_STG_SLEEP_DEVOFF  = 0x20,              /* device on_enter handlers     */
    PM_STG_SLEEP_PREPARE = 0x30,              /* save context / wakeup source */
    /* -- window A (GPIO only: RSVD holds the live wakeup entry) ------------ */
    PM_STG_SLEEP_HW      = 0x40,              /* deep-sleep mode configured   */
    PM_STG_SLEEP_WFI     = 0x70,              /* about to lose the core       */
    /* ---- wakeup path, GPIO codes 1..7 again -------------------------------*/
    PM_STG_WK_ENTRY      = PM_STG_WK | 0x10,  /* window C: first C code       */
    PM_STG_WK_CLOCK      = PM_STG_WK | 0x20,  /* BootClock restored           */
    PM_STG_WK_CACHE      = PM_STG_WK | 0x30,  /* I/D cache + ILM re-enabled   */
    PM_STG_WK_PSRAM      = PM_STG_WK | 0x31,  /* psram out of sleep           */
    PM_STG_WK_SNAPSHOT   = PM_STG_WK | 0x32,  /* snapshot restored            */
    PM_STG_WK_FLASH      = PM_STG_WK | 0x33,  /* flash controller re-inited   */
    PM_STG_WK_COPYTABLE  = PM_STG_WK | 0x40,  /* copy table replayed          */
    PM_STG_WK_IRQ        = PM_STG_WK | 0x41,  /* ECLIC / irq vectors re-inited*/
    PM_STG_WK_RESTORE    = PM_STG_WK | 0x50,  /* register context restored    */
    PM_STG_WK_WAKE_HOOK  = PM_STG_WK | 0x60,  /* device on_wake handlers      */
    PM_STG_WK_EXIT_HOOK  = PM_STG_WK | 0x70,  /* on_exit handlers             */
};

#define PM_STAGE_PHASE(s)    (((s) >> 4) & 0x7)   /* bit7 masked off naturally */
#define PM_STAGE_VALID(v)    (((v) & ~PM_STAGE_MASK) == PM_STAGE_MAGIC)
#define PM_STAGE_OF(v)       ((v) & PM_STAGE_CODE_MASK)
#define PM_STAGE_IS_WAKE(s)  (((s) & PM_STG_WK) != 0)

#if CONFIG_PM_DEBUG
/* only the selected core drives the shared GPIO pads*/
#if (BOOT_HARTID == PM_TRACE_GPIO_HARTID) && PM_TRACE_GPIO_ON
#define PM_TRACE_GPIO(phase) do { \
        PM_SET_GPIOB(PM_TRACE_GPIO0, (phase) & 1); \
        PM_SET_GPIOB(PM_TRACE_GPIO1, ((phase) >> 1) & 1); \
        PM_SET_GPIOB(PM_TRACE_GPIO2, ((phase) >> 2) & 1); \
    } while (0)
#else
#define PM_TRACE_GPIO(phase)
#endif

#if  PM_TRACE_HB_ON
#define PM_TRACE_HB()     PM_TOGGLE_GPIOB(PM_TRACE_HB_PIN)
#else
#define PM_TRACE_HB()
#endif


#define PM_TRACE(stage)   do { \
        PM_STAGE_REG = PM_STAGE_MAGIC | (uint32_t)(stage); \
        PM_TRACE_HB(); \
        PM_TRACE_GPIO(PM_STAGE_PHASE(stage)); \
    } while (0)

#define PM_TRACE_G(stage) do { \
        PM_TRACE_HB(); \
        PM_TRACE_GPIO(PM_STAGE_PHASE(stage)); \
    } while (0)

void pm_exc_dump(void);   /* called from the exception handler to revive UART */
#else
#define PM_TRACE_GPIO(phase)
#define PM_TRACE_HB()
#define PM_TRACE(stage)
#define PM_TRACE_G(stage)
#endif


#define PM_UART_IDLE_TIME  1000000
#define PM_GPIO_IDLE_TIME  1000000
#define PM_RAM_RETENTION_BIT_MASK  0xFFFF
#define PM_SNAPSHOT_MAGIC          0x504D534EU

#ifndef PM_SNAPSHOT_LOCK
#define PM_SNAPSHOT_LOCK()
#endif

#ifndef PM_SNAPSHOT_UNLOCK
#define PM_SNAPSHOT_UNLOCK()
#endif

extern uint32_t __pm_snapshot_psram_start;
extern uint32_t __pm_snapshot_psram_end;
extern uint32_t __pm_startup_reserved_start;
extern uint32_t __pm_startup_reserved_end;

#define PM_SNAPSHOT_PSRAM_BASE     ((uint32_t)&__pm_snapshot_psram_start)
#define PM_SNAPSHOT_PSRAM_SIZE     ((uint32_t)(&__pm_snapshot_psram_end) - (uint32_t)(&__pm_snapshot_psram_start))
#define PM_STARTUP_RESERVED_BASE   ((uint32_t)&__pm_startup_reserved_start)
#define PM_STARTUP_RESERVED_SIZE   ((uint32_t)(&__pm_startup_reserved_end) - (uint32_t)(&__pm_startup_reserved_start))


#if (CONFIG_CORE_NUM == 1) || (PM_CORE_PRIMARY == 0)
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

int32_t pm_snapshot_prepare(void);

int32_t pm_snapshot_add_region(uint32_t dst_addr, uint32_t size, uint32_t flags);

#if CONFIG_PM_CLOSE_AP
void pm_save_boot_gpio(void);

void pm_restore_boot_gpio(void);
#endif
#endif  /* _PM_IMPL_H_ */
