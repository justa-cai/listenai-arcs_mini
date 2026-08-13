/*
 * aon_wdt_nos_chk.c
 *
 *  Created on: 2025年5月27日
 *      Author: USER
 */
#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "Driver_AON_WDT.h"

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

static void* AON_WDT_Handler = NULL;

static void AON_WDT_Init_Handler(){
    AON_WDT_Handler = AON_WDT();
}

static void AON_WDT_AUTOFeed_EventCallback(void* workspace){
    AON_WDT_Refresh(AON_WDT_Handler);
    CLOGD("Aon wdt trigger, Feed!!!");
}


#define TEST_PATTERN 0x12345678
static void aon_wdt_reset_core_only_no_irq(void)
{
    int32_t ret;
    uint32_t i;
    const uint32_t test_loops = 10;

	uint32_t aon_val = IP_AON_CTRL->REG_AON_DIG_RSVD2.all;
	CLOGD("AON domain data: 0x%08x", aon_val);

	if(aon_val == TEST_PATTERN) {
		CLOGD("[Success] AON Domain Remain");
	} else {
		CLOGD("[Failed] AON Domain reset");
	}

	IP_AON_CTRL->REG_AON_DIG_RSVD2.all = TEST_PATTERN;

	AON_WDT_Initialize(AON_WDT_Handler, AON_WDT_AUTOFeed_EventCallback, NULL);

	AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

	AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_CTRL_RESET_MODE | HAL_AON_WDT_RST_CORE_DOMAIN, 0); // 0: no irq
	AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);

	AON_WDT_Enable(AON_WDT_Handler);

    for (i = 0; i < test_loops; i++) {
        SysTick_Delay_Ms(500);
        ret = AON_WDT_Refresh(AON_WDT_Handler);
        if (ret != CSK_DRIVER_OK) {
            CLOGE("AON_WDT_Refresh failed at loop %u: %d", i, ret);
            break;
        }
        CLOGD("Refresh %u", i + 1);
    }

    FAKE_WHILE();

    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}

static void aon_wdt_reset_core_aon_no_irq(void)
{
	int32_t ret;
    uint32_t i;
    const uint32_t test_loops = 10;

	uint32_t aon_val = IP_AON_CTRL->REG_AON_DIG_RSVD2.all;
	CLOGD("AON domain data: 0x%08x", aon_val);

	if(aon_val != TEST_PATTERN) {
		CLOGD("[Success] AON Domain reset Successfully");
	} else {
		CLOGD("[Failed] AON Domain reset Failed");
	}

	IP_AON_CTRL->REG_AON_DIG_RSVD2.all = TEST_PATTERN;

	AON_WDT_Initialize(AON_WDT_Handler, AON_WDT_AUTOFeed_EventCallback, NULL);

	AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

	AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_CTRL_RESET_MODE | HAL_AON_WDT_RST_PMU_DOMAIN, 0);
	AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);

	AON_WDT_Enable(AON_WDT_Handler);

    for (i = 0; i < test_loops; i++) {
        SysTick_Delay_Ms(500);
        ret = AON_WDT_Refresh(AON_WDT_Handler);
        if (ret != CSK_DRIVER_OK) {
            CLOGE("AON_WDT_Refresh failed at loop %u: %d", i, ret);
            break;
        }
        CLOGD("Refresh %u", i + 1);
    }

    FAKE_WHILE();

    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}

#define AON_TEST_PATTERN 0x55AA55AA
volatile uint32_t wdt_irq_count = 0;

void aon_wdt_irq_feed_handler(void* workspace)
{
    uint32_t aon_wdt_load_value = 0;
    AON_WDT_ReadLoadValue(AON_WDT_Handler, &aon_wdt_load_value);

    if (++wdt_irq_count < 5) {
        AON_WDT_Refresh(AON_WDT_Handler);
    }

    CLOGD("AON_WDT IRQ: WDT refreshed in interrupt, count=%lu, load_value=%d", wdt_irq_count, aon_wdt_load_value);
}

