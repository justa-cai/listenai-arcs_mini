#include <stdio.h>
#include <stdint.h>
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
#include "ftsdc021_sdio.h"


#define SDIO_DIR_WRITE (1)
#define SDIO_DIR_READ  (0)
#define SDIO_TRAN_INC  (1)
#define SDIO_TRAN_FIX  (0)

#define SDIO_ADMA_ACT_NOP   (0b00)
#define SDIO_ADMA_ACT_RSV   (0b01)
#define SDIO_ADMA_ACT_TRANS (0b10)
#define SDIO_ADMA_ACT_LINK  (0b11)

//DSR[4] sdio_debug_adma_en
//DSR[3] sdio_debug_sdma_en
//DSR[2] dbg_dma_valid
//DSR[1] sdio_debug_program done
//DSR[0] sdio_debug_mode
#define SDIO_DEBUG_MODE   (1 << 0)
#define SDIO_DEBUG_PRGM   (1 << 1)
#define SDIO_DEBUG_VALID  (1 << 2)
#define SDIO_DEBUG_SDMA   (1 << 3)
#define SDIO_DEBUG_ADMA   (1 << 4)


struct sdio_adma_desc {
	union {
		uint32_t sdio_adma_desc_low32;
        struct {
            uint32_t bit_valid             : 1; // bit 0
            uint32_t bit_end               : 1; // bit 1
            uint32_t bit_int               : 1; // bit 2
            uint32_t bit_reserved0         : 1; // bit 3
            uint32_t bit_act               : 2; // bit 4~5
            uint32_t bit_reserved1         : 10; // bit 6~15
            uint32_t bit_length            : 16; // bit 16~31
        };
	};

    uint32_t sdio_adma_desc_high32;
};


#define _DMA32  __attribute__((aligned(32)))
#define USE_4BIT_SDIO   0//1//
// Max. size of SRC/DST buffer
#define MaxLen          (4096)


static _DMA32 uint8_t  SourceBuf[MaxLen] = {0};
static _DMA32 uint8_t  DestinBuf[MaxLen] = {0};

extern volatile ftsdc021_reg* gpRegSD0;// = (ftsdc021_reg*) SDC_FTSDC021_0_PA_BASE;


