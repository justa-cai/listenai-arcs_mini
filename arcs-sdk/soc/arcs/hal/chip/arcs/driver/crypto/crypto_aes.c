/*
 * crypto.c
 *
 *  Created on: 2023/5/23
 *      Author: LAPTOP-07
 */
#include "dbg_assert.h"
#include "chip.h"
#include "crypto.h"
#include "log_print.h"
#include <string.h>


static int32_t crypto_config_dma(CRYPTO_RESOURCES *crypto, const uint32_t * p_source,
        uint32_t num_bytes, uint32_t * p_dest)
{
    int stat = 0;
    uint8_t in_ch = 0, out_ch = 1, ch;
    uint32_t control, config_low, config_high;
    uint32_t out_length = crypto->aes_reg->REG_AES_ENGRESS_DMA_TOTAL_NUM_REG.all;

//    crypto->aes_reg->REG_AES_INGRESS_DMA_BST_TYPE_REG.bit.DEST_TR_WIDTH = 2;
//    crypto->aes_reg->REG_AES_ENGRESS_DMA_BST_TYPE_REG.bit.SURC_TR_WIDTH = 2;
    crypto->aes_reg->REG_AES_INGRESS_DMA_BST_TYPE_REG.bit.DST_MSIZE = 3;
    crypto->aes_reg->REG_AES_ENGRESS_DMA_BST_TYPE_REG.bit.SURC_MSIZE = 3;
    crypto->aes_reg->REG_AES_INGRESS_DMA_BST_TYPE_REG.bit.DMA_EN = 1;
    crypto->aes_reg->REG_AES_ENGRESS_DMA_BST_TYPE_REG.bit.DMA_EN = 1;

    crypto->hsu_reg->REG_SOURCE_ADDR.all = (uint32_t)p_source;
    crypto->hsu_reg->REG_DESTINATION_ADDR.all = (uint32_t)p_dest;
    crypto->hsu_reg->REG_CONTROL.bit.MODE = HSU_MODE_AES;

    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_GO = 1;

    return 0;
}

static void crypto_fifo_send_data(CRYPTO_RESOURCES *crypto, const uint32_t * p_data,
        uint32_t num_bytes)
{
    for(int i=0; i<(num_bytes+3)/4; i++)
    {
        crypto->aes_reg->REG_AES_INGRESS_FIFO.bit.INGRESS_FIFO_DATA = p_data[i];
    }
}

static void crypto_fifo_receive_data(CRYPTO_RESOURCES *crypto, uint32_t * p_data,
        uint32_t num_bytes)
{
    for(int i=0; i<(num_bytes+3)/4; i++)
    {
        p_data[i] = crypto->aes_reg->REG_AES_ENGRESS_FIFO.bit.ENGRESS_FIFO_DATA;
    }
}

static int32_t crypto_config_fifo(CRYPTO_RESOURCES *crypto, const uint32_t * p_source,
        uint32_t num_bytes, uint32_t * p_dest)
{
    int i;
    uint16_t *data = (uint16_t *)p_source;
    crypto->aes_reg->REG_AES_INGRESS_DMA_BST_TYPE_REG.bit.DMA_EN = 0;
    crypto->aes_reg->REG_AES_ENGRESS_DMA_BST_TYPE_REG.bit.DMA_EN = 0;
    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_GO = 1;

    crypto_fifo_send_data(crypto, p_source, num_bytes);

    return 0;
}

