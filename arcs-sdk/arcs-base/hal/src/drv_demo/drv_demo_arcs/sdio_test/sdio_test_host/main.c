#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "chip.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "log_print.h"
#include "ftsdc021.h"
#include "sdioh_reg.h"
#include "Driver_GPIO.h"
#include "sdio_device.h"


static SemaphoreHandle_t sema_dev2host;

extern volatile ftsdc021_reg* gpRegSD0;



#define __HAL_CRM_SDIO_HOST_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 0x1; \
} while(0)


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



// host side
static void hw_init_host()
{
	disable_IRQ(IRQ_SDIOH_VECTOR);
	IP_AP_CFG->REG_SW_RESET.bit.SDIOH_RESET = 1;

	HAL_CRM_SetSDIOHClkDiv(1, 12);

	__HAL_CRM_SDIO_HOST_CLK_ENABLE();


//	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_ALTER15);  //sd_clk
//	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER15);  //sd_cmd
//
//	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_ALTER15);  //sd_dat0
//	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_ALTER15);  //sd_dat1
//#if USE_4BIT_SDIO
//	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER15);  //sd_dat2
//	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER15);  //sd_dat3
//#endif

//	HAL_CRM_SetSdio_hostClkSrc(CRM_IpSrcXtalClk); ////select xtal as the clock //0x1; //select syspll as the clock
//	__HAL_CRM_SDIO_HOST_CLK_ENABLE(); //enable clock

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER15);  //sd_clk
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER15);  //sd_cmd

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER15);  //sd_dat0
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER15);  //sd_dat1
#if USE_4BIT_SDIO
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 9, CSK_IOMUX_FUNC_ALTER15);  //sd_dat2
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 8, CSK_IOMUX_FUNC_ALTER15);  //sd_dat3
#endif


	IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 0x1; // Release Reset signal

	IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN = 0x1;
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW = 0x1; // Set SD power enable
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL = 0x3; // Set SD power enable
}


void sdioh_HCReset()
{
    u32 clk, i;
    sd_mutex_create(SD_0);

    SDHost[SD_0].sdcard_init_complete = 0;
    //SDHost[ip_idx].Card = (SDCardInfo*)(c+ ip_idx *512);
    memset(SDHost[SD_0].Card, 0, 512);
    SDHost[SD_0].Card->FlowSet.Erasing = 0;
    SDHost[SD_0].Card->FlowSet.autoCmd = 0;
    SDHost[SD_0].sdio_card_int = NULL;
    //SDHost[ip_idx].invert = 0;
    SDHost[SD_0].ErrRecover = 0;
    SDHost[SD_0].clock = 0;
    SDHost[SD_0].ocr_avail = 0;


    /* Reset the controller */
    gpRegSD0->SoftRst |= (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT);
    for (i = 0; i < 100; i++) {
        if ((gpRegSD0->SoftRst & (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT)) == 0)
            break;
        ftsdc021_delay(1);
    }
    clk = 25000000;
    SDHost[SD_0].max_clk = clk;
    SDHost[SD_0].min_clk = 200000;
    if (gpRegSD0->CapReg & SDHCI_CAP_VOLTAGE_33V)
        SDHost[SD_0].ocr_avail = (3 << 21);

    if (gpRegSD0->CapReg & SDHCI_CAP_VOLTAGE_30V)
        SDHost[SD_0].ocr_avail |= (3 << 17);

    SDHost[SD_0].Card->fifo_depth = 512;


    /* set timeout ctl */
    gpRegSD0->TimeOutCtl = 14;
    SDHost[SD_0].Card->FlowSet.timeout_ms = (1 << (gpRegSD0->TimeOutCtl + 13)) / clk;
    if (SDHost[SD_0].Card->FlowSet.timeout_ms == 0)
        SDHost[SD_0].Card->FlowSet.timeout_ms = 14;
    SDHost[SD_0].Card->FlowSet.timeout_ms *= 1000;

    extern void	ftsdc021_0_IntrHandler();
    register_ISR(IRQ_SDIOH_VECTOR, ftsdc021_0_IntrHandler, NULL);
    enable_IRQ(IRQ_SDIOH_VECTOR);

    ////ftsdc021_set_transfer_type(SD_0, SDMA, 4);  //tiger debug
    gpRegSD0->HCReg &= ~(u8) (0x3 << 3);
    gpRegSD0->DmaHndshk &= ~1;
    SDHost[SD_0].Card->FlowSet.UseDMA = SDMA;
    SDHost[SD_0].Card->FlowSet.lineBound = 4;
    SDHost[SD_0].Card->FlowSet.sdma_bound_mask = (1 << (SDHost[SD_0].Card->FlowSet.lineBound + 12)) - 1;
    gpRegSD0->IntrSigEn = SDHCI_INTR_EN_INIT;
    ///


    SDHost[SD_0].Card->already_init = true;
}

