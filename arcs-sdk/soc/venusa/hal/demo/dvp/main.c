/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "venusa_ap.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <event_groups.h>
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_DUAL_TIMER.h"
#include "csk_dvp.h"
#include "csk_dma2d.h"

#define DVP_IMAGE_WIDTH         1280
#define DVP_IMAGE_HEIGHT        720
#define SCALER_IMAGE_WIDTH      640
#define SCALER_IMAGE_HEIGHT     240

#define LOG_MESSAGE_MAX_LENGTH  256

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
TaskHandle_t hDVPTask;
TaskHandle_t hDMA2DTask;
QueueHandle_t logQueue;
QueueHandle_t dvp2scalerQueue;
SemaphoreHandle_t dvpDoneSemaphore;  // DVP DMA done
SemaphoreHandle_t dvpErrSemaphore;   // DVP DMA error
SemaphoreHandle_t scalerDoneSemaphore;  // DMA2D done
SemaphoreHandle_t cropDoneSemaphore;  // DMA2D done

#define _PSRAM_DATA             __attribute__ ((aligned(4), section (".psram_bss")))

_PSRAM_DATA uint8_t dvp_buf0[DVP_IMAGE_WIDTH * DVP_IMAGE_HEIGHT * 2];  // YUV422
_PSRAM_DATA uint8_t dvp_buf1[DVP_IMAGE_WIDTH * DVP_IMAGE_HEIGHT * 2];  // YUV422
_PSRAM_DATA uint8_t dma2d_buf[SCALER_IMAGE_WIDTH * SCALER_IMAGE_HEIGHT * 2];  // YUV422

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


void dvp_task(void* pvParameters)
{
    uint32_t image_cnt = 0;
    uint8_t error_flag = 0;
    void *buf_addr = NULL;
    uint8_t done_num, used_num, unused_num;

    CLOG("Enter to dvp_task\r\n");

    /* Start DVP to receive frames */
    dvp_init(dvpDoneSemaphore, dvpErrSemaphore);
    dvp_buf_add(dvp_buf0);
    dvp_buf_add(dvp_buf1);

    while (1)
    {
        if (xSemaphoreTake(dvpDoneSemaphore, 0) == pdTRUE) {
            //dvp_buf_num_get(&done_num, &used_num, &unused_num);
            //CLOG("done=%d used=%d unused=%d", done_num, used_num, unused_num);

            dvp_buf_get(&buf_addr);
            CLOG("dvp get image buf=0x%x cnt=%d", buf_addr, image_cnt++);

            if (error_flag == 0) {
#if 1  // dma2d_task
                xQueueSend(dvp2scalerQueue, &buf_addr, 0);
#else
                dvp_buf_add(buf_addr);
#endif
            } else {
                dvp_buf_num_get(&done_num, &used_num, &unused_num);
                if ((done_num == 0) && (used_num == 0) && (unused_num == 0)) {
                    error_flag = 0;
                    image_cnt = 0;
                    dvp_buf_add(dvp_buf0);
                    dvp_buf_add(dvp_buf1);
                }
            }

#if 0       /* only for error test */
            if(image_cnt == 16) {
                vTaskDelay(50);
                IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 0);  // HS
                vTaskDelay(1000);
                IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 13);  // HS
            }
#endif
        }

        if ((xSemaphoreTake(dvpErrSemaphore, 0) == pdTRUE) && (error_flag == 0) && (image_cnt >= 5)) {
            error_flag = 1;
            CLOG("DVP error_flag");
        }
    }
}


void dma2d_task(void* pvParameters)
{
    csk_dma2d_scaler_crop_t cfg = {
            .img_width_in = DVP_IMAGE_WIDTH,
            .img_height_in = DVP_IMAGE_HEIGHT,
            .img_width_out = SCALER_IMAGE_WIDTH,
            .img_height_out = SCALER_IMAGE_HEIGHT,
            .crop_x = 0,
            .crop_y = 0,
            .img_buf_in = NULL,
            .img_buf_out = dma2d_buf,
    };

    CLOG("Enter to dma2d_task\r\n");

    while (1)
    {
        if(xQueueReceive(dvp2scalerQueue, &cfg.img_buf_in, portMAX_DELAY) == pdPASS) {
            CLOG("received dvp image 0x%x", cfg.img_buf_in);
            dma2d_scaler_start(scalerDoneSemaphore, &cfg);
        }

        if (xSemaphoreTake(scalerDoneSemaphore, portMAX_DELAY) == pdTRUE) {
            /* luna */
            cfg.crop_x = 100;
            cfg.crop_y = 100;

            dma2d_crop_start(cropDoneSemaphore, &cfg);
        }

        if (xSemaphoreTake(cropDoneSemaphore, portMAX_DELAY) == pdTRUE) {
            dvp_buf_add(cfg.img_buf_in);
            cfg.img_buf_in = NULL;
        }
    }
}


int main( void )
{
    logInit(0, 921600);

    GPIO_Initialize(GPIOA(), NULL, NULL);
    GPIO_Initialize(GPIOB(), NULL, NULL);

    CLOG("DVP Demo");
    CLOG("dvp_buf0=0x%x size=0x%x", dvp_buf0, sizeof(dvp_buf0));
    CLOG("dvp_buf1=0x%x size=0x%x", dvp_buf1, sizeof(dvp_buf1));
    CLOG("dma2d_buf=0x%x size=0x%x", dma2d_buf, sizeof(dma2d_buf));

    BaseType_t xResult;

    logQueue = xQueueCreate(20, sizeof(LogMessage_t) + 16);
    if (logQueue == NULL) {
        CLOG("failed to create log queue.\n");
    }

    dvp2scalerQueue = xQueueCreate(2, sizeof(uint32_t));
    if (dvp2scalerQueue == NULL) {
        CLOG("failed to create dvp to dma2d scaler queue.\n");
    }

    dvpDoneSemaphore = xSemaphoreCreateBinary();
    if (dvpDoneSemaphore == NULL) {
        CLOG("failed to create semaphore.\n");
    }

    dvpErrSemaphore = xSemaphoreCreateBinary();
    if (dvpErrSemaphore == NULL) {
        CLOG("failed to create semaphore.\n");
    }

    scalerDoneSemaphore = xSemaphoreCreateBinary();
    if (scalerDoneSemaphore == NULL) {
        CLOG("failed to create semaphore.\n");
    }

    cropDoneSemaphore = xSemaphoreCreateBinary();
    if (cropDoneSemaphore == NULL) {
        CLOG("failed to create semaphore.\n");
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

    xResult = xTaskCreate((TaskFunction_t)dvp_task, (const char*)"dvp_task",
                (uint16_t)512, (void*)NULL, (UBaseType_t)2,
                &hDVPTask);
    if( xResult != pdPASS ) {
        CLOG("failed to create dvp_task.\n");
    } else {
        vTaskCoreAffinitySet(hDVPTask, (1 << 0)); // set affinity to core0
    }

    xResult = xTaskCreate((TaskFunction_t)dma2d_task, (const char*)"dma2d_task",
                (uint16_t)512, (void*)NULL, (UBaseType_t)2,
                &hDMA2DTask);
    if( xResult != pdPASS ) {
        CLOG("failed to create dma2d_task.\n");
    } else {
        vTaskCoreAffinitySet(hDMA2DTask, (1 << 0)); // set affinity to core0
    }

    /* Start the Core-1 */
    extern uint32_t _start;
    //start_core1((uint32_t)&_start);

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
