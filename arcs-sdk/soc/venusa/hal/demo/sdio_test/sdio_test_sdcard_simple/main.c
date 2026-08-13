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
//#include "nos_timer.h"
#include "lib_sdc.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include "IOMuxManager.h"
#include "log_print.h"
#include "Driver_GPIO.h"
#include "sdioh_reg.h"
#include "venusa_ap.h"
#include "ClockManager.h"
#include "ftsdc021.h"

#define _DMA32  __attribute__((aligned(32)))

#define SRAM_SDIO_RW_TEST

#ifdef  PSRAM_SDIO_RW_TEST
// Max. size of SRC/DST buffer
#define MaxLen          (1024*1024*4)
uint8_t *SourceBuf_psram = (uint8_t *)0x38000000;
uint8_t *DestinBuf_psram = (uint8_t *)(0x38000000 + MaxLen);
#endif

#ifdef SRAM_SDIO_RW_TEST
#define MaxLen          (1024*10)
_DMA32 uint8_t  SourceBuf[MaxLen];
_DMA32 uint8_t  DestinBuf[MaxLen];
#endif

#define SDIO_DEFAULT_BLOCK_SIZE  512
#define ENABLE_WIFI_INTR    1 //1:need cmd clk io0 io1 pins; 0:need cmd clk io0 pins
#define USE_4BIT_SDIO       1 //0 //
#define SDIO_RATE_STAT      0

#define CIS_FUNC_NUM        0
#define WIFI_FUNC_NUM       1
#define READ_DIRECTION      0
#define WRITE_DIRECTION     1


static SemaphoreHandle_t s_sema = NULL;

uint32_t TC_total = 0, TC_pass = 0, TC_fail = 0;

//=============================================================================


#define __HAL_CRM_SDIO_HOST_CLK_ENABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_SDIOH_CLK2X = 0x1; \
} while(0)

static void* GPIOB_Handler = NULL;

static void iomux_sel_sdc()
{
	__HAL_CRM_SDIO_HOST_CLK_ENABLE(); //enable clock

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

    ret = gm_sdc_api_sdio_cmd52(SD_0, WRITE_DIRECTION, CIS_FUNC_NUM, (int)0x110, in, &out);
    if (!ret) {
        in = blk[1];
        ret = gm_sdc_api_sdio_cmd52(SD_0, WRITE_DIRECTION, CIS_FUNC_NUM, (int)0x111, in, &out);
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
	static SDCardInfo ftsdc021_info;
	static _DMA32 uint8_t FTSDC021_SD_CARD_BUF[512];
	static _DMA32 uint8_t FTSDC021_SD_ADMA_BUF[512];

//	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | SDC_OPTION_CD_INVERT), 0,
//	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | SDC_OPTION_FIXED | SDC_OPTION_SDIO_STD_FUNC), 0,
	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | SDC_OPTION_FIXED | SDC_OPTION_SDIO_STD_FUNC | SDC_OPTION_SDIO_FORCE_3_3_V), 0,
			iomux_sel_sdc, (u32)&ftsdc021_info);
	gm_sdc_api_action(SD_0, GM_SDC_ACTION_SET_ADMA_BUFER, FTSDC021_SD_ADMA_BUF, NULL);

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

