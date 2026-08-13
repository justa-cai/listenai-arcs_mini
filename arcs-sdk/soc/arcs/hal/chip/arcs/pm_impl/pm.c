/****************************************************************************************
 *
 * @file pm_api.c
 *
 * @brief pm api
 *
 * Copyright (C) ListenAI 2026
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
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "ic_spinlock.h"
#include "ipc.h"
#include "log_print.h"
#include "pm_impl.h"
#include "pm.h"



int32_t pm_enable_gpio_wakeup(uint32_t mask, uint32_t level)
{
    pm_env.sleep_cfg->gpio_mask  = mask;
    pm_env.sleep_cfg->gpio_level = level;
    if (pm_env.sleep_cfg->gpio_mask != 0)
        pm_env.sleep_cfg->wakeup_src_mask |= 1 << PM_WAKEUP_GPIO;

    return 0;
}

int32_t pm_disable_gpio_wakeup(uint32_t mask)
{
    pm_env.sleep_cfg->gpio_mask &= ~mask;
    if (pm_env.sleep_cfg->gpio_mask == 0)
        pm_env.sleep_cfg->wakeup_src_mask &= ~(1 << PM_WAKEUP_GPIO);

    return 0;
}

int32_t pm_enable_timer_wakeup(uint32_t time_in_us)
{
    pm_env.sleep_cfg->wakeup_src_mask |= (1 << PM_WAKEUP_TIMER);
    pm_env.sleep_cfg->time_us = time_in_us;

    return 0;
}

int32_t pm_disable_timer_wakeup(void)
{
    pm_env.sleep_cfg->wakeup_src_mask &= ~(1 << PM_WAKEUP_TIMER);

    return 0;
}

int32_t pm_register_gpio_retention(uint32_t pad, uint32_t gpio)
{
    int32_t ret = -1;

    if ( ((pad == CSK_IOMUX_PAD_A) && (gpio < CSK_IOMUX_PAD_A_MAX_PIN))
        || ((pad == CSK_IOMUX_PAD_B) && (gpio < CSK_IOMUX_PAD_B_MAX_PIN))
       )
    {
        gpio += 1;
        for (int32_t i = 0; i < PM_GPIO_RETENTION_MAX; i++)
        {
            if (pm_env.gpio_retention[i].gpio_idx == 0)
            {
                pm_env.gpio_retention[i].gpio_idx = (pad << 16) | gpio;
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

int32_t pm_register_snapshot_region(uint32_t dst_addr, uint32_t size, uint32_t flags)
{
    #ifdef CONFIG_PM_PSRAM
    return pm_snapshot_add_region(dst_addr, size, flags);
    #else
    return PM_SNAPSHOT_ERR_STATE;
    #endif
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
        pm_save_boot_gpio();
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_NONE;
        IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.PU_AP_SUB = 1;
        while (IP_AON_CTRL->REG_AON_DIG_RSVD0.all);
        pm_restore_boot_gpio();
        vPortExitCritical();
    }
}
#endif
int32_t pm_get_sleep_config(pm_sleep_config_t *sleep_config)
{
    if (sleep_config != NULL)
    {
        sleep_config->time_us    = pm_env.sleep_cfg->time_us;
        sleep_config->gpio_mask  = pm_env.sleep_cfg->gpio_mask;
        sleep_config->gpio_level = pm_env.sleep_cfg->gpio_level;
        sleep_config->wakeup_src_mask = pm_env.sleep_cfg->wakeup_src_mask;
    }

    return 0;
}

int32_t pm_set_sleep_config(pm_sleep_config_t *sleep_config)
{
    memcpy((void*)pm_env.sleep_cfg, sleep_config, sizeof(pm_sleep_config_t));

    return 0;
}

int32_t pm_lock_acquire(pm_lock_t lock)
{
    if (lock < PM_LOCK_MAX)
    {
        taskENTER_CRITICAL();
        pm_env.lock_bits |= 1<<lock;
        taskEXIT_CRITICAL();
    }

    return 0;
}

int32_t pm_lock_release(pm_lock_t lock)
{
    if (lock < PM_LOCK_MAX)
    {
        taskENTER_CRITICAL();
        pm_env.lock_bits &= ~ (1<<lock);
        taskEXIT_CRITICAL();
    }

    return 0;
}

void pm_wakeup_other_core(void)
{
    ipc_send_signal(IPC_SIG_WAKEUP);
}

int32_t pm_set_config(pm_config_t *config)
{
    int32_t clock = -1;

    if (config->mode >= PM_MODE_MAX)
        return -1;

    if (config->mode > PM_MODE_ACTIVE)
    {
        if ((config->clock_level != pm_env.config->clock_level) && (config->clock_level < PM_CLOCK_LEVEL_COUNT))
        {
            pm_env.config->clock_level = config->clock_level;
            clock = config->clock_level;
        }
    }
    else if (pm_env.config->clock_level)
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

#if CONFIG_PM_CLOSE_AP
    pm_force_ap_off();
#endif

    pm_env.config->dbg_level   = config->dbg_level;
    pm_env.config->auto_mode   = config->auto_mode;
    pm_env.config->clock_level = config->clock_level;
    pm_env.config->mode = config->mode;

    return 0;
}

int32_t pm_hook_register(pm_hook_id_t hook_id, pm_handler_func_t enter, pm_handler_func_t exit)
{
    pm_handler_ops_t temp_ops = {.on_enter = enter, .on_exit = exit, .on_wake = NULL, .check_idle = NULL};

    return pm_internal_register(PM_HANDLE_TYPE_COMM, (int32_t)hook_id, &temp_ops);
}

int32_t pm_hook_unregister(pm_hook_id_t hook_id)
{
    return pm_internal_unregister(PM_HANDLE_TYPE_COMM, (int32_t)hook_id);
}

int32_t pm_init(void)
{
    return pm_impl_init();
}