static void crypto_aes_process_ccm_aad(CRYPTO_RESOURCES *crypto, uint8_t * p_source,
        uint32_t num_bytes, uint32_t * p_dest)
{
    uint32_t aad_packet[4];
    int mac_len = crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MAC_LEN;
    uint32_t data_len = crypto->aes_reg->REG_AES_MSG_TOTAL_BYTES.all;
    uint32_t aad_len = crypto->aes_info->aad_len;
    uint8_t *p_aad = (uint8_t *)aad_packet;
    int i, n;
    memset(p_aad, 0, sizeof(aad_packet));

    // calc B count
    if(aad_len < (0x10000 - 0x100))
        n = (2 + aad_len + 15)/16 + 1;
    else
        n = (6 + aad_len + 15)/16 + 1;
    // start for aad
    crypto->aes_info->aad_flag = 1;
    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MSG_END = 0;
    crypto->aes_reg->REG_AES_MSG_AAD_BYTES.all = ((n*16) << CRYPTO_AES_AES_MSG_AAD_BYTES_AAD_LEN_Pos) | (n*16);
    crypto->aes_reg->REG_GCM_MODE_AAD_INFO.all = n*16;
    crypto->aes_reg->REG_AES_ENGRESS_DMA_TOTAL_NUM_REG.all = 0;
    crypto->aes_reg->REG_AES_INGRESS_DMA_TOTAL_NUM_REG.all = n*16;
    crypto->aes_reg->REG_AES_INGRESS_DMA_BST_TYPE_REG.bit.DMA_EN = 0;
    crypto->aes_reg->REG_AES_ENGRESS_DMA_BST_TYPE_REG.bit.DMA_EN = 0;
    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_GO = 1;

    // set B0
    aad_packet[0] = crypto->aes_reg->REG_AES_CTX_CTR_0.all;
    aad_packet[1] = crypto->aes_reg->REG_AES_CTX_CTR_1.all;
    aad_packet[2] = crypto->aes_reg->REG_AES_CTX_CTR_2.all;
    aad_packet[3] = crypto->aes_reg->REG_AES_CTX_CTR_3.all;
    if(mac_len == 0)
        mac_len = 16;
    aad_packet[0] |= 0x40 | (((mac_len-2)/2)<<3);
    aad_packet[3] |= (data_len<<24) | ((data_len<<8)&0xFF0000) | ((data_len>>8)&0xFF00) | ((data_len>>24)&0xFF);
    crypto_fifo_send_data(crypto, aad_packet, 16);

    // set B1
    memset(p_aad, 0, sizeof(aad_packet));
    if(aad_len < (0x10000 - 0x100))
    {
        p_aad[0] = (uint8_t)(aad_len>>8);
        p_aad[1] = (uint8_t)aad_len;
        i = 2;
    }
    else
    {
        p_aad[0] = 0xFF;
        p_aad[1] = 0xFE;
        p_aad[2] = (uint8_t)(aad_len >> 24);
        p_aad[3] = (uint8_t)(aad_len >> 16);
        p_aad[4] = (uint8_t)(aad_len >> 8);
        p_aad[5] = (uint8_t)aad_len;
        i = 6;
    }
    n = 16 - i>aad_len?aad_len:16 - i;
    memcpy(p_aad+i, p_source, n);
    aad_len -= n;
    crypto_fifo_send_data(crypto, aad_packet, 16);

    // set rest of aad
    while(aad_len >= 16)
    {
        memcpy(p_aad, p_source + n, 16);
        crypto_fifo_send_data(crypto, aad_packet, 16);
        aad_len -= 16;
        n += 16;
    };

    // pading
    if(aad_len > 0)
    {
        memcpy(p_aad, p_source + n, aad_len);
        memset(p_aad + aad_len, 0, sizeof(aad_packet)-aad_len);
        crypto_fifo_send_data(crypto, aad_packet, 16);
    }

    crypto->info->cb_event(CSK_CRYPTO_EVENT_WAIT_DONE, CSK_DRIVER_OK, (void*)crypto);
}

