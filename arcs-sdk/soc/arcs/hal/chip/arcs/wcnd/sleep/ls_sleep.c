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
#include "bt_config.h"     // SW configuration


#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition
#include "ble_drv.h"
//#include "co_utils.h"      // common utility definition
//#include "co_math.h"       // common math functions
//#include "co_endian.h"     // endian definitions
//#include "rf.h"            // RF interface

//#include "btip.h"          // for RF API structure definition
//#include "Driver_Common.h"
//#include "Driver_PMU.h"
//#include "IOMuxManager.h"

//#include "dbg.h"

//#include "aon_ctrl_reg.h"
//#include "aon_sleep_reg.h"
//#include "ble_drv.h"
#include "log_print.h"    


//#define AON_SLEEP_CTRL_BASE		0x45F00000

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




//volatile AON_SLEEP_RegDef *AP_AON_SLEEP = (volatile AON_SLEEP_RegDef*)(AON_SLEEP_CTRL_BASE);

uint32_t  g_rccali_result = 24000; //31270; //(1<<LS_RCCALI_CYCLE_LENGTH)*1000; //250000;   // 2^rccal_length *24M XTAL /24k rc clk
uint32_t  g_rc_init_stat = RCCAL_INIT_NO_START;
uint8_t  g_rc_result_position = 0; // 0: correct time before rc calibration, next time use the cali result   1:correct time after rc calibration, current time use the cali result
uint8_t  g_rccali_cycle_length = 5;     //about256count*40.8us=10445us  // 32*30.517= 970us  ~ 2^5temp 1ms may 2ms
uint8_t  g_rc_clock_mod = 3;// 0: no supported  1: 32000Hz  2:32768Hz clock  3:rc32k 


extern void ls_chip_Wakeup_Event(void* param);
void UART_log32(uint8_t log_id, uint32_t log_data);


/**
 *****************************************************************************************
 * @brief Get the TX power as control structure TX power field from a value in dBm.
 *
 * @param[in] txpwr_dbm   TX power in dBm
 * @param[in] option	  If TXPWR_CS_LOWER, return index equal to or lower than requested
 *						  If TXPWR_CS_HIGHER, return index equal to or higher than requested
 *						  If TXPWR_CS_NEAREST, return index nearest to the desired value
 *
 * @return The index of the TX power
 *
 *****************************************************************************************
 */
void ls_chip_sleep_init(void)
{

}



void ls_chip_sleep_enter(uint16_t sleep_state)
{


}

void ls_chip_Wakeup_Event(void* param)
{

}

volatile uint32_t g_sleep_wakeup_status = 0;

/*
    @retval   0: power on, not wakeup from sleep   others: sleep wakeup
*/
bool HAL_PMU_Is_Sleep_WakeUp(void){

	return g_sleep_wakeup_status != 0;
}

/*

  @retval   0: power on   1: sleep wakeup
*/
bool ls_is_bt_timer_wakeup(void)
{
    return false;
}
void lsip_open_hz3200(void)
{ 

}


void lsip_rccali_init(void)
{ 

  return;
}

void lsip_rccali_start(void)
{ 

  return;
}

void lsip_rccali_irq_handle(void)
{    

    return;
}


void ls_rc_int_clear(void)
{

}

void hal_sw_trigger_clean(void){

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
    .rc_cali_init = NULL,
    .rc_cali_start = lsip_rccali_start,
    .rc_cali_irq_handler = NULL,
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
    printf("func:%s, line:%d\n", __func__, __LINE__);
    *api = &ls_sleep_api;
    return 0;
}



