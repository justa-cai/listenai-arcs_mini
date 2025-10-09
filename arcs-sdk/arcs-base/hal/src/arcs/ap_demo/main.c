/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "chip.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include "ota_config.h"
#include "ota.h"
#include "spiflash.h"
#include "ipc.h"
#include "mrpc.h"
#include "flash_if.h"
#include "ic_lock.h"

#ifdef CONFIG_QEMU_N300
#define CLOGD
#endif
#define FLASH_IF_TEST        1
#define FLASH_IF_TEST_SIZE   128
#define NVDS_FLASH_ADDRESS   (0x300000)

extern const ls_ota_header_t ap_main_header;
static SemaphoreHandle_t sema_task12;
#if FLASH_IF_TEST
uint32_t flash_rbuff[FLASH_IF_TEST_SIZE];
uint32_t flash_wbuff[FLASH_IF_TEST_SIZE];
#endif

#if FLASH_IF_TEST
FLASH_DEV arcs_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout =2000000,// 0x180000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

int arcs_flash_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    return 0;
}
#endif
static void test_task1( void *pvParameters )
{
    uint32_t i = 0;
    int cnt_1 = 0;

#if FLASH_IF_TEST
    arcs_flash_init();
#endif
    logDbg("enter test_task1\n");
    while(1) {
        vTaskDelay(2000);
        cnt_1++;
        logDbg("%s: %d\n", __FUNCTION__, cnt_1);

        if((cnt_1 % 2) == 0) {
#if FLASH_IF_TEST
            for (i = 0; i < sizeof(flash_rbuff)/4; i++)
            {
                flash_wbuff[i] = cnt_1 + i;
            }
            flash_if_write_protection_set(false);
            if (!flash_if_erase(NVDS_FLASH_ADDRESS, sizeof(flash_wbuff)))
            {
                logDbg("Write data: 0x%08x\n", cnt_1);
                if (flash_if_write(NVDS_FLASH_ADDRESS, flash_wbuff, sizeof(flash_wbuff)))
                {
                    logDbg("Failed to write flash\n");
                }
            }
            else
            {
                logDbg("Failed to erase flash\n");
            }
            flash_if_write_protection_set(true);
#endif
            xSemaphoreGive(sema_task12);
        }
    }
}

static void test_task2( void *pvParameters )
{
    uint32_t i = 0;
    int cnt_2 = 0;

    logDbg("enter test_task2\n");
    while(1) {
        xSemaphoreTake(sema_task12, -1);
        cnt_2++;
        logDbg("%s: %d\n", __FUNCTION__, cnt_2);
#if FLASH_IF_TEST
        memset(flash_rbuff, 0, sizeof(flash_rbuff));
        if (!flash_if_read(NVDS_FLASH_ADDRESS, flash_rbuff, sizeof(flash_rbuff)))
        {
            logDbg("Get data:\n");
            for (i = 0; i < sizeof(flash_rbuff)/4; i++)
            {
                if (i && ((i & 0x7) == 0))
                    logDbg("\n");
                logDbg("%08x ", flash_rbuff[i]);
            }
            logDbg("\n");
        }
        else
        {
            logDbg("Failed to read flash\n");
        }
#endif
    }
}

int main( void )
{
    ipc_mem_init(0);
    ic_lock_init();
#ifndef CONFIG_QEMU_N300
    logInit(1, 115200);
#endif
    CLOGD("FreeRTOS Test, OTA version = 0x%x", ap_main_header.version.version);
    ipc_master_init(NULL);
    BaseType_t xResult;

#ifdef CFG_AMP_IPC_MRPC_CLIENT
    mrpc_client_init(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_CMN_CLT, IPC_EP_MRPC_CMN_SRV);
#endif

    sema_task12 = xSemaphoreCreateBinary();
    if (sema_task12 == NULL) {
        CLOGD("fail to create semaphore.\n");
        while(1)
            ;
    }

    xResult = xTaskCreate(
			test_task1,	/* The function that implements the task. */
			"Task1",					/* Text name for the task. */
			256,						/* Stack depth in words. */
			NULL,						/* Task parameters. */
			3,							/* Priority and mode (user in this case). */
			NULL						/* Handle. */
		);

    if( xResult != pdPASS ) {
    	CLOGD("failed to create Task1.\n");
    }

    xResult = xTaskCreate(
			test_task2,	/* The function that implements the task. */
			"Task2",					/* Text name for the task. */
			256,						/* Stack depth in words. */
			NULL,						/* Task parameters. */
			4,							/* Priority and mode (user in this case). */
			NULL						/* Handle. */
		);

    if( xResult != pdPASS ) {
    	CLOGD("failed to create Task2.\n");
    }
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

extern void _start();

/// sign data size: CRC32 - 4, SHA256 - 32, ECSDA256 - 128, RSA2048 - 512
SIGN_DATA const uint32_t sign_data[512/4] =
{
        0
};


OTA_HEADER const ls_ota_header_t ap_main_header = {
        .valid_flag = 0xffffffff,
        .version = {.vendor_id = OTA_VENDOR_ID,
                    .device_id = OTA_DEVICE_ID,
                    .flash_id  = 1,
                    .zone_id   = OTA_ZONE_ID_AP,
                    .rom_ver   = 0x0100,
                    .version   = 0x01010001,
                    .date      = BUILD_DATE},
        .flags   = OTA_MODE,
        .address = (uint32_t)&ap_main_header,
        .entry   = (uint32_t)_start,
};
