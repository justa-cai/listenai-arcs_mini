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
#include "Driver_AON_TIMER.h"
#include "PowerManager.h"
#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "spiflash.h"
#include "platform.h"
#include "ic_spinlock.h"
#include "ipc.h"
#include "pm_impl.h"
#include "log_print.h"
#include "vrtc.h"
#if defined(CONFIG_PM) && defined(PSRAM_HEAP)
#include "PSRAMManager.h"
#endif

#if CONFIG_PM

#define SYSTICK_TICK_CONST          (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ)
#define portMAX_BIT_NUMBER          ( SysTimer_MTIMER_Msk )


#define WAKEUP_CAUSE_ALL            0x3FF007F
#define WAKEUP_DELAY_US             100

#define REG_PL_RD(addr)              (*(volatile uint32_t *)(addr))
#define REG_PL_WR(addr, value)       (*(volatile uint32_t *)(addr)) = (value)

//临时代码，后续UART的寄存器恢复放到设备层
#define UART1_IO_TX_PAD          (CSK_IOMUX_PAD_A)

#ifdef PSRAM_HEAP
#define PSRAM_TEST_BASE_ADDR            (PSRAM_BASE_ADDRESS + 0x0)
#define PSRAM_TEST_LENGTH               (0x80000) // 1MWord
#endif

extern void __light_sleep_save(void);
extern void __light_sleep_restore(void);
extern void __light_sleep_entry(void);
extern void ECLIC_Init(void);
extern void BootClock_Init();
extern void irq_vectors_reinit(void);
extern void mem_copy(void *dst, void *src, int32_t len);
extern void BootClock_restore(void);
extern void BootClock_save(void);
extern void vPortSetupTimerInterrupt(void);


#if (CONFIG_CORE_NUM == 1)
static pm_config_t       local_config;
static struct pm_core_context local_core_ctx[1];
static pm_sleep_config_t local_sleep_cfg;
static uint32_t          local_wakeup_cause;
#else
static uint8_t pm_mbx_default_priority;
#endif


extern int32_t _mem_copy_func_start, _mem_copy_func_end, _mem_copy_func_lma, __copy_table_start__, __copy_table_end__;

static FLASH_DEV arcs_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xff,  // 0 means divider=2 //0xff,  //0xff means divider=1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
};

volatile pmu_wakeupsrc_t pm_wakeup_cause = PMU_WAKEUP_NONE;

#if PM_GPIO_ACTIVE_DET
static bool pm_gpio_active = false;
#endif
struct pm_env_t pm_env;

#if CONFIG_PM_DEBUG
static uint32_t pm_irq_status[(IRQ_MAX + 31) / 32];
#endif
static struct pm_reg_info pm_reg_context[] =
{
#if IS_PM_CORE_PRIMARY
    {&IP_MAILBOX->REG_CP_MAILBOX_CTRL.all},
    {&IP_SYSCTRL->REG_PERI_CLK_CFG1.all},
    {&IP_SYSCTRL->REG_PERI_CLK_CFG2.all},
    {&IP_SYSCTRL->REG_PERI_CLK_CFG5.all},

#ifdef UART0_BASE
    {&IP_UART0->REG_CTRL.all},
    {&IP_UART0->REG_TRIGGERS.all},
    {&IP_UART0->REG_IRQ_MASK.all},
#endif

#ifdef UART1_BASE
    {&IP_UART1->REG_CTRL.all},
    {&IP_UART1->REG_TRIGGERS.all},
    {&IP_UART1->REG_IRQ_MASK.all},
#endif

#ifdef UART2_BASE
    {&IP_UART2->REG_CTRL.all},
    {&IP_UART2->REG_TRIGGERS.all},
    {&IP_UART2->REG_IRQ_MASK.all},
#endif

