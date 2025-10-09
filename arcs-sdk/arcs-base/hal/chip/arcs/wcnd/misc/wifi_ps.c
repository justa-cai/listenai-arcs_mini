/****************************************************************************************
 *
 * @file wifi_ps.c
 *
 * @brief WiFi power save
 *
 * Copyright (C) ListenAI 2023
 *
 * Created on: Dec 11, 2023
 *
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include <stdbool.h>
#include "rtos_al.h"
#include "Driver_AON_TIMER.h"
#include "PowerManager.h"
#include "ClockManager.h"
#include "arcs_ap.h"
#include "wifi_ps.h"
#include "rf_drv.h"


extern void psm_wakeup_from_isr(void);

int32_t wifi_power_off(uint32_t sleep_time, bool suspend)
{
    ls_rf_suspend(RF_MODE_WIFI);
    if (suspend)
    {
#if defined(WIFI_PS_WAKEUP_BY_TIMER)
        //wifi_ps_cali_rc32k();
        IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.PD_WF_SUB = 1;
        //wifi_enable_wakeup();
#elif defined(WIFI_PS_WAKEUP_BY_AON)
        IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.PD_WF_SUB = 1;
        //while (IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.WF_STATE_CURR != 5);
#endif
    }
    else
    {
#if defined(WIFI_PS_WAKEUP_BY_MAC)
       IP_WIFI_CTRL->REG_WIFI_CTRL_DOZE_WAKE_INT.bit.CFG_PLFDOZEWAKEUPEN   = 1;
       IP_WIFI_CTRL->REG_WIFI_CTRL_DOZE_WAKE_INT.bit.CFG_MASKPLFDOZEWAKEUP = 1;
#endif
    }

    return 0;
}

static int32_t wifi_power_on(bool suspend)
{
    ls_rf_resume(RF_MODE_WIFI);
    if (suspend)
    {
#if defined(WIFI_PS_WAKEUP_BY_TIMER)
        IP_AON_CTRL->REG_PMU_CTRL_CORE.bit.PU_WF_SUB = 1;
#endif
        while (IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.WF_STATE_CURR);
    }

    return 0;
}

#if defined(WIFI_PS_WAKEUP_BY_TIMER)

#define WIFI_RC_CALI_DELAY               125
#define WIFI_CPU_DELAY                   50
#define WIFI_WAKEUP_DELAY                2000

static void wifi_wakeup_event(void* param)
{
    //psm_trace_exit(PSM_TRACE_ND_CFM);
}

static void wifi_set_ps_timer(uint32_t sleep_time)
{
    sleep_time -= WIFI_RC_CALI_DELAY + WIFI_CPU_DELAY + WIFI_WAKEUP_DELAY;
    vrtc_set_timer(sleep_time, psm_wakeup_from_isr);
}

static void wifi_enable_wakeup(void)
{
    HAL_PMU_Uninitialize(PMU());
    HAL_PMU_PowerControl(PMU(), CSK_POWER_OFF);

    HAL_PMU_Initialize(PMU());
    HAL_PMU_RegisterCallback(PMU(), wifi_wakeup_event);
    HAL_PMU_PowerControl(PMU(), CSK_POWER_FULL);
    HAL_PMU_Control(PMU(), CSK_PMU_WAKE_SELECT_TIMER | CSK_PMU_WAKE_SELECT_IWDT);
    HAL_PMU_InterruptDisable(PMU());
}

#else

#define WIFI_RCCAL_LEN 4

static void wifi_ps_cali_rc32k(void)
{
    volatile int32_t res;
    int32_t freq, val;

    *((volatile int32_t*)(AON_CTRL_BASE + 0x15C)) |= 1 << 1;
    *((volatile int32_t*)(AON_CTRL_BASE + 0x160)) |= WIFI_RCCAL_LEN << 20;
    *((volatile int32_t*)(AON_CTRL_BASE + 0x160)) |= 1 << 24;
    while (!(*((volatile int32_t*)(AON_CTRL_BASE + 0x15c)) & 0x4));

    res  = (*((volatile int32_t*)(AON_CTRL_BASE + 0x160))) & 0xFFFFF;
    freq = CRM_GetSrcFreq(CRM_IpSrcXtalClk) / 1000000;
    val = (res * 1000) / (freq << WIFI_RCCAL_LEN);
    val += 10000;

    *((volatile int32_t*)(AON_CTRL_BASE + 0x148))  = (val % 1000) | (((val/1000) & 0xFF) << 20);
    *((volatile int32_t*)(AON_CTRL_BASE + 0x148)) |= 1<<28; //cfg_rccal_result_sel
}

void wifi_ps_set_beacon(uint16_t bcn_intv)
{
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_BEACONINT = bcn_intv;
}

void wifi_ps_set_dtim(uint8_t dtim_period)
{
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_DTIMPERIOD   = dtim_period;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN = 1;
}

void wifi_ps_set_listen_interval(uint16_t listen_intv)
{
    IP_AON_CTRL->REG_AON_WF_CTRL0.bit.CFG_LISTENINTERVAL = listen_intv;
    if (listen_intv)
        IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN = 0;
    else
        IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN = 1;
}

void wifi_ps_aon_wakup_isr(void)
{
    IP_AON_CTRL->REG_AON_WF_SLEEP_ONLY_WKUP_IRQ.bit.CFG_WF_LP_ONLY_WKUP_CLR = 1;
    IP_AON_CTRL->REG_WAKEUP_ICR.bit.WF_WAKEUP_ICR = 1;
    psm_wakeup_from_isr();
}

void wifi_ps_mac_wakup_isr(void)
{
    IP_WIFI_CTRL->REG_WIFI_CTRL_DOZE_WAKE_INT.bit.CFG_PLFDOZEWAKEUPCLR = 1;
    psm_wakeup_from_isr();
}

#endif

void wifi_power_save_init(void)
{
    struct wifi_ps_ops ops = {
            wifi_power_off,
            wifi_power_on,
    };
    extern void psm_register_cb(struct wifi_ps_ops *ops);
    psm_register_cb(&ops);

#ifndef WIFI_PS_WAKEUP_BY_TIMER
    register_ISR(IRQ_WF_LP_WKUP_VECTOR, (ISR)wifi_ps_aon_wakup_isr, NULL);
    enable_IRQ(IRQ_WF_LP_WKUP_VECTOR);

    register_ISR(IRQ_AON_WKUP_VECTOR, (ISR)wifi_ps_aon_wakup_isr, NULL);
    enable_IRQ(IRQ_AON_WKUP_VECTOR);

    /*Shared memory retention*/
    IP_AON_CTRL->REG_RAM_RETENTION_SEL.all |= 0xFF;
    /*wifi aon cfg*/
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_BEACONINT  = 100;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_DTIMPERIOD = 1;
    IP_AON_CTRL->REG_AON_WF_CTRL0.bit.CFG_LISTENINTERVAL  = 0;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN    = 1;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_SLEEP_RCCALI_EN = 1;
    IP_AON_CTRL->REG_POWER_WKUP_CTRL1.bit.EN_WF_ST_CK     = 1;
    IP_AON_CTRL->REG_AON_WF_SLEEP_ONLY_WKUP_IRQ.bit.CFG_WF_LP_ONLY_WKUP_IRQ_EN = 1;
    IP_AON_CTRL->REG_AON_WF_SLEEP_ONLY_WKUP_IRQ.bit.CFG_WF_LP_ONLY_WKUP_MASK   = 1;
    IP_AON_CTRL->REG_AON_WF_WAKEUP_TIME.bit.CFG_WF_RADIOWAKEUPTIME = 64;
    IP_AON_CTRL->REG_WAKEUP_ENABLE.bit.ENA_WF_WAKEUP = 1;
#endif
}
