/****************************************************************************************
 *
 * @file pm_impl.c
 *
 * @brief power management
 *
 * Copyright (C) ListenAI 2024
 *
 *
 *
 ****************************************************************************************
 */
#include <string.h>
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "PowerManager.h"
#include "Driver_AON_TIMER.h"
#include "PowerManager.h"
#include "ClockManager.h"
#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "spiflash.h"
#include "platform.h"
#include "ic_spinlock.h"
#include "ipc.h"
#include "pm_impl.h"
#include "wifi_ps_hw.h"
#include "log_print.h"
#include "shell_def.h"
#include "vrtc.h"
#include "mrpc_utils_m2s_api_client.h"


#if CONFIG_PM

#define SYSTICK_TICK_CONST          (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ)
#define portMAX_BIT_NUMBER          ( SysTimer_MTIMER_Msk )

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_FLASH       (0xBB)
#define WAKEUP_ACT_JUMP_NONE        (0xFF)

#define WAKEUP_CAUSE_ALL            0x3FF007F
#define WAKEUP_DELAY_US             100

#define REG_PL_RD(addr)              (*(volatile uint32_t *)(addr))
#define REG_PL_WR(addr, value)       (*(volatile uint32_t *)(addr)) = (value)

#if (BOOT_HARTID == 0)
#define  RV_ILM_BASE         0x00080000
#define  RV_ILM_LEN          0x00004000
#else
#define  RV_ILM_BASE         0x00280000
#define  RV_ILM_LEN          0x00004000
#endif
enum
{
    PM_CORE_MASTER,
    PM_CORE_SLAVE,
    PM_CORE_MAX
};

enum
{
    PM_CORE_STATE_ACTIVE,
    PM_CORE_STATE_IDLE,
    PM_CORE_STATE_SLEEP,
    PM_CORE_STATE_WAKEUP,
    PM_CORE_STATE_WAIT,
    PM_CORE_STATE_MAX
};

extern void __idle_save(void);
extern void __idle_restore(void);
extern void ECLIC_Init(void);
extern void BootClock_Init();
extern void irq_vectors_reinit(void);
extern void __light_sleep_entry(void);
extern void mem_copy(void *dst, void *src, int32_t len);
extern void BootClock_restore(void);
extern void BootClock_save(void);

extern void vPortSetupTimerInterrupt(void);
static int32_t pm_peripheral_ctrl(uint32_t cause, int32_t opteration);
static void pm_force_ap_pd(void);
#if CONFIG_PM_KEEP_ALIVE
static void pm_keep_alive(void);
#endif
struct pm_env_t
{
    pm_config_t config;
    pm_peripheral_dev_t *dev;
    uint32_t lock_bits;
    uint32_t wakeup_time;
#if CONFIG_PM_KEEP_ALIVE
    uint32_t arp_time;
    rtos_timer arp_timer_handle;
#endif
};


volatile pmu_wakeupsrc_t wakeup_cause = PMU_WAKEUP_NONE;
volatile uint32_t __stack_store_repo = 0;

static struct pm_env_t pm_env =
{
    .config.mode = PM_MODE_ACTIVE,
};

#if PM_GPIO_ACTIVE_DET
static bool pm_gpio_active = false;
#endif
static struct pm_reg_info pm_reg_context[] =
{
#if (BOOT_HARTID == 0) || !defined(CFG_AMP_IPC)
    {&IP_MAILBOX->REG_CP_MAILBOX_CTRL.all},
    {&IP_SYSCTRL->REG_PERI_CLK_CFG1.all},
    {&IP_SYSCTRL->REG_PERI_CLK_CFG2.all},
    {&IP_AON_CTRL->REG_POWER_WKUP_CTRL0.all},
    {&IP_AON_CTRL->REG_WAKEUP_ENABLE.all},
#if defined(UART0_IO_TX_PAD)
#if (UART0_IO_TX_PAD == CSK_IOMUX_PAD_B)
    {&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + UART0_IO_TX_PIN},
    {&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + UART0_IO_TX_PIN},
#else
    {&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + UART0_IO_TX_PIN},
#endif
#if (UART0_IO_RX_PAD == CSK_IOMUX_PAD_B)
    {&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + UART0_IO_RX_PIN},
    {&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + UART0_IO_RX_PIN},
#else
    {&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + UART0_IO_RX_PIN},
#endif
    {&IP_UART0->REG_CTRL.all},
    {&IP_UART0->REG_TRIGGERS.all},
    {&IP_UART0->REG_IRQ_MASK.all},
#endif
#if defined(UART1_IO_TX_PAD)
#if (UART1_IO_TX_PAD == CSK_IOMUX_PAD_B)
    {&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + UART1_IO_TX_PIN},
    {&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + UART1_IO_TX_PIN},
#else
    {&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + UART1_IO_TX_PIN},
#endif
#if (UART1_IO_RX_PAD == CSK_IOMUX_PAD_B)
    {&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + UART1_IO_RX_PIN},
    {&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + UART1_IO_RX_PIN},
#else
    {&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + UART1_IO_RX_PIN},
#endif
    {&IP_UART1->REG_CTRL.all},
    {&IP_UART1->REG_TRIGGERS.all},
    {&IP_UART1->REG_IRQ_MASK.all},
#endif

#if defined(UART2_IO_TX_PAD)
#if (UART2_IO_TX_PAD == CSK_IOMUX_PAD_B)
    {&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + UART2_IO_TX_PIN},
#else
    {&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + UART2_IO_TX_PIN},
#endif
#if (UART2_IO_RX_PAD == CSK_IOMUX_PAD_B)
    {&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + UART2_IO_RX_PIN},
#if CONFIG_PM_UART_WAKEUP
    {&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + UART2_IO_RX_PIN},
#endif
#else
    {&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + UART2_IO_RX_PIN},
#endif
    {&IP_UART2->REG_CTRL.all},
    {&IP_UART2->REG_TRIGGERS.all},
    {&IP_UART2->REG_IRQ_MASK.all},
#endif
#else
    {&IP_MAILBOX->REG_AP_MAILBOX_CTRL.all},
#endif

