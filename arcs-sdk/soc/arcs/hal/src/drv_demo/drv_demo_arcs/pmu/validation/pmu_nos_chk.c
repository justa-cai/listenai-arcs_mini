#include "stdio.h"
#include <string.h>

#include "PowerManager.h"
#include "ClockManager.h"
#include "log_print.h"
#include "IOMuxManager.h"
#include "Driver_AON_TIMER.h"
#include "Driver_AON_WDT.h"
#include "Driver_CALENDAR.h"
#include "systick.h"
#include "unity.h"

#define GPIO_PMU_WAKEUP_FUN_SEL		  	  	1

#define AON_TIMER_CLEAR_IRQ()	\
do{	\
	IP_AON_TIMER->REG_OS_TIMER_IRQ_CLR.all = 0x1;	\
	while(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);	\
}while(0)

#define AON_WDT_CLEAR_IRQ()	\
do{	\
	IP_AON_WDT->REG_AON_WDT_IRQ_CLR.all = 0x1;	\
	while(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);	\
}while(0)


#define WAKEUP_SOURCE_TO_STRING(src)	(	\
		src == PMU_WAKEUP_NONE ? "None wakeup" : \
		src == PMU_WAKEUP_TIMER ? "TIMER wakeup" : \
		src == PMU_WAKEUP_IWDT ? "IWDT wakeup" : \
		src == PMU_WAKEUP_KEY0 ? "KEY0 wakeup" : \
		src == PMU_WAKEUP_KEY1 ? "KEY1 wakeup" : \
		src == PMU_WAKEUP_RTC  ? "RTC  wakeup" : \
		src == PMU_WAKEUP_WIFI ? "WIFI  wakeup" : \
		src == PMU_WAKEUP_GPIOB_00 ? "GPIOB_00 wakeup" : \
		src == PMU_WAKEUP_GPIOB_01 ? "GPIOB_01 wakeup" : \
		src == PMU_WAKEUP_GPIOB_02 ? "GPIOB_02 wakeup" : \
		src == PMU_WAKEUP_GPIOB_03 ? "GPIOB_03 wakeup" : \
		src == PMU_WAKEUP_GPIOB_04 ? "GPIOB_04 wakeup" : \
		src == PMU_WAKEUP_GPIOB_05 ? "GPIOB_05 wakeup" : \
		src == PMU_WAKEUP_GPIOB_06 ? "GPIOB_06 wakeup" : \
		src == PMU_WAKEUP_GPIOB_07 ? "GPIOB_07 wakeup" : \
		src == PMU_WAKEUP_GPIOB_08 ? "GPIOB_08 wakeup" : \
		src == PMU_WAKEUP_GPIOB_09 ? "GPIOB_09 wakeup" : \
		"Unknown wakeup")

static void GetWakeupEvent(uint32_t wakesrc) {
	CLOG("%s\n", WAKEUP_SOURCE_TO_STRING(wakesrc));
}

static void pmu_deepsleep() {
	__HAL_PMU_RC32K_DISABLE(); //MODE3
//	__HAL_PMU_RC32K_ENABLE();  //MODE3
//	__HAL_PMU_XO24M_DISABLE(); // off:MODE2
//  __HAL_PMU_XO24M_ENABLE();  //on:MODE2

	HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE3, PMU_DEEPSLEEPENTRY_WFI); //switch mode

	CLOGD("Wrong!!!! CPU should not get here!!");
}

static void pmu_deepsleep_pbx_wakeup_high(void) {
	SysTick_Delay_Ms(500);

	CLOG("pmu_deepsleep_pbx_wakeup_high and PB4 high wake up");
	CLOG("wake up!!, source is 0x%x", HAL_PMU_GetWakeUpCause());
	GetWakeupEvent(HAL_PMU_GetWakeUpCause());

	CLOG("sys_rst_status is 0x%x", HAL_PMU_GetSysResetCause());

	SysTick_Delay_Ms(500);
    HAL_PMU_GPIOPolaritySelect(PMU_WAKEUP_GPIOB_04, 0);
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_GPIOB_04);

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, GPIO_PMU_WAKEUP_FUN_SEL);

    // Disable RC32K
    __HAL_PMU_RC32K_DISABLE();

    //enter deep sleep
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    CLOGD("Wrong!!!! CPU should not get here!!");

}

