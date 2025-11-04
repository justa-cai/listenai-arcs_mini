/*
 * aon_timer_nos_chk.c
 *
 *  Created on: Apr 6, 2022
 *      Author: USER
 */
#include "log_print.h"
#include "Driver_AON_WDT.h"
#include "PowerManager.h"
#include "IOMuxManager.h"
#include "systick.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "unity.h"

#define AON_WDT_CLEAR_IRQ()	\
do{	\
	IP_AON_WDT->REG_AON_WDT_IRQ_CLR.all = 0x1;	\
	while(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);	\
}while(0)

#define AON_WDT_CLEAR_RST()	\
do{	\
	IP_AON_WDT->REG_AON_WDT_IRQ_CLR.all = 0x1;	\
	while(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_RESET_OCURRED);	\
}while(0)

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

#define TEST_ASSERT_WDT_ENABLED() \
    TEST_ASSERT_TRUE(IP_AON_WDT->REG_AON_WDTTIMER_CTRL.bit.WDENABLED == 1)

#define TEST_ASSERT_WDT_DISABLED() \
    TEST_ASSERT_TRUE(IP_AON_WDT->REG_AON_WDTTIMER_CTRL.bit.WDENABLED == 0)

#define TEST_ASSERT_WDT_MODE(mode) \
    TEST_ASSERT_EQUAL_HEX(mode, IP_AON_WDT->REG_AON_WDTTIMER_CTRL.bit.WDT_MODE)


void setUp(void) {
}

void tearDown(void) {
}

typedef void (*function)(void);

static void AON_WDT_RstMode_Refresh_Test(void);
static void AON_WDT_Refresh_Test(void);
static void AON_WDT_Refresh_Stop_Test(void);
static void AON_WDT_INT_Feed_Test(void);
static void AON_WDT_RST_Test(void);
static void AON_WDT_Wakeup_Test(void);

static function test_function_array[] = {
	AON_WDT_RstMode_Refresh_Test,
	AON_WDT_Refresh_Test,
	AON_WDT_Refresh_Stop_Test,
	AON_WDT_INT_Feed_Test,
	AON_WDT_RST_Test,
	AON_WDT_Wakeup_Test,
};

static void* AON_WDT_Handler = NULL;


static void AON_WDT_Init_Handler(){
    AON_WDT_Handler = AON_WDT();
}

static void AON_WDT_RstMode_Refresh_Test() {
    CLOGD("Running AON_WDT_RstMode_Refresh_Test");
    //TEST_CASE("Running AON_WDT_RstMode_Refresh_Test");
    // Initialize WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Initialize(AON_WDT_Handler, NULL, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL));

    // Configure WDT
    uint32_t timeout = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, timeout));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler,
            HAL_AON_WDT_CTRL_RESET_MODE | HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_CORE_DOMAIN,
            1));
    //TEST_ASSERT_WDT_MODE(HAL_AON_WDT_CTRL_RESET_MODE);

    // Enable WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Enable(AON_WDT_Handler));
    TEST_ASSERT_WDT_ENABLED();

    // Test refresh
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Refresh(AON_WDT_Handler));
        CLOGD("Refresh %d", i+1);
        SysTick_Delay_Ms(250);

        // Verify WDT didn't trigger reset
        TEST_ASSERT_FALSE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_RESET_OCURRED);
    }

    // Cleanup
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Disable(AON_WDT_Handler));
    TEST_ASSERT_WDT_DISABLED();
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Uninitialize(AON_WDT_Handler));

    CLOGD("AON_WDT_RstMode_Refresh_Test PASSED");
}

static void AON_WDT_Refresh_Test() {
    CLOGD("Running AON_WDT_Refresh_Test");

    // Initialize WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Initialize(AON_WDT_Handler, NULL, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL));

    // Configure WDT
    uint32_t timeout = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, timeout));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler,
            HAL_AON_WDT_CTRL_INT_MODE | HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_CORE_DOMAIN,
            1));
    //TEST_ASSERT_WDT_MODE(HAL_AON_WDT_CTRL_INT_MODE);

    // Enable WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Enable(AON_WDT_Handler));
    TEST_ASSERT_WDT_ENABLED();

    // Test refresh
    for (int i = 0; i < 5; i++) {
        SysTick_Delay_Ms(500);
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Refresh(AON_WDT_Handler));
        CLOGD("Refresh %d", i+1);

        // Verify WDT didn't trigger interrupt
        TEST_ASSERT_FALSE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);
    }

    // Cleanup
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Disable(AON_WDT_Handler));
    TEST_ASSERT_WDT_DISABLED();
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Uninitialize(AON_WDT_Handler));

    CLOGD("AON_WDT_Refresh_Test PASSED");
}

static void AON_WDT_Refresh_Stop_Test() {
    CLOGD("Running AON_WDT_Refresh_Stop_Test");

    // Initialize WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Initialize(AON_WDT_Handler, NULL, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL));

    // Configure WDT
    uint32_t timeout = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, timeout));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler,
            HAL_AON_WDT_CTRL_INT_MODE | HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_CORE_DOMAIN,
            1));
    //TEST_ASSERT_WDT_MODE(HAL_AON_WDT_CTRL_INT_MODE);

    // Enable WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Enable(AON_WDT_Handler));
    TEST_ASSERT_WDT_ENABLED();

    // Test refresh
    for (int i = 0; i < 10; i++) {
        SysTick_Delay_Ms(500);
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Refresh(AON_WDT_Handler));
        CLOGD("Refresh %d", i+1);
    }

    // Final refresh and disable
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Refresh(AON_WDT_Handler));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Disable(AON_WDT_Handler));
    TEST_ASSERT_WDT_DISABLED();

    // Verify WDT can be properly stopped
    SysTick_Delay_Ms(2000);
    TEST_ASSERT_FALSE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);
    TEST_ASSERT_FALSE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_RESET_OCURRED);

    // Cleanup
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Uninitialize(AON_WDT_Handler));

    CLOGD("AON_WDT_Refresh_Stop_Test PASSED");
}