void sdioh_set_bus_width()
{
	IP_SDIOH->REG_HC1_PCR_BGCR.all &= ~(SDHCI_HC_BUS_WIDTH_8BIT | SDHCI_HC_BUS_WIDTH_4BIT);
#if USE_4BIT_SDIO
	IP_SDIOH->REG_HC1_PCR_BGCR.all |= (SDHCI_HC_BUS_WIDTH_4BIT);
    SDHost[SD_0].Card->bus_width = 4;
#else
    SDHost[SD_0].Card->bus_width = 1;
#endif
}

void sdioh_SetSDClock()
{
	u16 clk = (8 << 8); // div = sdclk / 2 / N
    clk |= SDHCI_CLKCNTL_INTERNALCLK_EN;
    gpRegSD0->ClkCtl = clk;

    ftsdc021_delay(1);

    clk |= SDHCI_CLKCNTL_SDCLK_EN;
    gpRegSD0->ClkCtl = clk;

    gpRegSD0->VendorReg0 &= ~((0x3F << 8) | 0x1);
    gpRegSD0->VendorReg0 |= (1); // | (0x1 << 8));
}

void sdioh_SetPower()
{
    IP_SDIOH->REG_HC1_PCR_BGCR.all &= ~((SDHCI_POWER_ON | SDHCI_POWER_330) << 8);
    IP_SDIOH->REG_HC1_PCR_BGCR.all |= ((SDHCI_POWER_ON | SDHCI_POWER_330) << 8);
}

void sdioh_intr_en()
{
	IP_SDIOH->REG_NISER_EISER.all = (SDHCI_INTR_EN_ALL | (SDHCI_ERR_EN_ALL << 16));
	IP_SDIOH->REG_NISEN_EISEN.all = (SDHCI_INTR_EN_ALL | (SDHCI_ERR_EN_ALL << 16));
}


static int sdio_probe()
{
    sdioh_HCReset();////ftsdc021_HCReset(SD_0, SD_SOFTRST_ALL);

	sdioh_set_bus_width();////ftsdc021_set_bus_width(SD_0, 1);
	sdioh_SetSDClock(); ////ftsdc021_SetSDClock(SD_0, 360000);    //SDHost[ip_idx].min_clk);
	sdioh_SetPower();//// ftsdc021_SetPower(SD_0, 21);      //3.3v
	sdioh_intr_en();////	ftsdc021_intr_en(SD_0, 0);

//	sdioh_ops_go_idle_state();

	ftsdc021_send_command(SD_0, SDHCI_IO_SEND_OP_COND, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R3R4, 0, 0xff8000);

	ftsdc021_send_command(SD_0, SDHCI_SEND_RELATIVE_ADDR, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0);

    ftsdc021_send_command(SD_0, SDHCI_SELECT_CARD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x00010000);

    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x800004fe);

    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x80022080); //function 1 ; block size 0x80(128B)

#if USE_4BIT_SDIO
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x80000e82); //bus width = 4 bits
#else
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x80000e80); //bus width = 1 bit
#endif

	return 0;
}


// Max. size of SRC/DST buffer
#define MaxLen          (2048)

// SRC buffer size
#define SourceLen       100 // (MaxLen-1)

// DST buffer size
#define ReceiveLen      100 // (MaxLen-1)

// unit length
#define UnitLen         4 // 1

