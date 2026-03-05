/*
 * lsip_sleep.c
 *
 *  sleep functions
 */

/*
 * INCLUDES
 ****************************************************************************************


/**
****************************************************************************************
* @addtogroup sleep_VEGA
* @ingroup sleep
* @brief VEGA sleep Driver
*
* This is the driver block for Vega sleep
* @{
****************************************************************************************
*/

/**
 *****************************************************************************************
 * INCLUDE FILES
 *****************************************************************************************
 */
#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition

#include "Driver_Common.h"
#include "PowerManager.h"
#include "IOMuxManager.h"
#include "patch.h"
//#include "dbg.h"
#include "ble_drv.h"
#include "log_print.h"    
#include "arcs_ap.h"
#include "bt_drv.h"


enum rccal_trig_type
{
    ///HW trig rccal type
    RCCAL_TRIG_TYPE_DISABLE = 0x00,          // disable automatic rc calibration
    RCCAL_TRIG_TYPE_WAKEUP_LP = 0x01,        // enable wakeup_lp trigger   bt wakeup time expire
    RCCAL_TRIG_TYPE_RADIO_EN_POSEDGE = 0x02, // enable radio_en posedge trigger
    RCCAL_TRIG_TYPE_RADIO_EN_NEGEDGE = 0x03, // enable radio_en negedge trigger(osc_en must be 1 until the end of rc calibration)
};

enum rccal_init_status
{
    ///HW trig rccal type
    RCCAL_INIT_NO_START = 0x00,          // first powerup
    RCCAL_INIT_SOFTWARE_START = 0x01,        // software start rc calibration
    RCCAL_INIT_HW_START = 0x02, // LP wakeup start rc calibration
};


volatile uint32_t g_sleep_wakeup_status = PMU_WAKEUP_NONE;

extern struct bt_sleep_api_str ls_sleep_api;

extern bool HAL_PMU_Is_PowerOn(void);

extern volatile pmu_wakeupsrc_t wakeup_cause;



void ls_chip_sleep_init(void)
{

}

void RamConfigure(void)
{
    PTCH(void, RamConfigure);

    if (HAL_PMU_Is_PowerOn())
    {
        //cfg memory retention
        HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK0);
        HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK1);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK0);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK1);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK2);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK3);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK4);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK5);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK6);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK7);
//        HAL_PMU_EnableRamRetention(PMU_WIFI_KEY_RAMBANK);
        HAL_PMU_EnableRamRetention(PMU_BT_RAMBANK0);


        // Enable wakeup source
        HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_BT);

        // Config chip can enter sleep mode after btdm_osc_en is 1(btdm_osc_en from bt)
        IP_AON_CTRL->REG_POWER_WKUP_CTRL1.bit.EN_BT_OSCEN = 1;

        // Sleep triggered by CP
        HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_CP);

        // set aon wakeup_power_on counter, value from vegaT
        // TODO: wait update to arcsd value
        AON_CTRL_P->REG_PWON_CNT_CFG0.bit.PWON_CNT0     = 10;
        AON_CTRL_P->REG_PWON_CNT_CFG0.bit.PWON_CNT1     = 10;
        AON_CTRL_P->REG_PWON_CNT_CFG0.bit.PWON_CNT2     = 10;
        AON_CTRL_P->REG_PWON_CNT_CFG1.bit.PWON_CNT3     = 10;
        AON_CTRL_P->REG_PWON_CNT_CFG1.bit.PWON_CNT4     = 10;
        AON_CTRL_P->REG_PWON_CNT_CFG1.bit.PWON_CNT5     = 10;
    }

}

void ls_chip_sleep_enter(uint16_t sleep_state)
{
	if (sleep_state == LSIP_CPU_SLEEP)
	{
        // do nothing
	}
	else if(sleep_state == LSIP_DEEP_SLEEP)
	{
		RamConfigure();

    	CLOG_FLUSH();

        HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI);
    }
	else
	{
		return;
	}

}

void ls_chip_Wakeup_Event(void* param)
{

}


/*
    @retval   0: power on, not wakeup from sleep   others: sleep wakeup
*/
uint32_t HAL_PMU_Is_Sleep_WakeUp(void){

	return g_sleep_wakeup_status;
}

