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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "ClockManager.h"

#include "venusa_ap.h"
#include "log_print.h"
#include "dbg_assert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
//#include "queue.h"

#include "tusb.h"

#include "video_device.h"

#define DEBUG_LOG   0 // 1
#if DEBUG_LOG
#define LOGD(fmt, ...)   CLOGD(fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)   ((void)0)
#endif // DEBUG_LOG

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void* param);
void usb_device_task(void *param);
void video_task(void* param);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void freertos_init_task(void);
#endif



// MS->TICKS: pdMS_TO_TICKS(x), see projdefs.h of FreeRTOS
// TICKS->MS: TICKS_TO_MS(x)
#define TICKS_TO_MS(x)  ((x) * 1000 / configTICK_RATE_HZ)

uint32_t board_millis(void)
{
    return TICKS_TO_MS(xTaskGetTickCount());
}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+


int main(void)
{
//  board_init();

    logInit(0, 115200); // uart0, baudrate=115200

    CLOGD("ENTER video_task main");
    //logInit(2, 115200); // uart2, baudrate=115200

    // check VBUS pin for USB hot plug & pull
#if CHK_VBUS_HOT_PLUG
#if (GPIO_GRP_TO_VBUS == 0)
    gGpioDev = GPIOA();
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIO_PIN_TO_VBUS, CSK_IOMUX_FUNC_DEFAULT);
#else
    gGpioDev = GPIOB();
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIO_PIN_TO_VBUS, CSK_IOMUX_FUNC_DEFAULT);
#endif
    //GPIO_Initialize(gGpioDev, NULL, NULL); // cb_GPIO_Edge_Intr
    GPIO_Initialize(gGpioDev, cb_GPIO_Edge_Intr, NULL);
    GPIO_Control(gGpioDev,
                CSK_GPIO_DEBOUNCE_ENABLE | CSK_GPIO_DEBOUNCE_CLK_EXT |
                //CSK_GPIO_INTR_DISABLE,
                //CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                CSK_GPIO_SET_INTR_DUAL_EDGE | CSK_GPIO_INTR_ENABLE,
                GPIO_BIT(GPIO_PIN_TO_VBUS));
    GPIO_Control(gGpioDev, CSK_GPIO_DEBOUNCE_SCALE, 0xff);
#endif // CHK_VBUS_HOT_PLUG

    //enable usb clock
    __HAL_CRM_USB_CLK_ENABLE();

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; //16bit mode

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host, i.e. require host to enumerate device...

//  while (1)
//  {
//    tud_task(); // tinyusb device task
//    cdc_task();
//  }

    BaseType_t ret;
    // create led_blinking task
    ret = xTaskCreate(video_task,
                    "video_task",
                    0x400,
                    NULL,
                    3,
                    NULL);
    assert (ret == pdPASS);

    vTaskStartScheduler();
    while(1); // don't exit...

    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+
static unsigned frame_num = 0;
static unsigned tx_busy = 0;
static unsigned interval_ms = 1000 / FRAME_RATE;

#ifdef CFG_EXAMPLE_VIDEO_READONLY
// For mcus that does not have enough SRAM for frame buffer, we use fixed frame data.
// To further reduce the size, we use MJPEG format instead of YUY2.
#include "images.h"

#if !defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
static struct {
  uint32_t       size;
  uint8_t const *buffer;
} const frames[] = {
  {color_bar_0_jpg_len, color_bar_0_jpg},
  {color_bar_1_jpg_len, color_bar_1_jpg},
  {color_bar_2_jpg_len, color_bar_2_jpg},
  {color_bar_3_jpg_len, color_bar_3_jpg},
  {color_bar_4_jpg_len, color_bar_4_jpg},
  {color_bar_5_jpg_len, color_bar_5_jpg},
  {color_bar_6_jpg_len, color_bar_6_jpg},
  {color_bar_7_jpg_len, color_bar_7_jpg},
};
#endif

#else

// YUY2 frame buffer
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 16 / 8];