#define SDIO_DEFAULT_BLOCK_SIZE  512
#define ENABLE_WIFI_INTR    1 //1:need cmd clk io0 io1 pins; 0:need cmd clk io0 pins
#define USE_4BIT_SDIO       0//1//
#define SDIO_RATE_STAT      0

#define CIS_FUNC_NUM        0
#define WIFI_FUNC_NUM       1
#define READ_DIRECTION      0
#define WRITE_DIRECTION     1

#define _DMA32  __attribute__((aligned(32)))

//static _DMA uint8_t SourceBuf[MaxLen] = {0};
static _DMA32 uint8_t  SourceBuf[MaxLen] = {0};
//static _DMA uint8_t DestinBuf[MaxLen] = {0};
static _DMA32 uint8_t  DestinBuf[MaxLen] = {0};

static _DMA32 uint8_t  DeviceBuf[MaxLen] = {0};

#define SDIO_DIR_WRITE (1)
#define SDIO_DIR_READ  (0)
#define SDIO_TRAN_INC  (1)
#define SDIO_TRAN_FIX  (0)

static void test_task_host( void *pvParameters )
{
	int ret;

	CLOGD("enter %s\n", __FUNCTION__);

	hw_init_host();

	static SDCardInfo ftsdc021_info;
	static _DMA32 uint8_t FTSDC021_IN_BUF[512];
	static _DMA32 uint8_t FTSDC021_CARD_BUF[512];
    SDHost[SD_0].Card = &ftsdc021_info;
	SDHost[SD_0].Card->FlowSet.UseDMA = SDMA;

	memset(SourceBuf, 0x55, sizeof(SourceBuf));
	memset(DestinBuf, 0x11, sizeof(DestinBuf));

	CLOGD("sdio host initiated, wait for semaphore\n");

	// wait for the device ready if both host and device run on the same core
	xSemaphoreTake(sema_dev2host, -1);

	ret = sdio_probe();

	if(ret != 0) {
		CLOGE("sdio probe failed\n");
		while(1)
			;
	}

	int block_cnt = 0;
	int block_sz  = 128;

	while(1)
    {
    	 block_cnt++;
    	 block_cnt %= 5;
    	 if(block_cnt == 0)
    	 	block_cnt = 1;

//    	 block_sz = ((block_sz == 128) ? 64 : 128);

    	 CLOGD("write & read %d blocks - %d\n", block_cnt, block_sz);

    	if(0 != ftsdc021_sdio_CMD53(SD_0, SDIO_DIR_WRITE, 1, (0x9)<<4, SDIO_TRAN_INC, (u32*)SourceBuf, block_cnt, block_sz))
    		CLOGD("write failed\n");

    	ftsdc021_delay(1);

    	if(0 != ftsdc021_sdio_CMD53(SD_0, SDIO_DIR_READ, 1, 1<<4, SDIO_TRAN_INC, (u32*)DestinBuf, block_cnt, block_sz))
    		CLOGD("read failed\n");

    	ftsdc021_delay(1);

    	for(int i = 0; i < sizeof(SourceBuf); i++)
    		SourceBuf[i] = DestinBuf[i] + 1;

    	ftsdc021_delay(5);
    }
}



// device side
#define BUFFER_SIZE            (1024 * 4)

_DMA32 uint8_t data_to_send[BUFFER_SIZE];
_DMA32 uint8_t data_to_recv[BUFFER_SIZE];

sdio_buf_t sdio_buf = {0};

volatile int flag_data_recv = 0;
volatile int flag_data_send = 0;

static void sdio_device_event_callback(uint32_t evt)
{
    SDIOD_HandleTypeDef *sdio_dev = SDIOD_Instance();
	if((evt & BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT)== BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT) {
		if(sdio_dev->cmd_info.dir == SDIO_TRANS_HOST2DEV)
		{
			flag_data_recv = 1;
		}
		else
		{
			flag_data_send = 1;
		}
	}
}



static void hw_init_device()
{
	disable_IRQ(IRQ_SDIOD_VECTOR);
	IP_CMN_SYS->REG_SW_RESET_CP2.bit.SMID_RESET = 1;

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER14);  //sd_clk
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER14);  //sd_cmd

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER14);  //sd_dat0
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER14);  //sd_dat1

}


