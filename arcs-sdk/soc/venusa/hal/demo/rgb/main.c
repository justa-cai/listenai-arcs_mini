/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "venusa_ap.h"
#include "cache.h"
#include "log_print.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <event_groups.h>
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_DUAL_TIMER.h"

#define RGB_IMAGE_WIDTH   800
#define RGB_IMAGE_HEIGHT  480

#define LOG_MESSAGE_MAX_LENGTH 256

typedef struct {
    char message[LOG_MESSAGE_MAX_LENGTH];
} LogMessage_t;

#undef  CLOGD
#define CLOGD(fmt, ...) do { \
   LogMessage_t logMessage; \
   if (snprintf(logMessage.message, LOG_MESSAGE_MAX_LENGTH, fmt, ##__VA_ARGS__) > 0) { \
       xQueueSend(logQueue, &logMessage, 0); \
   } \
} while (0)

TaskHandle_t hLogTask;
TaskHandle_t hLCDTask;
QueueHandle_t logQueue;
SemaphoreHandle_t lcdSemaphore;  // LCD DMA done flag

#define _PSRAM_DATA             __attribute__ ((aligned(4), section (".psram_bss")))

_PSRAM_DATA uint8_t lcd_buf0[RGB_IMAGE_WIDTH * RGB_IMAGE_HEIGHT * 2];  // RGB565
_PSRAM_DATA uint8_t lcd_buf1[RGB_IMAGE_WIDTH * RGB_IMAGE_HEIGHT * 2];  // RGB565

extern int32_t test_rgb_init(uint8_t *image_buf, SemaphoreHandle_t Semaphore);
extern void test_rgb_buf_update(uint8_t *image_buf);
extern void rgb565_colorbar_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t bar_height);
extern void rgb565_grid_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t grid_height);


void log_task(void *pvParameters)
{
    LogMessage_t logMessage;

    while (1) {
        if (xQueueReceive(logQueue, &logMessage, portMAX_DELAY) == pdPASS) {
            CLOG("%s, logging by core-%d", logMessage.message, (__RV_CSR_READ(CSR_MHARTID) & 0xFF));
        }
        
        static uint32_t tick_prev, tick_curr = 0;
        tick_curr = xTaskGetTickCount();
//        if (tick_curr - tick_prev > 1000) {
//            tick_prev = tick_curr;
//            CLOG("Free Heap: %d bytes\n", xPortGetFreeHeapSize());
//
//            uint64_t ulTotalRunTime = 0;
//            uint32_t ulTotalTask = uxTaskGetNumberOfTasks();
//            TaskStatus_t *pxTaskStatusArray = pvPortMalloc(ulTotalTask * sizeof(TaskStatus_t));
//
//            uxTaskGetSystemState(pxTaskStatusArray, ulTotalTask, &ulTotalRunTime);
//            for (int i=0; i < ulTotalTask; i++) {
//                float cpuUsage = (pxTaskStatusArray[i].ulRunTimeCounter * 100.0) / ulTotalRunTime;
////                int usage = (int)(cpuUsage * 10);
////                CLOG("Task: %s, CPU Usage: %d.%d%%\n", pxTaskStatusArray[i].pcTaskName, (int)(usage/10), (int)(usage % 10));
//                CLOG("Task: %s, CPU Usage: %.1f%%\n", pxTaskStatusArray[i].pcTaskName, cpuUsage);
//            }
//            vPortFree(pxTaskStatusArray);
//        }
    }
}


void lcd_task(void* pvParameters)
{
    uint32_t cnt = 0;
    uint8_t *raw_image_in_use = NULL;
    uint32_t image_size_byte = RGB_IMAGE_WIDTH * RGB_IMAGE_HEIGHT;

    CLOG("Enter to lcd_task\r\n");

    memset(lcd_buf0, 0, sizeof(lcd_buf0));
    HAL_FlushDCache_by_Addr((uint32_t*)lcd_buf0, sizeof(lcd_buf0));

    test_rgb_init(lcd_buf0, lcdSemaphore);

    while(1) {
        if (xSemaphoreTake(lcdSemaphore, portMAX_DELAY) == pdTRUE) {
            CLOG("lcd_task %d", cnt++);

            if((cnt % 60) == 0) {
                rgb565_colorbar_create((uint16_t *)lcd_buf0, RGB_IMAGE_WIDTH, RGB_IMAGE_HEIGHT, 40);
                rgb565_grid_create((uint16_t *)lcd_buf0, RGB_IMAGE_WIDTH, RGB_IMAGE_HEIGHT, 80);
                HAL_FlushDCache_by_Addr((uint32_t*)lcd_buf0, sizeof(lcd_buf0));
                CLOG("[%s:%d] lcd_buf0=0x%x", __func__, __LINE__, lcd_buf0);
                test_rgb_buf_update(lcd_buf0);
            } else if((cnt % 30) == 0) {
                rgb565_colorbar_create((uint16_t *)lcd_buf1, RGB_IMAGE_WIDTH, RGB_IMAGE_HEIGHT, 60);
                rgb565_grid_create((uint16_t *)lcd_buf1, RGB_IMAGE_WIDTH, RGB_IMAGE_HEIGHT, 80);
                HAL_FlushDCache_by_Addr((uint32_t*)lcd_buf1, sizeof(lcd_buf1));
                CLOG("[%s:%d] lcd_buf1=0x%x", __func__, __LINE__, lcd_buf1);
                test_rgb_buf_update(lcd_buf1);
            }
        }

        //vTaskDelay(1);
    }
}


int main( void )
{
    logInit(0, 921600);

    GPIO_Initialize(GPIOA(), NULL, NULL);
    GPIO_Initialize(GPIOB(), NULL, NULL);

    CLOG("RGB LCD Demo");
    CLOG("lcd_buf0=0x%x size=0x%x", lcd_buf0, sizeof(lcd_buf0));
    CLOG("lcd_buf1=0x%x size=0x%x", lcd_buf1, sizeof(lcd_buf1));

    BaseType_t xResult;

    logQueue = xQueueCreate(20, sizeof(LogMessage_t) + 16);
    if (logQueue == NULL) {
        CLOG("failed to create log queue.\n");
    }

    if (NULL == (lcdSemaphore = xSemaphoreCreateBinary())) {
        CLOG("failed to create lcd semaphore.\n");
    }

    // Create the log task without affinity. So it can run on any core.
    xResult = xTaskCreate(
            log_task,  /* The function that implements the task. */
            "LogTask", /* Text name for the task. */
            512,       /* Stack depth in words. */
            NULL,      /* Task parameters. */
            1,         /* Priority and mode (user in this case). */
            &hLogTask  /* Handle. */
        );

    if (xResult != pdPASS) {
        CLOG("failed to create LogTask.\n");
    }

    xResult = xTaskCreate((TaskFunction_t)lcd_task, (const char*)"lcd_task",
                (uint16_t)1024, (void*)NULL, (UBaseType_t)2,
                &hLCDTask);
    if( xResult != pdPASS ) {
        CLOG("failed to create lcd_task.\n");
    } else {
        vTaskCoreAffinitySet(hLCDTask, (1 << 0)); // set affinity to core0
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
    // for SMP system, we need to enable the non-cacheable region for shared memory
    non_cacheable_region_enable_0(0x20000000, 0x00040000); // 256KB

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