static void fill_color_bar(uint8_t* buffer, unsigned start_position) {
  /* EBU color bars: https://stackoverflow.com/questions/6939422 */
  static uint8_t const bar_color[8][4] = {
      /*  Y,   U,   Y,   V */
      { 235, 128, 235, 128}, /* 100% White */
      { 219,  16, 219, 138}, /* Yellow */
      { 188, 154, 188,  16}, /* Cyan */
      { 173,  42, 173,  26}, /* Green */
      {  78, 214,  78, 230}, /* Magenta */
      {  63, 102,  63, 240}, /* Red */
      {  32, 240,  32, 118}, /* Blue */
      {  16, 128,  16, 128}, /* Black */
  };
  uint8_t* p;

  /* Generate the 1st line */
  uint8_t* end = &buffer[FRAME_WIDTH * 2];
  unsigned idx = (FRAME_WIDTH / 2 - 1) - (start_position % (FRAME_WIDTH / 2));
  p = &buffer[idx * 4];
  for (unsigned i = 0; i < 8; ++i) {
    for (int j = 0; j < FRAME_WIDTH / (2 * 8); ++j) {
      memcpy(p, &bar_color[i], 4);
      p += 4;
      if (end <= p) {
        p = buffer;
      }
    }
  }

  /* Duplicate the 1st line to the others */
  p = &buffer[FRAME_WIDTH * 2];
  for (unsigned i = 1; i < FRAME_HEIGHT; ++i) {
    memcpy(p, buffer, FRAME_WIDTH * 2);
    p += FRAME_WIDTH * 2;
  }
}

#endif

extern bool tud_video_n_frame_xfer(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, void *buffer, size_t bufsize);

static void video_send_frame(void) {
  static unsigned start_ms = 0;
  static unsigned already_sent = 0;

  if (!tud_video_n_streaming(0, 0)) {
    already_sent = 0;
    frame_num = 0;
    return;
  }

  if (!already_sent) {
    already_sent = 1;
    tx_busy = 1;
    start_ms = board_millis();
#ifdef CFG_EXAMPLE_VIDEO_READONLY
    #if defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
    tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)&frame_buffer[(frame_num % (FRAME_WIDTH / 2)) * 4],
                           FRAME_WIDTH * FRAME_HEIGHT * 16/8);
    #else
    tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)frames[frame_num % 8].buffer, frames[frame_num % 8].size);
    #endif
#else
    fill_color_bar(frame_buffer, frame_num);
    tud_video_n_frame_xfer(0, 0, (void*) frame_buffer, FRAME_WIDTH * FRAME_HEIGHT * 16 / 8);
#endif
  }

  unsigned cur = board_millis();
  if (cur - start_ms < interval_ms) return; // not enough time
  if (tx_busy) return;
  start_ms += interval_ms;
  tx_busy = 1;

#ifdef CFG_EXAMPLE_VIDEO_READONLY
  #if defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
  tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)&frame_buffer[(frame_num % (FRAME_WIDTH / 2)) * 4],
                         FRAME_WIDTH * FRAME_HEIGHT * 16/8);
  #else
  tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)frames[frame_num % 8].buffer, frames[frame_num % 8].size);
  #endif
#else
  fill_color_bar(frame_buffer, frame_num);
  tud_video_n_frame_xfer(0, 0, (void*) frame_buffer, FRAME_WIDTH * FRAME_HEIGHT * 16 / 8);
#endif
}


void video_task(void* param) {
  (void) param;

  while(1) {
    video_send_frame();

    #if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(interval_ms / portTICK_PERIOD_MS);
    #else
    return;
    #endif
  }
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx) {
  (void) ctl_idx;
  (void) stm_idx;
  tx_busy = 0;
  /* flip buffer */
  ++frame_num;
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const* parameters) {
  (void) ctl_idx;
  (void) stm_idx;
  /* convert unit to ms from 100 ns */
  interval_ms = parameters->dwFrameInterval / 10000;
  return VIDEO_ERROR_NONE;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {
    #if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
    #else
    if (board_millis() - start_ms < blink_interval_ms) return; // not enough time
    #endif

    start_ms += blink_interval_ms;
    //board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE
#define VIDEO_STACK_SIZE    (configMINIMAL_STACK_SIZE*4)

#if TUSB_MCU_VENDOR_ESPRESSIF
  #define USBD_STACK_SIZE     4096
  int main(void);
  void app_main(void) {
    main();
  }
#else
  // Increase stack size when debug log is enabled
  #define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#endif

// static task
#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

StackType_t  video_stack[VIDEO_STACK_SIZE];
StaticTask_t video_taskdef;
#endif


#if 0
// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void *param) {
  (void) param;

  // init device stack on configured roothub port
  // This should be called after scheduler/kernel is started.
  // Otherwise, it could cause kernel issue since USB IRQ handler does use RTOS queue API.
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  // RTOS forever loop
  while (1) {
    // put this thread to waiting state until there is new events
    tud_task();
  }
}


void freertos_init_task(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);
  xTaskCreateStatic(video_task, "cdc", VIDEO_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, video_stack, &video_taskdef);
  #else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
  xTaskCreate(video_task, "video", VIDEO_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
  #endif

  // skip starting scheduler (and return) for ESP32-S2 or ESP32-S3
  #if !TUSB_MCU_VENDOR_ESPRESSIF
  vTaskStartScheduler();
  #endif
}
#endif
#endif


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
