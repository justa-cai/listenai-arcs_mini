/*
 * crypto_nos_chk.c
 *
 *  Created on: 2020/10/9
 *      Author: USER
 */
#include "dbg_assert.h"
#include "chip.h"
#include "Driver_CRYPTO.h"
#include "crypto_nos_chk.h"
#include "log_print.h"
#include "systick.h"
#include "efuse_ctrl_reg.h"
#include "ap_cfg_reg.h"
#include "IOMuxManager.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CFG_RTOS
#include <FreeRTOS.h>
#include <semphr.h>
#endif

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 100000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static function test_function_array[] = {
        // aes test functions
        CRYPTO0_AES128_ECB_Encrypt_User_Key,
        CRYPTO0_AES128_ECB_Decrypt_User_Key,
        CRYPTO0_AES192_ECB_Encrypt_User_Key,

        //CRYPTO0_AES128_Decrypt_Flash_Test,
        //CRYPTO0_AES128_Decrypt_PSRAM_Test,
        //CRYPTO0_PSRAM_Cihper_Region_Validation, // about 25 minutes

        CRYPTO0_AES192_CBC_Encrypt_User_Key,
        CRYPTO0_AES192_CBC_Decrypt_User_Key,
        CRYPTO0_AES256_CBC_Encrypt_User_Key,
        CRYPTO0_AES256_CBC_Encrypt_Efuse2_Key,

        CRYPTO0_AES192_CTR_Encrypt_User_Key,
        CRYPTO0_AES256_CTR_Encrypt_Efuse_User_Key,
        CRYPTO0_AES128_CCM_Encrypt_User_Key,
        CRYPTO0_AES256_CCM_Encrypt_User_Key,
        CRYPTO0_AES192_GCM_Encrypt_User_Key,
        CRYPTO0_AES256_GCM_Encrypt_User_Key,

        CRYPTO0_AES128_CMAC_Encrypt_User_Key,
        CRYPTO0_AES192_CMAC_Encrypt_User_Key,
        CRYPTO0_AES256_CMAC_Encrypt_User_Key,

        // sha test functions
        CRYPTO0_SHA1_LittleEndian_Test,
        CRYPTO0_SHA224_LittleEndian_Test,
        CRYPTO0_SHA256_LittleEndian_Test,

        CRYPTO0_SHA224_LittleEndian_LongStream_Test,
        CRYPTO0_SHA256_LittleEndian_LongStream_Light_Test,

        CRYPTO0_SHA384_LittleEndian_Test,
        CRYPTO0_SHA512_LittleEndian_Test,

        CRYPTO0_SHA384_LittleEndian_LongStream_Test,
        CRYPTO0_SHA512_LittleEndian_LongStream_Test,

        // hmac test functions
        CRYPTO0_HMAC_SHA1_LittleEndian_Test,
        CRYPTO0_HMAC_SHA256_LittleEndian_Test,
        CRYPTO0_HMAC_SHA512_LittleEndian_Test,

        CRYPTO0_HMAC_SHA224_LittleEndian_LongStream_Test,
        CRYPTO0_HMAC_SHA384_LittleEndian_LongStream_Test,

        // ecc test functions
        CRYPTO0_MOD_OPERATE_512_LittleEndian_Test,

        CRYPTO0_ECC_P192_Generate_Key,
        CRYPTO0_ECC_P224_Generate_Key,
        CRYPTO0_ECC_P512_Generate_Key,
        CRYPTO0_ECC_USER_CURVE_Generate_Key, // 320bit

        CRYPTO0_ECC_P224_ADD,
        CRYPTO0_ECC_P256_Multiply,
        CRYPTO0_ECC_USER_CURVE_Multiply, // 160bit, 128bit

        CRYPTO0_ECC_ECDH192_Test,
        CRYPTO0_ECC_ECDH384_Test,

        CRYPTO0_ECC_ECSDA256_Verify_Signature,
        //CRYPTO0_ECC_ECSDA256_Flash_Verify_Signature,

        // rsa test functions
        CRYPTO0_RSA1024_Encrypt_BigEndian,
        CRYPTO0_RSA2048_Encrypt_BigEndian,
        CRYPTO0_RSA4096_Encrypt_BigEndian,
        CRYPTO0_RSA2048_Signature_BigEndian,
        //CRYPTO0_ECC_RSA2048_Flash_Verify_Signature,
        CRYPTO0_MOD_EXP_Little_Endian,

        // hsu test fucntions
        CRYPTO0_HSU_TKIP_Test1,
        CRYPTO0_HSU_TKIP_Test2,
        CRYPTO0_HSU_TKIP_Test3,
        CRYPTO0_HSU_TKIP_Test4,
        CRYPTO0_HSU_IP_CHK_Test,
};

void* CRYPTO0_Handler = NULL;

static void CRYPTO_Init_Handler(void){
    CRYPTO0_Handler = CRYPTO0();
}

static volatile int32_t CRYPTO_Result = CSK_DRIVER_OK;

