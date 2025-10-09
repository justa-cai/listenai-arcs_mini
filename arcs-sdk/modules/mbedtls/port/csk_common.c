#include <stdint.h>
#include <stdbool.h>

#include "Driver_CRYPTO.h"
#include "crypto.h"

#include "log_print.h"
#include "csk_common.h"
#if 0
#include "FreeRTOS.h"
#include "semphr.h"

#if CONFIG_MBEDTLS_HARDWARE_IC_MUTEX
#include "ic_mutex.h"
#endif

// #define USE_FREERTOS CONFIG_MODULE_FREERTOS
#define USE_FREERTOS 0

#if CONFIG_MBEDTLS_HARDWARE_IC_MUTEX
static IC_Mutex crypto_ic_mutex;
#endif

static void* CRYPTO0_Handler = NULL;
static bool crypto_hardware_init_flag = false;
#if CONFIG_MBEDTLS_HARDWARE_IC_MUTEX
#else
static SemaphoreHandle_t crypto_sem;
#endif
static bool is_crypto_sem_get = false;

static void CRYPTO_Init_Handler(void){
    CRYPTO0_Handler = CRYPTO0();
}

#if USE_FREERTOS
static SemaphoreHandle_t CRYPTO_res_semaphore;
static SemaphoreHandle_t CRYPTO_tsk_semaphore;
static volatile int32_t CRYPTO_Result = CSK_DRIVER_OK;
static int32_t CRYPTO_EventCallback_RTOS(uint32_t event, int32_t result, void* workspace)
{
    //CLOGD("CRYPTO_EventCallback_RTOS, event:%d", event);
    if(CSK_CRYPTO_EVENT_WAIT_BUSY == event)
    {
        // wait other procedure finish
        // CLOG("cp wait busy");
        xSemaphoreTake(CRYPTO_res_semaphore, -1);
    }
    else if(CSK_CRYPTO_EVENT_FINISHED == event)
    {
        // CLOG("cp finished");
        xSemaphoreGive(CRYPTO_res_semaphore);
    }
    else if(CSK_CRYPTO_EVENT_WAIT_DONE == event)
    {
        // CLOG("cw");
        xSemaphoreTake(CRYPTO_tsk_semaphore, -1);
        return CSK_DRIVER_OK;
    }
    else if(CSK_CRYPTO_EVENT_DONE == event)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(CRYPTO_tsk_semaphore, &xHigherPriorityTaskWoken);
    }

    return CSK_DRIVER_OK;
}
#else
static int32_t CRYPTO_EventCallback_NOS(uint32_t event, int32_t result, void* workspace)
{
    static volatile int32_t CRYPTO_Result = CSK_DRIVER_OK;
    static volatile uint32_t CRYPTO_BUSY = 0;
    static volatile uint32_t CRYPTO_DONE = 0;
    
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
#endif

crypto_handler csk_crypto_acquire_hardware( csk_crypto_type_e type )
{
    if (!crypto_hardware_init_flag) {
    #if USE_FREERTOS
        CRYPTO_res_semaphore = xSemaphoreCreateBinary();
        CRYPTO_tsk_semaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(CRYPTO_res_semaphore);
        //xSemaphoreGive(CRYPTO_tsk_semaphore);
    #endif

        CRYPTO_Init_Handler();

    #if USE_FREERTOS
        CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_EventCallback_RTOS, NULL);
    #else
        CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_EventCallback_NOS, NULL);
    #endif

    #if CONFIG_MBEDTLS_HARDWARE_IC_MUTEX
        IC_Mutex_init(&crypto_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_TYPE_CRYPTO);
    #else
        crypto_sem = xSemaphoreCreateBinary();
        xSemaphoreGive(crypto_sem);
    #endif

        crypto_hardware_init_flag = true;

    }

#if CONFIG_MBEDTLS_HARDWARE_IC_MUTEX
    IC_Mutex_acquire(&crypto_ic_mutex);
#else
    if (xSemaphoreTake(crypto_sem, portMAX_DELAY) != pdTRUE) {
        CLOG("Failed to acquire crypto semaphore\n");
        return NULL;
    }
#endif
    is_crypto_sem_get = true;

    if (type == CSK_CRYPTO_TYPE_AES || type == CSK_CRYPTO_TYPE_SHA) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    } else if (type == CSK_CRYPTO_TYPE_RSA || type == CSK_CRYPTO_TYPE_ECC) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    }

    return CRYPTO0_Handler;
}

int csk_crypto_release_hardware( csk_crypto_type_e type )
{
    if (!is_crypto_sem_get) {
        CLOG("crypto_sem do not get, other process is using crypto");
        return -1;
    }

    if (type == CSK_CRYPTO_TYPE_AES || type == CSK_CRYPTO_TYPE_SHA) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
    } else if (type == CSK_CRYPTO_TYPE_RSA || type == CSK_CRYPTO_TYPE_ECC) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
    }

    // CRYPTO_Uninitialize(CRYPTO0_Handler);

#if CONFIG_MBEDTLS_HARDWARE_IC_MUTEX
    IC_Mutex_release(&crypto_ic_mutex);
#else
    xSemaphoreGive(crypto_sem);
#endif
    is_crypto_sem_get = false;

    return 0;
}

void csk_dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    logDbg("%s", info);
    for (int i = 0; i < len; i++) {
        logDbg("%s%02X%s", i % 16 == 0 ? "\n\t":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
    logDbg("\n");
}

#else

extern void* CRYPTO0_Handler;
extern void ls_crypto_init(void);

crypto_handler csk_crypto_acquire_hardware( csk_crypto_type_e type )
{
    ls_crypto_init();

    if (type == CSK_CRYPTO_TYPE_AES || type == CSK_CRYPTO_TYPE_SHA) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    } else if (type == CSK_CRYPTO_TYPE_RSA || type == CSK_CRYPTO_TYPE_ECC) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    }

    return CRYPTO0_Handler;
}

int csk_crypto_release_hardware( csk_crypto_type_e type )
{

    if (type == CSK_CRYPTO_TYPE_AES || type == CSK_CRYPTO_TYPE_SHA) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
    } else if (type == CSK_CRYPTO_TYPE_RSA || type == CSK_CRYPTO_TYPE_ECC) {
        CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
    }

    // CRYPTO_Uninitialize(CRYPTO0_Handler);

    return 0;
}

void csk_dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n\t":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
    printf("\n");
}
#endif
