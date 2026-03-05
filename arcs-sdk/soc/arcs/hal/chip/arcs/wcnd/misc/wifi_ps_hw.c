/****************************************************************************************
 *
 * @file wifi_ps_hw.c
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
#include "arcs_ap.h"
#include "wifi_ps_hw.h"
#include "rf_drv.h"

static int32_t wifi_ps_hw_suspend(uint32_t sleep_time, int32_t power_off)
{
    ls_rf_suspend(RF_MODE_WIFI, 1);
    if (!power_off)
    {
        IP_WIFI_CTRL->REG_WIFI_CTRL_DOZE_WAKE_INT.bit.CFG_PLFDOZEWAKEUPEN   = 1;
        IP_WIFI_CTRL->REG_WIFI_CTRL_DOZE_WAKE_INT.bit.CFG_MASKPLFDOZEWAKEUP = 1;
    }

    return 0;
}

_PM_TEXT_TEXT static int32_t wifi_ps_hw_resume(int32_t power_off)
{
    ls_rf_resume(RF_MODE_WIFI, power_off);
    if (power_off)
    {
        while (IP_AON_CTRL->REG_PMU_CORE_CTRL0.bit.WF_STATE_CURR);
    }

    return 0;
}

static void wifi_ps_hw_set_beacon_intv(uint16_t bcn_intv)
{
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_BEACONINT = bcn_intv;
}

static void wifi_ps_hw_set_dtim(uint8_t dtim_period)
{
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_DTIMPERIOD = dtim_period;
}

static void wifi_ps_hw_set_listen_intv(uint16_t listen_intv)
{
    IP_AON_CTRL->REG_AON_WF_CTRL0.bit.CFG_LISTENINTERVAL = listen_intv;
}

static void wifi_ps_hw_enable_dtim_wakeup(bool enable)
{
    if (enable)
        IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN = 1;
    else
        IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN = 0;
}

static void wifi_ps_hw_aon_wakup_isr(void)
{
    IP_AON_CTRL->REG_AON_WF_SLEEP_ONLY_WKUP_IRQ.bit.CFG_WF_LP_ONLY_WKUP_CLR = 1;
    IP_AON_CTRL->REG_WAKEUP_ICR.bit.WF_WAKEUP_ICR = 1;
}

static void wifi_ps_hw_mac_wakup_isr(void)
{
    IP_WIFI_CTRL->REG_WIFI_CTRL_DOZE_WAKE_INT.bit.CFG_PLFDOZEWAKEUPCLR = 1;
}

static int32_t wifi_ps_hw_check_idle(void)
{
    if (IP_AON_CTRL->REG_AON_WF_BCNINTCNT_RPT.bit.BCNINTCNT > 5000)
        return 1;

    return 0;
}

static void wifi_ps_hw_set_wakeup(uint32_t time)
{
    #ifndef CFG_AMP_IPC
    time -= 100;
    #endif
    time = time >> 5;/*In units of 32us*/
    IP_AON_CTRL->REG_AON_WF_WAKEUP_TIME.bit.CFG_CORE_RADIOWAKEUPTIME = time;
}

int32_t wifi_ps_hw_init(void)
{
    struct wifi_ps_hw_ops ops =
    {
        .suspend = wifi_ps_hw_suspend,
        .resume  = wifi_ps_hw_resume,
        .check_idle    = wifi_ps_hw_check_idle,
        .aon_wakup_isr = wifi_ps_hw_aon_wakup_isr,
        .mac_wakup_isr = wifi_ps_hw_mac_wakup_isr,
        .set_beacon_intv = wifi_ps_hw_set_beacon_intv,
        .set_listen_intv = wifi_ps_hw_set_listen_intv,
        .set_dtim = wifi_ps_hw_set_dtim,
        .enable_dtim_wakeup = wifi_ps_hw_enable_dtim_wakeup,
        .set_wakeup_time    = wifi_ps_hw_set_wakeup
	};

    wifi_ps_hw_register(&ops);

    /*wifi aon cfg*/
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_BEACONINT  = 100;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_DTIMPERIOD = 1;
    IP_AON_CTRL->REG_AON_WF_CTRL0.bit.CFG_LISTENINTERVAL = 0;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_WAKEUPDTIMEN   = 1;
    IP_AON_CTRL->REG_AON_WF_CTRL1.bit.CFG_SLEEP_RCCALI_EN  = 0;
    IP_AON_CTRL->REG_AON_WF_CTRL2.bit.CFG_RCCAL_RESULT_SEL = 1;
    IP_AON_CTRL->REG_POWER_WKUP_CTRL1.bit.EN_WF_ST_CK      = 1;
    IP_AON_CTRL->REG_AON_WF_SLEEP_ONLY_WKUP_IRQ.bit.CFG_WF_LP_ONLY_WKUP_IRQ_EN = 1;
    IP_AON_CTRL->REG_AON_WF_SLEEP_ONLY_WKUP_IRQ.bit.CFG_WF_LP_ONLY_WKUP_MASK   = 1;
    IP_AON_CTRL->REG_AON_WF_WAKEUP_TIME.bit.CFG_CORE_RADIOWAKEUPTIME = 130;
    IP_AON_CTRL->REG_AON_WF_WAKEUP_TIME.bit.CFG_WF_RADIOWAKEUPTIME   = 64;
    //IP_AON_CTRL->REG_WAKEUP_ENABLE.bit.ENA_WF_WAKEUP = 1;
    //IP_AON_CTRL->REG_AON_LDO_VMEM.bit.TUNE_LDOVMEM   = 5;
    //IP_NEW_DFE->REG_AGC_TOP_CFG0.bit.REG_CD_EN = 0;

    return 0;
}
