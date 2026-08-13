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
#include "cache.h"
#include "rtos_al.h"
#include "ic_spinlock.h"
#include "ipc.h"
#include "log_print.h"
#include "pm_impl.h"
#include "amp_shared.h"
#include "vrtc.h"

#if defined(CONFIG_PM) && defined(CONFIG_PM_PSRAM)
#include "PSRAMManager.h"
#endif

#if CONFIG_PM

#define SYSTICK_TICK_CONST          (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ)
#define portMAX_BIT_NUMBER          ( SysTimer_MTIMER_Msk )


#define WAKEUP_CAUSE_ALL            0x3FF007F
#define WAKEUP_DELAY_US             100

#define REG_PL_RD(addr)              (*(volatile uint32_t *)(addr))
#define REG_PL_WR(addr, value)       (*(volatile uint32_t *)(addr)) = (value)


#if (CONFIG_CORE_NUM == 2) && (PM_CORE_PRIMARY) && (BOOT_HARTID == 0)
#define PM_RESET_CP_TO_WAKEUP_ENTRY()  do {\
        IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;\
        IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;\
    } while (0)
#else
#define PM_RESET_CP_TO_WAKEUP_ENTRY()  do {\
    } while (0)
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

#if CONFIG_PM_CLOSE_AP
static uint32_t pm_boot_gpio_val;
#define PM_BOOT_GPIO_PIN   3
#endif
extern int32_t _mem_copy_func_start, _mem_copy_func_end, _mem_copy_func_lma, __copy_table_start__, __copy_table_end__;

static FLASH_DEV pm_arcs_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xff,  // 0 means divider=2 //0xff,  //0xff means divider=1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
};

volatile pmu_wakeupsrc_t pm_wakeup_cause;

#if PM_GPIO_ACTIVE_DET
static bool pm_gpio_active = false;
#endif
_PM_STARTUP_BSS struct pm_env_t pm_env;
static _PM_STARTUP_BSS bool pm_env_ready = false;

#ifdef CONFIG_PM_PSRAM
struct pm_snapshot_ctx_t {
    pm_image_header_t image;
    uint32_t psram_base;
    uint32_t psram_size;
    uint32_t psram_cursor;
};

static _PM_STARTUP_BSS struct pm_snapshot_ctx_t pm_snapshot_ctx;
#endif
#if CONFIG_PM_DEBUG
static uint32_t pm_irq_status[(IRQ_MAX + 31) / 32];
static volatile uint32_t pm_in_wakeup;
#endif

static _PM_STARTUP_BSS uint32_t pm_ipc_irq_enable;

static struct pm_reg_info pm_reg_context[] =
{
#if PM_CORE_PRIMARY
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
#endif

    {0}
};

static void pm_save_ipc_irq(void)
{
#if (BOOT_HARTID == 0)
    pm_ipc_irq_enable = IP_MAILBOX->REG_CP_MAILBOX_CTRL.all;
#else
    pm_ipc_irq_enable = IP_MAILBOX->REG_AP_MAILBOX_CTRL.all;
#endif
}

static void pm_restore_ipc_irq(void)
{
#if (BOOT_HARTID == 0)
    IP_MAILBOX->REG_CP_MAILBOX_CTRL.all = pm_ipc_irq_enable;
#else
    IP_MAILBOX->REG_AP_MAILBOX_CTRL.all = pm_ipc_irq_enable;
#endif
#if !PM_CORE_PRIMARY
    enable_IRQ(IRQ_MAILBOX_2_VECTOR);
#endif
}

static void pm_ack_ipc_irq(void)
{
#if (BOOT_HARTID == 0)
    IP_MAILBOX->REG_CP_MAILBOX_IRQ.all = pm_ipc_irq_enable;
#else
    IP_MAILBOX->REG_AP_MAILBOX_IRQ.all = pm_ipc_irq_enable;
#endif
}

#ifdef CONFIG_PM_PSRAM
extern int32_t _sstack, _estack;

