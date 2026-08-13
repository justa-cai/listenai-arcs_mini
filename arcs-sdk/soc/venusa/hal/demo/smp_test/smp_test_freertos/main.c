/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "venusa_ap.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include "Driver_DUAL_TIMER.h"


#define LOG_MESSAGE_MAX_LENGTH 128

typedef struct {
    char message[LOG_MESSAGE_MAX_LENGTH];
} LogMessage_t;

static QueueHandle_t logQueue;
TaskHandle_t hTestTask1, hTestTask2, hLogTask;
static SemaphoreHandle_t sema_from_timer;

#undef  CLOGD
#define CLOGD(fmt, ...) do { \
   LogMessage_t logMessage; \
   snprintf(logMessage.message, LOG_MESSAGE_MAX_LENGTH, fmt, ##__VA_ARGS__); \
   xQueueSend(logQueue, &logMessage, 0); \
} while (0)


static void log_task(void *pvParameters)
{
    LogMessage_t logMessage;

    while (1) {
        if (xQueueReceive(logQueue, &logMessage, portMAX_DELAY) == pdPASS) {
            CLOG("%s, logging by core-%d", logMessage.message, (__RV_CSR_READ(CSR_MHARTID) & 0xFF));
        }
        
        static uint32_t tick_prev, tick_curr = 0;
        tick_curr = xTaskGetTickCount();
        if (tick_curr - tick_prev > 1000) {
            tick_prev = tick_curr;
            CLOG("Free Heap: %d bytes\n", xPortGetFreeHeapSize());

            uint64_t ulTotalRunTime = 0;
            uint32_t ulTotalTask = uxTaskGetNumberOfTasks();
            TaskStatus_t *pxTaskStatusArray = pvPortMalloc(ulTotalTask * sizeof(TaskStatus_t));

            uxTaskGetSystemState(pxTaskStatusArray, ulTotalTask, &ulTotalRunTime);
            for (int i=0; i < ulTotalTask; i++) {
                float cpuUsage = (pxTaskStatusArray[i].ulRunTimeCounter * 100.0) / ulTotalRunTime;
                CLOG("Task: %s, CPU Usage: %.2f%%\n", pxTaskStatusArray[i].pcTaskName, cpuUsage);
            }
            vPortFree(pxTaskStatusArray);
        }
    }
}


static void* DUAL_TIMER_Handler = NULL;

static void DUAL_TIMER_Init_Handler(){
    DUAL_TIMER_Handler = DUALTIMERS0();
}

/*Global control macro*/
#define DUAL_TIMER_TEST_CHANNEL   (CSK_TIMER_CHANNEL_0)
#define DUAL_TIMER_RELOAD_COUNT   (1600)

/*Callback*/
static void DUAL_TIMER_EventCallback(uint32_t event, void* workspace){
    BaseType_t taskWoken = pdFALSE;

    xSemaphoreGiveFromISR(sema_from_timer, &taskWoken);
    portYIELD_FROM_ISR(taskWoken);
}

static void DUAL_TIMER_Interrupt_Periodic(void){
    DUALTIMERS_Initialize(DUAL_TIMER_Handler);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

    DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_Periodic | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

    DUALTIMERS_SetTimerPeriodByCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_RELOAD_COUNT);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

}

static void test_task1( void *pvParameters )
{
    CLOG("enter test_task1\n");

    int cnt_1 = 0;

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(50));
        CLOGD(" Core-%u: %s, %d", (__RV_CSR_READ(CSR_MHARTID) & 0xFF), __FUNCTION__, cnt_1++);
    }
}


static void test_task2( void *pvParameters )
{
    CLOG("enter test_task2\n");

    int cnt_2 = 0;
    DUAL_TIMER_Init_Handler();
    DUAL_TIMER_Interrupt_Periodic();
    ECLIC_SetLevelIRQ(IRQ_CMN_TIMER0_VECTOR, 2);

    while(1) {
        if( xSemaphoreTake(sema_from_timer, 1000) == pdTRUE )
        	CLOGD(" Core-%u: %s, %d", (__RV_CSR_READ(CSR_MHARTID) & 0xFF), __FUNCTION__, cnt_2++);
        else
        	CLOGD(" Time out. Core-%u: %s, %d", (__RV_CSR_READ(CSR_MHARTID) & 0xFF), __FUNCTION__, cnt_2++);
    }
}