    {&IP_GPADC->REG_ADC_IMR0.all},
    {&IP_GPADC->REG_ADC_IMR1.all},
    {&IP_GPADC->REG_ADC_IMR2.all},
#else
    {&IP_MAILBOX->REG_AP_MAILBOX_CTRL.all},
#endif

    {0}
};

static void pm_restore_gpio_reg(void)
{
    for (int32_t i = 0; i < PM_GPIO_RETENTION_MAX; i++)
    {
        uint32_t pad, gpio;
        volatile uint32_t *ptr = &IP_CMN_IOMUX->REG_PAD_GPIOA_00.all;

        if (pm_env.gpio_retention[i].gpio_idx)
        {
            pad   = pm_env.gpio_retention[i].gpio_idx >> 16;
            gpio  = pm_env.gpio_retention[i].gpio_idx & 0xFFFF;
            gpio -= 1;
            if (pad == CSK_IOMUX_PAD_A)
                ptr[gpio] = pm_env.gpio_retention[i].val;
            else
                ptr[gpio + 32] = pm_env.gpio_retention[i].val;
        }
    }
}

static _PM_RAM_TEXT void pm_restore_context(struct pm_reg_info *reg)
{
#if IS_PM_CORE_PRIMARY
    uint32_t val = 0;

#if defined(UART0_IO_TX_PAD)
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART0_RESET_Pos;
#endif
#if defined(UART1_IO_TX_PAD)
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART1_RESET_Pos;
#endif
#if defined(UART2_IO_TX_PAD)
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART2_RESET_Pos;
#endif

    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_GPADC_RESET_Pos;
    IP_SYSCTRL->REG_SW_RESET_CP2.all |= val;
#endif
    while (reg->addr)
    {
        REG_PL_WR(reg->addr, reg->value);
        reg++;
    }
#if IS_PM_CORE_PRIMARY
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

    pm_restore_gpio_reg();
}

static void pm_save_gpio_reg(void)
{
    for (int32_t i = 0; i < PM_GPIO_RETENTION_MAX; i++)
    {
        uint32_t pad, gpio;
        volatile uint32_t *ptr = &IP_CMN_IOMUX->REG_PAD_GPIOA_00.all;

        if (pm_env.gpio_retention[i].gpio_idx)
        {
            pad   = pm_env.gpio_retention[i].gpio_idx >> 16;
            gpio  = pm_env.gpio_retention[i].gpio_idx & 0xFFFF;
            gpio -= 1;
            if (pad == CSK_IOMUX_PAD_A)
                pm_env.gpio_retention[i].val = ptr[gpio];
            else
                pm_env.gpio_retention[i].val = ptr[gpio+32];
        }
    }
}

static _PM_RAM_TEXT void pm_save_context(struct pm_reg_info *reg)
{
    while (reg->addr)
    {
        reg->value = REG_PL_RD(reg->addr);
        reg++;
    }

    pm_save_gpio_reg();
}

static void pm_ram_retention(uint32_t reten_bits)
{
    reten_bits = reten_bits & PM_RAM_RETENTION_BIT_MASK;
    IP_AON_CTRL->REG_RAM_RETENTION_SEL.bit.RAM_RETENTION_SEL = reten_bits;
    IP_AON_CTRL->REG_RAM_PGEN_FRC_REG.bit.RAM_PGEN_FRC_REG   = PM_RAM_RETENTION_BIT_MASK & (~reten_bits);
    IP_AON_CTRL->REG_RAM_PGEN_FRC.bit.RAM_PGEN_FRC = PM_RAM_RETENTION_BIT_MASK & (~reten_bits);
}