_PM_RAM_TEXT static uint32_t pm_snapshot_checksum(const void *buf, uint32_t count)
{
    uint32_t count32, sum = 0;
    const uint32_t *p32 = buf;

    count32 = count >> 5;
    while (count32--)
    {
        sum += p32[0]; sum += p32[1];
        sum += p32[2]; sum += p32[3];
        sum += p32[4]; sum += p32[5];
        sum += p32[6]; sum += p32[7];
        p32 += 8;
    }

    const uint16_t *p16 = (const uint16_t *)p32;
    uint32_t tail = (count & 31) >> 1;
    while (tail--)
        sum += *p16++;

    if (count & 1)
        sum += *(const uint8_t *)p16;

    return (~sum);
}

static int32_t pm_snapshot_is_overlap(uint32_t addr0, uint32_t size0, uint32_t addr1, uint32_t size1)
{
    uint32_t end0 = addr0 + size0;
    uint32_t end1 = addr1 + size1;

    if ((end0 < addr0) || (end1 < addr1))
        return 1;

    return !((end0 <= addr1) || (end1 <= addr0));
}

static int32_t pm_snapshot_is_reserved_region(uint32_t dst_addr, uint32_t size)
{
    return (pm_snapshot_is_overlap(dst_addr, size, PM_STARTUP_RESERVED_BASE, PM_STARTUP_RESERVED_SIZE)
           || pm_snapshot_is_overlap(dst_addr, size, (uint32_t)&_sstack,
                                     (uint32_t)&_estack - (uint32_t)&_sstack));
}

static void pm_snapshot_reset_image(void)
{
    memset(&pm_snapshot_ctx.image, 0, sizeof(pm_snapshot_ctx.image));
    pm_snapshot_ctx.image.magic = PM_SNAPSHOT_MAGIC;
}

static void pm_snapshot_init_ctx(void)
{
    pm_snapshot_reset_image();
    pm_snapshot_ctx.psram_base   = PM_SNAPSHOT_PSRAM_BASE;
    pm_snapshot_ctx.psram_size   = PM_SNAPSHOT_PSRAM_SIZE;
    pm_snapshot_ctx.psram_cursor = 0;
}

int32_t pm_snapshot_add_region(uint32_t dst_addr, uint32_t size, uint32_t flags)
{
    uint32_t psram_addr;
    pm_image_desc_t *desc;

    if ((dst_addr == 0) || (size == 0))
        return PM_SNAPSHOT_ERR_ARG;

    if (pm_snapshot_is_reserved_region(dst_addr, size))
        return PM_SNAPSHOT_ERR_RESERVED;

    if (pm_snapshot_ctx.image.magic != PM_SNAPSHOT_MAGIC)
        return PM_SNAPSHOT_ERR_STATE;

    if (pm_snapshot_ctx.image.region_num >= PM_SNAPSHOT_MAX_REGION)
        return PM_SNAPSHOT_ERR_FULL;

    for (uint32_t i = 0; i < pm_snapshot_ctx.image.region_num; i++)
    {
        pm_image_desc_t *entry = &pm_snapshot_ctx.image.region[i];

        if (pm_snapshot_is_overlap(dst_addr, size, entry->dst_addr, entry->size))
            return PM_SNAPSHOT_ERR_OVERLAP;
    }

    psram_addr = pm_snapshot_ctx.psram_base + pm_snapshot_ctx.psram_cursor;
    if ((psram_addr < pm_snapshot_ctx.psram_base) || (size > (pm_snapshot_ctx.psram_size - pm_snapshot_ctx.psram_cursor)))
        return PM_SNAPSHOT_ERR_NOSPACE;

    desc = &pm_snapshot_ctx.image.region[pm_snapshot_ctx.image.region_num++];
    desc->dst_addr   = dst_addr;
    desc->size       = size;
    desc->psram_addr = psram_addr;
    desc->flags = flags | PM_SNAPSHOT_REGION_VALID | PM_SNAPSHOT_REGION_RESTORE_EN;
    pm_snapshot_ctx.psram_cursor += size;

    return PM_SNAPSHOT_OK;
}