int main( void )
{
    logInit(0, 921600);

    CLOG("FreeRTOS SMP Test");

    BaseType_t xResult;

    sema_from_timer = xSemaphoreCreateBinary();
    if (sema_from_timer == NULL) {
        CLOG("failed to create semaphore.\n");
    }

    logQueue = xQueueCreate(10, sizeof(LogMessage_t) + 16);
    if (logQueue == NULL) {
        CLOG("failed to create log queue.\n");
    }

    // Create the log task with affinity. So it can only run on the selected core.
    // xResult = xTaskCreateAffinitySet(
    //     log_task,  /* The function that implements the task. */
    //     "LogTask", /* Text name for the task. */
    //     256,       /* Stack depth in words. */
    //     NULL,      /* Task parameters. */
    //     2,         /* Priority and mode (user in this case). */
    //     (1<<1),    /* Affinity. Set to core 1. */
    //     &hLogTask  /* Handle. */
    // );

    // Create the log task without affinity. So it can run on any core.
    xResult = xTaskCreate(
            log_task,  /* The function that implements the task. */
            "LogTask", /* Text name for the task. */
            512,       /* Stack depth in words. */
            NULL,      /* Task parameters. */
            2,         /* Priority and mode (user in this case). */
            &hLogTask  /* Handle. */
        );

    if (xResult != pdPASS) {
        CLOG("failed to create LogTask.\n");
    }
    

    // xResult = xTaskCreateAffinitySet(
    //         test_task1,    /* The function that implements the task. */
    //         "Task1",       /* Text name for the task. */
    //         512,           /* Stack depth in words. */
    //         NULL,          /* Task parameters. */
    //         3,             /* Priority and mode (user in this case). */
    //         (1<<0),        /* Affinity. Set to core 0. */
    //         &hTestTask1    /* Handle. */
    //     );

    xResult = xTaskCreate(
            test_task1,    /* The function that implements the task. */
            "Task1",       /* Text name for the task. */
            512,           /* Stack depth in words. */
            NULL,          /* Task parameters. */
            3,             /* Priority and mode (user in this case). */
            &hTestTask1    /* Handle. */
        );

    if( xResult != pdPASS ) {
        CLOG("failed to create Task1.\n");
    }


    // xResult = xTaskCreateAffinitySet(
    //         test_task2,    /* The function that implements the task. */
    //         "Task2",       /* Text name for the task. */
    //         512,           /* Stack depth in words. */
    //         NULL,          /* Task parameters. */
    //         3,             /* Priority and mode (user in this case). */
    //         (1<<1),        /* Affinity. Set to core 1. */
    //         &hTestTask2    /* Handle. */
    //     );
    xResult = xTaskCreate(
            test_task2,    /* The function that implements the task. */
            "Task2",       /* Text name for the task. */
            512,           /* Stack depth in words. */
            NULL,          /* Task parameters. */
            3,             /* Priority and mode (user in this case). */
            &hTestTask2    /* Handle. */
        );

    if( xResult != pdPASS ) {
        CLOG("failed to create Task2.\n");
    }

    /* Start the Core-1 */
    extern uint32_t _start;
    start_core1((uint32_t)&_start);


    /* Start the scheduler. */
    vTaskStartScheduler();

    /* Will only get here if there was insufficient memory to create the idle
    task. */
    for( ;; );
}


void eclic_inter_core_int_handler()
{
    uint32_t sender_id = 0;
    unsigned long hartid = __get_hart_id();

    uint32_t val = CIDU_QueryCoreIntSenderMask(hartid);

    if(0 != val) {
        CIDU_ClearInterCoreIntReq(hartid == 0 ? 1 : 0, hartid);
        SysTimer_SetHartSWIRQ(hartid);
    }
}


void smp_main(void)
{
    if(__RV_CSR_READ(CSR_MHARTID) & 0xFF)
    {
        disable_GINT();
        CIDU_ClearInterCoreIntReq(0, 1);
        clear_IRQ(IRQ_IDU_VECTOR);
        enable_IRQ(IRQ_IDU_VECTOR);
    
        xPortStartScheduler();
    }
    else
    {
        disable_GINT();
        CIDU_ClearInterCoreIntReq(1, 0);
        clear_IRQ(IRQ_IDU_VECTOR);
        register_ISR(IRQ_IDU_VECTOR, (void*)eclic_inter_core_int_handler, NULL);
        enable_IRQ(IRQ_IDU_VECTOR);
    
        main();
    }
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
    CLOG("malloc failed\n");
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
    CLOG("Stack Overflow\n");
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
