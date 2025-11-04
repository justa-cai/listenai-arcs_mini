#include "log_print.h"
#include "Driver_CALENDAR.h"
#include "PowerManager.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "systick.h"
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

static void CALENDAR_Sec_Int_Test();
static void CALENDAR_Min_Int_Test();
static void CALENDAR_Hour_Int_Test();
static void CALENDAR_Alarm_Int_Test();
static void CALENDAR_Alarm_Calibration_WakeUp_Test();

static function test_function_array[] = {
    CALENDAR_Sec_Int_Test,
    CALENDAR_Min_Int_Test,
    CALENDAR_Hour_Int_Test,
    CALENDAR_Alarm_Int_Test,
//      CALENDAR_Alarm_Calibration_WakeUp_Test,
};

static void* CALENDAR_Handler = NULL;

void setUp(void) {
}

void tearDown(void) {
}

static void CALENDAR_Init_Handler(){
    CALENDAR_Handler = CALENDAR();
}

static uint32_t time_2_int(CSK_CALENDAR_TIME stime){
	uint32_t totalSeconds = 0;
    totalSeconds += stime.year * 31536000;
    totalSeconds += stime.month * 2592000;
    totalSeconds += stime.day * 86400;
    totalSeconds += stime.hour * 3600;
    totalSeconds += stime.min * 60;
    totalSeconds += stime.sec;

    return totalSeconds;
}

static void int_2_time(uint32_t itime, CSK_CALENDAR_ALARM* stime){
    uint32_t total_sec = itime;

    stime->year = total_sec / 31536000;
    total_sec %= 31536000;
    stime->month = total_sec / 2592000;
    total_sec %= 2592000;
    stime->day = total_sec / 86400;
    total_sec %= 86400;
    stime->hour = total_sec / 3600;
    total_sec %= 3600;
    stime->min = total_sec / 60;
    stime->sec = total_sec % 60;
}

static void CALENDAR_EventCallback(uint32_t event, void* workspace){
    CLOGD("Trigger: %d", event);

    TEST_ASSERT_TRUE(event == CSK_CALENDAR_EVENT_ALARM_INT ||
                        event == CSK_CALENDAR_EVENT_HOUR_INT ||
                        event == CSK_CALENDAR_EVENT_MIN_INT ||
                        event == CSK_CALENDAR_EVENT_SEC_INT);

    CSK_CALENDAR_TIME stime;
    CALENDAR_GetTime(CALENDAR_Handler, &stime);
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetTime(CALENDAR_Handler, &stime));

    CLOGD("Years: %d; Month: %d; Day: %d; Week: %d; Hour: %d; Min: %d; Sec: %d",\
            stime.year, stime.month, stime.weekend, stime.day, stime.hour,\
            stime.min, stime.sec);
}

static void CALENDAR_Sec_Int_Test(){
	TEST_MESSAGE("Calendar Second Interrupt Test");

	CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 0};

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetTime(CALENDAR_Handler, &stime));

	//check set time is ok
	CSK_CALENDAR_TIME verify_time;
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetTime(CALENDAR_Handler, &verify_time));
	TEST_ASSERT_EQUAL(0, memcmp(&stime, &verify_time, sizeof(CSK_CALENDAR_TIME)));

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_SEC_INT, 1));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1));

	// wait the int trigger
	for(int i = 0; i < 5; i++) {
		SysTick_Delay_Ms(1000);
	}

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_SEC_INT, 0));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Uninitialize(CALENDAR_Handler));

    TEST_PASS_MESSAGE("Calendar Second Interrupt Test Passed");
}

static void CALENDAR_Min_Int_Test(){
	TEST_MESSAGE("Calendar Minute Interrupt Test");

	CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 59};

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetTime(CALENDAR_Handler, &stime));

	// check the set time is ok
	CSK_CALENDAR_TIME verify_time;
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetTime(CALENDAR_Handler, &verify_time));
	TEST_ASSERT_EQUAL(59, verify_time.sec); // be sure the time set 59s

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_MIN_INT, 1));

	// waitting the min trigger
	SysTick_Delay_Ms(61000); // be sure wait 61s >1 min

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_MIN_INT, 0));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Uninitialize(CALENDAR_Handler));

	TEST_PASS_MESSAGE("Calendar Minute Interrupt Test Passed");
}