static int32_t pm_snapshot_save(void)
{
    if (pm_snapshot_ctx.image.magic != PM_SNAPSHOT_MAGIC)
        return PM_SNAPSHOT_ERR_STATE;

    for (uint32_t i = 0; i < pm_snapshot_ctx.image.region_num; i++)
    {
        pm_image_desc_t *desc = &pm_snapshot_ctx.image.region[i];

        memcpy((void *)desc->psram_addr, (const void *)desc->dst_addr, desc->size);

        desc->checksum = pm_snapshot_checksum((const void *)desc->dst_addr, desc->size);
    }

    pm_snapshot_ctx.image.total_size = pm_snapshot_ctx.psram_cursor;

    return PM_SNAPSHOT_OK;
}

static int32_t pm_snapshot_restore(void)
{
    if (pm_snapshot_ctx.image.magic != PM_SNAPSHOT_MAGIC)
        return PM_SNAPSHOT_ERR_MAGIC;

    for (uint32_t i = 0; i < pm_snapshot_ctx.image.region_num; i++)
    {
        pm_image_desc_t *desc = &pm_snapshot_ctx.image.region[i];

        if ((desc->flags & PM_SNAPSHOT_REGION_VALID) == 0)
            return PM_SNAPSHOT_ERR_INVALID;

        if (pm_snapshot_is_reserved_region(desc->dst_addr, desc->size))
            return PM_SNAPSHOT_ERR_RESERVED;

        if (desc->flags & PM_SNAPSHOT_REGION_RESTORE_EN)
            mem_copy((void *)desc->dst_addr, (void *)desc->psram_addr, desc->size);

        if (pm_snapshot_checksum((const void *)desc->dst_addr, desc->size) != desc->checksum)
            return PM_SNAPSHOT_ERR_CHECK;
    }

    return PM_SNAPSHOT_OK;
}
#endif

void pm_restore_gpio_reg(void)
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
#if PM_CORE_PRIMARY
    uint32_t val = 0;

#ifdef UART0_BASE
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART0_RESET_Pos;
#endif
#ifdef UART1_BASE
    val |= 1<<CMN_SYSCFG_SW_RESET_CP2_UART1_RESET_Pos;
#endif
#ifdef UART2_BASE
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
#if PM_CORE_PRIMARY
#ifdef UART0_BASE
    IP_SYSCTRL->REG_PERI_CLK_CFG1.all |= 1 << CMN_SYSCFG_PERI_CLK_CFG1_DIV_UART0_CLK_LD_Pos;
#endif
#ifdef UART1_BASE
    IP_SYSCTRL->REG_PERI_CLK_CFG2.all |= 1 << CMN_SYSCFG_PERI_CLK_CFG2_DIV_UART1_CLK_LD_Pos;
#endif
#ifdef UART2_BASE
    IP_SYSCTRL->REG_PERI_CLK_CFG3.all |= 1 << CMN_SYSCFG_PERI_CLK_CFG3_DIV_UART2_CLK_LD_Pos;
#endif
#endif

#if (CONFIG_CORE_NUM > 1)
    pm_restore_ipc_irq();
#endif
    pm_restore_gpio_reg();
}

#if CONFIG_PM_CLOSE_AP
void pm_save_boot_gpio(void)
{
    volatile uint32_t *ptr = &IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + PM_BOOT_GPIO_PIN;

    pm_boot_gpio_val = *ptr;
}

void pm_restore_boot_gpio(void)
{
    volatile uint32_t *ptr = &IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + PM_BOOT_GPIO_PIN;

    *ptr = pm_boot_gpio_val;
}
#endif

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

#if (CONFIG_CORE_NUM > 1)
    pm_save_ipc_irq();
#endif
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


    if (!pm_env_ready || pm_env.config == NULL || pm_env.core_ctx == NULL) {
        return 0;
    }

    if ((pm_env.config->mode != PM_MODE_LIGHT_SLEEP) ||  pm_env.lock_bits
        #if (CONFIG_CORE_NUM == 2)
        || pm_env.core_ctx[PM_CORE_CUR].cross_core_lock
        #endif
        )
    {
        #if CONFIG_PM_DEBUG
        if (pm_env.config->dbg_level >= PM_DBG_VRB)
            pm_dbg("[PM] sleep blocked: locked or not in light-sleep mode\n");
        #endif
        return 0;
    }

