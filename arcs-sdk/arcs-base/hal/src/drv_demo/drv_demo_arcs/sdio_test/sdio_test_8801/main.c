/*
 *
 *  Created on:
 *
 *  Revision:
 *
 */
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "nos_timer.h"
#include "lib_sdc.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include "IOMuxManager.h"
#include "log_print.h"
#include "Driver_GPIO.h"
#include "mbr.h"
#include "sdioh_reg.h"
#include <arcs_ap.h>
#include "ClockManager.h"
#include "wifi_app.h"


// Max. size of SRC/DST buffer
#define MaxLen          (1024)

// SRC buffer size
#define SourceLen       100 // (MaxLen-1)

// DST buffer size
#define ReceiveLen      100 // (MaxLen-1)

// unit length
#define UnitLen         4 // 1

#define SDIO_DEFAULT_BLOCK_SIZE  512
#define ENABLE_WIFI_INTR    1 //1:need cmd clk io0 io1 pins; 0:need cmd clk io0 pins
#define USE_4BIT_SDIO       0 //1 //
#define SDIO_RATE_STAT      0

#define CIS_FUNC_NUM        0
#define WIFI_FUNC_NUM       1
#define READ_DIRECTION      0
#define WRITE_DIRECTION     1

#define _DMA32  __attribute__((aligned(32)))

static _DMA32 uint8_t  SourceBuf[MaxLen] = {0};
static _DMA32 uint8_t  DestinBuf[MaxLen] = {0};

static SemaphoreHandle_t s_sema = NULL;

uint32_t TC_total = 0, TC_pass = 0, TC_fail = 0;

//=============================================================================
#define __HAL_CRM_SDIO_HOST_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 0x1; \
} while(0)

static void iomux_sel_sdc()
{

	__HAL_CRM_SDIO_HOST_CLK_ENABLE(); //enable clock

	//sdio-wifi
	//IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER15);  //sd_clk
	//IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER15);  //sd_cmd

	//IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER15);  //sd_dat0


	//sd card
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_B, 10, CSK_IOMUX_FUNC_ALTER1);  //sd_clk
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_B, 15, CSK_IOMUX_FUNC_ALTER1);  //sd_cmd

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_B, 11, CSK_IOMUX_FUNC_ALTER1);  //sd_dat0

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_B, 12, CSK_IOMUX_FUNC_ALTER1);  //sd_dat1
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_B, 13, CSK_IOMUX_FUNC_ALTER1);  //sd_dat2
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_B, 14, CSK_IOMUX_FUNC_ALTER1);  //sd_dat3

//	HAL_CRM_SetSdio_hostClkSrc(CRM_IpSrcXtalClk); ////select xtal as the clock //0x1; //select syspll as the clock

	IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 0x1; // Release Reset signal

	IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN = 0x1;
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW = 0x1; // Set SD power enable
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL = 0x3; // Set SD power enable
}

static bool sdio_host_enable_isr(bool enable)
{
    //BSD: GM_SDC_ACTION_ENABLE_IRQ / GM_SDC_ACTION_DISABLE_IRQ calls are NECESSARY for wifi interrupt!!
    int ret = 0;
    u32 in;
    u32 type;

    type = enable ? GM_SDC_ACTION_ENABLE_IRQ : GM_SDC_ACTION_DISABLE_IRQ;
    in = SDHCI_INTR_STS_CARD_INTR;

    ret = gm_sdc_api_action(SD_0, type, &in, NULL);
    if (0 != ret) {
//        SDIO_ERROR("%s: ret = %d enable = %d\r\n", __func__, ret, enable);
        return false;
    }

    return true;
}

__attribute__((aligned(4))) static void sdio_host_isr(u16 arg)
{
	BaseType_t wake = (BaseType_t)true;
    if (SDHCI_INTR_STS_CARD_INTR & arg) {
    	sdio_host_enable_isr(false);
    	xSemaphoreGiveFromISR(s_sema, &wake);
    }
}

bool sdio_set_block_size(unsigned int blksize)
{
    unsigned char blk[2];
    u8 in, out;
    int ret;

    if ((blksize == 0) || (blksize > 512)) {
        blksize = SDIO_DEFAULT_BLOCK_SIZE;
    }

    blk[0] = (blksize >> 0) & 0xff;
    blk[1] = (blksize >> 8) & 0xff;
    in = blk[0];
	
	//lyt:
    //ret = gm_sdc_api_sdio_cmd52(SD_0, WRITE_DIRECTION, CIS_FUNC_NUM,, (int)0x110, in, &out);
    ret = gm_sdc_api_sdio_cmd52(SD_0, WRITE_DIRECTION, 1, (int)0x110, in, &out);
    if (!ret) {
        in = blk[1];
        //ret = gm_sdc_api_sdio_cmd52(SD_0, WRITE_DIRECTION, CIS_FUNC_NUM, (int)0x111, in, &out);
        ret = gm_sdc_api_sdio_cmd52(SD_0, WRITE_DIRECTION, 1, (int)0x111, in, &out);
    }

    if (ret) {
//        SDIO_ERROR("sdio_set_block_size(%x), ret = %d\r\n", blksize, ret);
        return false;
    }

    return true;
}


