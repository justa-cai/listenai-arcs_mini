/*
 * main.c
 *
 *  Created on: 2025年5月14日
 *      Author: USER
 */
#include <stdio.h>
#include <stdlib.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "PowerManager.h"
#include "IOMuxManager.h"
#include "Driver_AON_TIMER.h"
#include "Driver_AON_WDT.h"
#include "Driver_CALENDAR.h"

#define GPIO_PMU_WAKEUP_FUN_SEL         0

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

#define WAKEUP_SOURCE_TO_STRING(src) (	\
    src == PMU_WAKEUP_NONE ?        "None wakeup" : \
    src == PMU_WAKEUP_TIMER ?       "TIMER wakeup" : \
    src == PMU_WAKEUP_IWDT ?        "IWDT wakeup" : \
    src == PMU_WAKEUP_KEY ?         "KEY wakeup" : \
    src == PMU_WAKEUP_RTC  ?        "RTC  wakeup" : \
    src == PMU_WAKEUP_GPIOB_00 ?    "GPIOB_00 wakeup" : \
    src == PMU_WAKEUP_GPIOB_01 ?    "GPIOB_01 wakeup" : \
    src == PMU_WAKEUP_GPIOB_02 ?    "GPIOB_02 wakeup" : \
    src == PMU_WAKEUP_GPIOB_03 ?    "GPIOB_03 wakeup" : \
    src == PMU_WAKEUP_GPIOB_04 ?    "GPIOB_04 wakeup" : \
    src == PMU_WAKEUP_GPIOB_05 ?    "GPIOB_05 wakeup" : \
                                    "Unknown wakeup")

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

static void GetWakeupEvent(uint32_t wakeup_src) {
    CLOGD("%s\n", WAKEUP_SOURCE_TO_STRING(wakeup_src));
    CLOG_FLUSH();
}

static void GetResetEvent(uint32_t rst_cause) {
	CLOGD("%s\n", RESET_CAUSE_TO_STRING(rst_cause));
	CLOG_FLUSH();
}

typedef void (*function)(void);

typedef struct {
	void (*function)(void);
	const char* name;
}test_case_t;

static void pmu_test_deepsleep(void) {
    __HAL_PMU_XO24M_DISABLE();

    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    CLOGD("Wrong!!!! CPU should not get here!!");
}

static void pmu_test_deepsleep_pbx_wakeup_high(void) {
    SysTick_Delay_Ms(300);

    CLOGD("pmu_deep_sleep_pbx_wakeup_high and PB3 high wake up");

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
    HAL_PMU_ClearWakeUpCause();
    GetWakeupEvent(wakeup_cause);

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    SysTick_Delay_Ms(300);
    HAL_PMU_GPIOPolaritySelect(PMU_POLARITY_GPIOB_03, 0);
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_GPIOB_03);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, GPIO_PMU_WAKEUP_FUN_SEL);

    CLOGD("Enter sleep\r\n");
    CLOG_FLUSH();

    // Disable RC32K
    __HAL_PMU_RC32K_DISABLE();
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    CLOGD("Wrong!!!! CPU should not get here!!");
}

static void pmu_test_deepsleep_pbx_wakeup_low(void) {
    SysTick_Delay_Ms(300);

    CLOGD("pmu_deep_sleep_pbx_wakeup_high and PB3 LOW wake up");

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
    HAL_PMU_ClearWakeUpCause();
    GetWakeupEvent(wakeup_cause);

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();  
    GetResetEvent(reset_cause);

    SysTick_Delay_Ms(300);
    HAL_PMU_GPIOPolaritySelect(PMU_POLARITY_GPIOB_03, 1);
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_GPIOB_03);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, GPIO_PMU_WAKEUP_FUN_SEL);

    CLOGD("Enter sleep\r\n");

    // Disable RC32K
    __HAL_PMU_RC32K_DISABLE();
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    CLOGD("Wrong!!!! CPU should not get here!!");    
}