    {0}
};

static _PM_RAM_TEXT void pm_restore_context(struct pm_reg_info *reg)
{
#if (BOOT_HARTID == 0) || !defined(CFG_AMP_IPC)
#if defined(UART0_IO_TX_PAD) || defined(UART1_IO_TX_PAD) || defined(UART2_IO_TX_PAD)
    uint32_t val = 0;
#endif
#if defined(UART0_IO_TX_PAD)
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART0_RESET_Pos;
#endif
#if defined(UART1_IO_TX_PAD)
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART1_RESET_Pos;
#endif
#if defined(UART2_IO_TX_PAD)
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART2_RESET_Pos;
#endif

#if defined(UART0_IO_TX_PAD) || defined(UART1_IO_TX_PAD) || defined(UART2_IO_TX_PAD)
    IP_SYSCTRL->REG_SW_RESET_CP2.all |= val;
#endif
#endif
    while (reg->addr)
    {
        REG_PL_WR(reg->addr, reg->value);
        reg++;
    }
#if (BOOT_HARTID == 0) || !defined(CFG_AMP_IPC)
#if defined(UART0_IO_TX_PAD)
    IP_SYSCTRL->REG_PERI_CLK_CFG1.all |= 1 << CMN_SYSCFG_PERI_CLK_CFG1_DIV_UART0_CLK_LD_Pos;
#endif
#if defined(UART1_IO_TX_PAD)
    IP_SYSCTRL->REG_PERI_CLK_CFG2.all |= 1 << CMN_SYSCFG_PERI_CLK_CFG2_DIV_UART1_CLK_LD_Pos;
#endif
#if defined(UART2_IO_TX_PAD)
    IP_SYSCTRL->REG_PERI_CLK_CFG3.all |= 1 << CMN_SYSCFG_PERI_CLK_CFG3_DIV_UART2_CLK_LD_Pos;
#endif
#endif
}

static _PM_RAM_TEXT void pm_save_context(struct pm_reg_info *reg)
{
    while (reg->addr)
    {
        reg->value = REG_PL_RD(reg->addr);
        reg++;
    }
}
#if CONFIG_PM_CLOSE_AP
void pm_force_ap_off(void)
{
    if (!IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.AP_STATE_CURR)
    {
        IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.PD_AP_SUB = 1;
    }
}

void pm_force_ap_on(void)
{
    if (IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.AP_STATE_CURR)
    {
        vPortEnterCritical();
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_NONE;
        IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.PU_AP_SUB = 1;
        while (IP_AON_CTRL->REG_AON_DIG_RSVD0.all);
        vPortExitCritical();
    }
}
#endif

static void pm_ram_retention(uint32_t reten_bits)
{
    reten_bits = reten_bits & PM_RAM_RETENTION_BIT_MASK;
    IP_AON_CTRL->REG_RAM_RETENTION_SEL.bit.RAM_RETENTION_SEL = reten_bits;
    IP_AON_CTRL->REG_RAM_PGEN_FRC_REG.bit.RAM_PGEN_FRC_REG   = PM_RAM_RETENTION_BIT_MASK & (~reten_bits);
    IP_AON_CTRL->REG_RAM_PGEN_FRC.bit.RAM_PGEN_FRC = PM_RAM_RETENTION_BIT_MASK & (~reten_bits);
}

