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
#include "main.h"
#include "device/usbd_pvt.h"
#include "ClockManager.h" // for Core Freq. change

#define DEBUG_LOG   0 // 1
#if DEBUG_LOG
#define LOGD(fmt, ...)   CLOGD(fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)   ((void)0)
#endif // DEBUG_LOG

//------------- prototypes -------------//

// vendor_task can run if signaled
static SemaphoreHandle_t semStartProcess = NULL;

#if !CFG_TUD_VENDOR_USE_FIFO

#define USE_PSRAM_BUF   0 //1 // EP buffer of Vendor device
#define USE_OS_SEM      0 //1 // use semaphore of RTOS

#if USE_OS_SEM
// vendor read is done if signaled
static SemaphoreHandle_t semReadDone = NULL;
static SemaphoreHandle_t semWriteDone = NULL;
#else
static volatile int32_t iReadDone = 0;
static volatile int32_t iWriteDone = 0;
#endif

static volatile uint32_t read_bytes = 0, written_bytes = 0;
static volatile uint32_t restart_echo = 0;

#if !DIRECT_RW_EP
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t ven0_epin_buf[EP_MPS_VEN0_IN];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t ven0_epout_buf[EP_MPS_VEN0_OUT];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t ven0_epin_ext_buf[EP_MPS_VEN0_IN_EXT];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t ven0_epout_ext_buf[EP_MPS_VEN0_OUT_EXT];
#if (CFG_TUD_VENDOR >= 2)
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t ven1_epin_buf[EP_MPS_VEN1_IN];
#endif
#if (CFG_TUD_VENDOR >= 3)
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t ven2_epout_buf[EP_MPS_VEN2_OUT];
#endif
#endif // !DIRECT_RW_EP

#endif // !CFG_TUD_VENDOR_USE_FIFO

static void vendor_task(void *arg);

/*------------- MAIN -------------*/

int main(void)
{
    // Init CRM core clock
    //CRM_InitCoreSrc(CRM_IpCore_300MHz);

    logInit(0, 115200); // uart0, baudrate=115200
    //logInit(2, 115200); // uart2, baudrate=115200

#if (TUD_OPT_HIGH_SPEED ? 1 : 0) // 1: USB2.0; 0: USB1.1
    CLOG("[%s:%d] USB2.0", __func__, __LINE__);  // high-speed
#else
    CLOG("[%s:%d] USB1.1", __func__, __LINE__);  // full-speed 12Mbps
#endif

#if (IC_BOARD == 0) // NOTE:  SYS PLL is NOT selected for IC_BOARD == 0!
    // enable SYSPLL
    IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_ENABLE = 1;

    // wait for lock...
    volatile uint32_t rdata;
    do {
        rdata = IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_LOCK;
    } while (rdata != 0x1);

    // select SYSPLL as source of root clock
    IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 1;
#endif // IC_BOARD == 0

    //enable APC for USB DBG test
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;

    //enable usb clock
    //IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1; //USB PHY clock is the root!!
    //IP_CMN_SYS->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    __HAL_CRM_USB_CLK_ENABLE();

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; //16bit mode

#if USE_PSRAM_BUF
    // disable cache for PSRAM
    //DisableICache();
    DisableDCache();
    __RWMB();
    __FENCE_I();

    // initialize PSRAM for disk storage
    extern int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search);
    PSRAM_Initialize(NULL, NULL, 1);
    LOGD("PSRAM CONFIG COMPLETE\n");
#endif // USE_PSRAM_BUF

    semStartProcess = xSemaphoreCreateBinary(); // non-signaled
    assert(semStartProcess != NULL);

#if !CFG_TUD_VENDOR_USE_FIFO
#if USE_OS_SEM
    semReadDone = xSemaphoreCreateBinary(); // non-signaled
    assert(semReadDone != NULL);
    semWriteDone = xSemaphoreCreateBinary(); // non-signaled
    assert(semWriteDone != NULL);
#else
    iReadDone = 0;
    iWriteDone = 0;