static void* AON_TIMER_Handler = NULL;

static void AON_TIMER_Init_Handler() {
    AON_TIMER_Handler = AON_TIMER();
}

static void pmu_test_deepsleep_aontimer_wakeup(void) {
    AON_TIMER_Init_Handler();

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
    HAL_PMU_ClearWakeUpCause();
    GetWakeupEvent(wakeup_cause);

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    SysTick_Delay_Ms(100);

    if (wakeup_cause == PMU_WAKEUP_TIMER) {
        // Clear interrupt source
        __HAL_PMU_AON_TIMER_CLEAR_IRQ();

        CLOGD("Aon_timer enter sleep mode again.\r\n");
        CLOG_FLUSH();

        HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

        // Enter sleep
        HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
        HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

	    __NOP();
	    __NOP();
	    __NOP();
	    __NOP();

	    CLOGD("[AON_TIMER] Can't Entry SleepMode!!!!!");
    }

    AON_TIMER_Initialize(AON_TIMER_Handler, NULL, NULL);
    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);
    AON_TIMER_Control(AON_TIMER_Handler, HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled);
    AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, 32000);
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);
    AON_TIMER_StartTimer(AON_TIMER_Handler);

    SysTick_Delay_Ms(100);

    CLOGD("Enter Sleep\r\n");
    CLOG_FLUSH();

    //enter deep sleep
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    CLOGD("[AON_TIMER] Can't Entry SleepMode!!!!!");
}

static void* AON_WDT_Handler = NULL;

static void AON_WDT_Init_Handler() {
    AON_WDT_Handler = AON_WDT();
}

static void pmu_test_deepsleep_aon_wdt_wakeup(void) {
	AON_WDT_Init_Handler();

	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();
	GetWakeupEvent(wakeup_cause);

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

	if (wakeup_cause == PMU_WAKEUP_IWDT){
		// Clear interrupt source
		__HAL_PMU_AON_WDT_CLEAR_IRQ();

		CLOG("Aon_wdt enter sleep mode again.\r");
		CLOG_FLUSH();

		AON_WDT_Refresh(AON_WDT_Handler);

		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_IWDT);

		HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
		HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

	    __NOP();
	    __NOP();
	    __NOP();
	    __NOP();

	    CLOGD("[AON_WDT] Can't Entry SleepMode!!!!!");
	}

	AON_WDT_Initialize(AON_WDT_Handler, NULL, NULL);
	AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);
	AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_CTRL_INT_MODE |\
    		HAL_AON_WDT_RST_PMU_DOMAIN, 1);

    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_IWDT);
    AON_WDT_Enable(AON_WDT_Handler);

	HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    CLOGD("[AON_WDT] Can't Entry SleepMode!!!!!");
}

static void* CALENDAR_Handler = NULL;

static void CALENDAR_Init_Handler(){
    CALENDAR_Handler = CALENDAR();
}