#if CONFIG_PM_KEEP_ALIVE
static void pm_arp_timer_cb(rtos_timer timer)
{
    net_arp_announce();
    if ((pm_env.config.mode == PM_MODE_ACTIVE) && (rtos_timer_get_period(timer) == 1))
        rtos_timer_schedule(timer, CONFIG_PM_KEEP_ALIVE_PERIOD);
}
#endif
int32_t pm_set_config(pm_config_t *config)
{
    int32_t clock = -1;

    if (config->mode >= PM_MODE_MAX)
        return -1;

    if (config->mode > PM_MODE_ACTIVE)
    {
        if ((config->clock_level != pm_env.config.clock_level) && (config->clock_level < PM_CLOCK_LEVEL_COUNT))
        {
            pm_env.config.clock_level = config->clock_level;
            clock = config->clock_level;
        }
    }
    else if (pm_env.config.clock_level)
    {
        clock = BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA;
    }

    if (clock >= 0)
    {
        vPortEnterCritical();
        HAL_CRM_SetHclkClkSrc(CRM_IpSrcXtalClk);
        CRM_InitCoreSrc(clock);
        HAL_CRM_SetHclkClkSrc(CRM_IpSrcCoreClk);
        vPortExitCritical();
    }

#ifdef CFG_AMP_IPC_MASTER
    pm_sync_config(config);
#endif
#if CONFIG_PM_CLOSE_AP
    pm_force_ap_off();
#endif
#if CONFIG_PM_KEEP_ALIVE
    if (pm_env.arp_timer_handle)
    {
        if (config->mode == PM_MODE_LIGHT_SLEEP)
        {
            if (config->keep_alive)
            {
                rtos_timer_set_reload_mode(pm_env.arp_timer_handle, false);
                pm_keep_alive();
            }
            else if (rtos_timer_is_active(pm_env.arp_timer_handle))
            {
                rtos_timer_stop(pm_env.arp_timer_handle);
            }
        }
        else if (config->mode == PM_MODE_ACTIVE)
        {
            rtos_timer_set_reload_mode(pm_env.arp_timer_handle, true);
            rtos_timer_schedule(pm_env.arp_timer_handle, 1);
            rtos_timer_start(pm_env.arp_timer_handle);
        }
    }
#endif
    pm_env.config = *config;

    return 0;
}
#ifdef CFG_AMP_IPC_SLAVE
int32_t pm_sync_config(pm_config_t *config)
{
	if (config->mode >= PM_MODE_MAX)
        return -1;

    pm_env.config = *config;

	return 0;
}
#endif
static int32_t pm_can_sleep(void)
{
    int32_t status = 1;
    pm_peripheral_dev_t *dev = pm_env.dev;

    if ((pm_env.config.mode != PM_MODE_LIGHT_SLEEP) ||  pm_env.lock_bits)
        return 0;
#if !defined(CFG_AMP_IPC) || defined(CFG_AMP_IPC_MASTER)
#if PM_GPIO_ACTIVE_DET
    if (pm_gpio_active && ((int32_t)((pm_env.wakeup_time + PM_GPIO_IDLE_TIME) - (uint32_t)rtos_get_sys_us()) > 0))
        return 0;
    pm_gpio_active = false;
#endif

#endif

    while (dev && status)
    {
        if (dev->pm_check_idle)
            status = dev->pm_check_idle(PM_MODE_LIGHT_SLEEP);
        dev = dev->next;
    }

    return status;
}

int32_t pm_lock_acquire(pm_lock_t lock)
{
    if (lock < PM_LOCK_MAX)
    {
        taskENTER_CRITICAL();
        pm_env.lock_bits |= lock;
        taskEXIT_CRITICAL();
    }

    return 0;
}

int32_t pm_lock_release(pm_lock_t lock)
{
    if (lock < PM_LOCK_MAX)
    {
        taskENTER_CRITICAL();
        pm_env.lock_bits &= ~lock;
        taskEXIT_CRITICAL();
    }

    return 0;
}

static uint32_t pm_get_wakeup_cause(void)
{
#ifndef CFG_AMP_IPC
    return IP_AON_CTRL->REG_WAKEUP_ISR.all;
#else
    volatile struct amp_shared_info* shared;

    shared = ipc_get_shared_info();
#ifdef CFG_AMP_IPC_MASTER
    if (ipc_get_app_status(IPC_APP_STATUS_VRTC_ALERT))
        return PMU_WAKEUP_TIMER;
#else
    shared->wakeup_cause = IP_AON_CTRL->REG_WAKEUP_ISR.all;
#endif
    return shared->wakeup_cause;
#endif
}
#ifdef CFG_AMP_IPC_SLAVE
void pm_wakeup_isr_from_ipc(void)
{
    ipc_send_notify(IPC_EVT_WAKEUP);
}

void pm_master_idle_request(void)
{
    volatile struct amp_shared_info* shared;

    shared = ipc_get_shared_info();
    if (shared->master_state.state == PM_CORE_STATE_IDLE)
        shared->master_state.state = PM_CORE_STATE_SLEEP;
}
#endif

#if (BOOT_HARTID == 0) && (CONFIG_PM)
typedef void (*proc_ptr) (void);
/*
* When using UART as a wake-up source, the low-level duration is too short for the AON module to accurately detect it,
* resulting in the REG_WAKEUP_ISR register not being set. Consequently, a fast jump cannot be performed in the ROM boot stage.
* As a workaround, the wake-up condition can only be checked in the secondary boot.
*/
void startup_check(void)
{
    #if CONFIG_PM_UART_WAKEUP && ((UART0_IO_RX_PAD == CSK_IOMUX_PAD_B) || (UART1_IO_RX_PAD == CSK_IOMUX_PAD_B))
    if ((IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_RAM) && IP_AON_CTRL->REG_AON_DIG_RSVD1.all)
    {
        proc_ptr entry = (proc_ptr)(IP_AON_CTRL->REG_AON_DIG_RSVD1.all);
        entry();
    }
    #endif
}
#endif

