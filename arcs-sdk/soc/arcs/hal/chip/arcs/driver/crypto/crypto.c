/*
 * crypto.c
 *
 *  Created on: 2023/5/23
 *      Author: LAPTOP-07
 */
#include "dbg_assert.h"
#include "arcs_ap.h"
#include "crypto.h"
#include "Driver_GPDMA.h"
#include "ClockManager.h"
#include "cache.h"

#define CSK_CRYPTO_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

// Driver Version
static const CSK_DRIVER_VERSION crypto_driver_version = {
    CSK_CRYPTO_API_VERSION,
    CSK_CRYPTO_DRV_VERSION
};

static void CRYPTO0_AES_IRQHandler (void);
static void CRYPTO0_ECC_IRQHandler (void);
static void CRYPTO0_HSU_IRQHandler (void);

static AES_INFO crypto0_aes = {0};

static SHA_INFO crypto0_sha = {0};

static ECC_INFO crypto0_ecc = {0};

static RSA_INFO crypto0_rsa = {0};

static CRYPTO_INFO crypto0_info = {
        NULL,
        NULL,
        0,
        0,
        0,
};

const CRYPTO_RESOURCES crypto0_resources = {
    ((CRYPTO_AES_RegDef *) AES_BASE),
    ((CRYPTO_ECC_RegDef *) ECC_BASE),
    ((CRYPTO_HSU_RegDef *) HSU_BASE),
    IRQ_AES_VECTOR,
    IRQ_ECC_VECTOR,
    IRQ_HSU_VECTOR,
    CRYPTO0_AES_IRQHandler,
    CRYPTO0_ECC_IRQHandler,
    CRYPTO0_HSU_IRQHandler,
    &crypto0_aes,
    &crypto0_sha,
    &crypto0_ecc,
    &crypto0_rsa,
    &crypto0_info,
};


// common functions
CSK_DRIVER_VERSION
CRYPTO_GetVersion (void)
{
    return crypto_driver_version;
}

void* CRYPTO0()
{
    return (void*) &crypto0_resources;
}

int32_t
CRYPTO_Initialize (void *res, CSK_CRYPTO_SignalEvent_t cb_event, void* workspace)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    if (crypto->info->flags & CRYPTO_FLAG_INITIALIZED){
        return CSK_DRIVER_ERROR_BUSY;
    }

    if(cb_event == NULL)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    crypto->info->cb_event = cb_event;
    crypto->info->workspace = workspace;

    crypto->info->little_endian = 0;
    crypto->info->power_on = 0;

    // clear information
    __builtin_memset(crypto->aes_info, 0, sizeof(AES_INFO));
    __builtin_memset(crypto->sha_info, 0, sizeof(SHA_INFO));
    __builtin_memset(crypto->ecc_info, 0, sizeof(ECC_INFO));
    __builtin_memset(crypto->rsa_info, 0, sizeof(RSA_INFO));


    crypto->info->flags = CRYPTO_FLAG_INITIALIZED;
    return CSK_DRIVER_OK;
}

int32_t
CRYPTO_Uninitialize (void *res)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    crypto->info->cb_event = NULL;
    crypto->info->workspace = NULL;

    crypto->info->little_endian = 0;
    crypto->info->power_on = 0;

    // clear information
    __builtin_memset(crypto->aes_info, 0, sizeof(AES_INFO));
    __builtin_memset(crypto->sha_info, 0, sizeof(SHA_INFO));
    __builtin_memset(crypto->ecc_info, 0, sizeof(ECC_INFO));
    __builtin_memset(crypto->rsa_info, 0, sizeof(RSA_INFO));

    crypto->info->flags = 0;

    return CSK_DRIVER_OK;
}

int32_t
CRYPTO_PowerControl (void* res, CRYPTO_HW_MODULE module, CSK_POWER_STATE state)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;
    LOGD("CRYPTO_PowerControl state:%d, module:%d, power_on:%d", state ,module, crypto->info->power_on);
    if(crypto->info->flags != CRYPTO_FLAG_INITIALIZED)
        return CSK_DRIVER_ERROR;

    switch (state) {
    case CSK_POWER_OFF:
        __disable_irq();
        if(crypto->info->power_on>0)
            crypto->info->power_on --;

        if(crypto->info->power_on==0)
        {
            __HAL_CRM_CRYPTO_CLK_DISABLE();

            // Disable IRQ
            disable_IRQ(crypto->irq_num_aes);
            disable_IRQ(crypto->irq_num_hsu);
            disable_IRQ(crypto->irq_num_ecc);

            // Uninstall IRQ Handler
            register_ISR(crypto->irq_num_aes, NULL, NULL);
            register_ISR(crypto->irq_num_hsu, NULL, NULL);
            register_ISR(crypto->irq_num_ecc, NULL, NULL);
            #if CONFIG_PM && CONFIG_PM_CLOSE_AP
            pm_force_ap_off();
            #endif
        }

        __enable_irq();

        crypto->info->cb_event(CSK_CRYPTO_EVENT_FINISHED, CSK_DRIVER_OK, (void*)crypto->info->workspace);

        break;
    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        crypto->info->cb_event(CSK_CRYPTO_EVENT_WAIT_BUSY, CSK_DRIVER_OK, (void*)crypto->info->workspace);
        __disable_irq();
        if(crypto->info->power_on++==0)
        {
            #if CONFIG_PM && CONFIG_PM_CLOSE_AP
            pm_force_ap_on();
            #endif

            __HAL_CRM_CRYPTO_CLK_ENABLE();
            IP_AP_CFG->REG_SW_RESET.bit.CRYPTO_RESET = 1;

            // clear information
            crypto->info->little_endian = 0;
            __builtin_memset(crypto->aes_info, 0, sizeof(AES_INFO));
            __builtin_memset(crypto->sha_info, 0, sizeof(SHA_INFO));
            __builtin_memset(crypto->ecc_info, 0, sizeof(ECC_INFO));
            __builtin_memset(crypto->rsa_info, 0, sizeof(RSA_INFO));

            // Install IRQ Handler
            register_ISR(crypto->irq_num_aes, crypto->irq_handler_aes, NULL);
            register_ISR(crypto->irq_num_ecc, crypto->irq_handler_ecc, NULL);
            register_ISR(crypto->irq_num_hsu, crypto->irq_handler_hsu, NULL);

            // Enable IRQ
            enable_IRQ(crypto->irq_num_aes);
            enable_IRQ(crypto->irq_num_ecc);
            enable_IRQ(crypto->irq_num_hsu);
        }
        __enable_irq();

        break;
    }
    return CSK_DRIVER_OK;
}

