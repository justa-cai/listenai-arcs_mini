/*
 * wdt_nos_chk.c
 *
 *  Created on: 2020年8月27日
 *      Author: USER
 */

#include "Driver_WDT.h"
#include "log_print.h"
#include "systick.h"
#include "chip.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 1000000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void WDT_32KCLK_Trigger_Reset(void);
static void WDT_32KCLK_Trigger_Reset_1(void);
static void WDT_PCLK_Trigger_Reset(void);
static void WDT_PCLK_Trigger_Reset_1(void);
static void WDT_EXTCLK_Refresh(void);
static void WDT_EXTCLK_Disable(void);

static function test_function_array[] = {
    WDT_32KCLK_Trigger_Reset,
    WDT_32KCLK_Trigger_Reset_1,
    WDT_PCLK_Trigger_Reset,
    WDT_PCLK_Trigger_Reset_1,
    WDT_EXTCLK_Refresh,
    WDT_EXTCLK_Disable,
};

/*
 * wdt handler
 * */
static void* WDT_Handler = NULL;

static void WDT_Init_Handler(){
    WDT_Handler = WDT();
}

static hal_driver_wdt_cfg_t wdt_cfg = {0};

static void WDT_Callback_hook(void* workspace){
	CLOGD("Hello");
}

static void WDT_32KCLK_Trigger_Reset(void){
    WDT_Initialize(WDT_Handler, WDT_Callback_hook, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    wdt_cfg.clk_src = hal_driver_wdt_clk_src_32k;
    wdt_cfg.int_time = hal_driver_wdt_int_time_15;
    wdt_cfg.rst_time = hal_driver_wdt_rst_time_14;

    // extern clock = 32k
    // 2^15 / 32k = 1s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);

    CLOGD("Eable 32K WDT");

//    IP_AP_CFG->REG_SW_RESET.bit.APWDT2APSOC_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNWDT2CMN_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNWDT2CP_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNWDT2AP_RST_EN = 1;

    WDT_Enable(WDT_Handler);

    uint32_t i = 0;
    while(1){
    	CLOGD("i: %d", i++);
    }

    WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);

    WDT_Uninitialize(WDT_Handler);
}

static void WDT_32KCLK_Trigger_Reset_1(void){
	WDT_Initialize(WDT_Handler, NULL, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    wdt_cfg.clk_src = hal_driver_wdt_clk_src_32k;
    wdt_cfg.int_time = hal_driver_wdt_int_time_17;
    wdt_cfg.rst_time = hal_driver_wdt_rst_time_14;

    // extern clock = 32k
    // 2^17 / 32k = 4s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);

    CLOGD("Eable EXT WDT");

    WDT_Enable(WDT_Handler);

    FAKE_WHILE();
}

static void WDT_PCLK_Trigger_Reset(void){
	WDT_Initialize(WDT_Handler, NULL, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    wdt_cfg.clk_src = hal_driver_wdt_clk_src_apb;
    wdt_cfg.int_time = hal_driver_wdt_int_time_25;
    wdt_cfg.rst_time = hal_driver_wdt_rst_time_14;

    // apb clock = 48M
    // 2^25 / 48M = 0.7s (interrupt stage)
    // 2^14 / 48M = 0.34ms (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);

    CLOGD("Eable PCLK WDT");

    WDT_Enable(WDT_Handler);

    FAKE_WHILE();
}

static void WDT_PCLK_Trigger_Reset_1(void){
	WDT_Initialize(WDT_Handler, NULL, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    wdt_cfg.clk_src = hal_driver_wdt_clk_src_apb;
    wdt_cfg.int_time = hal_driver_wdt_int_time_27;
    wdt_cfg.rst_time = hal_driver_wdt_rst_time_14;

    // apb clock = 48M
    // 2^27 / 48M = 2.8s (interrupt stage)
    // 2^14 / 48M = 0.34ms (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);

    CLOGD("Eable PCLK WDT");

    WDT_Enable(WDT_Handler);

    FAKE_WHILE();
}

static void WDT_EXTCLK_Refresh(void){
	WDT_Initialize(WDT_Handler, NULL, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    wdt_cfg.clk_src = hal_driver_wdt_clk_src_32k;
    wdt_cfg.int_time = hal_driver_wdt_int_time_15;
    wdt_cfg.rst_time = hal_driver_wdt_rst_time_14;

    // extern clock = 32k
    // 2^15 / 32k = 1s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);

    CLOGD("Eable EXT WDT and wait to refresh");

    WDT_Enable(WDT_Handler);

    while(1){
        SysTick_Delay_Ms(500);
        WDT_Refresh(WDT_Handler);
        CLOGD("Refresh");
    }

    FAKE_WHILE();
}

static void WDT_EXTCLK_Disable(void){
    uint32_t i = 0;

    WDT_Initialize(WDT_Handler, NULL, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    wdt_cfg.clk_src = hal_driver_wdt_clk_src_32k;
    wdt_cfg.int_time = hal_driver_wdt_int_time_15;
    wdt_cfg.rst_time = hal_driver_wdt_rst_time_14;

    // extern clock = 32k
    // 2^15 / 32k = 1s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);


    CLOGD("Eable EXT WDT and wait to refresh");

    WDT_Enable(WDT_Handler);

    while(1){
        SysTick_Delay_Ms(500);
        WDT_Refresh(WDT_Handler);
        CLOGD("Refresh");
        if(i++ > 10){
            break;
        }
    }

    WDT_Disable(WDT_Handler);
    CLOGD("Disable EXT WDT");

    FAKE_WHILE();
}

int main(){
    uint32_t times;
    logInit(0, 115200);
    enable_GINT();
    WDT_Init_Handler();
    CLOGD("Initialize WDT");
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