static void sd_task( void *pvParameters )
{
	uint32_t snum, i;

	uint32_t t0=0, t1=0, t2=0;
	uint32_t t0_psram=0, t1_psram=0, t2_psram=0;
	uint32_t fail = 0;

	CLOGD("enter sd_task\n");


//	Enable PHY internal pull-up register
//	*((volatile uint32_t*) 0x441000A8) = *((volatile uint32_t*) 0x441000a8) | 0x60000 ;
	*((volatile uint32_t*) 0x441000AC) = *((volatile uint32_t*) 0x441000AC) | 0x60000 ;
	*((volatile uint32_t*) 0x441000B0) = *((volatile uint32_t*) 0x441000B0) | 0x60000 ;
	*((volatile uint32_t*) 0x441000B4) = *((volatile uint32_t*) 0x441000B4) | 0x60000 ;
	*((volatile uint32_t*) 0x441000B8) = *((volatile uint32_t*) 0x441000B8) | 0x60000 ;
	*((volatile uint32_t*) 0x441000BC) = *((volatile uint32_t*) 0x441000BC) | 0x60000 ;



	for(int j=0; j < 1; j++){

		fail = 0;

		i = sdio_bus_probe();

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
			CLOGD("SD CARD READ WRITE TEST FINNISH\n");
		}else
		{
			CLOGD("not find partition in mbr\n");
		}
#endif

		//simple read and write test
		//CLOGD("sd card read write begin\n");


#ifdef SRAM_SDIO_RW_TEST
		for(i = 0; i < MaxLen; i++) {
			DestinBuf[i] = (i+j) % 16 + '7';;
			SourceBuf[i] = 0;
		}


		//t0 = xTaskGetTickCount();
		t0 = __get_rv_cycle();
		//write and read
		gm_sdc_api_sdcard_sector_write(SD_0, 0, MaxLen/512, DestinBuf);
		//t1 = xTaskGetTickCount();
		t1 = __get_rv_cycle();
		//CLOGD("WRITE FINISH\n");
		gm_sdc_api_sdcard_sector_read(SD_0, 0, MaxLen/512, SourceBuf);
		//t2 = xTaskGetTickCount();
		t2 = __get_rv_cycle();

		//CLOGD("READ FINISH\n");
		//compare
		for(i = 0; i < MaxLen; i++) {
			if(DestinBuf[i] != SourceBuf[i]) {
				fail = 1;
				CLOGD("sram    0x%02x(dst idx-%d) != 0x%02x(src idx-%d)", DestinBuf[i], i, SourceBuf[i], i);
				CLOGD("[Failed]");
				break;
			}
		}

		if(fail == 0){
			CLOGD("WRITE AND READ %d kB data TEST FINISH\n", MaxLen/1024);
			CLOGD("sram read-write ADDR: %x, %x", DestinBuf, SourceBuf);
			CLOGD("sram: result t0 = %u", t0);
			CLOGD("sram: result t1 = %u, gap = %u", t1, t1-t0);
			CLOGD("sram: result t2 = %u, gap = %u", t2, t2-t1);
			CLOGD("sram: write speed ：%f MB", ((float)MaxLen*300000000)/(t1-t0)/1024/1024);
			CLOGD("sram: read speed ：%f MB\n\n", ((float)MaxLen*300000000)/(t2-t1)/1024/1024);
			CLOGD("[Success]");
		}
#endif

#ifdef PSRAM_SDIO_RW_TEST

		PSRAM_Initialize(NULL, NULL, 1);

		for(i = 0; i < MaxLen; i++) {
			*((char *)DestinBuf_psram + i) = (i+j) % 16 + '4';
			*((char *)SourceBuf_psram + i) = 0;
		}

		//t0 = xTaskGetTickCount();
		t0_psram = __get_rv_cycle();
		//write and read
		gm_sdc_api_sdcard_sector_write(SD_0, 0, MaxLen/512, DestinBuf_psram);
		//t1 = xTaskGetTickCount();
		t1_psram = __get_rv_cycle();
	//	CLOGD("WRITE FINISH\n");
		gm_sdc_api_sdcard_sector_read(SD_0, 0, MaxLen/512, SourceBuf_psram);
		//t2 = xTaskGetTickCount();
		t2_psram = __get_rv_cycle();

		//CLOGD("READ FINISH\n");

		//compare
		for(i = 0; i < MaxLen; i++) {
			if(DestinBuf_psram[i] != SourceBuf_psram[i]) {
						CLOGD("psaram:    0x%02x(dst idx-%d) != 0x%02x(src idx-%d)", DestinBuf_psram[i], i, SourceBuf_psram[i], i);
						while(1);
						break;
					}
		}
		CLOGD("WRITE AND READ %d kB data TEST FINISH\n", MaxLen/1024);
		CLOGD("psram read-write ADDR: %x, %x", DestinBuf_psram, SourceBuf_psram);
		CLOGD("psram result t0 = %u", t0_psram);
		CLOGD("psram result t1 = %u, gap = %u", t1_psram, t1_psram-t0_psram);
		CLOGD("psram result t2 = %u, gap = %u", t2_psram, t2_psram-t1_psram);
		CLOGD("psram write speed ：%f MB", ((float)MaxLen*300000000)/(t1_psram-t0_psram)/1024/1024);
		CLOGD("psram read speed ：%f MB\n", ((float)MaxLen*300000000)/(t2_psram-t1_psram)/1024/1024);

#endif

		vSemaphoreDelete(SDHost[SD_0].sd_mutex);
		SDHost[SD_0].sdcard_init_complete = 0;

	}
//  CRM_InitFlashSrc(CRM_IpFlash_100MHz);
END:
	for(;;) {
	//	CLOGD("result :%u", xTaskGetTickCount());

	}
}


int32_t HAL_CRM_SetSDIOHClkDiv(uint32_t div_n, uint32_t div_m){
#if 0
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_LD = 0x0;
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_N = div_n;
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_M = div_m;
    IP_AP_CFG->REG_CLK_CFG1.bit.SEL_SDIOH_CLK2X = 0x1;
    // Avoid the compiler out-of-order
    __COMPILER_BARRIER();
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_LD = 0x1;
#endif
    return CSK_DRIVER_OK;
}


int main()
{
//	CRM_InitFlashSrc(CRM_IpFlash_200MHz);

	DisableDCache();


	HAL_CRM_SetSDIOHClkDiv(1, 1);
	logInit(0, 115200);

	CLOGD("enter simple test sdcard main\n");

	xTaskCreate(
			sd_task,	/* The function that implements the task. */
			"Task1",					/* Text name for the task. */
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