static void crypto_aes_process(CRYPTO_RESOURCES *crypto, const uint32_t * p_source,
        uint32_t num_bytes, uint32_t * p_dest, uint8_t start)
{
    uint32_t out_length = num_bytes;
    uint8_t aad_proc_flag = 0;

    // save buffer addresses
    crypto->aes_info->source = p_source;
    crypto->aes_info->result = p_dest;

    // first segment, set BEGIN register
    if(start && crypto->aes_info->done_len == 0 && crypto->aes_info->aad_flag == 0)
    {
        crypto->aes_reg->REG_AES_CTX_CFG.bit.AES_CTX_RET = 1;
        crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MSG_BEGIN = 1;
        // AAD must in the first segment
        //crypto->aes_reg->REG_AES_MSG_AAD_BYTES.all = (crypto->aes_info->aad_len << CRYPTO_AES_AES_MSG_AAD_BYTES_AAD_LEN_Pos);
        if(crypto->aes_info->aad_len > 0)
        {
            aad_proc_flag = 1;
            if(num_bytes > crypto->aes_info->aad_len)
            {
                crypto->aes_info->last_len = num_bytes - crypto->aes_info->aad_len;
                num_bytes = crypto->aes_info->aad_len;
            }
            else
                crypto->aes_info->last_len = 0;

            if(crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_CCM)
            {
                // process AAD data for CCM mode
                crypto_aes_process_ccm_aad(crypto, (uint8_t*)p_source, num_bytes, p_dest);
                return;
            }
            else
            {
                crypto->aes_info->aad_flag = 1;
                out_length = 0;
            }
        }
    }
    else
    {
        crypto->aes_info->aad_len = 0;
        crypto->aes_reg->REG_AES_CTX_CFG.bit.AES_CTX_RET = 0;
        crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MSG_BEGIN = 0;
    }

    if(out_length + crypto->aes_info->done_len >=
            crypto->aes_reg->REG_AES_MSG_TOTAL_BYTES.bit.TOTAL_BYTES)
        crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MSG_END = 1;
    else
        crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MSG_END = 0;

    crypto->aes_reg->REG_AES_MSG_AAD_BYTES.all = (crypto->aes_info->aad_len << CRYPTO_AES_AES_MSG_AAD_BYTES_AAD_LEN_Pos)|num_bytes;

    if(crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_CMAC)
        out_length = 0;

    crypto->aes_reg->REG_AES_ENGRESS_DMA_TOTAL_NUM_REG.all = out_length;
    crypto->aes_reg->REG_AES_INGRESS_DMA_TOTAL_NUM_REG.all = (num_bytes + 3) & ~ 3;

    if(num_bytes < CRYPTO_AES_BLOCK_SIZE || aad_proc_flag)
    {
        crypto_config_fifo(crypto, p_source, num_bytes, p_dest);
    }
    else
    {
        if(num_bytes % CRYPTO_AES_BLOCK_SIZE)
        {
            crypto->aes_info->last_len = num_bytes % CRYPTO_AES_BLOCK_SIZE;
            num_bytes = num_bytes - crypto->aes_info->last_len;
        }
        crypto_config_dma(crypto, p_source, num_bytes, p_dest);
    }

    crypto->info->cb_event(CSK_CRYPTO_EVENT_WAIT_DONE, CSK_DRIVER_OK, (void*)crypto);
}

static void crypto_aes_continue(CRYPTO_RESOURCES *crypto)
{
    if(crypto->aes_info->last_len)
    {
        int proc_len = crypto->aes_info->last_len;
        if(crypto->aes_info->last_len > CRYPTO_AES_BLOCK_SIZE)
        {
            crypto->aes_info->last_len %= CRYPTO_AES_BLOCK_SIZE;
            proc_len -= crypto->aes_info->last_len;
        }
        else
            crypto->aes_info->last_len = 0;

        crypto_aes_process(crypto, crypto->aes_info->source,
                    proc_len, crypto->aes_info->result, 0);
    }

}