static void pmu_deepsleep_pbx_wakeup_low(void) {
	SysTick_Delay_Ms(500);
	CLOG("pmu_deepsleep_pbx_wakeup_low and PB4 low wake up");
	CLOG("wake up!!, source is 0x%x", HAL_PMU_GetWakeUpCause());
	GetWakeupEvent(HAL_PMU_GetWakeUpCause());

	CLOG("sys_rst_status is 0x%x", HAL_PMU_GetSysResetCause());

	SysTick_Delay_Ms(500);
    HAL_PMU_GPIOPolaritySelect(PMU_WAKEUP_GPIOB_04, 1);
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_GPIOB_04);

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, GPIO_PMU_WAKEUP_FUN_SEL);

    // Disable RC32K
    __HAL_PMU_RC32K_DISABLE();

    //enter deep sleep
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    CLOGD("Wrong!!!! CPU should not get here!!");
}

static void* AON_TIMER_Handler = NULL;

static void AON_TIMER_Init_Handler(){
    AON_TIMER_Handler = AON_TIMER();
}

static void pmu_deepsleep_aontimer_wakeup(void) {
	AON_TIMER_Init_Handler();

	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();

	CLOGD("[AON_TIMER] Mode2 WakeUp cause -> %d", wakeup_cause);
	GetWakeupEvent(wakeup_cause);

	SysTick_Delay_Ms(500);

	if (wakeup_cause == PMU_WAKEUP_TIMER){
		CLOG("Aon_timer enter sleep mode again.\r\n");

		AON_TIMER_CLEAR_IRQ();

		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

	    //enter deep sleep
		HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
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

    // Enable wakeup source
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

    AON_TIMER_StartTimer(AON_TIMER_Handler);

    SysTick_Delay_Ms(500);

    //enter deep sleep
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    CLOGD("[AON_TIMER] Can't Entry SleepMode!!!!!");

}

static void* AON_WDT_Handler = NULL;

static void AON_WDT_Init_Handler(){
    AON_WDT_Handler = AON_WDT();
}

static void pmu_deepsleep_aonwdt_wakeup(void) {
	AON_WDT_Init_Handler();

	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();

	CLOGD("[AON_WDT] Mode2 WakeUp cause -> %d", wakeup_cause);

	if (wakeup_cause == PMU_WAKEUP_IWDT){
		CLOG("Aon_wdt enter sleep mode again.\r\n");
		// Clear interrupt source
		AON_WDT_CLEAR_IRQ();

		AON_WDT_Refresh(AON_WDT_Handler);

		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_IWDT);

		HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
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

	HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
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



static void pmu_deepsleep_calendar_alarm_calibration_wakeup(void) {
	CALENDAR_Init_Handler();

	CLOG("wakeup!!, source is 0x%x", IP_AON_CTRL->REG_WAKEUP_ISR.all);

	pmu_wakeupsrc_t wk_cause = HAL_PMU_GetWakeUpCause();

	if(wk_cause == PMU_WAKEUP_RTC) {
		CLOG("Calendar enter sleep mode again.\r\n");

		CSK_CALENDAR_TIME stime;

		CALENDAR_Initialize(CALENDAR_Handler, NULL, NULL);

		CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);

        CALENDAR_GetTime(CALENDAR_Handler, &stime);
        CLOGD("Time Years: %d; Month: %d; Day: %d; Week: %d; Hour: %d; Min: %d; Sec: %d",\
                stime.year, stime.month, stime.day, stime.weekend, stime.hour,\
                stime.min, stime.sec);

        CSK_CALENDAR_ALARM satime = {0};
        int_2_time((time_2_int(stime) + 30), &satime);

        CLOGD("Alarm Years: %d; Month: %d; Day: %d; Hour: %d; Min: %d; Sec: %d",\
        		satime.year, satime.month,  satime.day, satime.hour,\
				satime.min, satime.sec);

        log_flush();

        CALENDAR_SetAlarm(CALENDAR_Handler, &satime);
        CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

        HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
    	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

        __NOP();
        __NOP();
        __NOP();
        __NOP();

        CLOGD("Can't go here");
	}

    CSK_CALENDAR_TIME stime = {2, 3, 0, 15, 14, 30, 45};
    CSK_CALENDAR_ALARM satime = {0};

    int_2_time((time_2_int(stime) + 30), &satime);

    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);
    CALENDAR_SetTime(CALENDAR_Handler, &stime);
    CALENDAR_SetAlarm(CALENDAR_Handler, &satime);

    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_RTC);

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);
    // need add some delay
    for(uint32_t i = 0; i<0x5ffff; i++);

    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    CLOGD("Can't go here");
}

#define WIFI_RAM_ADDR		0x20000000
#define WIFI_RAM_SIZE		(256 * 1024)	//256K
#define BT_RAM_ADDR			0x200C0000
#define BT_RAM_SIZE			(32 * 1024)		//32K
#define CP_RAM_ADDR			0x20040000
#define CP_RAM_SIZE			(64 * 1024)		//64K