#if PM_CORE_PRIMARY
    if (!vrtc_is_allow_sleep(pm_env.config->auto_mode))
    {
        #if CONFIG_PM_DEBUG
        if (pm_env.config->dbg_level >= PM_DBG_VRB)
            pm_dbg("[PM] sleep blocked: vrtc not ready\n");
        #endif
        return 0;
    }
#endif

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
#if !PM_CORE_PRIMARY
    if (amp_app_status_get(AMP_APP_STATUS_VRTC_ALERT))
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
    #if CONFIG_PM_UART_WAKEUP
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
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = entry;;
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
        for (int32_t i = 0; i < PM_GPIO_PIN_MAX; i++)
        {
            if (pm_env.sleep_cfg->gpio_mask & (1<<i))
            {
                #if CONFIG_PM_UART_WAKEUP
                /*Restore them directly to their default configurations.*/
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

                /* gpio_level:
                 * - 0: Set the polarity to low active.
                 * - 1: Set the polarity to high active
                */
                if (pm_env.sleep_cfg->gpio_level & (1<<i))
                    HAL_PMU_GPIOPolaritySelect(PMU_WAKEUP_GPIOB_00 + i, 0);
                else
                    HAL_PMU_GPIOPolaritySelect(PMU_WAKEUP_GPIOB_00 + i, 1);
                IP_AON_CTRL->REG_WAKEUP_ENABLE.all |= 1 << (PMU_WAKEUP_GPIOB_00 + i);
            }
        }
        IP_AON_CTRL->REG_GPIO_WAKEUP_CTRL.bit.GPIO_DB_CNT = 1;
    }
}

#if PM_CORE_PRIMARY && defined(CONFIG_PM_PSRAM)
static void pm_psram_enter_sleep(void)
{
    HAL_FlushDCache();
    __RWMB();

    /*24us*/
    for (volatile int i = 0; i < 150; i++)
        __NOP();

    psram_cfg_save_and_sleep();
}

#ifdef CONFIG_PM_HEAP_IN_PSRAM
void pm_psram_exit_sleep(void)
#else
static void pm_psram_exit_sleep(void)
#endif
{
    psram_cfg_restore_and_wakeup();
}
#endif

void pm_early_dump(const char* err_string, int32_t code)
{
    pm_restore_context(pm_reg_context);
    pm_dbg("[PM] early fault: %s (%d)\n", err_string, code);
    while(1);
}

#if CONFIG_PM_DEBUG && PM_CORE_PRIMARY
/*
 * Called from the common exception handler. During the wakeup-startup window
 * a fault lands here before UART has been re-initialised, so revive the UART
 * (and clocks) from the saved context first, then report the stage we died in.
 * Outside that window UART is already alive and this is a no-op.
 */
void pm_exc_dump(void)
{
    uint32_t stage;

    if (!pm_in_wakeup)
        return;

    pm_restore_context(pm_reg_context);

    stage = PM_STAGE_REG;
    pm_dbg("[PM] fault during wakeup: %s stage 0x%02x, wake cause 0x%x\n",
           PM_STAGE_IS_WAKE(stage) ? "wake" : "sleep",
           PM_STAGE_VALID(stage) ? PM_STAGE_OF(stage) : 0xFFu, pm_wakeup_cause);
}
#endif

