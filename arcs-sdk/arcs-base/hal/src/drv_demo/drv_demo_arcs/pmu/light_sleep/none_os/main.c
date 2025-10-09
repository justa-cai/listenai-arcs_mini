#include "stdio.h"
#include <string.h>
#include "log_print.h"
#include "systick.h"
#include "chip.h"

#include "IOMuxManager.h"
#include "PowerManager.h"
#include "Driver_AON_TIMER.h"
#include "spiflash.h"
#include "PowerManager.h"
#include "ClockManager.h"

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_FLASH       (0xBB)

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

#define AON_TIMER_CLEAR_IRQ()	\
do{	\
	IP_AON_TIMER->REG_OS_TIMER_IRQ_CLR.all = 0x1;	\
	while(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);	\
}while(0)

_CP_RAM_DATA static volatile uint32_t glb_value_cp_ram = 0;
_CP_RAM_DATA static volatile uint8_t software_lock = 0;

void enable_wakeup_jump(uint32_t wakeup_addr, uint32_t wakeup_action) {
	// Set the wakeup action
	IP_AON_CTRL->REG_AON_DIG_RSVD0.all = wakeup_action;

	// Set the wakeup address
	IP_AON_CTRL->REG_AON_DIG_RSVD1.all = wakeup_addr;
}

_CP_RAM_TEXT void software_handler(void){
    *(uint32_t*)(0xe0031000 - 4) = 0;

    software_lock = 1;

    CLOGD("In software hook");
}

_CP_RAM_TEXT void Fast_init_function(void){
	logInit(0, 115200);

	CLOGD("[%s, %d]", __func__, __LINE__);
	CLOGD("Hello World!\r\n");

    // enable ICache
    EnableICache();
    __RWMB();
    __FENCE_I();

    extern volatile uint32_t SystemCoreClock;
    SystemCoreClock = CRM_GetCpuFreq();

    extern void ECLIC_Init(void);
    ECLIC_Init();

    extern void BootClock_Init();
    BootClock_Init();

    extern void irq_vectors_init(void);
    irq_vectors_init();

    main();
}

extern void _light_sleep_entry(void);

int main(void) {
//	SystemInit_Copy();

    logInit(0, 115200);

    // Enable global interrupt
    enable_GINT();

    register_ISR(IRQ_Software_VECTOR, software_handler, NULL);
    enable_IRQ(IRQ_Software_VECTOR);

    pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
//    CLOGD("%d, %d",wakeup_cause, __LINE__);
    GetWakeupEvent(wakeup_cause);

    HAL_PMU_ClearWakeUpCause();

    if (wakeup_cause == PMU_WAKEUP_TIMER) {
        // Software interrupt, need remove system_RISCVN300.c PMP_INIT
        *(uint32_t*)(0xe0031000 - 4) = 1;

        AON_TIMER_CLEAR_IRQ();

        glb_value_cp_ram++;
        CLOGD("[%s, %d] -> %d", __func__, __LINE__, glb_value_cp_ram);

        while (!software_lock);
        software_lock = 0;

	    CLOG("enter sleep!\r\n");
	    CLOG_FLUSH();

        HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);
	    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI); //switch mode
        
        __NOP();
        __NOP();
        __NOP();
        __NOP();
 
        CLOGD("[AON_TIMER] Can't Entry SleepMode!!!!!");
    } else {
        CLOGD("Not in aon timer wakeup[%s, %d]", __func__, __LINE__);
    }
    
    AON_TIMER_Initialize(AON_TIMER(), NULL, NULL);
    AON_TIMER_PowerControl(AON_TIMER(), CSK_POWER_FULL);
    AON_TIMER_Control(AON_TIMER(), HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled);

    AON_TIMER_SetTimerPeriodByCount(AON_TIMER(), 32000);

    // Enable wakeup source
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

    AON_TIMER_StartTimer(AON_TIMER());

    CLOGD("Configure light sleep 0x%x", (uint32_t)&_light_sleep_entry);

    CLOG_FLUSH();

    // Ramretention
    HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK0);
    HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK1);

    // Attention
    enable_wakeup_jump((uint32_t)&_light_sleep_entry, WAKEUP_ACT_JUMP_RAM);

//    enable_wakeup_jump((uint32_t)&_start, WAKEUP_ACT_JUMP_RAM);


    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI); //switch mode

    while (1);
    
}