static int32_t pm_can_sleep(void)
{
    pm_handler_t *hnd = (pm_handler_t*)pm_env.handle[PM_HANDLE_TYPE_DEV];

    if ((pm_env.config->mode != PM_MODE_LIGHT_SLEEP) ||  pm_env.lock_bits 
        #if (CONFIG_CORE_NUM == 2)
        || pm_env.core_ctx[PM_CORE_CUR].cross_core_lock
        #endif
        )
    {
        return 0;
    }

#if PM_GPIO_ACTIVE_DET
    if (pm_gpio_active && ((int32_t)((pm_env.wakeup_time + PM_GPIO_IDLE_TIME) - (uint32_t)rtos_get_sys_us()) > 0))
        return 0;
    pm_gpio_active = false;
#endif

    while (hnd)
    {
        if ((hnd->ops.check_idle) && hnd->ops.check_idle(PM_MODE_LIGHT_SLEEP) == 0)
        {
            return 0;
        }
        hnd = hnd->next;
    }

    return 1;
}

static uint32_t pm_get_wakeup_cause(void)
{
#if (CONFIG_CORE_NUM == 1)
    *pm_env.last_wakeup_cause = IP_AON_CTRL->REG_WAKEUP_ISR.all;
#else
#if !IS_PM_CORE_PRIMARY
    if (ipc_get_app_status(IPC_APP_STATUS_VRTC_ALERT))
        return PMU_WAKEUP_TIMER;
#else
    *pm_env.last_wakeup_cause = IP_AON_CTRL->REG_WAKEUP_ISR.all;
#endif
#endif
    return *pm_env.last_wakeup_cause;
}

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
#if (CONFIG_CORE_NUM == 2)
    /*Dual-core application: CP is waiting for AP to reset it*/
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = (uint32_t)pm_dead_loop;
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = entry;
#else
    /*Single-core application: AP enters WFI state after startup.*/
    IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_RAM;
    IP_AON_CTRL->REG_AON_DIG_RSVD1.all = (uint32_t)pm_dead_loop;
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = entry;
#endif
#endif
}

static void pm_prevent_other_core_sleep(void)
{
#if (CONFIG_CORE_NUM == 2)
    pm_env.core_ctx[PM_CORE_PEER].cross_core_lock = 1;
    __asm__ volatile ("fence rw, rw" ::: "memory");
#endif
}

static void pm_allow_other_core_sleep(void)
{
#if (CONFIG_CORE_NUM == 2)
    pm_env.core_ctx[PM_CORE_PEER].cross_core_lock = 0;
    __asm__ volatile ("fence rw, rw" ::: "memory");
#endif
}

static void pm_set_core_state(uint32_t state)
{
#if (CONFIG_CORE_NUM == 2)
    pm_env.core_ctx[PM_CORE_CUR].state = state;
    __asm__ volatile ("fence rw, rw" ::: "memory");
#endif
}

static uint32_t pm_get_core_state(uint32_t core_id)
{
    return pm_env.core_ctx[core_id].state;
}