// host side
static void hw_init_host()
{
	disable_IRQ(IRQ_SDIOH_VECTOR);
	IP_AP_CFG->REG_SW_RESET.bit.SDIOH_RESET = 1;


	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_ALTER15);  //sd_clk
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER15);  //sd_cmd

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_ALTER15);  //sd_dat0
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_ALTER15);  //sd_dat1
#if USE_4BIT_SDIO
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_ALTER15);  //sd_dat2
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 23, CSK_IOMUX_FUNC_ALTER15);  //sd_dat3
#endif

	HAL_CRM_SetSdio_hClkSrc(CRM_IpSrcXtalClk); ////select xtal as the clock //0x1; //select syspll as the clock
	__HAL_CRM_SDIO_H_CLK_ENABLE(); //enable clock

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

	IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN       = 0x1;
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW     = 0x1; // Set SD power enable
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL     = 0x3; // Set SD power enable

    /* Reset the controller */
    gpRegSD0->SoftRst |= (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT);
    for (i = 0; i < 100; i++) {
        if ((gpRegSD0->SoftRst & (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT)) == 0)
            break;
        ftsdc021_delay(2);
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

    ftsdc021_delay(2);

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


void sdio_rw_mem(unsigned int dma_addr, unsigned int dma_data){
	CLOG("dma_addr = 0x%x; dma_data = 0x%x\n", dma_addr, dma_data);

	//**************CONFIG sdio device DMA/ADMA addr*********//
	//Argument[7:0] are data to regs FN2 reg0 use for dma addr
	unsigned int arg;
	arg =  (dma_addr & 0xFF) + 0xa0024000;
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
    ftsdc021_delay(2);
    arg =  ((dma_addr >>8) & 0xFF) + 0xa0024200;
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
    ftsdc021_delay(2);
    arg =  ((dma_addr >>16) & 0xFF) + 0xa0024400;
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
    ftsdc021_delay(2);
    arg =  ((dma_addr >>24) & 0xFF) + 0xa0024600;
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
    ftsdc021_delay(2);


    //Open sdio debug mode
    ftsdc021_send_command(SD_0, 4, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_NO_RESPONSE, 0, ((SDIO_DEBUG_MODE) << 16));
    ftsdc021_delay(2);

    {
        uint32_t multi_blk = 0, blk_cnt_en = 0;
        uint32_t blocks = 1;
        uint32_t blksz = 4;

        SDCardInfo* card = SDHost[SD_0].Card;
        SDHost[SD_0].Card->FlowSet.timeout_ms = 7;

        sd_mutex_lock(SD_0);
        arg = 0x80000000;
        arg |= (1 << 28);  // function 1
        arg |= 0x04000000; // incrementing address
        arg |= ((0x1 << 4) << 9); // 0x10 - block length register
		arg |= blksz; // byte or block count //(0x08000000 | blocks); /* block mode */

		blk_cnt_en = 0;//1;
		multi_blk = 0;//1;


        ftsdc021_set_transfer_mode(SD_0, blk_cnt_en, 0,
                SDHCI_TXMODE_WRITE_DIRECTION,
                multi_blk);
        ftsdc021_prepare_data(SD_0, blocks, blksz, (u32) (&dma_data), WRITE);

        /* CMD 53 */
        if (ftsdc021_send_command(SD_0, SDHCI_IO_RW_EXTENDED,
                SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg)) {
            sd_mutex_unlock(SD_0);
            return;
        }

        if (ftsdc021_transfer_data(SD_0, WRITE, &dma_data, (blocks * blksz)));

        sd_mutex_unlock(SD_0);
    }

    ftsdc021_delay(2);

    // CMD4 config dma valid 0xd0000
    ftsdc021_send_command(SD_0, 4, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_NO_RESPONSE, 0, \
    		((SDIO_DEBUG_MODE | SDIO_DEBUG_VALID | SDIO_DEBUG_SDMA) << 16));
    ftsdc021_delay(2);  //finish

    // CMD4 config dma valid 0xf0000
    ftsdc021_send_command(SD_0, 4, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_NO_RESPONSE, 0, \
    		((SDIO_DEBUG_MODE | SDIO_DEBUG_PRGM | SDIO_DEBUG_VALID | SDIO_DEBUG_SDMA) << 16));
    ftsdc021_delay(2);  //finish
}

void sdio_cmd53_adma(unsigned int dma_addr, unsigned int* recv_buf) {
	//**************CONFIG sdio device DMA/ADMA addr*********//
	//Argument[7:0] are data to regs FN2 reg0 use for dma addr
	unsigned int arg;
	arg =  (dma_addr & 0xFF) + 0xa0024000;
	ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
	ftsdc021_delay(2);
	arg =  ((dma_addr >>8) & 0xFF) + 0xa0024200;
	ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
	ftsdc021_delay(2);
	arg =  ((dma_addr >>16) & 0xFF) + 0xa0024400;
	ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
	ftsdc021_delay(2);
	arg =  ((dma_addr >>24) & 0xFF) + 0xa0024600;
	ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
	ftsdc021_delay(2);


    ftsdc021_send_command(SD_0, 4, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_NO_RESPONSE, 0, \
    		((SDIO_DEBUG_ADMA) << 16));
    ftsdc021_delay(2);  //finish

    ftsdc021_send_command(SD_0, 4, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_NO_RESPONSE, 0, \
    		((SDIO_DEBUG_MODE | SDIO_DEBUG_ADMA) << 16));
    ftsdc021_delay(2);  //finish

    {
        uint32_t multi_blk = 0, blk_cnt_en = 0;
        uint32_t blocks = 2;
        uint32_t blksz = 128;

        SDCardInfo* card = SDHost[SD_0].Card;
        sd_mutex_lock(SD_0);
        arg = 0x00000000;
        arg |= (1 << 28);
		arg |= (0x08000000 | blocks); /* block mode */

		blk_cnt_en = 1;
		multi_blk = 1;


        ftsdc021_set_transfer_mode(SD_0, blk_cnt_en, 0,
                SDHCI_TXMODE_READ_DIRECTION,
                multi_blk);
        ftsdc021_prepare_data(SD_0, blocks, blksz, (u32) recv_buf, READ);

        /* CMD 53 */
        if (ftsdc021_send_command(SD_0, SDHCI_IO_RW_EXTENDED,
                SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg)) {
            sd_mutex_unlock(SD_0);
            return;
        }

        if (ftsdc021_transfer_data(SD_0, READ, recv_buf, (blocks * blksz)));

        sd_mutex_unlock(SD_0);
    }
    ftsdc021_delay(2);

}


static void test_task_mem_dump( void *pvParameters )
{
	int ret;

	CLOGD("enter %s\n", __FUNCTION__);

	hw_init_host();

	static _DMA32 uint8_t FTSDC021_IN_BUF[512];
	static _DMA32 uint8_t FTSDC021_CARD_BUF[512];
    SDHost[SD_0].Card = (SDCardInfo*) (FTSDC021_CARD_BUF);
	SDHost[SD_0].Card->FlowSet.UseDMA = SDMA;

	memset(SourceBuf, 0x55, sizeof(SourceBuf));
	memset(DestinBuf, 0x11, sizeof(DestinBuf));


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
    //function 1 ; block size 0x04
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x90022004);//0x80022004);
    //enable function 2
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x8000043e);



    static _DMA32 uint8_t  DeviceDes[MaxLen] = {0};
    static _DMA32 uint8_t  DeviceBuf[MaxLen] = {0};

	memset(SourceBuf, 0x55, sizeof(SourceBuf));
	memset(DestinBuf, 0x00, sizeof(DestinBuf));

	memset(DeviceDes, 0x00, sizeof(DeviceDes));
	for(int i = 0; i < sizeof(DeviceBuf); i++)
		DeviceBuf[i] = ((uint8_t)i);


    struct sdio_adma_desc desc1 = {0};
    struct sdio_adma_desc desc2 = {0};

    desc1.bit_valid = 1;
    desc1.bit_act = SDIO_ADMA_ACT_TRANS;
    desc1.bit_length = 128 * 2;
    desc1.sdio_adma_desc_high32 = ((uint32_t)(DeviceBuf) + 0x200);

    desc2.bit_end = 1;


    // write the adma descriptor 1 - transfer
    sdio_rw_mem(DeviceDes + 0x100, desc1.sdio_adma_desc_low32);
    sdio_rw_mem(DeviceDes + 0x104, desc1.sdio_adma_desc_high32);
    // write the adma descriptor 2 - end of transfer
    sdio_rw_mem(DeviceDes + 0x108, desc2.sdio_adma_desc_low32);


    //use cmd52 config block size to 0x80
    ftsdc021_send_command(SD_0, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0x80022080);
	ftsdc021_delay(2);

    sdio_cmd53_adma(DeviceDes+0x100, DestinBuf+0x100);


	while(1)
    {

    }
}


int main( void )
{
	logInit(0, 115200);
	CLOGD("SDIO Memory Dumping Test");

	xTaskCreate(
			test_task_mem_dump,	/* The function that implements the task. */
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