static void pm_dead_loop(void)
{
    while (1)
      __WFI();
}

static void pm_set_wakeup_entry(uint32_t entry)
{
#if (BOOT_HARTID == 0)
    IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_RAM;
    IP_AON_CTRL->REG_AON_DIG_RSVD1.all = entry;
#else
    IP_AON_CTRL->REG_AON_DIG_RSVD2.all = WAKEUP_ACT_JUMP_RAM;
#ifdef CFG_AMP_IPC
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = (uint32_t)pm_dead_loop;
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = entry;
#else
    IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_RAM;
    IP_AON_CTRL->REG_AON_DIG_RSVD1.all = pm_dead_loop;
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = entry;
#endif
#endif
}

static void pm_set_wakeup_source(void)
{
    if (pm_env.config.auto_mode)
        IP_AON_CTRL->REG_WAKEUP_ENABLE.all = (1 << PMU_WAKEUP_WIFI) | (1 << PMU_WAKEUP_TIMER);
    else
        IP_AON_CTRL->REG_WAKEUP_ENABLE.all = (1 << PMU_WAKEUP_WIFI);
    if (pm_env.config.gpio_pin_mask)
    {
        volatile union CORE_IOMUX_REG_PAD_GPIOB_00 *ptr;

        for (int32_t i = 0; i < PM_GPIO_PIN_MAX; i++)
        {
            if (pm_env.config.gpio_pin_mask & (1<<i))
            {
                #if CONFIG_PM_UART_WAKEUP && ((UART0_IO_RX_PAD == CSK_IOMUX_PAD_B) || (UART1_IO_RX_PAD == CSK_IOMUX_PAD_B))
                if ((i == UART0_IO_RX_PIN) || (i == UART1_IO_RX_PIN))
                    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, i, 0);
                #endif
                if (i != 7)
                {
                    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, i, 1);
                }
                else
                {
                    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, i, HAL_IOMUX_PULLUP_MODE);
                    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, i, 0);
                }
                HAL_PMU_GPIOPolaritySelect(PMU_WAKEUP_GPIOB_00 + i, 1);
                IP_AON_CTRL->REG_WAKEUP_ENABLE.all |= 1 << (PMU_WAKEUP_GPIOB_00 + i);
            }
        }
        IP_AON_CTRL->REG_GPIO_WAKEUP_CTRL.bit.GPIO_DB_CNT = 1;
    }
}

extern int32_t _mem_copy_func_start, _mem_copy_func_end, _mem_copy_func_lma, __copy_table_start__, __copy_table_end__;

static FLASH_DEV arcs_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xff,  // 0 means divider=2 //0xff,  //0xff means divider=1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
};

#ifdef CFG_AMP_IPC
_PM_RAM_TEXT void pm_sleep_startup(void)
{
    volatile int32_t *dst = &_mem_copy_func_start, *src = &_mem_copy_func_lma, *end = &_mem_copy_func_end;
    volatile int32_t *cpy_tb_entry, *cpy_tb_end;
    #ifdef CFG_AMP_IPC_MASTER
    volatile struct amp_shared_info* shared;
    #endif

    PM_SET_GPIOB(PM_SLEEP_PIN, 1);
    IP_AON_CTRL->REG_RAM_PGEN_FRC_REG.bit.RAM_PGEN_FRC_REG = 0;
    IP_AON_CTRL->REG_RAM_PGEN_FRC.bit.RAM_PGEN_FRC = 0;

    EnableICache();
    EnableDCache();
    // Set cache region mask
    __RV_CSR_WRITE(CSR_MNOCM,  ~(CMN_PSRAM_REGION - WIFI_RAM_REGION - 1));
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MNOCB, WIFI_RAM_REGION | 0x1);
    __RWMB();
    __FENCE_I();
#if (BOOT_HARTID == 0)
    flash_init(&arcs_flash_dev, 0, 0);
    BootClock_restore();
#endif
#ifdef CFG_AMP_IPC_MASTER
    shared = ipc_get_shared_info();
    shared->master_state.state = PM_CORE_STATE_WAIT;
#endif

    PM_SET_GPIOB(PM_SLEEP_PIN, 0);

    if (((uint32_t)dst >= RV_ILM_BASE) && ((uint32_t)dst < (RV_ILM_BASE + RV_ILM_LEN)))
    {
        if (dst != src)
        {
            while (dst < end)
                *dst++ = *src++;
        }

        cpy_tb_entry = &__copy_table_start__;
        cpy_tb_end   = &__copy_table_end__;
        while (cpy_tb_entry < cpy_tb_end)
        {
            int32_t len;
            src = (int32_t*)(*cpy_tb_entry++);
            dst = (int32_t*)(*cpy_tb_entry++);
            len = (int32_t)(*cpy_tb_entry++) - (int32_t)dst;

            if (dst != src)
                mem_copy((void*)dst, (void*)src, len);
        }
    }
    PM_SET_GPIOB(PM_SLEEP_PIN, 1);