static int32_t crypto_aes_start(CRYPTO_RESOURCES *crypto, const uint32_t * p_source,
        uint32_t num_bytes, uint32_t * p_dest, uint8_t is_enc)
{
    LOGD("[%s]: num_bytes=%d, is_enc=%d\r\n", __func__, num_bytes, is_enc);

    // check segment length
    if(num_bytes % CRYPTO_AES_BLOCK_SIZE)
    {
        if(crypto->aes_info->mode <= CSK_CRYPTO_AES_MODE_CBC)
            return CSK_DRIVER_ERROR_PARAMETER;

        if(!((crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_CCM||crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_GCM)
                && crypto->aes_info->aad_flag == 0)
                && num_bytes > CRYPTO_AES_BLOCK_SIZE)
        {
            crypto->aes_info->last_len = num_bytes % CRYPTO_AES_BLOCK_SIZE;
            num_bytes = num_bytes - crypto->aes_info->last_len;
        }
    }
    else
        crypto->aes_info->last_len = 0;

    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_ENCRYPT = is_enc;

    crypto_aes_process(crypto, p_source, num_bytes, p_dest, 1);

    while(crypto->aes_info->last_len)
        crypto_aes_continue(crypto);

    return CSK_DRIVER_OK;
}


static void crypto_aes_write_dummy_key(CRYPTO_RESOURCES *crypto)
{
    crypto->aes_reg->REG_AES_CTX_KEY_0.all = 0;
    crypto->aes_reg->REG_AES_CTX_KEY_1.all = 0;
    crypto->aes_reg->REG_AES_CTX_KEY_2.all = 0;
    crypto->aes_reg->REG_AES_CTX_KEY_3.all = 0;
    if(crypto->aes_info->key_size >= CRYPTO_AES_KEY_SIZE_192)
    {
        crypto->aes_reg->REG_AES_CTX_KEY_4.all = 0;
        crypto->aes_reg->REG_AES_CTX_KEY_5.all = 0;
        if(crypto->aes_info->key_size == CRYPTO_AES_KEY_SIZE_256)
        {
            crypto->aes_reg->REG_AES_CTX_KEY_6.all = 0;
            crypto->aes_reg->REG_AES_CTX_KEY_7.all = 0;
        }
    }
}

// aes functions
int32_t
CRYPTO_AES_Encrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    return crypto_aes_start(res, p_source, num_bytes, p_dest, 1);
}


int32_t
CRYPTO_AES_Decrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    return crypto_aes_start(res, p_source, num_bytes, p_dest, 0);
}

int32_t
CRYPTO_AES_Decrypt_Flash (void* res, uint32_t flash_addr, uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{




    return CSK_DRIVER_OK;
}

int32_t
CRYPTO_ECB_Encrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    crypto_aes_set_mode(crypto, CSK_CRYPTO_AES_MODE_ECB);


    return CRYPTO_AES_Encrypt(res, p_source, num_bytes, p_dest);
}


int32_t
CRYPTO_ECB_Decrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    crypto_aes_set_mode(crypto, CSK_CRYPTO_AES_MODE_ECB);

    return CRYPTO_AES_Decrypt(res, p_source, num_bytes, p_dest);
}


int32_t
CRYPTO_CBC_Encrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    crypto_aes_set_mode(crypto, CSK_CRYPTO_AES_MODE_CBC);

    return CRYPTO_AES_Encrypt(res, p_source, num_bytes, p_dest);
}


int32_t
CRYPTO_CBC_Decrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    crypto_aes_set_mode(crypto, CSK_CRYPTO_AES_MODE_CBC);

    return CRYPTO_AES_Decrypt(res, p_source, num_bytes, p_dest);
}