static void aon_wdt_set_irq_and_reset_core_only(void)
{
	CLOGD("wdt_irq_count = %d", wdt_irq_count);
	uint32_t aon_val = IP_AON_CTRL->REG_AON_DIG_RSVD3.all;
	CLOGD("AON domain data: 0x%08x", aon_val);

	if(aon_val == AON_TEST_PATTERN) {
		CLOGD("[Success] AON Domain Remain");
	} else {
		CLOGD("[Failed] AON Domain reset");
	}

	IP_AON_CTRL->REG_AON_DIG_RSVD3.all = AON_TEST_PATTERN;

    AON_WDT_Initialize(AON_WDT_Handler, aon_wdt_irq_feed_handler, NULL);
    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

    // Configure WDT: reset only core domain, enable IRQ
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_CORE_DOMAIN | HAL_AON_WDT_CTRL_INT_MODE, 1);
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);

    //need reload option
    IP_AON_WDT->REG_AON_WDTTIMER_CTRL.bit.RELOAD = 0x1;
    //need some delay
    SysTick_Delay_Us(100);
    CLOGD("Reload successed\r\n");

    AON_WDT_Enable(AON_WDT_Handler);

    FAKE_WHILE();

    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}

static void aon_wdt_set_irq_and_reset_core_aon(void)
{
	CLOGD("wdt_irq_count = %d", wdt_irq_count);
	uint32_t aon_val = IP_AON_CTRL->REG_AON_DIG_RSVD3.all;
	CLOGD("AON domain data: 0x%08x", aon_val);

	if(aon_val != AON_TEST_PATTERN) {
		CLOGD("[Success] AON Domain reset Successfully");
	} else {
		CLOGD("[Failed] AON Domain reset Failed");
	}

	IP_AON_CTRL->REG_AON_DIG_RSVD3.all = AON_TEST_PATTERN;

    AON_WDT_Initialize(AON_WDT_Handler, aon_wdt_irq_feed_handler, NULL);
    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

    // Configure WDT: reset only core domain, enable IRQ
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_PMU_DOMAIN | HAL_AON_WDT_CTRL_INT_MODE, 1);
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);

    AON_WDT_Enable(AON_WDT_Handler);

    FAKE_WHILE();

    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}

void aon_wdt_irq_feed_handler_change_value(void* workspace)
{
    uint32_t aon_wdt_load_value = 0;
    AON_WDT_ReadLoadValue(AON_WDT_Handler, &aon_wdt_load_value);

    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 64000);

    if (++wdt_irq_count < 5) {
        AON_WDT_Refresh(AON_WDT_Handler);
    }

    CLOGD("AON_WDT IRQ: WDT refreshed in interrupt, count=%lu, load_value=%d", wdt_irq_count, aon_wdt_load_value);
}

static void aon_wdt_set_irq_and_reset_core_aon_change_load_value(void)
{
	CLOGD("wdt_irq_count = %d", wdt_irq_count);
	uint32_t aon_val = IP_AON_CTRL->REG_AON_DIG_RSVD3.all;
	CLOGD("AON domain data: 0x%08x", aon_val);

	if(aon_val != AON_TEST_PATTERN) {
		CLOGD("[Success] AON Domain reset Successfully");
	} else {
		CLOGD("[Failed] AON Domain reset Failed");
	}

	IP_AON_CTRL->REG_AON_DIG_RSVD3.all = AON_TEST_PATTERN;

    AON_WDT_Initialize(AON_WDT_Handler, aon_wdt_irq_feed_handler_change_value, NULL);
    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

    // Configure WDT: reset only core domain, enable IRQ
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_PMU_DOMAIN | HAL_AON_WDT_CTRL_INT_MODE, 1);
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);

    AON_WDT_Enable(AON_WDT_Handler);

    FAKE_WHILE();

    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}


typedef void (*function)(void);

typedef struct {
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
    // {aon_wdt_reset_core_only_no_irq,       "aon_wdt_reset_core_only_no_irq"},
    // {aon_wdt_reset_core_aon_no_irq,        "aon_wdt_reset_core_aon_no_irq"},
    // {aon_wdt_set_irq_and_reset_core_only,  "aon_wdt_set_irq_and_reset_core_only"},
    // {aon_wdt_set_irq_and_reset_core_aon,   "aon_wdt_set_irq_and_reset_core_aon"},
    // {aon_wdt_set_irq_and_reset_core_aon_change_load_value, "aon_wdt_set_irq_and_reset_core_aon_change_load_value"},
};

int main(void) {
	logInit(0, 115200);
	CLOG("enter main: aon_wdt test\r\n");

	AON_WDT_Init_Handler();

	uint32_t index;
	for(index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++) {
		test_case_t item = test_array[index];
		CLOG("test case : %s", item.name);
		item.function();
	}

	while(1);

	return 0;
}