//    ECLIC_Init();//mth
    ECLIC_SetCfgNlbits(__ECLIC_INTCTLBITS);
    wakeup_cause = pm_get_wakeup_cause();
    #ifdef CFG_AMP_IPC_MASTER
    #if CONFIG_PM_UART_WAKEUP && ((UART0_IO_RX_PAD == CSK_IOMUX_PAD_B) || (UART1_IO_RX_PAD == CSK_IOMUX_PAD_B))
    if (wakeup_cause == 0)
    {
        pm_gpio_active = true;
    }
    else
    #endif
    #if PM_GPIO_ACTIVE_DET
    if (wakeup_cause & (((1 << PM_GPIO_PIN_MAX) - 1) << PMU_WAKEUP_GPIOB_00))
        pm_gpio_active = true;
    #endif
    #endif

#if (BOOT_HARTID == 0)
    HAL_PMU_ClearWakeUpCause();
#if CONFIG_PM_DEBUG
    //wifi_ps_gpio_init();
#endif
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif
    irq_vectors_reinit();
    ECLIC_SetShvIRQ(SysTimerSW_IRQn, ECLIC_VECTOR_INTERRUPT);
    PM_SET_GPIOB(PM_SLEEP_PIN, 0);
    __idle_restore();
}

#if CONFIG_PM_DEBUG
static uint32_t pm_irq_status[(IRQ_MAX + 31) / 32];
#endif
static void pm_light_sleep(TickType_t xExpectedIdleTime)
{
    uint8_t priority;
    uint32_t complete_tick_periods;
    volatile uint64_t wakeup_time, sleep_time, system_timer;
    uint32_t gap, ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements;
    volatile TickType_t xModifiableIdleTime, XLastLoadValue;
    TickType_t xMaximumPossibleSuppressedTicks = (TickType_t)(portMAX_BIT_NUMBER / SYSTICK_TICK_CONST);
    volatile struct amp_shared_info* shared;

    if (xExpectedIdleTime > xMaximumPossibleSuppressedTicks)
        xExpectedIdleTime = xMaximumPossibleSuppressedTicks;

    #if 1
    SysTimer_Stop();
    #else
    ECLIC_DisableIRQ(SysTimer_IRQn);
    #endif
#ifdef CFG_AMP_IPC_MASTER
    vPortEnterCritical();
#endif
    __disable_irq();
    log_flush();

    if (eTaskConfirmSleepModeStatus() != eAbortSleep)
    {
#ifdef CFG_AMP_IPC_MASTER
        priority = ECLIC_GetLevelIRQ(IRQ_MAILBOX_2_VECTOR);
        ECLIC_SetLevelIRQ(IRQ_MAILBOX_2_VECTOR, configMAX_SYSCALL_INTERRUPT_PRIORITY);
#endif
        shared  = ipc_get_shared_info();
        SysTimer_ClearSWIRQ();
#ifdef CFG_AMP_IPC_SLAVE
        //if ((shared->master_state.state != PM_CORE_STATE_SLEEP) || (shared->master_state.wakeup_time < (sleep_time + 1000)))
        if (shared->master_state.state != PM_CORE_STATE_SLEEP)
        {
            #if 1
            SysTimer_Start();
            #else
            ECLIC_EnableIRQ(SysTimer_IRQn);
            #endif
            SysTick_Reload(SYSTICK_TICK_CONST);
            __WFI();
            __enable_irq();
            return;
        }

        sleep_time   = vrtc_get_time_us();
        system_timer = SysTimer_GetLoadValue();
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        pm_peripheral_ctrl(PM_MODE_LIGHT_SLEEP, 1);
        pm_save_context(pm_reg_context);
        BootClock_save();
        if (pm_env.config.auto_mode)
            vrtc_set_timer(VRTC_TIMER_IDX_LOCAL, ((xExpectedIdleTime * 1000) - WAKEUP_DELAY_US), NULL);
        pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
        pm_set_wakeup_source();
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
        HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI);
        pm_ram_retention(0xDFF);
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        wakeup_cause = 0;
        __idle_save();
        pm_restore_context(pm_reg_context);
#else
        sleep_time   = vrtc_get_time_us();
        system_timer = SysTimer_GetLoadValue();
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        ipc_clear_app_status(IPC_APP_STATUS_VRTC_ALERT);
        pm_save_context(pm_reg_context);
        if (pm_env.config.auto_mode)
            vrtc_set_timer(VRTC_TIMER_IDX_IPC, ((xExpectedIdleTime * 1000) - WAKEUP_DELAY_US), NULL);
        pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
        shared->master_state.sleep_time  = sleep_time;
        shared->master_state.wakeup_time = sleep_time + xExpectedIdleTime*1000;
        shared->master_state.state       = PM_CORE_STATE_IDLE;
        ipc_send_notify(IPC_EVT_ENTER_IDLE);
        __set_wfi_sleepmode(WFI_DEEP_SLEEP);
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        wakeup_cause = 0;
        __idle_save();
        pm_restore_context(pm_reg_context);
        if (shared->master_state.state == PM_CORE_STATE_WAIT)
            __WFI();
        shared->master_state.state = PM_CORE_STATE_ACTIVE;
#endif
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
#if CONFIG_PM_DEBUG
        if (pm_env.config.dbg_level)
        {
            pm_irq_status[0] = 0;
            pm_irq_status[1] = 0;
            pm_irq_status[2] = 0;
            for (int32_t i = 0; i < IRQ_MAX; i++)
            {
                if (ECLIC_GetPendingIRQ(i) && ECLIC_GetEnableIRQ(i))
                    pm_irq_status[i>>5] |= 1 << (i & 0x1F);
            }
        }
#endif
        wakeup_time = vrtc_get_time_us();
        gap  = (uint32_t)(wakeup_time - sleep_time);
        SysTimer_SetLoadValue(system_timer + gap + 20);
#if 0//def CFG_AMP_IPC_SLAVE
        __enable_irq();
        __FENCE_I();
        __NOP();
        __disable_irq();
#endif
        #ifdef CFG_AMP_IPC_SLAVE
        pm_peripheral_ctrl(wakeup_cause, 0);

        if (shared->master_state.state == PM_CORE_STATE_WAIT)
            ipc_send_notify(IPC_EVT_WAKEUP);
        #endif
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        complete_tick_periods = (gap/1000) / (1000/configTICK_RATE_HZ);
        //logDbg("T 0x%x %u %u %u\n", wakeup_cause, gap, complete_tick_periods, xExpectedIdleTime);

        if (complete_tick_periods > xExpectedIdleTime)
            vTaskStepTick(xExpectedIdleTime);
        else
            vTaskStepTick(complete_tick_periods);
        vPortSetupTimerInterrupt();
        SysTimer_Start();
#if PM_GPIO_ACTIVE_DET
        pm_env.wakeup_time = rtos_get_sys_us();
#endif
#ifdef CFG_AMP_IPC_MASTER
        ECLIC_SetLevelIRQ(IRQ_MAILBOX_2_VECTOR, priority);
#endif
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
#if CONFIG_PM_DEBUG
        if (pm_env.config.dbg_level && pm_env.config.dbg_level <= PM_DBG_MAX)
        {
            logDbg("wk:%u cause:0x%x\n", gap, wakeup_cause);
            if (pm_env.config.dbg_level >= PM_DBG_INF)
            {
                for (int32_t i = 0; i < sizeof(pm_irq_status) / sizeof(uint32_t); i++)
                {
                    if (pm_irq_status[i])
                        logDbg("irq[%d]=0x%x\n", i, pm_irq_status[i]);
                }
                if (pm_env.config.dbg_level >= PM_DBG_VRB)
                    logDbg("sleep:%u wake:%u\n", sleep_time, wakeup_time);
            }
        }
#endif
#if CONFIG_PM_KEEP_ALIVE
        pm_keep_alive();
#endif
    }
    else
    {
        #if 1
        SysTimer_Start();
        SysTick_Reload(SYSTICK_TICK_CONST);
        #else
        ECLIC_EnableIRQ(SysTimer_IRQn);
        #endif
    }