static void AON_WDT_AUTOFeed_EventCallback(void* workspace) {
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Refresh(AON_WDT_Handler));
    CLOGD("Aon wdt trigger, Feed!!!");

    // Verify interrupt status
    TEST_ASSERT_TRUE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);
    AON_WDT_CLEAR_IRQ();
    TEST_ASSERT_FALSE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);
}

static void AON_WDT_INT_Feed_Test() {
    CLOGD("Running AON_WDT_INT_Feed_Test");

    // Initialize WDT with callback
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Initialize(AON_WDT_Handler, AON_WDT_AUTOFeed_EventCallback, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL));

    // Configure WDT
    uint32_t timeout = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, timeout));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler,
            HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_CTRL_INT_MODE | HAL_AON_WDT_RST_PMU_DOMAIN,
            1));
    //TEST_ASSERT_WDT_MODE(HAL_AON_WDT_CTRL_INT_MODE);

    // Enable WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Enable(AON_WDT_Handler));
    TEST_ASSERT_WDT_ENABLED();

    // Let the callback handle the refresh
    for (int i = 0; i < 5; i++) {
        SysTick_Delay_Ms(1000);
        CLOGD("Waiting for WDT interrupt %d", i+1);
    }

    // Cleanup
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Disable(AON_WDT_Handler));
    TEST_ASSERT_WDT_DISABLED();
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Uninitialize(AON_WDT_Handler));

    CLOGD("AON_WDT_INT_Feed_Test PASSED");
}

static void AON_WDT_RST_Test() {
    CLOGD("Running AON_WDT_RST_Test");

    // Initialize WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Initialize(AON_WDT_Handler, NULL, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL));

    // Configure WDT for reset mode
    uint32_t timeout = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, timeout));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler,
            HAL_AON_WDT_CTRL_INT_MODE | HAL_AON_WDT_RST_CORE_DOMAIN,
            0));
    //TEST_ASSERT_WDT_MODE(HAL_AON_WDT_CTRL_INT_MODE);

    // Enable WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Enable(AON_WDT_Handler));
    TEST_ASSERT_WDT_ENABLED();

    // Note: This test will trigger a reset, so we can't verify after this point
    // In a real system, you would need to check reset status after reboot
    CLOGD("System should reset soon...");

    // This will never be reached if reset works
    TEST_FAIL_MESSAGE("WDT reset failed to occur");

    // Cleanup
     TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Disable(AON_WDT_Handler));
     TEST_ASSERT_WDT_DISABLED();
     TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF));
     TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Uninitialize(AON_WDT_Handler));
}

static void AON_WDT_Wakeup_Test() {
    CLOGD("Running AON_WDT_Wakeup_Test");

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
    HAL_PMU_ClearWakeUpCause();

    CLOGD("[AON_WDT] Mode2 WakeUp cause -> %d", wakeup_cause);

    if (wakeup_cause == PMU_WAKEUP_IWDT) {
        // Verify we woke up from WDT
        TEST_ASSERT_EQUAL(PMU_WAKEUP_IWDT, wakeup_cause);

        // Clear interrupt source
        AON_WDT_CLEAR_IRQ();
        TEST_ASSERT_FALSE(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);

        // Refresh WDT
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Refresh(AON_WDT_Handler));

        // Re-enable wakeup source
        HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_IWDT);

        CLOGD("Woke up from WDT interrupt");
        return;
    }

    // Initialize WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Initialize(AON_WDT_Handler, NULL, NULL));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL));

    // Configure WDT
    uint32_t timeout = 32000;
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, timeout));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        AON_WDT_Control(AON_WDT_Handler,
            HAL_AON_WDT_CTRL_INT_MODE | HAL_AON_WDT_RST_PMU_DOMAIN,
            1));
    //TEST_ASSERT_WDT_MODE(HAL_AON_WDT_CTRL_INT_MODE);

    // Enable wakeup source
   HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_IWDT);

    // Enable WDT
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Enable(AON_WDT_Handler));
    TEST_ASSERT_WDT_ENABLED();

    // Enter deep sleep - should not return if successful
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    // Should not reach here if sleep was successful
    TEST_FAIL_MESSAGE("Failed to enter deep sleep mode or WDT didn't wake up system");

    // Cleanup
     TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Disable(AON_WDT_Handler));
     TEST_ASSERT_WDT_DISABLED();
     TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF));
     TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_WDT_Uninitialize(AON_WDT_Handler));
}

int main(){
    uint32_t times;

    logInit(0, 115200);
    CLOGD("AON WDT validation");

    AON_WDT_Init_Handler();
    UNITY_BEGIN();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
    	RUN_TEST(test_function_array[times]);
    }
    return UNITY_END();
    while(1);
}