_PM_RAM_TEXT void pm_sleep_startup(void)
{
    int32_t *dst = &_mem_copy_func_start, *src = &_mem_copy_func_lma, *end = &_mem_copy_func_end;
    int32_t *cpy_tb_entry, *cpy_tb_end;

#if CONFIG_PM_DEBUG
    pm_in_wakeup = 1;
#endif
    PM_TRACE(PM_STG_WK_ENTRY);
#if (CONFIG_CORE_NUM > 1) && (!PM_CORE_PRIMARY)
    pm_set_core_state(PM_CORE_STATE_STARTUP);
    pm_restore_ipc_irq();
    __WFI();
    pm_ack_ipc_irq();
    pm_prevent_other_core_sleep();
#endif

#if PM_CORE_PRIMARY
    IP_AON_CTRL->REG_RAM_PGEN_FRC_REG.bit.RAM_PGEN_FRC_REG = 0;
    IP_AON_CTRL->REG_RAM_PGEN_FRC.bit.RAM_PGEN_FRC = 0;

    BootClock_restore();
    vrtc_set_flag(VRTC_FLAGS_WAITING_RCCALI_DONE);
#endif
    PM_TRACE(PM_STG_WK_CLOCK);

    EnableICache();
    EnableDCache();
    // Set cache region mask
    __RV_CSR_WRITE(CSR_MNOCM,  ~(CMN_PSRAM_REGION - WIFI_RAM_REGION - 1));
    // Set base physical address and enable
    __RV_CSR_WRITE(CSR_MNOCB, WIFI_RAM_REGION | 0x1);
    __RWMB();
    __FENCE_I();
    PM_TRACE(PM_STG_WK_CACHE);

    if (((uint32_t)dst >= RV_ILM_BASE) && ((uint32_t)dst < (RV_ILM_BASE + RV_ILM_LEN)))
    {
        if (dst != src)
        {
            while (dst < end)
                *dst++ = *src++;
        }
    }

#if PM_CORE_PRIMARY && defined(CONFIG_PM_PSRAM)
    pm_psram_exit_sleep();
#endif

    PM_TRACE(PM_STG_WK_PSRAM);

#ifdef CONFIG_PM_PSRAM
    int32_t ret;

    ret = pm_snapshot_restore();
    if (ret != PM_SNAPSHOT_OK)
        pm_early_dump("snapshort error:", ret);
#endif
    PM_TRACE(PM_STG_WK_SNAPSHOT);

#if PM_CORE_PRIMARY
    flash_init(&pm_arcs_flash_dev, 0, 0);
#endif
    PM_TRACE(PM_STG_WK_FLASH);

    cpy_tb_entry = &__copy_table_start__;
    cpy_tb_end   = &__copy_table_end__;
    while (cpy_tb_entry < cpy_tb_end)
    {
        int32_t len;
        src = (int32_t*)(*cpy_tb_entry++);
        dst = (int32_t*)(*cpy_tb_entry++);
        len = (int32_t)(*cpy_tb_entry++) - (int32_t)dst;

        if (((uint32_t)dst >= RV_ILM_BASE) && ((uint32_t)dst < (RV_ILM_BASE + RV_ILM_LEN)) && (dst != src))
            mem_copy((void*)dst, (void*)src, len);
    }

    PM_TRACE(PM_STG_WK_COPYTABLE);
//    ECLIC_Init();//mth
    ECLIC_SetCfgNlbits(__ECLIC_INTCTLBITS);
    pm_wakeup_cause = pm_get_wakeup_cause();
#if (CONFIG_CORE_NUM == 1) || !PM_CORE_PRIMARY
#if CONFIG_PM_UART_WAKEUP
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

#if PM_CORE_PRIMARY
#if CONFIG_PM_DEBUG
    //wifi_ps_gpio_init();
#endif
#endif

    irq_vectors_reinit();
    ECLIC_SetShvIRQ(SysTimerSW_IRQn, ECLIC_VECTOR_INTERRUPT);
    PM_TRACE(PM_STG_WK_IRQ);
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
    /* check if pm env was initialized */
    if (!pm_env.config){
        pm_err("ERR: pm was not initialized !!! \n");
        return -1;
    }

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
    pm_save_context(pm_reg_context);
#if PM_CORE_PRIMARY
    HAL_PMU_ClearWakeUpCause();
    BootClock_save();
    pm_set_wakeup_source();
    pm_ram_retention(0xDFF);

    if (pm_env.config->auto_mode)
        vrtc_set_timer(VRTC_TIMER_IDX_LOCAL, ((sleep_time * 1000) - WAKEUP_DELAY_US), NULL);
#else
    pm_mbx_default_priority = ECLIC_GetLevelIRQ(IRQ_MAILBOX_2_VECTOR);
    ECLIC_SetLevelIRQ(IRQ_MAILBOX_2_VECTOR, configMAX_SYSCALL_INTERRUPT_PRIORITY);
    amp_app_status_clear(AMP_APP_STATUS_VRTC_ALERT);
#endif
}

