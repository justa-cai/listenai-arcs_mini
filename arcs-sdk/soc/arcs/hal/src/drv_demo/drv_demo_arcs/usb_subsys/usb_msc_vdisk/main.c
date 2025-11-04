/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>

//#include "bsp/board.h"
//#include "tusb.h"
#include "main.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "PSRAMManager.h"

#define DEBUG_LOG   1 // 1
#if DEBUG_LOG
#define LOGD(fmt, ...)   CLOGD(fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)   ((void)0)
#endif // DEBUG_LOG

//JUST HERE!! bauldeng. 2023.3.13.

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

#define LED_PIN         16  // A16， FUNC=0, OUT

#define LED_STATE_ON    1
#define LED_STATE_OFF   0

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static void *gGpioDev = NULL;
static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void *arg);

uint32_t board_millis(void);
void board_led_write(bool state);

/*------------- MAIN -------------*/
int main(void)
{
    logInit(0, 115200); // uart0, baudrate=115200
    //logInit(2, 115200); // uart2, baudrate=115200

    __HAL_CRM_USB_CLK_ENABLE();

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; //16bit mode

    //BTN & LED GPIO configuration
    gGpioDev = GPIOA();
    GPIO_Initialize(gGpioDev, NULL, NULL);
    GPIO_SetDir(gGpioDev, (1UL << LED_PIN), CSK_GPIO_DIR_OUTPUT); // LED, OUT

    // disable cache for PSRAM
    //DisableICache();
    DisableDCache();
    __RWMB();
    __FENCE_I();

    // initialize PSRAM for disk storage
    PSRAM_Initialize(NULL, NULL, 1);
    LOGD("PSRAM CONFIG COMPLETE\n");

#if TUD_OPT_HIGH_SPEED
    LOGD("[%s:%d] USB2.0 HS, MSC_USER_EP_BUF = %d\n", __func__, __LINE__, MSC_USER_EP_BUF);  // high-speed
#else
    LOGD("[%s:%d] USB1.1 FS\n", __func__, __LINE__);  // full-speed 12Mbps
#endif

    if (CFG_TUD_MSC_EP_BUFSIZE >= 1024)
        LOGD("MSC EP_BUFSIZE = %dKB\n", CFG_TUD_MSC_EP_BUFSIZE / 1024);
    else
        LOGD("MSC EP_BUFSIZE = %dB\n", CFG_TUD_MSC_EP_BUFSIZE);

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host, i.e. require host to enumerate device...

//  while (1)
//  {
//    tud_task(); // tinyusb device task
//    led_blinking_task();
//  }

    BaseType_t ret;
    // create led_blinking task
    ret = xTaskCreate(led_blinking_task,
                    "led_blinking",
                    0x400,
                    NULL,
                    3,
                    NULL);
    TU_ASSERT (ret == pdPASS);

    vTaskStartScheduler();
    while(1); // don't exit...

    return 0;
}


/*-----------------------------------------------------------*/

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

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    /* If configCHECK_FOR_STACK_OVERFLOW is set to either 1 or 2 then this
    function will automatically get called if a task overflows its stack. */
    ( void ) pxTask;
    ( void ) pcTaskName;
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

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void *arg)
{
    (void)arg;
    static uint32_t start_ms = 0;
    static bool led_state = false;

    while(1) {
        // Blink every interval ms
        if ( board_millis() - start_ms < blink_interval_ms) {
            //return; // not enough time
            //taskYIELD();
            vTaskDelay(2);
            continue;
        }

        start_ms += blink_interval_ms;

        board_led_write(led_state);
        led_state = !led_state; // toggle
    }
}

//--------------------------------------------------------------------+
// subsidiary functions
//--------------------------------------------------------------------+

// MS->TICKS: pdMS_TO_TICKS(x), see projdefs.h of FreeRTOS
// TICKS->MS: TICKS_TO_MS(x)
#define TICKS_TO_MS(x)  ((x) * 1000 / configTICK_RATE_HZ)

uint32_t board_millis(void)
{
    return TICKS_TO_MS(xTaskGetTickCount());
}

void board_led_write(bool state)
{
    GPIO_PinWrite(gGpioDev, (1UL << LED_PIN), state ? LED_STATE_ON : LED_STATE_OFF);
}