int32_t
CRYPTO_Control (void* res, uint32_t control, uint32_t arg0)
{
    uint32_t temp = 0;

    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    if(control & CSK_CRYPTO_AES_KEY_MODE_Msk)
        return crypto_aes_set_key(crypto, control & CSK_CRYPTO_AES_KEY_MODE_Msk, arg0);
    if(control & CSK_CRYPTO_SET_AES_MODE_Msk)
        return crypto_aes_set_mode(crypto, arg0);
    if(control & CSK_CRYPTO_SET_AES_KEY_SIZE_Msk)
        return crypto_aes_set_key_size(crypto, (control & CSK_CRYPTO_SET_AES_KEY_SIZE_Msk)>>CSK_CRYPTO_SET_AES_KEY_SIZE_Pos);
    if(control & CSK_CRYPTO_SET_AES_IV_Msk)
        return crypto_aes_set_iv(crypto, (uint32_t*)arg0);
    if(control & CSK_CRYPTO_SET_AES_LENGTHS_Msk)
        return crypto_aes_set_lengths(crypto, (uint32_t*)arg0);
    if(control & CSK_CRYPTO_GET_AES_MAC_Msk)
        return crypto_aes_get_mac(crypto, (uint32_t*)arg0);
    if(control & CSK_CRYPTO_SET_LITTLE_ENDIAN_Msk)
        crypto->info->little_endian = arg0;
    if(control & CSK_CRYPTO_RESET_HASH_Msk)
        return crypto_sha_reset(crypto);
    if(control & CSK_CRYPTO_SET_HASH_MODE_Msk)
        return crypto_sha_set_mode(crypto, arg0);

    if(control & CSK_CRYPTO_SET_RSA_MODE_Msk)
        return crypto_rsa_set_mode(crypto, (control & CSK_CRYPTO_SET_RSA_MODE_Msk)>>CSK_CRYPTO_SET_RSA_MODE_Pos);
    if(control & CSK_CRYPTO_SET_RSA_PADDING_MODE_Msk)
        return crypto_rsa_set_padding_mode(crypto, arg0);
    if(control & CSK_CRYPTO_SET_RSA_PADDING_LABEL_Msk)
        return crypto_rsa_set_padding_label(crypto, (char *)arg0);
    if(control & CSK_CRYPTO_SET_ECC_CURVE_Msk)
        return crypto_ecc_select_curve(crypto, (CRYPTO_ECC_CURVE *)arg0);
    if(control & CSK_CRYPTO_SET_MIC_KEY_Msk)
        return crypto_hsu_set_key(crypto, (uint32_t*)arg0);

    return CSK_DRIVER_OK;
}
int32_t
CRYPTO_SWAP_Bytes(void *res, const uint32_t *data, uint32_t length, uint32_t *out)
{
    uint8_t *p, *q;
    int i, tmp;

    p = (uint8_t*)data;
    q = (uint8_t*)out;

    for(i=0; i<length/2; i++)
    {
        tmp = p[i];
        q[i] = p[length-1-i];
        q[length-1-i] = tmp;
    }

    return CSK_DRIVER_OK;
}

static void
CRYPTO0_AES_IRQHandler (void){
    crypto_aes_irq_handler(&crypto0_resources);
}

static void
CRYPTO0_ECC_IRQHandler (void){
    crypto_ecc_irq_handler(&crypto0_resources);
}

static void
CRYPTO0_HSU_IRQHandler (void){
    // check irq reason
    if(crypto0_resources.hsu_reg->REG_STATUS_SET.bit.DONE_SET_RSA)
        crypto_rsa_irq_handler(&crypto0_resources);
    else
        crypto_sha_irq_handler(&crypto0_resources);

    // clear irq
    crypto0_resources.hsu_reg->REG_IRQ_CTRL_EN.bit.CRYPTO_IRQ_EN = 0;
}