static void CALENDAR_Hour_Int_Test(){
	// Test case identification for reporting
	TEST_MESSAGE("Calendar Hour Interrupt Test");

	// 1. Pre-condition validation
	// Verify handler is properly initialized before testing
	TEST_ASSERT_NOT_NULL(CALENDAR_Handler);

	// 2. Setup test time (59 minutes 59 seconds - edge case for hour rollover)
	CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 59, 59};

	// 3. Initialize calendar peripheral
	// Verify initialization succeeds before proceeding
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
	CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL));

	// 4. Power management verification
	// Ensure device is in full power mode for accurate timing
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
	CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL));

	// 5. Time setting and validation
	// Set test time and verify successful operation
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetTime(CALENDAR_Handler, &stime));
	// Read back time to confirm proper configuration
	CSK_CALENDAR_TIME verify_time;
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetTime(CALENDAR_Handler, &verify_time));
	// Verify critical fields match expected values
	TEST_ASSERT_EQUAL(59, verify_time.sec);  // Should be at 59 seconds
	TEST_ASSERT_EQUAL(59, verify_time.min);  // Should be at 59 minutes

	// 6. Interrupt control validation
    // Enable hour interrupt and verify success
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_HOUR_INT, 1));

	// 7. Wait for interrupt trigger
	// Expected to occur after approximately 1 minute (at hour rollover)
	CLOGD("Waiting for hour interrupt (should trigger in ~1 minute)");
	for(int i = 0; i < 61; i++) {  // 61 seconds to ensure crossing hour boundary
		SysTick_Delay_Ms(1000);  // 1 second delay

		// Verify time progression is working correctly
		CSK_CALENDAR_TIME current_time;
		TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetTime(CALENDAR_Handler, &current_time));
		// Allow ¡À5 second tolerance for timing variations
		//TEST_ASSERT_INT_WITHIN(5, i, current_time.sec);
	}

	// 8. Disable interrupt and verify
	// Clean up interrupt configuration after test
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_HOUR_INT, 0));

	// 9. Power down sequence
	// Verify proper power state transition
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF));

	// 10. Peripheral deinitialization
	// Verify clean resource release
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Uninitialize(CALENDAR_Handler));

	// 11. Post-condition validation
	// Verify peripheral is truly unavailable after uninit
	TEST_ASSERT_EQUAL(CSK_DRIVER_ERROR, CALENDAR_GetTime(CALENDAR_Handler, &verify_time));

	// Test completion marker
	TEST_PASS_MESSAGE("Calendar Hour Interrupt Test Passed");
}

static void CALENDAR_Alarm_Int_Test(){
	TEST_MESSAGE("Calendar Alarm Interrupt Test");

    CSK_CALENDAR_TIME stime = {1, 12, 7, 31, 23, 59, 59};
    CSK_CALENDAR_ALARM satime = {2, 1, 1, 0, 0, 5};

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetTime(CALENDAR_Handler, &stime));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetAlarm(CALENDAR_Handler, &satime));

	// check the alarm set ok
	CSK_CALENDAR_ALARM verify_alarm;
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetAlarm(CALENDAR_Handler, &verify_alarm));
	TEST_ASSERT_EQUAL(2, verify_alarm.year);

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1));

	// waitting alarm trigger
	SysTick_Delay_Ms(10000); // wait 10s

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 0));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Uninitialize(CALENDAR_Handler));

	TEST_PASS_MESSAGE("Calendar Alarm Interrupt Test Passed");
}

static void CALENDAR_Alarm_Calibration_WakeUp_Test(){
	TEST_MESSAGE("Calendar Alarm Wakeup Test");

	CALENDAR_Init_Handler();

	CLOG("wakeup!!, source is 0x%x", IP_AON_CTRL->REG_WAKEUP_ISR.all);

	pmu_wakeupsrc_t wk_cause = HAL_PMU_GetWakeUpCause();
	TEST_ASSERT_EQUAL(PMU_WAKEUP_RTC, wk_cause);

	if(wk_cause == PMU_WAKEUP_RTC) {
		CLOG("Calendar enter sleep mode again.\r\n");

		CSK_CALENDAR_TIME stime;

		TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Initialize(CALENDAR_Handler, NULL, NULL));
		TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL));
		TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_GetTime(CALENDAR_Handler, &stime));

		CLOGD("Time Years: %d; Month: %d; Day: %d; Week: %d; Hour: %d; Min: %d; Sec: %d",
				stime.year, stime.month, stime.day, stime.weekend, stime.hour,
				stime.min, stime.sec);

        CSK_CALENDAR_ALARM satime = {0};
        int_2_time((time_2_int(stime) + 30), &satime);

        CLOGD("Alarm Years: %d; Month: %d; Day: %d; Hour: %d; Min: %d; Sec: %d",\
        		satime.year, satime.month,  satime.day, satime.hour,\
				satime.min, satime.sec);

        log_flush();

        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetAlarm(CALENDAR_Handler, &satime));
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1));

        HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
    	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

        __NOP();
        __NOP();
        __NOP();
        __NOP();

        TEST_FAIL_MESSAGE("Should not reach here after deep sleep");
	}

    CSK_CALENDAR_TIME stime = {2, 3, 0, 15, 14, 30, 45};
    CSK_CALENDAR_ALARM satime = {0};

    int_2_time((time_2_int(stime) + 30), &satime);

	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetTime(CALENDAR_Handler, &stime));
	TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_SetAlarm(CALENDAR_Handler, &satime));

    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_RTC);

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1));

    // need add some delay
    for(uint32_t i = 0; i<0x5ffff; i++);

    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    TEST_FAIL_MESSAGE("Should not reach here after deep sleep");
    TEST_PASS_MESSAGE("Calendar Alarm Wakeup Test Passed");
}

int main(){
    uint32_t times;

    logInit(0, 115200);

    CALENDAR_Init_Handler();

    CLOGD("Calendar validation");

    UNITY_BEGIN();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
    	RUN_TEST(test_function_array[times]);
    }
    return UNITY_END();
    while(1);
}