#ifdef CFG_AMP_IPC_MASTER
    vPortExitCritical();
#endif
    __enable_irq();
}
#else
_PM_RAM_TEXT void pm_sleep_startup(void)
{
    volatile int32_t *dst = &_mem_copy_func_start, *src = &_mem_copy_func_lma, *end = &_mem_copy_func_end;
    volatile int32_t *cpy_tb_entry, *cpy_tb_end;

    PM_SET_GPIOB(0, 0);
    PM_SET_GPIOB(0, 1);
    IP_AON_CTRL->REG_RAM_PGEN_FRC_REG.bit.RAM_PGEN_FRC_REG = 0;
    IP_AON_CTRL->REG_RAM_PGEN_FRC.bit.RAM_PGEN_FRC = 0;

    EnableICache();
    EnableDCache();
    // Set cache region mask
    __RV_CSR_WRITE(CSR_MNOCM,  ~(CMN_PSRAM_REGION - WIFI_RAM_REGION - 1));
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MNOCB, WIFI_RAM_REGION | 0x1);
    __RWMB();
    __FENCE_I();

    flash_init(&arcs_flash_dev, 0, 0);
    BootClock_restore();
    PM_SET_GPIOB(0, 0);

    if (((uint32_t)dst >= RV_ILM_BASE) && ((uint32_t)dst < (RV_ILM_BASE + RV_ILM_LEN)))
    {
        if (dst != src)
        {
            while (dst < end)
                *dst++ = *src++;
        }

        cpy_tb_entry = &__copy_table_start__;
        cpy_tb_end   = &__copy_table_end__;
        while (cpy_tb_entry < cpy_tb_end)
        {
            int32_t len;
            src = (int32_t*)(*cpy_tb_entry++);
            dst = (int32_t*)(*cpy_tb_entry++);
            len = (int32_t)(*cpy_tb_entry++) - (int32_t)dst;

            if (dst != src)
                mem_copy((void*)dst, (void*)src, len);
        }
    }

    PM_SET_GPIOB(0, 1);