static void CALENDAR_EventCallback(uint32_t event, void* workspace){
    CLOGD("Trigger: %d", event);

    CSK_CALENDAR_TIME stime;
    CALENDAR_GetTime(CALENDAR_Handler, &stime);
    CLOGD("Years: %d; Month: %d; Day: %d; Week: %d; Hour: %d; Min: %d; Sec: %d",\
            stime.year, stime.month, stime.weekend, stime.day, stime.hour,\
            stime.min, stime.sec);
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

static void pmu_test_deepsleep_calendar_alarm_calibration_wakeup(void) {
    CALENDAR_Init_Handler();

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();
	GetWakeupEvent(wakeup_cause);

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

   if (wakeup_cause == PMU_WAKEUP_RTC) {
       CLOGD("Calendar enter sleep mode again.\r\n");
       CLOG_FLUSH();

       CSK_CALENDAR_TIME stime;
       CALENDAR_Initialize(CALENDAR_Handler, NULL, NULL);
       CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);
       CALENDAR_GetTime(CALENDAR_Handler, &stime);
       CLOGD("Time Years: %d; Month: %d; Day: %d; Week: %d; Hour: %d; Min: %d; Sec: %d",\
               stime.year, stime.month, stime.day, stime.weekend, stime.hour,\
               stime.min, stime.sec);

       CSK_CALENDAR_ALARM satime = {0};
       int_2_time((time_2_int(stime) + 10), &satime);
               CLOGD("Alarm Years: %d; Month: %d; Day: %d; Hour: %d; Min: %d; Sec: %d",\
       		satime.year, satime.month,  satime.day, satime.hour,\
				satime.min, satime.sec);
       CLOG_FLUSH();

       CALENDAR_SetAlarm(CALENDAR_Handler, &satime);
       CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

       HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_RTC);

	    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
	    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

       __NOP();
       __NOP();
       __NOP();
       __NOP();

       CLOGD("Can't go here");
   }

    CSK_CALENDAR_TIME stime = {2, 3, 0, 15, 14, 30, 45};
    CSK_CALENDAR_ALARM satime = {0};

    int_2_time((time_2_int(stime) + 10), &satime);

    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);
    CALENDAR_SetTime(CALENDAR_Handler, &stime);
    CALENDAR_SetAlarm(CALENDAR_Handler, &satime);

    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_RTC);

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);
    // need add some delay
    for(uint32_t i = 0; i<0x5ffff; i++);

    CLOGD("Need Some Time Enter Sleep\r\n");
    CLOG_FLUSH();

    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_TRIGGER_BY_CORE0);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    CLOGD("Can't go here");   
}

#define CORE0_TIMER_INTERVAL		5	// ap_run 5s
#define CORE1_TIMER_INTERVAL		10	// cp_run 10s
#define CPU_FREQ_MHZ			    400

static void pmu_dual_core_wfi_sleep() {
#if (BOOT_HARTID == 0)
	CLOGD("Initializing Core_0 task...\n");
	CLOGD("Core_0: Timer started for %d seconds\n", CORE0_TIMER_INTERVAL);

    uint64_t cycles_prev = __get_rv_cycle();
	uint64_t cycles_curr = 0;
	uint64_t target_cycles = CPU_FREQ_MHZ * 1e6 * CORE0_TIMER_INTERVAL;

    while(1) {
		cycles_curr = __get_rv_cycle();
		if(cycles_curr - cycles_prev >= target_cycles) {
			CLOG("AP Core: Timer expired. Entering WFI...\n");
			IP_CMN_SYSCFG->REG_N300_CORE1_RST_ADDR.all = 0x30010000;
			IP_CMN_SYSCFG->REG_SW_RESET_CORE1.all = 0xCAFE000A;
			HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_NONE);
			HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

			while(1);
		}
	}
#else
	CLOG("Initializing Core_1 task...\n");
	CLOG("Core_1: Timer started for %d seconds\n", CORE1_TIMER_INTERVAL);

    uint64_t cycles_prev = __get_rv_cycle();
	uint64_t cycles_curr = 0;
	uint64_t target_cycles = CPU_FREQ_MHZ * 1e6 * CORE1_TIMER_INTERVAL;

    while (1) {
        cycles_curr = __get_rv_cycle();
        if (cycles_curr - cycles_prev >= target_cycles) {
            CLOG("Core_1: Timer expired. Entering WFI...\n");
            HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_NONE);
        	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);
        }
    }
#endif
}

static inline uint32_t get_pc(void)
{
    uint32_t pc;
    asm volatile("auipc %0, 0" : "=r"(pc));
    return pc;
}

void pmu_test_print_aonreset_software() {

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    uint32_t test_times = 0;

    for(uint32_t i = 0; i < 5; i++) {
        uint32_t pc = get_pc();
        CLOGD("[%s:%d], Current PC = 0x%08x\n", __func__, __LINE__, pc);
        CLOGD("Hello world %d", test_times++);
        SysTick_Delay_Ms(500);
    }

    //AON_SW_RESET
    __HAL_PMU_AON_SOFTWARE_RESET_FULL_CHIP();
}

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