#endif
#endif

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host, i.e. require host to enumerate device...

    BaseType_t ret;
    // create led_blinking task
    ret = xTaskCreate(vendor_task,
                    "vendor_task",
                    256, //0x400,
                    NULL,
                    3,
                    NULL);
    assert (ret == pdPASS);

#if 0
    // if schedule is invoked, system cannot enter sleep??
    //vTaskStartScheduler();

    //__WFI(); // sleep until interrupt is coming...

    //deep sleep via PMU setting
    //volatile uint32_t i = 0;
    //while(i++ < 5000000);

//#include "systick.h"
//    SysTick_Delay_Ms(5000);

    extern void delay_1ms(uint32_t count);
    delay_1ms(8000);

//#define WKP_GPIO_BX     0x2
//    IP_AON_CTRL->REG_PMU_CTRL2.bit.GPIO_WAKEUP_POL &= ~(WKP_GPIO_BX + 9);
//    IP_AON_CTRL->REG_PMU_CTRL2.bit.ENA_GPIO_WAKEUP |= (0x1 << WKP_GPIO_BX);

    IP_AON_CTRL->REG_AON_CRM_CTRL1.bit.PD_RCO32K_IN_PW_MODE3 = 1;

    IP_AON_CTRL->REG_PMU_CTRL1.bit.ENA_DEEPSLEEP = 0x1;
    IP_AON_CTRL->REG_PMU_CTRL1.bit.ENA_POWMODE = 0x3;
    IP_AON_CTRL->REG_PMU_CTRL2.bit.MASK_CP_ENTER_SLEEP = 1;
    IP_AON_CTRL->REG_PMU_CTRL2.bit.MASK_AP_ENTER_SLEEP = 1;

#endif // 0

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

#if USE_OS_SEM
    xQueueReset(semReadDone);
    xQueueReset(semWriteDone);
#else
    iReadDone = 0;
    iWriteDone = 0;
#endif

    //portYIELD_FROM_ISR(xHPWoken);
    portYIELD_FROM_ISR(pdTRUE);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  //NOTE: it CANNOT be invoked without outer circuit monitoring VBUS level change...
  //TODO:
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  restart_echo = 1;

#if USE_OS_SEM
  xSemaphoreGiveFromISR(semReadDone, NULL);
  xSemaphoreGiveFromISR(semWriteDone, NULL);
#else
    iReadDone = 1;
    iWriteDone = 1;
#endif
  portYIELD_FROM_ISR(pdTRUE);

  //TODO:
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
#if USE_OS_SEM
    xQueueReset(semReadDone);
    xQueueReset(semWriteDone);
#else
    iReadDone = 0;
    iWriteDone = 0;
#endif

    //TODO:
    portYIELD_FROM_ISR(pdTRUE);
}


//--------------------------------------------------------------------+
// Vendor Callback (all are weak, optional)
//--------------------------------------------------------------------+