int32_t crypto_aes_set_key(CRYPTO_RESOURCES *crypto, uint32_t key_mode, uint32_t arg0)
{
    uint32_t len;

    switch(key_mode)
    {
    case CRYPTO_AES_KEY_USER:
        if(arg0 == 0)
            return CSK_DRIVER_ERROR_PARAMETER;

        crypto->aes_reg->REG_AES_CTX_KEY_MODE_SEL.bit.KEY_SEL = 1;
        {
            uint32_t *key = (uint32_t*)arg0;

            crypto->aes_reg->REG_AES_CTX_KEY_0.all = key[0];
            crypto->aes_reg->REG_AES_CTX_KEY_1.all = key[1];
            crypto->aes_reg->REG_AES_CTX_KEY_2.all = key[2];
            crypto->aes_reg->REG_AES_CTX_KEY_3.all = key[3];
            if(crypto->aes_info->key_size >= CRYPTO_AES_KEY_SIZE_192)
            {
                crypto->aes_reg->REG_AES_CTX_KEY_4.all = key[4];
                crypto->aes_reg->REG_AES_CTX_KEY_5.all = key[5];
                if(crypto->aes_info->key_size == CRYPTO_AES_KEY_SIZE_256)
                {
                    crypto->aes_reg->REG_AES_CTX_KEY_6.all = key[6];
                    crypto->aes_reg->REG_AES_CTX_KEY_7.all = key[7];
                }
            }
        }
        break;
    case CRYPTO_AES_KEY_EFUSE1:
        crypto->aes_reg->REG_AES_CTX_KEY_MODE_SEL.bit.KEY_SEL = 2;
        crypto_aes_write_dummy_key(crypto);
        break;

    case CRYPTO_AES_KEY_EFUSE2:
        crypto->aes_reg->REG_AES_CTX_KEY_MODE_SEL.bit.KEY_SEL = 4;
        crypto_aes_write_dummy_key(crypto);
        break;
    case CRYPTO_AES_KEY_EFUSE3:
        crypto->aes_reg->REG_AES_CTX_KEY_MODE_SEL.bit.KEY_SEL = 2;
        IP_EFUSE_CTRL->REG_AES_KEY1_OFFSET.bit.AES_KEY1_OFFSET = arg0;
        crypto_aes_write_dummy_key(crypto);
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
        break;
    }
    return CSK_DRIVER_OK;
}

int32_t crypto_aes_set_mode(CRYPTO_RESOURCES *crypto, uint32_t mode)
{
    crypto->aes_info->mode = mode;
    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_ALG_MODE = mode - 1;
    crypto->aes_info->done_len = 0;
    crypto->aes_info->last_len = 0;
    crypto->aes_info->aad_flag = 0;
    return CSK_DRIVER_OK;
}

int32_t crypto_aes_set_key_size(CRYPTO_RESOURCES *crypto, uint32_t key_size)
{
    crypto->aes_info->key_size = key_size;
    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_KEY_SIZE = key_size-1;
    return CSK_DRIVER_OK;
}