static void test_task_device( void *pvParameters )
{
	CLOGD("enter %s\n", __FUNCTION__);

	hw_init_device();

    SDIOD_HandleTypeDef *sdio_dev = NULL;

    sdio_buf.data_send = data_to_send;
    sdio_buf.data_recv = data_to_recv;

    sdio_device_config_t config = {
    	.buf_info           = &sdio_buf,
        .send_buffer_size   = BUFFER_SIZE,
        .recv_buffer_size   = BUFFER_SIZE,
        .event_cb           = sdio_device_event_callback,
    };

    sdio_dev = sdio_device_initialize(&config);
    if(sdio_dev == NULL)
    {
    	CLOGD("can't initialize sdio device");
    	while(1)
    		;
    }
    int ret = sdio_device_start();
    if(ret != CSK_DRIVER_OK)
    {
    	CLOGD("can't start sdio device");
    	while(1)
    		;
    }

    CLOGD("sdio device ready");
	xSemaphoreGive(sema_dev2host);

    memset(data_to_send, 0, BUFFER_SIZE);

    for(;;) {
        // each byte in the receive buffer will be added by 1 and stored in the send buffer
    	if(flag_data_recv){
    		flag_data_recv = 0;

    		sdio_dev->buf_used->size_recv = sdio_dev->cmd_info.block_size * sdio_dev->cmd_info.block_cnt;

    		if(sdio_dev->buf_used->size_recv > 0)
    		{
    			if(sdio_dev->buf_used->size_recv > config.recv_buffer_size)
    			{
    				while(1)
    					;
    			}

    			for(int i = 0; i < sdio_dev->buf_used->size_recv; i++)
    			{
    				sdio_dev->buf_used->data_send[i] = sdio_dev->buf_used->data_recv[i] + 1;
    			}
    			sdio_dev->buf_used->size_recv = 0;
    		}
    		sdio_dev->Instance->REG_SMID_CONTROL_REG.bit.PROGRAM_DONE = 0x1;
    	}

    	if(flag_data_send)
    	{
    		flag_data_send = 0;
    	}

    }
}


int main( void )
{
	logInit(0, 115200);
	CLOGD("SDIO Test");

	disable_IRQ(IRQ_SDIOH_VECTOR);
	IP_AP_CFG->REG_SW_RESET.bit.SDIOH_RESET = 1;

	sema_dev2host = xSemaphoreCreateBinary();
	if (sema_dev2host == NULL) {
		CLOGD("fail to create semaphore.\n");
		while(1)
			;
	}

	// suppose the sdio device is ready to communicate with
	xSemaphoreGive(sema_dev2host);

	xTaskCreate(
			test_task_host,	/* The function that implements the task. */
			"TaskHost",					/* Text name for the task. */
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
}

// The above main function only runs sdio_host task, which requires sdio_device
// task is running on another core(either in the same chip or in another chip).
// The following main function can run both host and device tasks in the meantime.

//int main( void )
//{
//	logInit(0, 115200);
//	CLOGD("SDIO Test");
//
//	sema_dev2host = xSemaphoreCreateBinary();
//    if (sema_dev2host == NULL) {
//    	CLOGD("fail to create semaphore.\n");
//    	while(1)
//    		;
//    }
//
//	xTaskCreate(
//			test_task_host,	/* The function that implements the task. */
//			"TaskHost",					/* Text name for the task. */
//			1024,						/* Stack depth in words. */
//			NULL,						/* Task parameters. */
//			3,							/* Priority and mode (user in this case). */
//			NULL						/* Handle. */
//		);
//
//	xTaskCreate(
//			test_task_device,	/* The function that implements the task. */
//			"TaskDevice",			    /* Text name for the task. */
//			1024,						/* Stack depth in words. */
//			NULL,						/* Task parameters. */
//			3,							/* Priority and mode (user in this case). */
//			NULL						/* Handle. */
//		);
//
//	/* Start the scheduler. */
//	vTaskStartScheduler();
//
//	/* Will only get here if there was insufficient memory to create the idle
//	task. */
//	for( ;; );
//}


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