#define UNIT_SIZE			(32 * 1024)		//32K

static void pmu_deepsleep_ramretention(void) {
	AON_TIMER_Init_Handler();

	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();

	CLOGD("[AON_TIMER] Mode2 WakeUp cause -> %d", wakeup_cause);
	GetWakeupEvent(wakeup_cause);

	SysTick_Delay_Ms(500);

	uint32_t i;
	uint32_t *wifiRamAddr = (uint32_t *)WIFI_RAM_ADDR;
	uint32_t *btRamAddr = (uint32_t *)BT_RAM_ADDR;
	uint32_t *cpRamAddr = (uint32_t *)CP_RAM_ADDR;
	uint32_t wifiUnitCount = WIFI_RAM_SIZE / UNIT_SIZE;
	uint32_t btUnitCount = BT_RAM_SIZE / UNIT_SIZE;
	uint32_t cpUnitCount = CP_RAM_SIZE / UNIT_SIZE;

	if (wakeup_cause == PMU_WAKEUP_TIMER) {
//		CLOG("Aon_timer enter sleep mode again.\r\n");

		AON_TIMER_CLEAR_IRQ();

		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

//		CLOGD("[WIFI_RAM]Waking up and check RAM retention...");
	    for(int unit = 0; unit < wifiUnitCount; unit++) {
	        uint32_t *unitAddr = wifiRamAddr + (unit * UNIT_SIZE / sizeof(uint32_t));
	        for (i = 0; i < UNIT_SIZE / sizeof(uint32_t); i++) {
	            if (unitAddr[i] != (0xA5A5A5A5 + (unit * 1024 + i))) {
	                CLOGD("[WIFI_RAM]RAM retention failed at address 0x%08X: expected 0x%08X, got 0x%08X",
	                    (WIFI_RAM_ADDR + (unit * UNIT_SIZE) + i * sizeof(uint32_t)),
	                    (0xA5A5A5A5 + (unit * 1024 + i)),
						unitAddr[i]);
	                return;
	            }
	        }
	    }

//		CLOGD("[BT_RAM]Waking up and check RAM retention...");
	    for(int unit = 0; unit < btUnitCount; unit++) {
	        uint32_t *unitAddr = btRamAddr + (unit * UNIT_SIZE / sizeof(uint32_t));
	        for (i = 0; i < UNIT_SIZE / sizeof(uint32_t); i++) {
	            if (unitAddr[i] != (0x11223344 + (unit * 1024 + i))) {
	                CLOGD("[BT_RAM]RAM retention failed at address 0x%08X: expected 0x%08X, got 0x%08X",
	                    (BT_RAM_ADDR + (unit * UNIT_SIZE) + i * sizeof(uint32_t)),
	                    (0x11223344 + (unit * 1024 + i)),
						unitAddr[i]);
	                return;
	            }
	        }
	    }

//		CLOGD("[CP_RAM]Waking up and check RAM retention...");
	    for(int unit = 0; unit < cpUnitCount; unit++) {
	        uint32_t *unitAddr = cpRamAddr + (unit * UNIT_SIZE / sizeof(uint32_t));
	        for (i = 0; i < UNIT_SIZE / sizeof(uint32_t); i++) {
	            if (unitAddr[i] != (0x5A5A5A5A + (unit * 1024 + i))) {
	                CLOGD("[CP_RAM]RAM retention failed at address 0x%08X: expected 0x%08X, got 0x%08X",
	                    (CP_RAM_ADDR + (unit * UNIT_SIZE) + i * sizeof(uint32_t)),
	                    (0x5A5A5A5A + (unit * 1024 + i)),
						unitAddr[i]);
	                return;
	            }
	        }
	    }

	    CLOGD("RAM retention passed, all data intact.\r\n");

	    //enter deep sleep
	    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
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

    // Enable wakeup source
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

    AON_TIMER_StartTimer(AON_TIMER_Handler);

    SysTick_Delay_Ms(500);

	// init wifi_ram area
	CLOGD("[WIFI_RAM]Initialing WIFI_RAM area...");
	for(int unit = 0; unit < wifiUnitCount; unit++) {
		// calculate 32K starting address
		uint32_t *uintAddr =  wifiRamAddr + (unit * UNIT_SIZE / sizeof(uint32_t));
		for(i = 0; i < UNIT_SIZE / sizeof(uint32_t); i++) {
			uintAddr[i] = 0xA5A5A5A5 + (unit * 1024 + i);
		}
		HAL_PMU_EnableRamRetention(PMU_WIFI_SUB_RAMBANK0 + unit);
	}

	// init bt_ram area
	CLOGD("[BT_RAM]Initialing BT_RAM area...");
	for(int unit = 0; unit < btUnitCount; unit++) {
		// calculate 32K starting address
		uint32_t *uintAddr =  btRamAddr + (unit * UNIT_SIZE / sizeof(uint32_t));
		for(i = 0; i < UNIT_SIZE / sizeof(uint32_t); i++) {
			uintAddr[i] = 0x11223344 + (unit * 1024 + i);
		}
		HAL_PMU_EnableRamRetention(PMU_BT_RAMBANK0);
	}

	// init cp_ram area
	CLOGD("[CP_RAM]Initialing CP_RAM area...");
	for(int unit = 0; unit < cpUnitCount; unit++) {
		// calculate 32K starting address
		uint32_t *uintAddr =  cpRamAddr + (unit * UNIT_SIZE / sizeof(uint32_t));
		for(i = 0; i < UNIT_SIZE / sizeof(uint32_t); i++) {
			uintAddr[i] = 0x5A5A5A5A + (unit * 1024 + i);
		}
		HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK0 + unit);
	}

    //enter deep sleep
	HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

    __NOP();
    __NOP();
    __NOP();
    __NOP();

    CLOGD("[AON_TIMER] Can't Entry SleepMode!!!!!");
}

