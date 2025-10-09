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
#include <ctype.h>

#include "chip.h"
#include "ClockManager.h"
#include "main.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "tusb.h"
#include "cdc.h"
#include "cdc_device.h"
#include "systick.h"


#define DEBUG_LOG   0 // 1
#if DEBUG_LOG
#define LOGD(fmt, ...)   CLOGD(fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)   ((void)0)
#endif // DEBUG_LOG

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
static uint32_t cur_cnt = 0, cur_val = 0;

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t hid_epin_buf[CFG_TUD_HID_EP_BUFSIZE];


// cdc_task can run if signaled
static SemaphoreHandle_t semStartProcess = NULL;

#if !CFG_TUD_CDC_USE_FIFO
// cdc read is done if signaled
static SemaphoreHandle_t semReadDone = NULL;
static SemaphoreHandle_t semWriteDone = NULL;
static uint32_t read_bytes = 0, written_bytes = 0;
#endif // !CFG_TUD_CDC_USE_FIFO

static void cdc_task(void *arg);
static void hid_task(void *arg);

/*------------- MAIN -------------*/

static uint32_t mmio_read32(uint32_t addr)
{
    return *(volatile uint32_t *)(addr);
}

static void mmio_write32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)(addr) = value;
}

#define DELAY_MS(_ms)  SysTick_Delay_Ms(_ms)

int main(void)
{
    uint32_t times = 0;
//  board_init();

    logInit(0, 115200); // uart0, baudrate=115200
    //logInit(2, 115200); // uart2, baudrate=115200

    CLOG("[%s:%d] usb_cdc_hid demo", __func__, __LINE__);

    //enable usb clock
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; //16bit mode

    semStartProcess = xSemaphoreCreateBinary(); // non-signaled
    assert(semStartProcess != NULL);

#if !CFG_TUD_CDC_USE_FIFO
    semReadDone = xSemaphoreCreateBinary(); // non-signaled
    assert(semReadDone != NULL);
    semWriteDone = xSemaphoreCreateBinary(); // non-signaled
    assert(semWriteDone != NULL);
#endif

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host, i.e. require host to enumerate device...

    BaseType_t ret;
    // create led_blinking task
    ret = xTaskCreate(cdc_task,
                    "cdc_task",
                    0x200,
                    NULL,
                    3,
                    NULL);
    assert (ret == pdPASS);

#ifdef EPADDR_HID_IN
    ret = xTaskCreate(hid_task,
                    "hid_task",
                    0x200,
                    NULL,
                    3,
                    NULL);
    assert (ret == pdPASS);
#endif

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
    TU_LOG2("%s called\r\n", __func__);
    BaseType_t xHPWoken = pdFALSE;
    xSemaphoreGiveFromISR(semStartProcess, &xHPWoken);
    portYIELD_FROM_ISR(xHPWoken);

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
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint32_t btn_bits)
{
    // use to avoid send multiple consecutive zero report
    static bool has_consumer_key = false;

    // each bit for a button, 1 = key down, 0 = key up
    if ( btn_bits ) {
      tud_hid_report(0, &btn_bits, HID_REPORT_LEN);
      has_consumer_key = true;
    } else {
      // send empty key report (release key) if previously has key pressed
      if (has_consumer_key) tud_hid_report(0, &btn_bits, HID_REPORT_LEN);
      has_consumer_key = false;
    }
}


bool tud_hid_ep_buffer_cb(uint8_t itf, uint8_t **epin_buf_pp, uint8_t **epout_buf_pp,
                          uint16_t *epin_buf_len_p, uint16_t *epout_buf_len_p)
{
    (void) itf;

    *epin_buf_pp = hid_epin_buf;
    *epout_buf_pp = NULL;
    *epin_buf_len_p = CFG_TUD_HID_EP_BUFSIZE;
    *epout_buf_len_p = 0;
    return true;
}


// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const* report, uint8_t len)
{
  (void) itf;
  (void) len;

//  uint8_t next_report_id = report[0] + 1;
//
//  if (next_report_id < REPORT_ID_COUNT)
//  {
//    send_hid_report(next_report_id, btn1_read());
//  }

//#if USE_VAR_INPUT
//  send_hid_report(cur_btn_bits());
//#else
//  send_hid_report(cur_btn());
//#endif

//  if (++cur_cnt < REPEAT_COUNT * 2)
//      send_hid_report((cur_cnt & 0x1) ? 0 : cur_val);
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // TODO set LED based on CAPLOCK, NUMLOCK etc...
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}


//--------------------------------------------------------------------+
// CDC Callback (all are weak, optional)
//--------------------------------------------------------------------+

// Invoked when received new data
void tud_cdc_rx_cb(uint8_t itf)
{
    TU_LOG2("%s: itf = %d\r\n", __func__, itf);
}

// Invoked when received `wanted_char`
void tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char)
{
    TU_LOG2("%s: itf = %d, wanted = 0x%x\r\n", __func__, itf, wanted_char);
}

