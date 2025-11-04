/*
 * crypto.c
 *
 *  Created on: 2023/5/23
 *      Author: LAPTOP-07
 */
#include "dbg_assert.h"
#include "crypto.h"
#include "dma.h"
#include <string.h>

// Event flag
static const uint8_t crypto_sha_size[] = {0x14, 0x1c, 0x20, 0x30, 0x40};
static const uint8_t crypto_sha_mode[] = {HSU_MODE_SHA_1, HSU_MODE_SHA_224, HSU_MODE_SHA_256, HSU_MODE_SHA_384, HSU_MODE_SHA_512};
static const uint8_t crypto_hmac_mode[] = {HSU_MODE_HMAC_SHA1, HSU_MODE_HMAC_SHA224, HSU_MODE_HMAC_SHA256, HSU_MODE_HMAC_SHA384, HSU_MODE_HMAC_SHA512};

static int32_t crypto_sha_start(CRYPTO_RESOURCES *crypto, const uint32_t * p_source, uint32_t num_bytes, uint32_t *p_dest)
{
    uint32_t res = CSK_CRYPTO_EVENT_DONE;
    int length = crypto_sha_size[crypto->sha_info->mode-1];

    LOGD("[%s]: num_bytes=%d\r\n", __func__,
            num_bytes);

    //hsu_source_addr_set(CPU2HW(hsu_buf.sha));
    crypto->hsu_reg->REG_SOURCE_ADDR.all = (uint32_t)p_source;
    //hsu_length_set(length);
    crypto->hsu_reg->REG_LENGTH.all = num_bytes;
    //hsu_status_clear_set(HSU_DONE_CLEAR_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;
    //hsu_control_set(ctrl);
    //crypto->hsu_reg->REG_CONTROL.bit.MODE = crypto_sha_mode[crypto->sha_info->mode-1];
    if(p_dest != NULL)
        crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 1;
    else
        crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 0;
    crypto->hsu_reg->REG_IRQ_CTRL_EN.bit.CRYPTO_IRQ_EN = 1;
    crypto->hsu_reg->REG_CONTROL.bit.START = 1;
    //hsu_wait_done(HSU_DONE_SET_SHA_BIT, false);
    crypto->info->cb_event(CSK_CRYPTO_EVENT_WAIT_DONE, CSK_DRIVER_OK, NULL);

    if(p_dest != NULL)
    {
        int len = crypto_sha_size[crypto->sha_info->mode-1]/4;
        uint32_t *sha_tab = (uint32_t *)(HSU_BASE + CRYPTO_HSU_SHA_TAB_OFFSET);
        for(int i=0; i<len; i++)
            p_dest[i] = sha_tab[i];
    }

    return CSK_DRIVER_OK;
}


// hash functions
int32_t
CRYPTO_Hash (void* res, const uint32_t * p_source,
            uint32_t num_bytes, uint32_t * p_dest, uint32_t update)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    if(!update)
    {
        crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 1;
        crypto->hsu_reg->REG_CONTROL.bit.MODE = crypto_sha_mode[crypto->sha_info->mode-1];
    }
    else
    {
        crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 0;
    }

    crypto_sha_start(crypto, p_source, num_bytes, p_dest);

    return CSK_DRIVER_OK;
}


// hmac functions, call CRYPTO_Hash for more data
int32_t
CRYPTO_HMAC (void* res, const uint32_t * p_source, uint32_t num_bytes,
        const uint32_t * key, uint32_t key_bytes, uint32_t * p_result)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    if(key_bytes > 128 || (crypto->sha_info->mode <= CSK_CRYPTO_HASH_SHA256 && key_bytes > 64))
    {
        crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 1;
        crypto->hsu_reg->REG_CONTROL.bit.MODE = crypto_sha_mode[crypto->sha_info->mode-1];
        // reuse p_result for the temp key
        crypto_sha_start(crypto, key, key_bytes, p_result);

        key_bytes = crypto_sha_size[crypto->sha_info->mode-1];
        crypto->hsu_reg->REG_SOURCE_ADDR.all = (uint32_t)p_result;
    }
    else
    {
        //hsu_source_addr_set(CPU2HW(hsu_buf.sha));
        crypto->hsu_reg->REG_SOURCE_ADDR.all = (uint32_t)key;
    }

    crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 1;
    crypto->hsu_reg->REG_CONTROL.bit.MODE = crypto_hmac_mode[crypto->sha_info->mode-1];

    //hsu_length_set(key_len);
    crypto->hsu_reg->REG_LENGTH.all = key_bytes;
    //hsu_status_clear_set(HSU_DONE_CLEAR_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;
    //hsu_control_set(ctrl);
    crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 0;
    crypto->hsu_reg->REG_IRQ_CTRL_EN.bit.CRYPTO_IRQ_EN = 1;
    crypto->hsu_reg->REG_CONTROL.bit.START = 1;
    //hsu_wait_done(HSU_DONE_SET_SHA_BIT, false);
    //crypto->sha_info->wait_done = 1;
    //while(crypto->sha_info->wait_done == 1);
    crypto->info->cb_event(CSK_CRYPTO_EVENT_WAIT_DONE, CSK_DRIVER_OK, NULL);
    //CRYPTO_HSU_WAIT_DONE(HMAC);
    //crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;

    crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 0;
    crypto_sha_start(crypto, p_source, num_bytes, p_result);

    return CSK_DRIVER_OK;
}

int32_t CRYPTO_Get_Hash(void* res, uint32_t * p_result)
{
    CHECK_RESOURCES(res);
    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    if(p_result != NULL)
    {
        int len = crypto_sha_size[crypto->sha_info->mode-1]/4;
        uint32_t *sha_tab = (uint32_t *)(HSU_BASE + CRYPTO_HSU_SHA_TAB_OFFSET);
        for(int i=0; i<len; i++)
            p_result[i] = sha_tab[i];
    }
    else
        return CSK_DRIVER_OK;
    return CSK_DRIVER_OK;
}

int32_t crypto_sha_reset(CRYPTO_RESOURCES *crypto)
{
    return CSK_DRIVER_OK;
}

int32_t crypto_sha_set_mode(CRYPTO_RESOURCES *crypto, uint32_t mode)
{
    crypto->sha_info->mode = mode;
    return CSK_DRIVER_OK;
}

void crypto_sha_irq_handler(CRYPTO_RESOURCES *crypto)
{
    uint32_t res = CSK_DRIVER_OK;

    LOGD("[%s] mode=%d\r\n", __func__, crypto->sha_info->mode);

    if(crypto->info->cb_event)
        crypto->info->cb_event(CSK_CRYPTO_EVENT_DONE, res, (void*)crypto);
}