#define AP_TIMER_INTERVAL		5	// ap_run 5s
#define CP_TIMER_INTERVAL		10	// cp_run 10s
#define CPU_FREQ_MHZ			300

void pmu_dual_core_wfi_sleep(void) {

#if (BOOT_HARTID == 0)
	CLOG("Initializing AP task...\n");
	CLOG("AP Core: Timer started for %d seconds\n", AP_TIMER_INTERVAL);

	uint64_t cycles_prev = __get_rv_cycle();
	uint64_t cycles_curr = 0;
	uint64_t target_cycles = CPU_FREQ_MHZ * 1e6 * AP_TIMER_INTERVAL;

	while(1) {
		cycles_curr = __get_rv_cycle();
		if(cycles_curr - cycles_prev >= target_cycles) {
			CLOG("AP Core: Timer expired. Entering WFI...\n");
			IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30010000;
			IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
			HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_NONE);
			HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);

			while(1);
		}
	}

#else
	CLOG("Initializing CP task...\n");
	CLOG("CP Core: Timer started for %d seconds\n", CP_TIMER_INTERVAL);

	uint64_t cycles_prev = __get_rv_cycle();
	uint64_t cycles_curr = 0;
	uint64_t target_cycles = CPU_FREQ_MHZ * 1e6 * CP_TIMER_INTERVAL;

    while (1) {
        cycles_curr = __get_rv_cycle();
        if (cycles_curr - cycles_prev >= target_cycles) {
            CLOG("CP Core: Timer expired. Entering WFI...\n");
            HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_NONE);
        	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);
        }
    }

#endif
}

void pmu_ap_core_sleep(void) {
#if (BOOT_HARTID == 0)
	CLOG("Initializing AP task...\n");
	//switch system clk 24m
	IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 0; //osc24m
	//close system pll
	IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_ENABLE = 0;

	HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE1, PMU_DEEPSLEEPENTRY_WFI); //4.5mA

	while(1);
#endif
}

typedef void (*function)(void);

typedef struct {
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
//		{pmu_deepsleep, "pmu_deepsleep"},
//		{pmu_deepsleep_pbx_wakeup_high, "pmu_deepsleep_pbx_wakeup_high"},
//		{pmu_deepsleep_pbx_wakeup_low, "pmu_deepsleep_pbx_wakeup_low"},
//		{pmu_deepsleep_aontimer_wakeup, "pmu_deepsleep_aontimer_wakeup"},
//		{pmu_deepsleep_aonwdt_wakeup, "pmu_deepsleep_aonwdt_wakeup"},
		{pmu_deepsleep_calendar_alarm_calibration_wakeup, "pmu_deepsleep_calendar_alarm_calibration_wakeup"},
//		{pmu_deepsleep_ramretention, "pmu_deepsleep_ramretention"},
//		{pmu_dual_core_wfi_sleep, "pmu_dual_core_wfi_sleep"},
//		{pmu_ap_core_sleep, "pmu_ap_core_sleep"},
};


int main(void) {
	logInit(0, 115200);
	CLOG("enter main: pmu test\r\n");
	uint32_t index;
	for(index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++) {
		test_case_t item = test_array[index];
		CLOG("test case : %s", item.name);
		item.function();
	}

	while(1);

	return 0;
}