// Invoked when space becomes available in TX buffer
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    TU_LOG2("%s: itf = %d\r\n", __func__, itf);
}

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    TU_LOG2("%s: itf = %d, DTR = %d, RTS = %d\r\n", __func__, itf, dtr, rts);
}

// Invoked when line coding is change via SET_LINE_CODING
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding)
{
//    uint32_t bit_rate;
//    uint8_t  stop_bits; ///< 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
//    uint8_t  parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
//    uint8_t  data_bits; ///< can be 5, 6, 7, 8 or 16

    TU_LOG2("%s: itf = %d, bit_rate = %d, data_bits = %d\r\n",
            __func__, itf, p_line_coding->bit_rate, p_line_coding->data_bits);
}

// Invoked when received send break
void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms)
{
    TU_LOG2("%s: itf = %d, duration = %dms\r\n", __func__, itf, duration_ms);
}

void tud_cdc_read_done_cb (uint8_t itf, uint8_t status, void* buffer, uint32_t xferred_bytes)
{
    TU_LOG2("%s: itf = %d, status = %d, xferred = %d bytes, buf[0]=%c", __func__, itf, status, xferred_bytes, *(uint8_t *)buffer);
    read_bytes = xferred_bytes;

    BaseType_t xHPWoken = pdFALSE;
    xSemaphoreGiveFromISR(semReadDone, &xHPWoken);
    portYIELD_FROM_ISR(xHPWoken);
}

void tud_cdc_write_done_cb (uint8_t itf, uint8_t status, void const* buffer, uint32_t xferred_bytes)
{
    TU_LOG2("%s: itf = %d, status = %d, xferred = %d bytes, buf[0]=%c", __func__, itf, status, xferred_bytes, *(uint8_t *)buffer);
    written_bytes = xferred_bytes;

    BaseType_t xHPWoken = pdFALSE;
    xSemaphoreGiveFromISR(semWriteDone, &xHPWoken);
    portYIELD_FROM_ISR(xHPWoken);
}


//--------------------------------------------------------------------+
// TASK
//--------------------------------------------------------------------+

// echo to either Serial0 or Serial1
// with Serial0 as all lower case, Serial1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
  uint32_t num, i=0;

  CLOG("[%s:%d] count=%d", __func__, __LINE__, count);

  for(; i<count; i++)
  {
    if (itf == 0)
    {
      // echo back 1st port as lower case
      if (isupper(buf[i])) buf[i] += 'a' - 'A';
    }
    else
    {
      // echo back 2nd port as upper case
      if (islower(buf[i])) buf[i] -= 'a' - 'A';
    }
  }

  tud_cdc_n_write_immed(itf, buf, count);
}


volatile static uint8_t hid_flag = 0;

static void cdc_task(void *arg)
{
    (void)arg;

    uint8_t itf;
    //uint8_t buf[64];
    uint8_t buf[64];
    uint32_t count, total;

    CLOG("[%s:%d]", __func__, __LINE__);

    xSemaphoreTake(semStartProcess, portMAX_DELAY);
    vTaskDelay(10); // delay 10 ticks

    CLOG("[%s:%d]", __func__, __LINE__);
    hid_flag = 1;

    while (1) {
        if (tud_ready()) {
            // connected() check for DTR bit
            // Most but not all terminal client set this when making connection
            //if (!tud_cdc_n_connected(itf)) //  || tud_cdc_n_available(itf) == 0
            //    continue;

            read_bytes = 0;
            tud_cdc_n_read_immed(0, buf, sizeof(buf));

            // wait read done
            xSemaphoreTake(semReadDone, portMAX_DELAY);
            total = read_bytes;

            // echo back to both serial ports
            echo_serial_port(0, buf, total);

    #if (CFG_TUD_CDC >= 2)
            // wait write done
            xSemaphoreTake(semWriteDone, portMAX_DELAY);
            echo_serial_port(1, buf, total);
    #endif
            // wait write done
            xSemaphoreTake(semWriteDone, portMAX_DELAY);

        } // end tud_ready

        //vTaskDelay(10); // delay 10 ticks
    } // end while
}

void hid_task(void *arg)
{
  (void)arg;

  uint32_t btn_val = 0;
  uint32_t times = 0;
  uint32_t flag = 0;

  CLOG("[%s:%d]", __func__, __LINE__);

  while(!hid_flag);

  while(1)
  {
      vTaskDelay(1000);

      if(flag == 0) {
          times++;
          btn_val = (0x1 << 1);        // BTN2_PIN  +
      } else {
          times--;
          btn_val = (0x1 << 0);        // BTN1_PIN  -
      }

      if(times == 10) {
          flag = 1;
      }
      if(times == 0) {
          flag = 0;
      }

      cur_cnt = 0;
      cur_val = btn_val;

      CLOG("[%s:%d] btn_val=%d", __func__, __LINE__, btn_val);
      send_hid_report(btn_val);

  }
}