void pmu_test_print_aonreset_aonwdt(){

    AON_WDT_Init_Handler();

    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    AON_WDT_Initialize(AON_WDT_Handler, aon_wdt_irq_feed_handler, NULL);
    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

    // Configure WDT: reset core and aon domain, enable IRQ
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_RST_PMU_DOMAIN | HAL_AON_WDT_CTRL_INT_MODE, 1);
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, 32000);

    AON_WDT_Enable(AON_WDT_Handler);

    FAKE_WHILE();

    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}

void pmu_test_print_cmn_sw1_reset() {
    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    uint32_t sw_reset_cfg0 = IP_CMN_SYS->REG_SW_RESET_CFG0.all;
    CLOGD("sw_reset_cfg0 value = 0x%08x",sw_reset_cfg0);

    uint32_t test_times = 0;

    for(uint32_t i = 0; i < 5; i++) {
        uint32_t pc = get_pc();
        CLOGD("[%s:%d], Current PC = 0x%08x\n", __func__, __LINE__, pc);
        CLOGD("Hello world %d", test_times++);
        SysTick_Delay_Ms(500);
    }
    
    __HAL_PMU_CMN_SOFTWARE_CORE1_RESET();
}

void pmu_test_print_cmn_sw0_reset() {
    uint32_t reset_cause = HAL_PMU_GetSysResetCause();
    HAL_PMU_ClearSysResetCause();
    GetResetEvent(reset_cause);

    uint32_t sw_reset_cfg0 = IP_CMN_SYS->REG_SW_RESET_CFG0.all;
    CLOGD("sw_reset_cfg0 value = 0x%08x",sw_reset_cfg0);

    uint32_t test_times = 0;

    for(uint32_t i = 0; i < 5; i++) {
        uint32_t pc = get_pc();
        CLOGD("[%s:%d], Current PC = 0x%08x\n", __func__, __LINE__, pc);
        CLOGD("Hello world %d", test_times++);
        SysTick_Delay_Ms(500);
    }
    
    __HAL_PMU_CMN_SOFTWARE_CORE0_RESET();
}

static test_case_t test_array[] = {
    // {pmu_test_deepsleep, "pmu_test_deepsleep"},
    // {pmu_test_deepsleep_pbx_wakeup_high, "pmu_test_deepsleep_pbx_wakeup_high"},
    // {pmu_test_deepsleep_pbx_wakeup_low, "pmu_test_deepsleep_pbx_wakeup_low"},
    // {pmu_test_deepsleep_aontimer_wakeup, "pmu_test_deepsleep_aontimer_wakeup"},
    // {pmu_test_deepsleep_aon_wdt_wakeup, "pmu_test_deepsleep_aon_wdt_wakeup"},
    // {pmu_test_deepsleep_calendar_alarm_calibration_wakeup, "pmu_test_deepsleep_calendar_alarm_calibration_wakeup"},
    
    // {pmu_dual_core_wfi_sleep, "pmu_dual_core_wfi_sleep"},
    // {pmu_test_print_aonreset_software,"pmu_test_print_aonreset_software"},
    // {pmu_test_print_aonreset_aonwdt, "pmu_test_print_aonreset_aonwdt"},
    // {pmu_test_print_cmn_sw1_reset, "pmu_test_print_cmn_sw1_reset"},
    // {pmu_test_print_cmn_sw0_reset, "pmu_test_print_cmn_sw0_reset"}
};

int main(void) {
    logInit(0, 115200);
    CLOGD("enter main: pmu test\r\n");
    
    uint32_t index;
	for(index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++) {
		test_case_t item = test_array[index];
		CLOG("test case : %s", item.name);
		item.function();
	}

    while (1);
    
    return 0;
}