static void pm_set_wakeup_source(void)
{
    if (pm_env.config->auto_mode)
        IP_AON_CTRL->REG_WAKEUP_ENABLE.all = (1 << PMU_WAKEUP_WIFI) | (1 << PMU_WAKEUP_TIMER);
    else
        IP_AON_CTRL->REG_WAKEUP_ENABLE.all = (1 << PMU_WAKEUP_WIFI);
    if ((pm_env.sleep_cfg->wakeup_src_mask & (1<<PM_WAKEUP_GPIO)) &&pm_env.sleep_cfg->gpio_mask)
    {
        volatile union CORE_IOMUX_REG_PAD_GPIOB_00 *ptr;

        for (int32_t i = 0; i < PM_GPIO_PIN_MAX; i++)
        {
            if (pm_env.sleep_cfg->gpio_mask & (1<<i))
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

_PM_RAM_TEXT void pm_sleep_startup(void)
{
    volatile int32_t *dst = &_mem_copy_func_start, *src = &_mem_copy_func_lma, *end = &_mem_copy_func_end;
    volatile int32_t *cpy_tb_entry, *cpy_tb_end;

    PM_SET_GPIOB(PM_SLEEP_PIN, 1);
    IP_AON_CTRL->REG_RAM_PGEN_FRC_REG.bit.RAM_PGEN_FRC_REG = 0;
    IP_AON_CTRL->REG_RAM_PGEN_FRC.bit.RAM_PGEN_FRC = 0;
#if !IS_PM_CORE_PRIMARY
    pm_set_core_state(PM_CORE_STATE_STARTUP);
#endif
    EnableICache();
    EnableDCache();
    // Set cache region mask
    __RV_CSR_WRITE(CSR_MNOCM,  ~(CMN_PSRAM_REGION - WIFI_RAM_REGION - 1));
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MNOCB, WIFI_RAM_REGION | 0x1);
    __RWMB();
    __FENCE_I();
#if IS_PM_CORE_PRIMARY
    flash_init(&arcs_flash_dev, 0, 0);
    BootClock_restore();
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
    pm_wakeup_cause = pm_get_wakeup_cause();
#if (CONFIG_CORE_NUM == 1) || !IS_PM_CORE_PRIMARY
#if CONFIG_PM_UART_WAKEUP && ((UART0_IO_RX_PAD == CSK_IOMUX_PAD_B) || (UART1_IO_RX_PAD == CSK_IOMUX_PAD_B))
    if (pm_wakeup_cause == 0)
    {
        pm_gpio_active = true;
    }
    else
#endif
    {
#if PM_GPIO_ACTIVE_DET
    if (pm_wakeup_cause & (((1 << PM_GPIO_PIN_MAX) - 1) << PMU_WAKEUP_GPIOB_00))
        pm_gpio_active = true;
#endif
    }
#endif
    pm_wakeup_cause |= 1 << 31;

#if IS_PM_CORE_PRIMARY
    HAL_PMU_ClearWakeUpCause();
#if CONFIG_PM_DEBUG
    //wifi_ps_gpio_init();
#endif
#endif
#if (CONFIG_CORE_NUM == 2) && (IS_PM_CORE_PRIMARY)
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif
    irq_vectors_reinit();
    ECLIC_SetShvIRQ(SysTimerSW_IRQn, ECLIC_VECTOR_INTERRUPT);
    PM_SET_GPIOB(PM_SLEEP_PIN, 0);
    __light_sleep_restore();
}

uint64_t pm_get_startup_time(void)
{
    return pm_env.startup_time;
}

static int32_t pm_execute_enter_handler(uint32_t sleep_time_us, int32_t mode)
{
    for (int32_t i = (PM_HANDLE_TYPE_MAX - 1); i >= 0; i--)
    {
        pm_handler_t *handle = pm_env.handle[i];

        while (handle)
        {
            if (handle->ops.on_enter)
                    handle->ops.on_enter(sleep_time_us, (void*)mode);
            handle = handle->next;
        }
    }

    return 0;
}

static int32_t pm_execute_exit_handler(uint32_t sleep_time_us, int32_t cause)
{
    for (int32_t i = 0; i < PM_HANDLE_TYPE_MAX; i++)
    {
        pm_handler_t *handle = pm_env.handle[i];

        while (handle)
        {
            if (handle->ops.on_exit)
                    handle->ops.on_exit(sleep_time_us, (void*)cause);
            handle = handle->next;
        }
    }

    return 0;
}

static int32_t pm_execute_wake_handler(uint32_t sleep_time_us, int32_t cause)
{
    pm_handler_t *handle = pm_env.handle[PM_HANDLE_TYPE_DEV];

    while (handle)
    {
        if (handle->ops.on_wake)
                handle->ops.on_wake(sleep_time_us, (void*)cause);
        handle = handle->next;
    }

    return 0;
}

int32_t pm_internal_register(int32_t type, int32_t id, pm_handler_ops_t *ops)
{
    pm_handler_t *new_node;
    pm_handler_t **prev = &pm_env.handle[type];

    if (ops == NULL)
        return -1;

    new_node = rtos_malloc(sizeof(pm_handler_t));
    if (new_node == NULL)
        return -1;

    new_node->ops.check_idle = ops->check_idle;
    new_node->ops.on_enter   = ops->on_enter;
    new_node->ops.on_exit    = ops->on_exit;
    new_node->ops.on_wake    = ops->on_wake;
    new_node->id = id;

    taskENTER_CRITICAL();

    while (*prev != NULL)
    {
        if ((*prev)->id == id)
        {
            taskEXIT_CRITICAL();
            rtos_free(new_node);
            return -2;
        }

        if ((*prev)->id > id)
            break;

        prev = &((*prev)->next);
    }

    new_node->next = *prev;
    *prev = new_node;

    taskEXIT_CRITICAL();

    return 0;
}

int32_t pm_internal_unregister(int32_t type, int32_t id)
{
    pm_handler_t **prev = &pm_env.handle[type];
    pm_handler_t *entry = NULL;

    taskENTER_CRITICAL();

    while (*prev != NULL)
    {
        if ((*prev)->id == id)
        {
            entry = *prev;
            *prev = entry->next;
            break;
        }

        if ((*prev)->id > id)
            break;

        prev = &((*prev)->next);
    }

    taskEXIT_CRITICAL();

    if (entry)
    {
        rtos_free(entry);
        return 0;
    }

    return -1;
}

#if CONFIG_PM_DEBUG
static void pm_record_irq_state(void)
{
    if (pm_env.config->dbg_level)
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
}
#endif

static void pm_light_sleep_prepare(uint32_t sleep_time)
{
#if IS_PM_CORE_PRIMARY
    /*
    * If an IRQ occurs between the _WFI call and the hardware entering deep sleep,
    * it will cause a hardware state machine error. Therefore, the AON timer IRQ must be disabled
    */
    ECLIC_DisableIRQ(IRQ_AON_TIMER_VECTOR);
    BootClock_save();
    pm_set_wakeup_source();
    pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
    pm_ram_retention(0xDFF);
#else
    pm_mbx_default_priority = ECLIC_GetLevelIRQ(IRQ_MAILBOX_2_VECTOR);
    ECLIC_SetLevelIRQ(IRQ_MAILBOX_2_VECTOR, configMAX_SYSCALL_INTERRUPT_PRIORITY);
    ipc_clear_app_status(IPC_APP_STATUS_VRTC_ALERT);
    pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
#endif
    pm_save_context(pm_reg_context);

    if (pm_env.config->auto_mode)
        vrtc_set_timer(VRTC_TIMER_IDX_LOCAL, ((sleep_time * 1000) - WAKEUP_DELAY_US), NULL);
}

static int32_t pm_hw_execute_sleep(TickType_t xExpectedIdleTime)
{
#if IS_PM_CORE_PRIMARY
#if (CONFIG_CORE_NUM == 2)
    if (pm_env.core_ctx[PM_CORE_CUR].cross_core_lock)
        return -1;
#else
#if (BOOT_HARTID == 0)
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
#else
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_CP);
#endif
#endif
    PM_SET_GPIOB(PM_SLEEP_PIN, 0);
    HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI);
#ifdef PSRAM_HEAP
    HAL_FlushDCache_by_Addr((uint32_t*)PSRAM_TEST_BASE_ADDR, (uint32_t)(PSRAM_TEST_LENGTH * sizeof(uint32_t)));
    PSRAM_EnterSleepMode(PSRAM_SLEEP_MODE_HALF_SLEEP);
#endif
    IP_AON_CTRL->REG_AON_LDOVAON.bit.LDO_AON_VTRIM = 0x22;
    __light_sleep_save();
    PM_SET_GPIOB(PM_SLEEP_PIN, 1);
#else
    PM_SET_GPIOB(PM_SLEEP_PIN, 0);
    pm_set_core_state(PM_CORE_STATE_IDLE);
    pm_allow_other_core_sleep();
    __set_wfi_sleepmode(WFI_DEEP_SLEEP);
    ipc_send_notify(IPC_EVT_ENTER_IDLE);
    __light_sleep_save();
    pm_prevent_other_core_sleep();
    /*Restore registers to receive Inter-Core interrupts*/
    if (pm_wakeup_cause)
        pm_restore_context(pm_reg_context);
    PM_SET_GPIOB(PM_SLEEP_PIN, 1);
#if (CONFIG_CORE_NUM == 2)
    if (pm_get_core_state(PM_CORE_CUR) == PM_CORE_STATE_STARTUP)
        __WFI();
#endif
    pm_set_core_state(PM_CORE_STATE_ACTIVE);
#endif
     return 0;
}

