/*
 * aon_timer_nos_chk.c
 *
 *  Created on: Apr 6, 2022
 *      Author: USER
 */
#include "log_print.h"
#include "Driver_AON_TIMER.h"
#include "Driver_AON_WDT.h"
#include "ClockManager.h"
#include "PowerManager.h"

#include "systick.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "unity.h"

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

static void AON_TIMER_NormalMode_Test();
static void AON_TIMER_WrapMode_Test();
static void AON_TIMER_RepeatMode_RC32K_Test();
static void AON_TIMER_RepeatMode_D32K_Test();
static void AON_TIMER_RepeatMode_RC32K_CALI_Test();
static void AON_TIMER_WakeUp_M2_Test();

void setUp(void) {
}

void tearDown(void) {
}

static function test_function_array[] = {
    AON_TIMER_NormalMode_Test,
	AON_TIMER_WrapMode_Test,
	AON_TIMER_RepeatMode_RC32K_Test,
	AON_TIMER_RepeatMode_D32K_Test,
	AON_TIMER_RepeatMode_RC32K_CALI_Test,
//	AON_TIMER_WakeUp_M2_Test,
};

static void* AON_TIMER_Handler = NULL;

static void AON_TIMER_WDT_Init_Handler(){
    AON_TIMER_Handler = AON_TIMER();
}

static void AON_TIMER_EventCallback(uint32_t event, void* workspace){
    CLOGD("Aon timer trigger");

    uint32_t counter;

    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);

    CLOGD("Current counter -> 0x%x", counter);
}

static void VerifyTimerCount(uint32_t expected_min, uint32_t expected_max) {
    uint32_t counter;
    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);
    CLOGD("Current counter -> 0x%x", counter);
    TEST_ASSERT_UINT32_WITHIN(expected_min, expected_max, counter);
}

static void AON_TIMER_NormalMode_Test() {
    CLOGD("Running AON_TIMER_NormalMode_Test");

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Control(AON_TIMER_Handler,
        HAL_AON_TIMER_MODE_Normal | HAL_AON_TIMER_INTERRUPT_Enabled));

    uint32_t period = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, period));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StartTimer(AON_TIMER_Handler));

    // Verify timer is counting
    SysTick_Delay_Ms(100);
    VerifyTimerCount(1, period-1);

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StopTimer(AON_TIMER_Handler));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Uninitialize(AON_TIMER_Handler));

    CLOGD("AON_TIMER_NormalMode_Test PASSED");
}

static void AON_TIMER_WrapMode_Test() {
    CLOGD("Running AON_TIMER_WrapMode_Test");

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Control(AON_TIMER_Handler,
        HAL_AON_TIMER_MODE_Wrapping | HAL_AON_TIMER_INTERRUPT_Enabled));

    uint32_t period = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, period));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StartTimer(AON_TIMER_Handler));

    // Verify timer is counting and wrapping
    SysTick_Delay_Ms(100);
    VerifyTimerCount(1, period-1);

    // Wait for wrap-around
    SysTick_Delay_Ms(2000);
    VerifyTimerCount(1, period-1);

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StopTimer(AON_TIMER_Handler));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Uninitialize(AON_TIMER_Handler));

    CLOGD("AON_TIMER_WrapMode_Test PASSED");
}

static void AON_TIMER_RepeatMode_RC32K_Test() {
    CLOGD("Running AON_TIMER_RepeatMode_RC32K_Test");

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Control(AON_TIMER_Handler,
        HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32k));

    uint32_t period = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, period));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StartTimer(AON_TIMER_Handler));

    // Verify timer is counting and repeating
    for (int i = 0; i < 3; i++) {
        SysTick_Delay_Ms(1100); // Slightly more than 1 second
        VerifyTimerCount(0, period);
    }

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StopTimer(AON_TIMER_Handler));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Uninitialize(AON_TIMER_Handler));

    CLOGD("AON_TIMER_RepeatMode_RC32K_Test PASSED");
}

static void AON_TIMER_RepeatMode_D32K_Test() {
    CLOGD("Running AON_TIMER_RepeatMode_D32K_Test");

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Control(AON_TIMER_Handler,
        HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Xo32k));

    uint32_t period = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, period));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StartTimer(AON_TIMER_Handler));

    // Verify timer is counting and repeating with XO32K clock
    for (int i = 0; i < 3; i++) {
        SysTick_Delay_Ms(1100); // Slightly more than 1 second
        VerifyTimerCount(0, period);
    }

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StopTimer(AON_TIMER_Handler));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Uninitialize(AON_TIMER_Handler));

    CLOGD("AON_TIMER_RepeatMode_D32K_Test PASSED");
}

static void AON_TIMER_RepeatMode_RC32K_CALI_Test() {
    CLOGD("Running AON_TIMER_RepeatMode_RC32K_CALI_Test");

    uint32_t rc32_clk = CRM_GetSrcFreq(CRM_IpSrcAon32kClk);
    CLOGD("[Cali]Rc32K -> %d", rc32_clk);
    TEST_ASSERT_UINT32_WITHIN(30000, 34000, rc32_clk); // Verify RC32K is within expected range

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Control(AON_TIMER_Handler,
        HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32k));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, rc32_clk));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StartTimer(AON_TIMER_Handler));

    // Verify timer completes one full cycle in about 1 second
    uint32_t start_count, end_count;
    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &start_count);
    SysTick_Delay_Ms(1000);
    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &end_count);

    uint32_t delta = (end_count > start_count) ? (end_count - start_count) : (rc32_clk - start_count + end_count);
    TEST_ASSERT_UINT32_WITHIN(rc32_clk * 0.9, rc32_clk * 1.1, delta); // Within 10% of expected

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StopTimer(AON_TIMER_Handler));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Uninitialize(AON_TIMER_Handler));

    CLOGD("AON_TIMER_RepeatMode_RC32K_CALI_Test PASSED");
}

static void AON_TIMER_WakeUp_M2_Test() {
    CLOGD("Running AON_TIMER_WakeUp_M2_Test");

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
    CLOGD("[AON_TIMER] Mode2 WakeUp cause -> %d", wakeup_cause);

    if (wakeup_cause == PMU_WAKEUP_TIMER) {
        CLOGD("Woke up from AON_TIMER");
        TEST_ASSERT_EQUAL(PMU_WAKEUP_TIMER, wakeup_cause);
        HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);
        return;
    }

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_Control(AON_TIMER_Handler,
        HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32k));

    uint32_t period = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, period));

    // Enable wakeup source
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_TIMER_StartTimer(AON_TIMER_Handler));

    CLOGD("Entering deep sleep mode...");
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    // Should not reach here if sleep was successful
    TEST_FAIL_MESSAGE("Failed to enter deep sleep mode");

    CLOGD("AON_TIMER_WakeUp_M2_Test PASSED");
}

int main(){
    uint32_t times;

    logInit(0, 115200);
    CLOGD("AON TIMER validation");

    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK = 1;

    enable_GINT();

    AON_TIMER_WDT_Init_Handler();
    UNITY_BEGIN();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
    	RUN_TEST(test_function_array[times]);
    }
    return UNITY_END();
    while(1);
}