static int32_t pm_hw_execute_sleep(TickType_t xExpectedIdleTime)
{
#if PM_CORE_PRIMARY
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

    /*
    * If an IRQ occurs between the _WFI call and the hardware entering deep sleep,
    * it will cause a hardware state machine error. Therefore, the AON timer IRQ must be disabled
    */
    ECLIC_DisableIRQ(IRQ_AON_TIMER_VECTOR);
    pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
    PM_TRACE_G(PM_STG_SLEEP_HW);
    HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI);

#ifdef CONFIG_PM_PSRAM
    pm_snapshot_save();
    pm_psram_enter_sleep();
#endif

    PM_TRACE_G(PM_STG_SLEEP_WFI);
    __light_sleep_save();

    if (pm_wakeup_cause)
    {
        PM_TRACE(PM_STG_WK_RESTORE);
    }
    else
    {
        PM_TRACE_G(PM_STG_WK_RESTORE);
    }
#else
    pm_set_wakeup_entry((uint32_t)__light_sleep_entry);
#ifdef CONFIG_PM_PSRAM
    pm_snapshot_save();
    HAL_FlushDCache();
    __RWMB();
#endif
    PM_TRACE_G(PM_STG_SLEEP_HW);
    pm_set_core_state(PM_CORE_STATE_IDLE);
    pm_allow_other_core_sleep();
    __set_wfi_sleepmode(WFI_DEEP_SLEEP);
    ipc_send_signal(IPC_SIG_ENTER_IDLE);
    PM_TRACE_G(PM_STG_SLEEP_WFI);
    __light_sleep_save();
    pm_prevent_other_core_sleep();
    /*Restore registers to receive Inter-Core interrupts*/
    if (pm_wakeup_cause)
        pm_restore_context(pm_reg_context);

    PM_TRACE_G(PM_STG_WK_RESTORE);

    pm_set_core_state(PM_CORE_STATE_ACTIVE);
#endif
     return 0;
}

static void pm_light_sleep_restore(uint32_t sleep_time)
{
#if PM_CORE_PRIMARY
    if (pm_wakeup_cause)
    {
        pm_restore_context(pm_reg_context);
    }
    else
    {
        ECLIC_EnableIRQ(IRQ_AON_TIMER_VECTOR);
    }

    PM_TRACE(PM_STG_WK_WAKE_HOOK);
    pm_execute_wake_handler(sleep_time, pm_wakeup_cause);

#if defined(CONFIG_PM_PSRAM) && !defined(CONFIG_PM_HEAP_IN_PSRAM)
    if (pm_wakeup_cause == 0)
        pm_psram_exit_sleep();
#elif defined(CONFIG_PM_HEAP_IN_PSRAM)
    /* warm 路径(pm_wakeup_cause==0)已在 __restore_from_wfi 中唤醒 PSRAM;
     * 冷路径(cause!=0)在 pm_sleep_startup 中唤醒。此处无需再唤醒。 */
#endif

#if (CONFIG_CORE_NUM > 1)
    if (pm_get_core_state(PM_CORE_PEER) == PM_CORE_STATE_STARTUP)
    {
        ipc_send_signal(IPC_SIG_WAKEUP);
    }
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

    SysTimer_Stop();

#if !PM_CORE_PRIMARY
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
        PM_TRACE(PM_STG_SLEEP_ENTER);
        SysTimer_ClearSWIRQ();
        PM_TRACE(PM_STG_SLEEP_DEVOFF);
        pm_execute_enter_handler(xExpectedIdleTime, PM_MODE_LIGHT_SLEEP);
        PM_TRACE(PM_STG_SLEEP_PREPARE);
        pm_light_sleep_prepare(xExpectedIdleTime);
        pm_wakeup_cause = 0;
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
            #if PM_CORE_PRIMARY
            uint32_t curr_time = SysTimer_GetLoadValue();
            pm_env.startup_time = system_timer + sleep_time - curr_time;
            #endif
            SysTimer_SetLoadValue(system_timer + sleep_time);
        }
        SysTimer_Start();
        pm_light_sleep_restore(sleep_time);

        complete_tick_periods = (sleep_time/1000) / (1000/configTICK_RATE_HZ);
        vTaskStepTick(complete_tick_periods);
        vPortSetupTimerInterrupt();

        PM_TRACE(PM_STG_WK_EXIT_HOOK);
        pm_execute_exit_handler(sleep_time, pm_wakeup_cause);
        PM_TRACE(PM_STG_ACTIVE);
