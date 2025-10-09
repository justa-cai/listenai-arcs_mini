/*
 * secure.c
 *
 *  Created on: 2023.8
 *      Author: 
 */



#include "chip.h"
#include <stdio.h>
#include <string.h>

#include "Driver_CRYPTO.h"
#include "secure.h"

static uint32_t boot_sec_count = 0;
static uint8_t  boot_enc_ready = 0;
void* CRYPTO0_Handler;

static volatile int32_t CRYPTO_Result = CSK_DRIVER_OK;
static volatile uint32_t CRYPTO_DONE = 0;

static int32_t CRYPTO_BOOT_EventCallback(uint32_t event, int32_t result, void* workspace){
    if(CSK_CRYPTO_EVENT_WAIT_DONE == event)
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

// initialize secure module
int secure_init()
{
    boot_sec_count = 0;
    boot_enc_ready = 0;
    CRYPTO0_Handler = CRYPTO0();
    CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_BOOT_EventCallback, NULL);
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P256);

    return 1;
}

// shutdown secure module
int secure_shutdown()
{
    boot_enc_ready = 0;
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
    CRYPTO_Uninitialize(CRYPTO0_Handler);

    return 1;
}