int32_t crypto_aes_set_iv(CRYPTO_RESOURCES *crypto, uint32_t* iv)
{
    if(iv == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    // reset length
    crypto->aes_info->done_len = 0;
    crypto->aes_info->last_len = 0;

    if(crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_CTR || crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_GCM)
    {
        crypto->aes_reg->REG_AES_CTX_CTR_0.all = iv[0];
        crypto->aes_reg->REG_AES_CTX_CTR_1.all = iv[1];
        crypto->aes_reg->REG_AES_CTX_CTR_2.all = iv[2];
        if(crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_GCM)
            crypto->aes_reg->REG_AES_CTX_CTR_3.all = 0x01000000;
        else
            crypto->aes_reg->REG_AES_CTX_CTR_3.all = iv[3];
    }
    else if(crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_CCM)
    {
        int q = 15 - crypto->aes_info->iv_len;
        uint32_t ccm_count[4];
        uint8_t *p_count = (uint8_t *)ccm_count;
        memset(p_count, 0, sizeof(ccm_count));
        p_count[0] = q -1;
        memcpy(p_count+1, iv, crypto->aes_info->iv_len);
        crypto->aes_reg->REG_AES_CTX_CTR_0.all = ccm_count[0];
        crypto->aes_reg->REG_AES_CTX_CTR_1.all = ccm_count[1];
        crypto->aes_reg->REG_AES_CTX_CTR_2.all = ccm_count[2];
        crypto->aes_reg->REG_AES_CTX_CTR_3.all = ccm_count[3];
    }
    else
    {
        crypto->aes_reg->REG_AES_CTX_IV_0.all = iv[0];
        crypto->aes_reg->REG_AES_CTX_IV_1.all = iv[1];
        crypto->aes_reg->REG_AES_CTX_IV_2.all = iv[2];
        crypto->aes_reg->REG_AES_CTX_IV_3.all = iv[3];
    }
    return CSK_DRIVER_OK;
}

int32_t crypto_aes_get_mac(CRYPTO_RESOURCES *crypto, uint32_t* mac)
{
    if(mac == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    mac[0] = crypto->aes_reg->REG_AES_CTX_MAC_0.all;
    mac[1] = crypto->aes_reg->REG_AES_CTX_MAC_1.all;
    mac[2] = crypto->aes_reg->REG_AES_CTX_MAC_2.all;
    mac[3] = crypto->aes_reg->REG_AES_CTX_MAC_3.all;
    return CSK_DRIVER_OK;
}

int32_t crypto_aes_set_lengths(CRYPTO_RESOURCES *crypto, uint32_t* lengths)
{
    if(lengths == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;
    if(lengths[0] > 16 || lengths[1] > 16)
        return CSK_DRIVER_ERROR_PARAMETER;

    crypto->aes_info->iv_len = lengths[1];
    crypto->aes_info->aad_len = lengths[2];
    crypto->aes_info->done_len = 0;
    crypto->aes_info->last_len = 0;
    crypto->aes_info->aad_flag = 0;

    crypto->aes_reg->REG_AES_MSG_CFG.bit.AES_MAC_LEN = lengths[0] & 0xf;
    crypto->aes_reg->REG_AES_MSG_TOTAL_BYTES.all = lengths[3];
    crypto->aes_reg->REG_GCM_MODE_AAD_INFO.all = lengths[2];
    return CSK_DRIVER_OK;
}

void crypto_aes_irq_handler(CRYPTO_RESOURCES *crypto)
{
    uint32_t *p_mac_buff = NULL;
    int msg_len;

    LOGD("[%s]: sate=%d\r\n", __func__,
            crypto->aes_reg->REG_AES_DONE_STAT_REG.all);

    if(crypto->aes_info->mode == CSK_CRYPTO_AES_MODE_CMAC)
        msg_len = crypto->aes_reg->REG_AES_INGRESS_DMA_TOTAL_NUM_REG.all;
    else
        msg_len = crypto->aes_reg->REG_AES_ENGRESS_DMA_TOTAL_NUM_REG.all;

    if(msg_len != 0 && msg_len < CRYPTO_AES_BLOCK_SIZE && crypto->aes_info->result != NULL
            && (crypto->aes_info->mode != CSK_CRYPTO_AES_MODE_CMAC))
    {
        crypto_fifo_receive_data(crypto, crypto->aes_info->result, msg_len);
    }
    crypto->aes_info->done_len += msg_len;

    LOGD("[%s]: done_len = %d, last_len=%d\r\n", __func__,
            crypto->aes_info->done_len, crypto->aes_info->last_len);

    if(crypto->aes_info->last_len)
    {
        if(crypto->aes_info->aad_flag == 1)
        {
            crypto->aes_info->aad_flag = 2;
            crypto->aes_info->done_len = 0;
            crypto->aes_info->source += crypto->aes_info->aad_len / 4;
        }
        else
        {
            crypto->aes_info->source += msg_len / 4;
            crypto->aes_info->result += msg_len / 4;
        }
    }
    else
    {
        if(crypto->aes_info->aad_flag == 1)
        {
            crypto->aes_info->aad_flag = 2;
        }
    }

    crypto->aes_reg->REG_AES_DONE_STAT_REG.all = 0;

    if(crypto->info->cb_event)
        crypto->info->cb_event(CSK_CRYPTO_EVENT_DONE, CSK_DRIVER_OK, (void*)crypto);
}