static void pm_light_sleep_restore(uint32_t sleep_time)
{
#if IS_PM_CORE_PRIMARY
    uint32_t curr_time;

    if (pm_wakeup_cause)
        pm_restore_context(pm_reg_context);
    else
        IP_AON_CTRL->REG_AON_LDOVAON.bit.LDO_AON_VTRIM = 0;

    pm_execute_wake_handler(sleep_time, pm_wakeup_cause);
#ifdef PSRAM_HEAP
    PSRAM_Reinit(0x50, 0x40);
#endif
#if (CONFIG_CORE_NUM == 2)
    if (pm_get_core_state(PM_CORE_PEER) == PM_CORE_STATE_STARTUP)
        ipc_send_notify(IPC_EVT_WAKEUP);
#endif
#else
    ECLIC_SetLevelIRQ(IRQ_MAILBOX_2_VECTOR, pm_mbx_default_priority);
#endif
    __set_wfi_sleepmode(WFI_SHALLOW_SLEEP);
}

static void pm_light_sleep(TickType_t xExpectedIdleTime)
{
    uint32_t sleep_time, complete_tick_periods;
    volatile uint64_t sleep_start, sleep_end, system_timer;
    volatile struct amp_shared_info* shared;

    SysTimer_Stop();

#if !IS_PM_CORE_PRIMARY
    vPortEnterCritical();
#endif
    __disable_irq();
    log_flush();

    if (eTaskConfirmSleepModeStatus() != eAbortSleep)
    {
#if (CONFIG_CORE_NUM == 2)
        if (pm_env.core_ctx[PM_CORE_CUR].cross_core_lock)
            goto FAILED_TO_SLEEP;
#endif

        SysTimer_ClearSWIRQ();
        pm_wakeup_cause = 0;
        pm_execute_enter_handler(xExpectedIdleTime, PM_MODE_LIGHT_SLEEP);
        pm_light_sleep_prepare(xExpectedIdleTime);
        system_timer = SysTimer_GetLoadValue();
        sleep_start  = vrtc_get_time_us();
        if (pm_hw_execute_sleep(xExpectedIdleTime))
            goto FAILED_TO_SLEEP;
#if CONFIG_PM_DEBUG
        pm_record_irq_state();
#endif
        sleep_end  = vrtc_get_time_us();
        sleep_time = (uint32_t)(sleep_end - sleep_start);
        if (pm_wakeup_cause)
        {
            #if IS_PM_CORE_PRIMARY
            uint32_t curr_time = SysTimer_GetLoadValue();
            pm_env.startup_time = system_timer + sleep_time - curr_time;
            #endif
            SysTimer_SetLoadValue(system_timer + sleep_time);
        }
        PM_SET_GPIOB(PM_SLEEP_PIN, 0);
        pm_light_sleep_restore(sleep_time);

        complete_tick_periods = (sleep_time/1000) / (1000/configTICK_RATE_HZ);
        vTaskStepTick(complete_tick_periods);
        vPortSetupTimerInterrupt();
        SysTimer_Start();

        pm_execute_exit_handler(sleep_time, pm_wakeup_cause);
        PM_SET_GPIOB(PM_SLEEP_PIN, 1);
#if PM_GPIO_ACTIVE_DET
        pm_env.wakeup_time = rtos_get_sys_us();
#endif

#if CONFIG_PM_DEBUG
        if (pm_env.config->dbg_level && pm_env.config->dbg_level <= PM_DBG_MAX)
        {
            logDbg("wk:%u cause:0x%x\n", sleep_time, pm_wakeup_cause);
            if (pm_env.config->dbg_level >= PM_DBG_INF)
            {
                for (int32_t i = 0; i < sizeof(pm_irq_status) / sizeof(uint32_t); i++)
                {
                    if (pm_irq_status[i])
                        logDbg("irq[%d]=0x%x\n", i, pm_irq_status[i]);
                }
                if (pm_env.config->dbg_level >= PM_DBG_VRB)
                    logDbg("sleep:%u wake:%u\n", sleep_start, sleep_end);
            }
        }
#endif
    }
    else
    {
        SysTimer_Start();
        SysTick_Reload(SYSTICK_TICK_CONST);
    }
#if !IS_PM_CORE_PRIMARY
    vPortExitCritical();
#endif
    __enable_irq();

    return;

FAILED_TO_SLEEP:
#if (CONFIG_CORE_NUM == 2)
    SysTimer_Start();
    SysTick_Reload(SYSTICK_TICK_CONST);
   // PM_SET_GPIOB(9, 0);
    __WFI();
   // PM_SET_GPIOB(9, 1);
    __enable_irq();
#endif
    return;
}