#ifndef CFG_RTOS
static volatile uint32_t CRYPTO_BUSY = 0;
static volatile uint32_t CRYPTO_DONE = 0;
static int32_t CRYPTO_EventCallback_NOS(uint32_t event, int32_t result, void* workspace)
{
    if(CSK_CRYPTO_EVENT_WAIT_BUSY == event)
    {
        // wait other procedure finish
        while(CRYPTO_BUSY);
        CRYPTO_BUSY = 1;
    }
    else if(CSK_CRYPTO_EVENT_FINISHED == event)
    {
        CRYPTO_BUSY = 0;
    }
    else if(CSK_CRYPTO_EVENT_WAIT_DONE == event)
    {
        while(!CRYPTO_DONE);
        CRYPTO_DONE = 0;
        return CRYPTO_Result;
    }
    else if(CSK_CRYPTO_EVENT_DONE == event)
    {
        CRYPTO_Result = result;
        CRYPTO_DONE = 1;
    }

    return CSK_DRIVER_OK;
}
#else
static SemaphoreHandle_t CRYPTO_HW_semaphore;
static SemaphoreHandle_t CRYPTO_SW_semaphore;
static int32_t CRYPTO_EventCallback_RTOS(uint32_t event, int32_t result, void* workspace){
    CLOGD("CRYPTO_EventCallback_RTOS, event:%d", event);
    if(CSK_CRYPTO_EVENT_WAIT_BUSY == event)
    {
        // wait other procedure finish
        xSemaphoreTake(CRYPTO_HW_semaphore, -1);
    }
    else if(CSK_CRYPTO_EVENT_FINISHED == event)
    {
        xSemaphoreGive(CRYPTO_HW_semaphore);
    }
    else if(CSK_CRYPTO_EVENT_WAIT_DONE == event)
    {
        xSemaphoreTake(CRYPTO_SW_semaphore, -1);
        return CRYPTO_Result;
    }
    else if(CSK_CRYPTO_EVENT_DONE == event)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        CRYPTO_Result = result;
        xSemaphoreGiveFromISR(CRYPTO_SW_semaphore, &xHigherPriorityTaskWoken);
    }

    return CSK_DRIVER_OK;
}


static void test_task1( void *pvParameters )
{
    uint32_t times = 0;
    UBaseType_t uxHighWaterMark = 0;

    CLOGD("enter test_task1\n");
    static int cnt_1 = 0;
    while(1) {
        cnt_1++;
        for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0])-1; times++){
#if INCLUDE_uxTaskGetStackHighWaterMark
            uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
#endif
            CLOGD("task1 uxHighWaterMark=%d\n", uxHighWaterMark);
            test_function_array[times]();
            vTaskDelay(50);
        }
    }
}

static void test_task2( void *pvParameters )
{
    uint32_t times = 0;
    UBaseType_t uxHighWaterMark=0;

    CLOGD("enter test_task2\n");
    static int cnt_1 = 0;
    while(1) {
#if INCLUDE_uxTaskGetStackHighWaterMark
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
#endif
        CLOGD("task2 uxHighWaterMark=%d\n", uxHighWaterMark);
        times = rand()%(sizeof(test_function_array)/sizeof(test_function_array[0])-1);
        test_function_array[times]();
        vTaskDelay(100);
    }
}

static void test_task3( void *pvParameters )
{
    uint32_t times = 0;
    UBaseType_t uxHighWaterMark=0;

    CLOGD("enter test_task3\n");
    static int cnt_1 = 0;
    while(1) {
        cnt_1 ++;
        if(cnt_1 >= 100)
        {
#if INCLUDE_uxTaskGetStackHighWaterMark
            uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
#endif
            CLOGD("task3 uxHighWaterMark=%d\n", uxHighWaterMark);
            cnt_1 = 0;
        }
        CRYPTO0_HSU_IP_CHK_Test();
        vTaskDelay(200);
    }
}
#endif

int main(){
    int i;
    uint32_t times;
    //cm_backtrace_init("crypto_nos_chk", "B0", "1");
    logInit(0, 115200);
    CLOGD("CRYPTO VALIDATION");

#ifdef CFG_RTOS
    CRYPTO_HW_semaphore = xSemaphoreCreateBinary();
    CRYPTO_SW_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(CRYPTO_HW_semaphore);
    //xSemaphoreGive(CRYPTO_SW_semaphore);
#endif

    CRYPTO_Init_Handler();
#ifndef CFG_RTOS
    CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_EventCallback_NOS, NULL);
#else
    CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_EventCallback_RTOS, NULL);
#endif

    //CRYPTO0_AES_Write_Efuse_Key();

#if 0 // debug port
    for(i=16; i<22; i++)
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, i, 19);
    for(i=28; i<32; i++)
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, i, 19);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, 19);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, 19);
    for(i=0; i<5; i++)
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, i, 19);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 18);
#endif

    SysTick_Open(SYSTICK_MAX_INT);
    //enable_GINT();
    uint32_t systime_1 = SysTick_Value();

#ifndef CFG_RTOS
    //DisableDCache();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
#else
    xTaskCreate(
            test_task1, /* The function that implements the task. */
            "Task1",                    /* Text name for the task. */
            256 * 4,                        /* Stack depth in words. */
            NULL,                       /* Task parameters. */
            3,                          /* Priority and mode (user in this case). */
            NULL                        /* Handle. */
        );
    xTaskCreate(
            test_task2, /* The function that implements the task. */
            "Task2",                    /* Text name for the task. */
            256 * 4,                        /* Stack depth in words. */
            NULL,                       /* Task parameters. */
            3,                          /* Priority and mode (user in this case). */
            NULL                        /* Handle. */
        );
    xTaskCreate(
            test_task3, /* The function that implements the task. */
            "Task3",                    /* Text name for the task. */
            256,                        /* Stack depth in words. */
            NULL,                       /* Task parameters. */
            3,                          /* Priority and mode (user in this case). */
            NULL                        /* Handle. */
        );

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* Will only get here if there was insufficient memory to create the idle
    task. */
    for( ;; );
#endif

    uint32_t systime_2 = SysTick_Value();
    SysTick_Close();
    CRYPTO_Uninitialize(CRYPTO0_Handler);
    CLOGD("CRYPTO VALIDATION FINISHED, total time %dms", (systime_2-systime_1)/1000);
    while(1);
}

#ifdef CFG_RTOS
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
#endif
