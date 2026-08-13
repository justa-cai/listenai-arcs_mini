/*
 * wdt_check.c
 *
 *  Created on: 2025年5月19日
 *      Author: USER
 */
#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "Driver_WDT.h"
#include "PowerManager.h"

#define RESET_CAUSE_TO_STRING(x) \
( ((x) == PMU_RST_NONE)               ? "None reset"               : \
  ((x) == PMU_RST_POR)                ? "PMU_RST_POR"              : \
  ((x) == PMU_RST_AON)                ? "PMU_RST_AON"              : \
  ((x) == PMU_RST_SYSRESETREQ_CORE1)  ? "PMU_RST_SYSRESETREQ_CORE1": \
  ((x) == PMU_RST_SYSRESETREQ_CORE0)  ? "PMU_RST_SYSRESETREQ_CORE0": \
  ((x) == PMU_RST_SW1)                ? "PMU_RST_SW1"              : \
  ((x) == PMU_RST_SW0)                ? "PMU_RST_SW0"              : \
  ((x) == PMU_RST_WDT_CORE1)          ? "PMU_RST_WDT_CORE1"        : \
  ((x) == PMU_RST_WDT_CORE0)          ? "PMU_RST_WDT_CORE0"        : \
                                        "Unknown reset" )

static void* WDT_Handler = NULL;

static void WDT_Init_Handler() {
    WDT_Handler = WDT();
}

static void WDT_Callback_hook(void* workspace){
	CLOGD("WDT interrupt triggered");
}

static void GetResetEvent(uint32_t rst_cause) {
	CLOGD("%s\n", RESET_CAUSE_TO_STRING(rst_cause));
	CLOG_FLUSH();
}

static void wdt_test_reset_core0(void) {
    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    WDT_Initialize(WDT_Handler, WDT_Callback_hook, NULL);

    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    HAL_DRIVER_WDT_Cfg_t wdt_cfg = {
        .clk_src =  hal_driver_wdt_clk_src_32K,
        .int_time = hal_driver_wdt_int_time_15,
        .rst_time = hal_driver_wdt_rst_time_14,
    };

    // extern clock = 32k
    // 2^15 / 32k = 1s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);
    CLOGD("Enable 32K WDT, 1s interrupt, 0.5s reset");

    __HAL_PMU_WDT0_RESET_CMN_ENABLE();

    WDT_Enable(WDT_Handler);

    uint32_t i = 0;
    while (1)
    {
        SysTick_Delay_Ms(500);
        CLOGD("i: %d", i++);
    }
    
    WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);

    WDT_Uninitialize(WDT_Handler);
}

static void wdt_test_reset_core0_irq_refresh_callback_hook(void* workspace){
    WDT_Feed(WDT_Handler);
	CLOGD("WDT interrupt triggered and fed");
}

static void wdt_test_reset_core0_irq_refresh(void) {
    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    WDT_Initialize(WDT_Handler, wdt_test_reset_core0_irq_refresh_callback_hook, NULL);

    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    HAL_DRIVER_WDT_Cfg_t wdt_cfg = {
        .clk_src =  hal_driver_wdt_clk_src_32K,
        .int_time = hal_driver_wdt_int_time_17,
        .rst_time = hal_driver_wdt_rst_time_14,
    };

    // extern clock = 32k
    // 2^17 / 32k = 4s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);
    CLOGD("Enable 32K WDT, 4s interrupt, 0.5s reset");

    __HAL_PMU_WDT0_RESET_CMN_ENABLE();

    WDT_Enable(WDT_Handler);

    uint32_t i = 0;
    while (1)
    {
        SysTick_Delay_Ms(500);
        CLOGD("i: %d", i++);
    }
    
    WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);

    WDT_Uninitialize(WDT_Handler);
}

static void wdt_test_reset_core0_no_irq_refresh(void) {
    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    WDT_Initialize(WDT_Handler, NULL, NULL);

    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    HAL_DRIVER_WDT_Cfg_t wdt_cfg = {
        .clk_src =  hal_driver_wdt_clk_src_32K,
        .int_time = hal_driver_wdt_int_time_17,
        .rst_time = hal_driver_wdt_rst_time_14,
    };

    // extern clock = 32k
    // 2^17 / 32k = 4s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);
    CLOGD("Enable 32K WDT, 4s interrupt, 0.5s reset");

    __HAL_PMU_WDT0_RESET_CMN_ENABLE();

    WDT_Enable(WDT_Handler);

    uint32_t i = 0;
    while (1)
    {
        SysTick_Delay_Ms(500);
        CLOGD("i: %d", i++);
        WDT_Feed(WDT_Handler);
    }
    
    WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);

    WDT_Uninitialize(WDT_Handler);
}

static void wdt_test_reset_core0_fixed_num_refresh(void) {
    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    WDT_Initialize(WDT_Handler, NULL, NULL);

    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);

    HAL_DRIVER_WDT_Cfg_t wdt_cfg = {
        .clk_src =  hal_driver_wdt_clk_src_32K,
        .int_time = hal_driver_wdt_int_time_17,
        .rst_time = hal_driver_wdt_rst_time_14,
    };

    // extern clock = 32k
    // 2^17 / 32k = 4s (interrupt stage)
    // 2^14 / 32k = 0.5s (reset stage)
    WDT_Control(WDT_Handler, &wdt_cfg);
    CLOGD("Enable 32K WDT, 4s interrupt, 0.5s reset");

    __HAL_PMU_WDT0_RESET_CMN_ENABLE();

    WDT_Enable(WDT_Handler);

    uint32_t i = 0;
    while (1)
    {
        SysTick_Delay_Ms(500);
        CLOGD("i: %d", i++);
        if (i == 5) {
            CLOGD("stop feeding WDT");
            break;
        }
        WDT_Feed(WDT_Handler);
    }
    
    while(1);

    WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);

    WDT_Uninitialize(WDT_Handler);
}

typedef void (*function)(void);

typedef struct {
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
    // {wdt_test_reset_core0, "wdt_test_reset_core0"},
    // {wdt_test_reset_core0_irq_refresh, "wdt_test_reset_core0_irq_refresh"},
    // {wdt_test_reset_core0_no_irq_refresh, "wdt_test_reset_core0_no_irq_refresh"},
    // {wdt_test_reset_core0_fixed_num_refresh, "wdt_test_reset_core0_fixed_num_refresh"},
};

int main(void) {
	logInit(0, 115200);
	CLOG("enter main: wdt test\r\n");

    WDT_Init_Handler();

	uint32_t index;
	for(index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++) {
		test_case_t item = test_array[index];
		CLOG("test case : %s", item.name);
		item.function();
	}

	while(1);

	return 0;
}
