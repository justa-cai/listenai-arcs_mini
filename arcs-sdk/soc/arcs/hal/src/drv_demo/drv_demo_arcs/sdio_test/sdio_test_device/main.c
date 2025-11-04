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

#define _DMA64  __attribute__((aligned(64)))

#define BUFFER_SIZE            (1024 * 2)


_DMA64 volatile uint8_t data_to_send[BUFFER_SIZE];
_DMA64 volatile uint8_t data_to_recv[BUFFER_SIZE];

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
	__HAL_CRM_SMID_CLK_DISABLE(); //disable clock
	IP_CMN_SYS->REG_SW_RESET_CP2.bit.SMID_RESET = 1;

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER14);  //sd_clk
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_ALTER14);  //sd_cmd

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER14);  //sd_dat0
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER14);  //sd_dat1
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER14);  //sd_dat2
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER14);  //sd_dat3
	__HAL_CRM_SMID_CLK_ENABLE(); //enable clock

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
    		vTaskDelay(1);
    		sdio_dev->Instance->REG_SMID_CONTROL_REG.bit.PROGRAM_DONE = 0x1;
    	}

    	if(flag_data_send)
    	{
    		flag_data_send = 0;
    	}

    }
}

// Enable SDIO_MEM_DUMP_ENABLE if only the IO and functions should be enabled
// in memory dumping mode
//#define SDIO_MEM_DUMP_ENABLE
int main( void )
{
#ifdef SDIO_MEM_DUMP_ENABLE
	disable_IRQ(IRQ_SDIOD_VECTOR);
	IP_CMN_SYS->REG_SW_RESET_CP2.bit.SMID_RESET = 1;

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER14);  //sd_clk
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER14);  //sd_cmd

	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER14);  //sd_dat0
	IOMuxManager_PinConfigure (CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER14);  //sd_dat1

	__HAL_CRM_SMID_CLK_ENABLE(); //enable clock


	IP_SDIOD->REG_SMID_CARD_RDY.bit.FUNCTION1_READY       = 0x1; //enable io1
	IP_SDIOD->REG_SMID_CARD_RDY.bit.FUNCTION2_READY       = 0x1; //enable io2

	IP_SDIOD->REG_SMID_INT_STAT_EN.all = 0x0;
	IP_SDIOD->REG_SMID_INT_SIG_EN.all = 0x0;
	IP_SDIOD->REG_SMID_INT_STAT_EN2.all =0x0;
	IP_SDIOD->REG_SMID_INT_SIG_EN2.all =0x0;

	while(1)
		;
#endif

	logInit(0, 115200);
	CLOGD("SDIO Test");


	xTaskCreate(
			test_task_device,	/* The function that implements the task. */
			"TaskDevice",			    /* Text name for the task. */
			512,						/* Stack depth in words. */
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

