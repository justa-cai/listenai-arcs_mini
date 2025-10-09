/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "chip.h"
#include <FreeRTOS.h>
#include <semphr.h>

#ifdef CONFIG_QEMU_N300
#define CLOGD
#endif


static SemaphoreHandle_t sema_task12;

static void test_task1( void *pvParameters )
{
	uint32_t i = 0;

	CLOGD("enter test_task1\n");
    static int cnt_1 = 0;
    while(1) {
    	vTaskDelay(pdMS_TO_TICKS(500));
        cnt_1++;
        CLOGD("%s: %d", __FUNCTION__, cnt_1);

        if((cnt_1 % 2) == 0) {
        	xSemaphoreGive(sema_task12);
        }

	}
}

static void test_task2( void *pvParameters )
{
	uint32_t i = 0;

	CLOGD("enter test_task2\n");
    static int cnt_2 = 0;
    while(1) {
    	xSemaphoreTake(sema_task12, -1);
        cnt_2++;
        CLOGD("%s: %d", __FUNCTION__, cnt_2);

	}
}


int main( void )
{
#ifndef CONFIG_QEMU_N300
	logInit(0, 115200);
#endif
	CLOGD("FreeRTOS Test");

	BaseType_t xResult;

	sema_task12 = xSemaphoreCreateBinary();
    if (sema_task12 == NULL) {
    	CLOGD("failed to create semaphore.\n");
    }


    xResult = xTaskCreate(
			test_task1,	/* The function that implements the task. */
			"Task1",					/* Text name for the task. */
			256,						/* Stack depth in words. */
			NULL,						/* Task parameters. */
			3,							/* Priority and mode (user in this case). */
			NULL						/* Handle. */
		);

    if( xResult != pdPASS ) {
    	CLOGD("failed to create Task1.\n");
    }

    xResult = xTaskCreate(
			test_task2,	/* The function that implements the task. */
			"Task2",					/* Text name for the task. */
			256,						/* Stack depth in words. */
			NULL,						/* Task parameters. */
			3,							/* Priority and mode (user in this case). */
			NULL						/* Handle. */
		);

    if( xResult != pdPASS ) {
    	CLOGD("failed to create Task2.\n");
    }
	/* Start the scheduler. */
	vTaskStartScheduler();

	/* Will only get here if there was insufficient memory to create the idle
	task. */
	for( ;; );
}


void vApplicationTickHook(void)
{
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* The RTOS tick hook function is enabled by setting configUSE_TICK_HOOK to
    1 in FreeRTOSConfig.h.

    "Give" the semaphore on every 500th tick interrupt. */

    /* If xHigherPriorityTaskWoken is pdTRUE then a context switch should
    normally be performed before leaving the interrupt (because during the
    execution of the interrupt a task of equal or higher priority than the
    running task was unblocked).  The syntax required to context switch from
    an interrupt is port dependent, so check the documentation of the port you
    are using.

    In this case, the function is running in the context of the tick interrupt,
    which will automatically check for the higher priority task to run anyway,
    so no further action is required. */
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* The malloc failed hook is enabled by setting
    configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

    Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
    CLOGD("malloc failed\n");
    while (1);
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    /* Run time stack overflow checking is performed if
    configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected.  pxCurrentTCB can be
    inspected in the debugger if the task name passed into this function is
    corrupt. */
    CLOGD("Stack Overflow\n");
    while (1);
}
/*-----------------------------------------------------------*/

//extern UBaseType_t uxCriticalNesting;
void vApplicationIdleHook(void)
{
    // volatile size_t xFreeStackSpace;
    /* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
    FreeRTOSConfig.h.

    This function is called on each cycle of the idle task.  In this case it
    does nothing useful, other than report the amount of FreeRTOS heap that
    remains unallocated. */
    /* By now, the kernel has allocated everything it is going to, so
    if there is a lot of heap remaining unallocated then
    the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
    reduced accordingly. */
}