#if !CFG_TUD_VENDOR_USE_FIFO
#if !DIRECT_RW_EP
//void tud_vendor_ep_buf_cb(uint8_t itf, uint8_t ep_addr, uint8_t **ep_buf_pp, uint16_t *ep_buf_len_p);
void tud_vendor_ep_buf_cb(uint8_t itf, uint8_t ep_addr, uint8_t **ep_buf_pp, uint32_t *ep_buf_len_p)
{
    if (ep_buf_pp == NULL || ep_buf_len_p == NULL) {
        TU_LOG2("%s: pointer to buf and buf_len should NOT be NULL!\r\n", __func__);
        return;
    }

    if (itf == 0) { // Vendor0
        if (ep_addr == EPADDR_VEN0_IN) { // EP IN
            *ep_buf_pp = ven0_epin_buf;
            *ep_buf_len_p = EP_MPS_VEN0_IN;
        } else if (ep_addr == EPADDR_VEN0_OUT) { // EP OUT
            *ep_buf_pp = ven0_epout_buf;
            *ep_buf_len_p = EP_MPS_VEN0_OUT;
        } else if (ep_addr == EPADDR_VEN0_IN_EXT) { // EP IN EXT
            *ep_buf_pp = ven0_epin_ext_buf;
            *ep_buf_len_p = EP_MPS_VEN0_IN_EXT;
        } else if (ep_addr == EPADDR_VEN0_OUT_EXT) { // EP OUT EXT
            *ep_buf_pp = ven0_epout_ext_buf;
            *ep_buf_len_p = EP_MPS_VEN0_OUT_EXT;
        } else {
            TU_LOG2("%s: NO EP(addr = 0x%x) in Vendor0 interface!!\r\n", __func__, ep_addr);
            return;
        }
#if (CFG_TUD_VENDOR >= 2)
    } else if (itf == 1) { // Vendor1
        if (ep_addr == EPADDR_VEN1_IN) { // EP IN
            *ep_buf_pp = ven1_epin_buf;
            *ep_buf_len_p = EP_MPS_VEN1_IN;
        } else {
            TU_LOG2("%s: NO EP(addr = 0x%x) in Vendor1 interface!!\r\n", __func__, ep_addr);
            return;
        }
#endif
#if (CFG_TUD_VENDOR >= 3)
    } else if (itf == 2) { // Vendor2
        if (ep_addr == EPADDR_VEN2_OUT) { // EP OUT
            *ep_buf_pp = ven2_epout_buf;
            *ep_buf_len_p = EP_MPS_VEN2_OUT;
        } else {
            TU_LOG2("%s: NO EP(addr = 0x%x) in Vendor2 interface!!\r\n", __func__, ep_addr);
            return;
        }
#endif
    } else {
        TU_LOG2("%s: NO Vendor%d interface!!\r\n", __func__, itf);
        return;
    }
}
#endif // !DIRECT_RW_EP

//void tud_cdc_read_done_cb (uint8_t itf, uint8_t status, void* buffer, uint32_t xferred_bytes)
void tud_vendor_read_done_cb (uint8_t itf, uint8_t ep_addr, uint8_t status, void* buffer, uint32_t xferred_bytes)
{
//    TU_LOG2("%s: itf = %d, ep_addr = %d, xferred = %d bytes\r\n", __func__, itf, ep_addr, xferred_bytes);
    read_bytes = xferred_bytes;

#if USE_OS_SEM
    BaseType_t xHPWoken = pdFALSE;
    xSemaphoreGiveFromISR(semReadDone, &xHPWoken);
    portYIELD_FROM_ISR(xHPWoken);
#else
    iReadDone = 1;
#endif
}

//void tud_cdc_write_done_cb (uint8_t itf, uint8_t status, void const* buffer, uint32_t xferred_bytes)
void tud_vendor_write_done_cb (uint8_t itf, uint8_t ep_addr, uint8_t status, void const* buffer, uint32_t xferred_bytes)
{
//    TU_LOG2("%s: itf = %d, ep_addr = %d, xferred = %d bytes\r\n", __func__, itf, ep_addr, xferred_bytes);
    written_bytes = xferred_bytes;

#if USE_OS_SEM
    BaseType_t xHPWoken = pdFALSE;
    xSemaphoreGiveFromISR(semWriteDone, &xHPWoken);
    portYIELD_FROM_ISR(xHPWoken);
#else
    iWriteDone = 1;
#endif

}

#endif // !CFG_TUD_VENDOR_USE_FIFO

//--------------------------------------------------------------------+
// USB Vendor
//--------------------------------------------------------------------+

#if CFG_TUD_VENDOR_USE_FIFO

// echo to either Vendor0 or Vendor1
// with Vendor0 as all lower case, Vendor1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
  uint32_t i=0;
  while (i < count) {
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
  tud_vendor_n_write(itf, buf, count);
  //vTaskDelay(1); // delay 1 ticks
  }
}