int sdio_bus_probe(void)
{
	int ret;
	static _DMA32 uint8_t FTSDC021_SD_CARD_BUF[512];
	static _DMA32 uint8_t FTSDC021_SD_ADMA_BUF[512];

//	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | DC_OPTION_CD_INVERT | SDC_OPTION_FIXED | SDC_OPTION_SDIO_STD_FUNC), 0,
	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | SDC_OPTION_SDIO_STD_FUNC), 0,
			iomux_sel_sdc, (uint32_t) FTSDC021_SD_CARD_BUF);
//	gm_sdc_api_action(SD_0, GM_SDC_ACTION_SET_ADMA_BUFER, FTSDC021_SD_ADMA_BUF, NULL);
//	gm_sdc_api_action(SD_0, GM_SDC_ACTION_REG_SDIO_APP_INIT, (void*)NULL, NULL);
	ret = (int)gm_sdc_api_action(SD_0, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
	if (ret != 0) {
		CLOGD("%s (return %u): NO sdcard was found!! \r\n", __func__, ret);
		return ret;
	}

//	u32 card_type = 0;
//	ret = (int)gm_sdc_api_action(SD_0, GM_SDC_ACTION_GET_CARD_TYPE, NULL, &card_type);
//	if (ret != 0 || (card_type != SDIO_TYPE_CARD && card_type != MEMORY_SDIO_COMBO)) {
//		SDIO_ERROR("%s (return %u): NOT sdio card (card_type = %u)!! \r\n",
//					__func__, ret, card_type);
//		return ret;
//	}
//
//	//init semaphore
//	s_sema = xSemaphoreCreateCounting((1UL << 31), 0);
//
//	// set block size
//	sdio_set_block_size(SDIO_DEFAULT_BLOCK_SIZE);
//
//	//enable sdio interrupt
//	ret = (int)gm_sdc_api_action(SD_0, GM_SDC_ACTION_SDIO_REG_IRQ, (void *)(sdio_host_isr), NULL);
//	if (ret != 0) {
//		return ret;
//	}
//
//	sdio_host_enable_isr(true);

#if USE_4BIT_SDIO
	u32 bus_width = 4; //1 for 1-bit SDIO, 4 for 4-bit SDIO
	ret = gm_sdc_api_action(SD_0, GM_SDC_ACTION_SET_BUS_WIDTH, &bus_width, NULL);
#endif

	return ret;
}

static void sdio_8801_task( void *pvParameters )
{
	uint32_t snum, i;

	CLOGD("enter %s\n\r", __FUNCTION__);

	//host and device init
	i = sdio_bus_probe();

	wifi_cb_t tmp;
	wifi_start(&tmp);
#if 0
	//read sector 0
	gm_sdc_api_sdcard_sector_read(SD_0, 0, 1, SourceBuf);

	if (mbr_get_snum_start((uint8_t*)SourceBuf)) {
		//find partition id which equal 0xDA in mbr
		snum = mbr_get_snum_next((uint8_t*)SourceBuf);
		if (! snum) {
			CLOGD("not find partition in mbr\n");
//			goto END;
		}
		//read (1K bytes = 2 sectors)
		gm_sdc_api_sdcard_sector_read(SD_0, snum, 2, SourceBuf);
		snum += 2;
		for(i = 0; i < MaxLen; i++) {
			*((char *)DestinBuf + i) = i % 10 + '0';
		}

		//write and read
		gm_sdc_api_sdcard_sector_write(SD_0, snum, 2, DestinBuf);
		gm_sdc_api_sdcard_sector_read(SD_0, snum, 2, SourceBuf);

		//compare
		for(i = 0; i < MaxLen; i++) {
			if(DestinBuf[i] != SourceBuf[i]) {
				CLOGD("    0x%02x(dst idx-%d) != 0x%02x(src idx-%d)", DestinBuf[i], i, SourceBuf[i], i);
				break;
			}
		}
		CLOGD("SD CARD READ WRITE TEST PASS\n\r");
	}
#endif
END:
	for(;;) {

	}
}


/**********************************SRC************************************/
int32_t HAL_CRM_SetSDIOHClkDiv(uint32_t div_n, uint32_t div_m){
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_LD = 0x0;
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_N = div_n;
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_M = div_m;
    // Avoid the compiler out-of-order
    __COMPILER_BARRIER();
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_LD = 0x1;
    return CSK_DRIVER_OK;
}

int main()
{
	HAL_CRM_SetSDIOHClkDiv(1, 1);
	outw(0x45000000, inw(0x45000000) | 0x1<<4);

	logInit(0, 115200);
	CLOGD("enter sd card test, date:%s, time:%s\n\r", __DATE__, __TIME__);

	sdio_8801_task(NULL);
#if 0   //lyt
	xTaskCreate(
			sdio_8801_task,	/* The function that implements the task. */
			"Task_sdcard",					/* Text name for the task. */
			1024,						/* Stack depth in words. */
			NULL,						/* Task parameters. */
			3,							/* Priority and mode (user in this case). */
			NULL						/* Handle. */
		);

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* Will only get here if there was insufficient memory to create the idle
	task. */
	for( ;; );
#endif
    return 0;
}

QueueHandle_t xGlobalScopeCheckQueue = NULL;
#define configPRINT_SYSTEM_STATUS			3


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

/*-----------------------------------------------------------*/