//    ECLIC_Init();//mth
    ECLIC_SetCfgNlbits(__ECLIC_INTCTLBITS);
    wakeup_cause = pm_get_wakeup_cause();
    #if CONFIG_PM_UART_WAKEUP && ((UART0_IO_RX_PAD == CSK_IOMUX_PAD_B) || (UART1_IO_RX_PAD == CSK_IOMUX_PAD_B))
    if (wakeup_cause == 0)
    {
        pm_gpio_active = true;
    }
    else
    #endif
    #if PM_GPIO_ACTIVE_DET
    if (wakeup_cause & (((1 << PM_GPIO_PIN_MAX) - 1) << PMU_WAKEUP_GPIOB_00))
        pm_gpio_active = true;
    #endif

    HAL_PMU_ClearWakeUpCause();
    irq_vectors_reinit();
    PM_SET_GPIOB(0, 0);
    PM_SET_GPIOB(0, 1);
#if CONFIG_PM_DEBUG
    //wifi_ps_gpio_init();
#endif
    ECLIC_SetShvIRQ(SysTimerSW_IRQn, ECLIC_VECTOR_INTERRUPT);
    __idle_restore();
}

#if CONFIG_PM_DEBUG
static uint32_t pm_irq_status[(IRQ_MAX + 31) / 32];
#endif
static void pm_light_sleep(TickType_t xExpectedIdleTime)
{
    uint32_t complete_tick_periods;
    volatile uint64_t wakeup_time, sleep_time, system_timer;
    uint32_t gap, ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements;
    volatile TickType_t xModifiableIdleTime, XLastLoadValue;
    TickType_t xMaximumPossibleSuppressedTicks = (TickType_t)(portMAX_BIT_NUMBER / SYSTICK_TICK_CONST);

    if (xExpectedIdleTime > xMaximumPossibleSuppressedTicks)
        xExpectedIdleTime = xMaximumPossibleSuppressedTicks;

    #if 1
    SysTimer_Stop();
    #else
    ECLIC_DisableIRQ(SysTimer_IRQn);
    #endif

    __disable_irq();
    log_flush();

    if (eTaskConfirmSleepModeStatus() != eAbortSleep)
    {
        SysTimer_ClearSWIRQ();
        sleep_time   = vrtc_get_time_us();
        system_timer = SysTimer_GetLoadValue();
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        pm_peripheral_ctrl(PM_MODE_LIGHT_SLEEP, 1);
        pm_save_context(pm_reg_context);

        BootClock_save();
        if (pm_env.config.auto_mode)
            vrtc_set_timer(VRTC_TIMER_IDX_IPC, ((xExpectedIdleTime * 1000) - WAKEUP_DELAY_US), NULL);
        pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
        pm_set_wakeup_source();
        //PM_SET_GPIOB(PM_SLEEP_PIN, 1);
        HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_CP);
        HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI);
        pm_ram_retention(0xDFF);

        wakeup_cause = 0;
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        __idle_save();

        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
        pm_restore_context(pm_reg_context);
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
#if CONFIG_PM_DEBUG
        if (pm_env.config.dbg_level)
        {
            pm_irq_status[0] = 0;
            pm_irq_status[1] = 0;
            pm_irq_status[2] = 0;
            for (int32_t i = 0; i < IRQ_MAX; i++)
            {
                if (ECLIC_GetPendingIRQ(i) && ECLIC_GetEnableIRQ(i))
                    pm_irq_status[i>>5] |= 1 << (i & 0x1F);
            }
        }
#endif
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
        wakeup_time = vrtc_get_time_us();
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        gap  = (uint32_t)(wakeup_time - sleep_time);
        SysTimer_SetLoadValue(system_timer + gap + 20);

        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
#if 0//def CFG_AMP_IPC_SLAVE
        __enable_irq();
        __FENCE_I();
        __NOP();
        __disable_irq();
#endif
  //      PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        pm_peripheral_ctrl(wakeup_cause, 0);

        complete_tick_periods = (gap/1000) / (1000/configTICK_RATE_HZ);
        if (complete_tick_periods <= xExpectedIdleTime)
            vTaskStepTick(complete_tick_periods);
        else
            vTaskStepTick(xExpectedIdleTime);
        vPortSetupTimerInterrupt();
        SysTimer_Start();
#if PM_GPIO_ACTIVE_DET
        pm_env.wakeup_time = rtos_get_sys_us();
#endif
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);

#if CONFIG_PM_DEBUG
        if (pm_env.config.dbg_level && pm_env.config.dbg_level <= PM_DBG_MAX)
        {
            logDbg("wk:%u cause:0x%x\n", gap, wakeup_cause);
            if (pm_env.config.dbg_level >= PM_DBG_INF)
            {
                for (int32_t i = 0; i < sizeof(pm_irq_status) / sizeof(uint32_t); i++)
                {
                    if (pm_irq_status[i])
                        logDbg("irq[%d]=0x%x\n", i, pm_irq_status[i]);
                }
                if (pm_env.config.dbg_level >= PM_DBG_VRB)
                    logDbg("slep:%u wake:%u\n", sleep_time, wakeup_time);
            }
        }
#endif
#if CONFIG_PM_KEEP_ALIVE
        pm_keep_alive();
#endif
    }
    else
    {
        #if 1
        SysTimer_Start();
        SysTick_Reload(SYSTICK_TICK_CONST);
        #else
        ECLIC_EnableIRQ(SysTimer_IRQn);
        #endif
    }

    __enable_irq();
}
#endif
_PM_RAM_TEXT void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    if (pm_can_sleep())
        pm_light_sleep(xExpectedIdleTime);
    else
        __WFI();
}

