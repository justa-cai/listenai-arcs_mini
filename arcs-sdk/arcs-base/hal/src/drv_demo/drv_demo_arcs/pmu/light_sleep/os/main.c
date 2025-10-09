#include "stdio.h"
#include <string.h>
#include "log_print.h"
#include "systick.h"
#include "chip.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "FreeRTOSConfig.h"
#include "log_print.h"

#include "PowerManager.h"
#include "Driver_AON_TIMER.h"

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_FLASH       (0xBB)

#define AON_TIMER_CLEAR_IRQ()	\
do{	\
	IP_AON_TIMER->REG_OS_TIMER_IRQ_CLR.all = 0x1;	\
	while(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);	\
}while(0)


_CP_RAM_DATA static volatile uint32_t glb_value = 0;
static volatile pmu_wakeupsrc_t wakeup_cause = PMU_WAKEUP_NONE;
static volatile uint32_t tick_tick = 0;
static volatile uint8_t can_sleep = 0;

__attribute__((used))_CP_RAM_DATA volatile uint32_t __stack_store_repo = 0;


void enable_wakeup_jump(uint32_t wakeup_addr, uint32_t wakeup_action) {
	// Set the wakeup action
	IP_AON_CTRL->REG_AON_DIG_RSVD0.all = wakeup_action;

	// Set the wakeup address
	IP_AON_CTRL->REG_AON_DIG_RSVD1.all = wakeup_addr;
}

_CP_RAM_TEXT void Fast_init_function(void) {
    extern void ECLIC_Init(void);
    ECLIC_Init();

    extern void irq_vectors_init(void);
    irq_vectors_init();

    wakeup_cause = HAL_PMU_GetWakeUpCause();
    HAL_PMU_ClearWakeUpCause();

    AON_TIMER_CLEAR_IRQ();

    extern void __idle_restore(void);
    __idle_restore();
}


static void print_task( void *pvParameters )
{
	TickType_t the_tick = 0;

	CLOGD("Enter task");

	vTaskDelay(3000);

	while(1) {
		the_tick = xTaskGetTickCount();

		CLOGD("In print task: %d, %d, %d", glb_value++, (uint32_t)the_tick, __LINE__);

		CLOG_FLUSH();

		can_sleep = 1;

		vTaskDelay(1000);
	}
}

int main(void)
{
	//SystemInit_Copy();

	// enable global interrupt
	enable_GINT();

	logInit(0, 115200);
	CLOGD("Enter main\n");

	xTaskCreate(
			print_task,					/* The function that implements the task. */
			"print_task",				/* Text name for the task. */
			configMINIMAL_STACK_SIZE,	/* Stack depth in words. */
			NULL,						/* Task parameters. */
			3,							/* Priority and mode (user in this case). */
			NULL						/* Handle. */
		);

	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1);
}

extern void __idle_store(void);

__attribute__((optimize("O0"))) void vApplicationIdleHook(void)
{
	if(can_sleep) {
		TickType_t the_tick = 0;

		can_sleep = 0;
		SysTimer_Stop();

		the_tick = xTaskGetTickCount();

		CLOGD("Pre-Idle task, %d", (uint32_t)the_tick);

	    __disable_irq();
        /* Make sure interrupt disable is executed */
        __RWMB();
        __FENCE_I();
        __NOP();

        /* Disable the SysTick clock.  Again,
        the time the SysTick is stopped for is accounted for as best it can
        be, but using the tickless mode will inevitably result in some tiny
        drift of the time maintained by the kernel with respect to calendar
        time*/
        ECLIC_DisableIRQ(SysTimer_IRQn);

        if(wakeup_cause == PMU_WAKEUP_NONE) {
        	extern void _light_sleep_entry(void);
        	enable_wakeup_jump((uint32_t)&_light_sleep_entry, WAKEUP_ACT_JUMP_RAM);

            // Ramretention
            HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK0);
            HAL_PMU_EnableRamRetention(PMU_CP_RAMBANK1);

            AON_TIMER_Initialize(AON_TIMER(), NULL, NULL);
            AON_TIMER_PowerControl(AON_TIMER(), CSK_POWER_FULL);
            AON_TIMER_Control(AON_TIMER(), HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled);

            AON_TIMER_SetTimerPeriodByCount(AON_TIMER(), 32000);

            AON_TIMER_StartTimer(AON_TIMER());
        }

        CLOG_FLUSH();

        // Enable wakeup source
        HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_TIMER);

        HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	    HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI); //no sleep

	    __idle_store();

	    BaseType_t yeild_value = 0;

	    yeild_value = xTaskCatchUpTicks(1000);

	    logInit(0, 115200);

        /* Restart SysTick */
        vPortSetupTimerInterrupt();

        the_tick = xTaskGetTickCount();

        CLOGD("Ext-Idle task %d, %d", (uint32_t)the_tick, (uint32_t)yeild_value);

        /* Exit with interrupts enabled. */
        ECLIC_EnableIRQ(SysTimer_IRQn);
        __enable_irq();
	}
}

void vApplicationTickHook(void){
	static uint32_t times = 0;

    if (tick_tick++ >= 100){
        tick_tick = 0;
        times++;

        CLOGD("tick hook, %d", times);

        CLOG_FLUSH();
    }
}

__attribute__((weak))
void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
     * configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     * function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();

    for( ; ; )
    {
    }
}

void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
     * configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
     * function that will get called if a call to pvPortMalloc() fails.
     * pvPortMalloc() is called internally by the kernel whenever a task, queue,
     * timer or semaphore is created.  It is also called by various parts of the
     * demo application.  If heap_1.c or heap_2.c are used, then the size of the
     * heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
     * FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
     * to query the size of free heap space that remains (although it does not
     * provide information on how the remaining heap might be fragmented). */
    taskDISABLE_INTERRUPTS();

    for( ; ; )
    {
    }
}
