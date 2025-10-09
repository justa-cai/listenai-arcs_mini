/*
 * dma_chk_nos.c
 *
 *  Created on: Jul 22, 2020
 *
 *  Revision:
 *
 */
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "log_print.h"
#include "chip.h"

// module: ic
#include "ic.h"
#include "ic_mutex.h"
#include "ic_fence.h"
#include "ic_proxy.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#ifdef CONFIG_QEMU_N300
#define CLOGD
#endif

//------------------------------------------------
// 工具

#define ASSERT(a) \
    if ((a) != true) { \
        CLOGD("ASSERT: %s", __func__); \
        while(1); \
    }

//--------------------------------------------------------------

//#define CLOGD(fmt, ...)

// \boottask------------------------------------------------------------

extern int g_mbx_isr_entry_counter;
extern int g_mbx_irq_event_counter;

// 全局: 核间资源.
IC_Mutex g_ic_mutex;

// 全局: 核间资源. ICFence id: 1
static ICFenceHandle ic_fence_1;

volatile int *gPtrCounter = (int *) 0xdeaddead;

void
boot_task(void *param)
{
    int ret;

    // 多核系统初始化
    ret = IC_System_initialize();
    ASSERT(ret == IC_OK);

    // 多核系统同步barrier
    ret = IC_System_sync();
    ASSERT(ret == IC_OK);
    //--------------------------------------------------------------

    /* ~~~全局约定, 勿删~~~ */
    /* 全局 核间原语 占用约定, 由各模块约定占用核间原语. */
    /* AP从机 创建: 核间原语对象. */
    {
        //--------------------------------------------------------------
        // ICFence: 数量按需配置

        // ICFence id:0 - IC_Mutex 实现内部已占用

        // ICFence id:1 - 本测试程序使用
        // 此时server端的ICFence对象已就绪
        ic_fence_1 = (ICFenceHandle) IC_Proxy_getRemoteFence(1);

        //--------------------------------------------------------------
        // IC_Stream: 数量按需配置

        //--------------------------------------------------------------
        // IC_Mutex: 共16个, id:0~15

        // IC_Mutex id:0 - 本测试程序使用
        ret = IC_Mutex_init(&g_ic_mutex, IC_MUTEX_SLEEP_WAIT, 0);
        ASSERT(ret == IC_OK);
    }

    // 对象级的核间同步, 以确认对端已经就绪.
    ICFence_syncWithRemote(ic_fence_1);
    ICFence_wait(ic_fence_1, (uint32_t*) &gPtrCounter);

    for (int i = 0; i < 1000000; ++i) {
        ret = IC_Mutex_acquire(&g_ic_mutex);
        ASSERT(ret == IC_OK);

        // 默认: memory_order_seq_cst
        (*gPtrCounter) ++;

        // atomic_fetch_add_explicit(&gCounter, 1, memory_order_seq_cst);

        ret = IC_Mutex_release(&g_ic_mutex);
        ASSERT(ret == IC_OK);
    }

    CLOGD("gCounter == %d\n", *gPtrCounter);

    vTaskSuspend(NULL);
}

//--------------------------------------------------------------
int main (void)
{
#ifndef CONFIG_QEMU_N300
    logInit(0, 115200);
#endif
    CLOGD("enter main\n");

    CLOGD("gPtrCounter @ %p\n", gPtrCounter);

#ifdef CONFIG_TRACE
    xTraceEnable(TRC_START);
#endif

    BaseType_t result;

    result = xTaskCreate(boot_task,   /* The function that implements the task. */
                "boot_task",                 /* Text name for the task. */
                2048,                       /* Stack depth in words. */
                NULL,                       /* Task parameters. */
                10,                         /* Priority and mode (user in this case). */
                NULL                        /* Handle. */
            );
    ASSERT(pdPASS == result);

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* Will only get here if there was insufficient memory to create the idle
    task. */
    for( ;; );

    return 0;
}

QueueHandle_t xGlobalScopeCheckQueue = NULL;
#define configPRINT_SYSTEM_STATUS			3

void vApplicationTickHook( void )
{
    static uint32_t ulCallCount = 0;
    const uint32_t ulCallsBetweenSends = pdMS_TO_TICKS( 1000 );
    const uint32_t ulMessage = configPRINT_SYSTEM_STATUS;
    portBASE_TYPE xDummy;

    /* If configUSE_TICK_HOOK is set to 1 then this function will get called
    from each RTOS tick.  It is called from the tick interrupt and therefore
    will be executing in the privileged state. */

    ulCallCount++;

    /* Is it time to print out the pass/fail message again? */
    if( ulCallCount >= ulCallsBetweenSends )
    {
        ulCallCount = 0;

        /* Send a message to the check task to command it to check that all
        the tasks are still running then print out the status.

        This is running in an ISR so has to use the "FromISR" version of
        xQueueSend().  Because it is in an ISR it is running with privileges
        so can access xGlobalScopeCheckQueue directly. */
        xQueueSendFromISR( xGlobalScopeCheckQueue, &ulMessage, &xDummy );
    }
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    /* If configCHECK_FOR_STACK_OVERFLOW is set to either 1 or 2 then this
    function will automatically get called if a task overflows its stack. */
    ( void ) pxTask;
    ( void ) pcTaskName;
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* If configUSE_MALLOC_FAILED_HOOK is set to 1 then this function will
    be called automatically if a call to pvPortMalloc() fails.  pvPortMalloc()
    is called automatically when a task, queue or semaphore is created. */
    for( ;; );
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