static int32_t pm_peripheral_ctrl(uint32_t cause, int32_t opteration)
{
    pm_peripheral_dev_t *dev = pm_env.dev;

    while (dev)
    {
        if (opteration == 0)
        {
            if (dev->pm_resume)
                dev->pm_resume(cause);
        }
        else if (opteration == 1)
        {
            if (dev->pm_suspend)
                dev->pm_suspend(cause);
        }
        else
        {
            break;
        }

        dev = dev->next;
    }

    return 0;
}

int32_t pm_peripheral_register(pm_peripheral_dev_t *pm_dev)
{
    pm_peripheral_dev_t *dev = pm_env.dev;

    if (!pm_dev)
        return -1;

    taskENTER_CRITICAL();
    while (dev)
    {
        if (dev == pm_dev)
            goto OUT;

        dev = dev->next;
    }
    pm_dev->next = pm_env.dev;
    pm_env.dev   = pm_dev;

OUT:
    taskEXIT_CRITICAL();

    return 0;
}

int32_t pm_peripheral_unregister(pm_peripheral_dev_t *pm_dev)
{
    pm_peripheral_dev_t *dev = pm_env.dev;
    int32_t found = 0;

    if (!pm_dev)
        return -1;

    taskENTER_CRITICAL();
    if (dev == pm_dev)
    {
        pm_env.dev = pm_dev->next;
        found = 1;
    }
    else
    {
        while (dev)
        {
            if (dev->next == pm_dev)
            {
                dev->next = pm_dev->next;
                found = 1;
                break;
            }
            dev = dev->next;
        }
    }
    taskEXIT_CRITICAL();

    return found ? 0 : -1;
}

int32_t pm_enable_keep_alive(bool enable)
{
#if CONFIG_PM_KEEP_ALIVE
    if (enable)
    {
        if (!pm_env.arp_timer_handle)
            pm_env.arp_timer_handle = rtos_timer_create(NULL, true, 1, pm_arp_timer_cb);
        rtos_timer_start(pm_env.arp_timer_handle);
    }
    else
    {
        rtos_timer_stop(pm_env.arp_timer_handle);
    }
#endif
    return 0;
}
#if CONFIG_PM_KEEP_ALIVE
static void pm_keep_alive(void)
{
    if (pm_env.config.keep_alive)
    {
        if ((pm_env.arp_time == 0) || (((int32_t)((pm_env.arp_time + CONFIG_PM_KEEP_ALIVE_PERIOD*1000) - (uint32_t)rtos_get_sys_us())) < 0))
        {
            rtos_timer_schedule(pm_env.arp_timer_handle, 1);
            pm_env.arp_time = (uint32_t)rtos_get_sys_us();
        }
    }
}
#endif

int32_t pm_init(void)
{
#if !defined(CFG_AMP_IPC) || defined(CFG_AMP_IPC_SLAVE)
    #if PM_GPIO_DBG
    int32_t i = 0, pin_num[] = {0<<2, 1<<2, 7<<2, 8<<2, 9<<2, -1};
    volatile union AON_IOMUX_REG_PAD_AON_GPIOB_00 *ptr;

    do
    {
        ptr = (volatile union AON_IOMUX_REG_PAD_AON_GPIOB_00*)(0x48100000UL + pin_num[i]);
        ptr->bit.PAD_AON_GPIOB_00_OUT_REG = 1;
        ptr->bit.PAD_AON_GPIOB_00_OUT_FRC = 1;
    } while (pin_num[++i] != -1);

    ptr = (volatile union AON_IOMUX_REG_PAD_AON_GPIOB_00*)(0x48100000UL + (7<<2));
    ptr->bit.PAD_AON_GPIOB_00_FSEL = 1;
    IP_AON_CTRL->REG_AON_DBG_OUT_SEL.bit.AON_DBG_OUT_SEL = 3;
    #endif

    IP_AON_CTRL->REG_PWON_CNT_CFG0.bit.PWON_CNT0 = 4;
    IP_AON_CTRL->REG_PWON_CNT_CFG0.bit.PWON_CNT1 = 4;
    IP_AON_CTRL->REG_PWON_CNT_CFG0.bit.PWON_CNT2 = 4;
    IP_AON_CTRL->REG_PWON_CNT_CFG1.bit.PWON_CNT3 = 12;
    IP_AON_CTRL->REG_PWON_CNT_CFG1.bit.PWON_CNT4 = 12;
    IP_AON_CTRL->REG_PWON_CNT_CFG1.bit.PWON_CNT5 = 12;

#endif

    return 0;
}
#else
int32_t pm_peripheral_register(pm_peripheral_dev_t *pm_dev)
{
    (void)pm_dev;

    return 0;
}

int32_t pm_peripheral_unregister(pm_peripheral_dev_t *pm_dev)
{
    (void)pm_dev;

    return 0;
}

#ifdef CFG_AMP_IPC_SLAVE
int32_t pm_sync_config(pm_config_t *config)
{
    (void)config;

    return 0;
}
#endif
#endif