static void vendor_task(void *arg)
{
    (void)arg;

    uint8_t itf;
    //uint8_t buf[64];
    uint8_t buf[256];
    uint32_t count, total;

    xSemaphoreTake(semStartProcess, portMAX_DELAY);

    while (1) {
        if (tud_ready()) {
        for (itf = 0; itf < CFG_TUD_VENDOR; itf++) {

            total = 0;
/*
            do {
                count = tud_vendor_n_read(itf, buf+total, sizeof(buf)-total);
                total += count;
            } while (count > 0);
            if (total == 0)
                continue;
*/

            total = tud_vendor_n_read(itf, buf, sizeof(buf));

            // echo back to both serial ports
            echo_serial_port(itf, buf, total);

    #if (CFG_TUD_VENDOR >= 2)
            echo_serial_port((itf==0 ? 1 : 0), buf, total);
    #endif

        } // end for
        } // end tud_ready

        vTaskDelay(10); // delay 10 ticks
    } // end while

}

#else // !CFG_TUD_VENDOR_USE_FIFO

// echo to either Vendor0 or Vendor1
// with Vendor0 as all lower case, Vendor1 as all upper case
static int32_t echo_serial_port(uint8_t itf, uint8_t ep_addr, uint8_t buf[], uint32_t count)
{
#define ECHO_INVERSE_CASE   0

#if ECHO_INVERSE_CASE
  uint32_t i=0;
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
#endif // ECHO_INVERSE_CASE

  int32_t ret = tud_vendor_n_write_immed(itf, ep_addr, buf, count);
  if (ret < 0) {
      TU_LOG1("Failed to call tud_vendor_n_write_immed!\n");
  }

  return ret;
}

#if USE_PSRAM_BUF
#define __PSRAM_BASE    0x28000000 // PSRAM Start Address
uint8_t *usr_buf = (uint8_t *)(__PSRAM_BASE);
uint32_t usr_bufsize = 8192;
#else // !USE_PSRAM_BUF
//CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t usr_buf[4096];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t usr_buf[8192];
//CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t usr_buf[2048];
//CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t usr_buf[512 * 12];
//CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t usr_buf[1024 * 16];
uint32_t usr_bufsize = 0;
#endif // USE_PSRAM_BUF

#define EPADDR_OUT  EPADDR_VEN0_OUT
#define EPADDR_IN   EPADDR_VEN0_IN
//#define EPADDR_OUT  EPADDR_VEN0_OUT_EXT
//#define EPADDR_IN   EPADDR_VEN0_IN_EXT

static void vendor_task(void *arg)
{
    (void)arg;

    uint8_t itf;
    uint32_t count, total;
    int32_t ret;

    xSemaphoreTake(semStartProcess, portMAX_DELAY);
    vTaskDelay(10); // delay 10 ticks

    CLOG("[%s:%d]", __func__, __LINE__);

    while (1) {
        if (tud_ready()) {
            // connected() check for DTR bit
            // Most but not all terminal client set this when making connection
            //if (!tud_cdc_n_connected(itf)) //  || tud_cdc_n_available(itf) == 0
            //    continue;

            restart_echo = 0;
            read_bytes = 0;

#if !USE_PSRAM_BUF
            if (usr_bufsize == 0)
                usr_bufsize = sizeof(usr_buf);
#endif
            ret = tud_vendor_n_read_immed(0, EPADDR_OUT, usr_buf, usr_bufsize);
            if (ret < 0) {
                CLOG("Failed to call tud_vendor_n_read_immed!\n");
                //return;
                continue;

            } else if (ret == 0) {
            // wait read done
#if USE_OS_SEM
            xSemaphoreTake(semReadDone, portMAX_DELAY);
#else
            while(!iReadDone);
            iReadDone = 0;
#endif
            if (restart_echo) {
                usbd_edpt_canecl_xfer(0, EPADDR_OUT);
                continue;
            }
            }

            total = read_bytes;

            // echo back to both serial ports
            ret = echo_serial_port(0, EPADDR_IN, usr_buf, total);

            if (ret < 0)
                //return;
                continue;

            if (ret == 0) {
            // wait write done
#if USE_OS_SEM
            xSemaphoreTake(semWriteDone, portMAX_DELAY);
#else
            while(!iWriteDone);
            iWriteDone = 0;
#endif
            if (restart_echo) {
                usbd_edpt_canecl_xfer(0, EPADDR_IN);
                continue;
            }
            }

        } // end tud_ready

    } // end while
}

#endif // CFG_TUD_VENDOR_USE_FIFO