bool HAL_PMU_Is_PowerOn(void){

    return (g_sleep_wakeup_status == PMU_WAKEUP_NONE);
}
/*

  @retval   0: power on   1: sleep wakeup
*/
bool ls_is_bt_timer_wakeup(void)
{
    return (g_sleep_wakeup_status == PMU_WAKEUP_BT);
}

void lsip_open_hz3200(void)
{ 

}

void lsip_rccali_init(void)
{ 
    AON_CTRL_P->REG_BT_RC_CALI.bit.RCCAL_LENGTH = ls_sleep_api.rc_cali_cycle_length; // time = 2^rccal_length   set rc cali time
    AON_CTRL_P->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_MASK = 1; // irq mask

    AON_CTRL_P->REG_BT_RC_CALI.bit.RCCAL_START = 1; // trig first time rc calibration at powerup. software start rc cali   w1s register
    ls_sleep_api.rc_init_stat = RCCAL_INIT_SOFTWARE_START;
    //lsip_rccali_start();

  return;
}

void lsip_rccali_start(void)
{ 
    //AON_SLEEP_P->REG_BT_RC_CALI.bit.RCCAL_START = 1; // software start rc cali  // wait 1 pulse register
    AON_CTRL_P->REG_BT_RC_CALI.bit.RCCAL_AUTO_TRIG_SEL = RCCAL_TRIG_TYPE_WAKEUP_LP; //
  return;
}

void lsip_rccali_irq_handle(void)
{
    PTCH_FST(void, lsip_rccali_irq_handle);

    ls_rc_int_clear();
    if (ls_sleep_api.rc_clock_mod == 3)
    {
        AON_CTRL_P->REG_BT_RC_CALI.bit.RCCAL_AUTO_TRIG_SEL = RCCAL_TRIG_TYPE_DISABLE; 

        ls_sleep_api.rc_cali_result = AON_CTRL_P->REG_BT_RC_CALI.bit.RCCAL_RESULT ;

//        CLOGD("g_rccali_result=0x%d", ls_sleep_api.rc_cali_result);

        if (ls_sleep_api.rc_result_position == 1) // 0: correct time before rc calibration, next time use the cali result   1:correct time after rc calibration, current time use the cali result
        {
            if (ls_sleep_api.rc_init_stat != RCCAL_INIT_SOFTWARE_START)
            {
                lsip_wakeup();
            }
            ls_sleep_api.rc_init_stat = RCCAL_INIT_HW_START;
        }

        lsip_rccali_start();
    }
    return;
}


void ls_rc_int_clear(void)
{
    AON_CTRL_P->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_CLR = 1;// first clean rccal inner irq
    clear_IRQ(IRQ_RCCAL_DONE_VECTOR);
}

// if bt wakeup is triggered by software, should clean before enter sleep
void hal_sw_trigger_clean(void)
{

}

void ls_wakeup_start()
{
    g_sleep_wakeup_status = wakeup_cause;

	Bt_BootClock_Init();

    // rf init
    uint32_t tmp = cloglvl;
//    cloglvl = 1;
    extern void rf_init_bt();
     rf_init_bt();
//     cloglvl = tmp;

    // bt init
    ble_task_init();

    // enable bt sleep
    //lsip_prevent_sleep_clear(0x100);
}



struct bt_sleep_api_str ls_sleep_api =
{
    .sleep_init = ls_chip_sleep_init,
    .enter_sleep = ls_chip_sleep_enter,
    .is_power_on = NULL,
    .is_wakeup = HAL_PMU_Is_Sleep_WakeUp,
    .get_wakeup_state = NULL,
    .is_bt_wakeup = NULL,
    .en_32kHZ = NULL,
    .rc_cali_init = lsip_rccali_init,
    .rc_cali_start = lsip_rccali_start,
    .rc_cali_irq_handler = lsip_rccali_irq_handle,
    .clean_bt_wakeup_singnal = hal_sw_trigger_clean,

    .rc_cali_result = 24000,
    .rc_init_stat = RCCAL_INIT_NO_START,
    .rc_result_position = 0,
    .rc_cali_cycle_length = 5,
#if IC_BOARD
    .rc_clock_mod = 3,
#else
    .rc_clock_mod = 1,
#endif

};


uint8_t bt_sleep_api_init(void **api)
{
    *api = &ls_sleep_api;
    return 0;
}