#if CONFIG_PM_DEBUG
        pm_in_wakeup = 0;
#endif
#if PM_GPIO_ACTIVE_DET
        pm_env.wakeup_time = rtos_get_sys_us();
#endif

#if CONFIG_PM_DEBUG
        if (pm_env.config->dbg_level && pm_env.config->dbg_level <= PM_DBG_MAX)
        {
            pm_dbg("[PM] woke after %u us, cause 0x%x\n", sleep_time, pm_wakeup_cause);
            if (pm_env.config->dbg_level >= PM_DBG_INF)
            {
                for (int32_t i = 0; i < sizeof(pm_irq_status) / sizeof(uint32_t); i++)
                {
                    if (pm_irq_status[i])
                        pm_dbg("[PM]   pending irq[%d] 0x%08x\n", i, pm_irq_status[i]);
                }
                if (pm_env.config->dbg_level >= PM_DBG_VRB)
                    pm_dbg("[PM]   slept %u -> %u us\n", (uint32_t)sleep_start, (uint32_t)sleep_end);
            }
        }
#endif
    }
    else
    {
        SysTimer_Start();
        SysTick_Reload(SYSTICK_TICK_CONST);
    }
#if !PM_CORE_PRIMARY
    vPortExitCritical();
#endif
    __enable_irq();
    return;

FAILED_TO_SLEEP:
#if (CONFIG_CORE_NUM == 2)
    SysTimer_Start();
    SysTick_Reload(SYSTICK_TICK_CONST);
    __WFI();
    PM_TRACE(PM_STG_ACTIVE);
    #if !PM_CORE_PRIMARY
    vPortExitCritical();
    #endif
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
        __WFI();
}

int32_t pm_impl_init(void)
{
#if CONFIG_PM_DEBUG
    /* If a watchdog/lockup reset interrupted a previous sleep/wakeup, the stage
     * code is still latched in the AON register: report where we died. */
    {
        uint32_t stage = PM_STAGE_REG;
        uint32_t rst   = HAL_PMU_GetSysResetCauseRaw();

        if ((rst & ((1u << PMU_RST_CP_WDT) | (1u << PMU_RST_AP_SW_WDT)))
            && PM_STAGE_VALID(stage))
            pm_dbg("[PM] previous reset (wdt) occurred in %s stage 0x%02x\n",
                   PM_STAGE_IS_WAKE(stage) ? "wake" : "sleep", PM_STAGE_OF(stage));
    }
#endif
    memset(&pm_env, 0, sizeof(struct pm_env_t));
#if PM_CORE_PRIMARY
#if PM_TRACE_GPIO_ON || PM_TRACE_HB_ON
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
    IP_AON_CTRL->REG_EFU_LOAD_REG.bit.LDEFU_LDO_AON_VTRIM = 1;

#if (CONFIG_CORE_NUM == 2)
    volatile struct amp_shared_info* shared = amp_shared_get();

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
#ifdef CONFIG_PM_PSRAM
    pm_snapshot_init_ctx();
#endif
#if (CONFIG_CORE_NUM == 2)
#if PM_CORE_PRIMARY
    pm_env.core_ctx[PM_CORE_CUR].cross_core_lock = 1;
#else
    pm_env.core_ctx[PM_CORE_CUR].cross_core_lock = 0;
#endif
#endif

    pm_env_ready = true;
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