int32_t pm_device_register(int32_t dev_id, pm_handler_ops_t *ops)
{
    return pm_internal_register(PM_HANDLE_TYPE_DEV, dev_id, ops);
}

int32_t pm_device_unregister(int32_t dev_id)
{
    return pm_internal_unregister(PM_HANDLE_TYPE_DEV, dev_id);
}

_PM_RAM_TEXT void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    if (pm_can_sleep())
        pm_light_sleep(xExpectedIdleTime);
    else
    {
        #if IS_PM_CORE_PRIMARY
        PM_SET_GPIOB(8, 0);
        #endif
        __WFI();
        #if IS_PM_CORE_PRIMARY
        PM_SET_GPIOB(8, 1);
        #endif
    }
}

int32_t pm_impl_init(void)
{
#if IS_PM_CORE_PRIMARY
#if PM_GPIO_DBG
    int32_t i = 0, pin_num[] = {0<<2, 1<<2, 4<<2, 7<<2, 8<<2, 9<<2, -1};
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

#if (CONFIG_CORE_NUM == 2)
    volatile struct amp_shared_info* shared = ipc_get_shared_info();

    pm_env.config    = &shared->pm_data.config;
    pm_env.core_ctx  = shared->pm_data.core_ctx;
    pm_env.sleep_cfg = &shared->pm_data.sleep_cfg;
    pm_env.last_wakeup_cause = &shared->pm_data.last_wakeup_cause;
#else
    pm_env.config    = &local_config;
    pm_env.core_ctx  = local_core_ctx;
    pm_env.sleep_cfg = &local_sleep_cfg;
    pm_env.last_wakeup_cause = &local_wakeup_cause;
#endif
    pm_env.config->mode = PM_MODE_ACTIVE;
#if (CONFIG_CORE_NUM == 2)
#if IS_PM_CORE_PRIMARY
    pm_env.core_ctx[PM_CORE_CUR].cross_core_lock = 1;
#else
    pm_env.core_ctx[PM_CORE_CUR].cross_core_lock = 0;
#endif
#endif

    return 0;
}
#else
int32_t pm_device_register(int32_t dev_id, pm_handler_ops_t *ops)
{
    return 0;
}

int32_t pm_device_unregister(int32_t dev_id)
{
    return 0;
}

uint64_t pm_get_startup_time(void)
{
    return 0;
}
#endif
