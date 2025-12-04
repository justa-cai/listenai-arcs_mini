/*
 * crypto.c
 *
 *  Created on: 2023/5/23
 *      Author: LAPTOP-07
 */
#include "cache.h"
#include "dbg_assert.h"
#include "crypto.h"
#include "PSRAMManager.h"
#include "dma.h"
#include "log_print.h"
#include <string.h>
#include <FreeRTOS.h>


int32_t crypto_hsu_set_key(CRYPTO_RESOURCES *crypto, uint32_t* key)
{
    if(key != NULL)
    {
        uint32_t *mic_tab = (uint32_t*)&crypto->hsu_reg->REG_MIC_TAB.all;
        mic_tab[0] = key[0];
        mic_tab[1] = key[1];
        crypto->hsu_reg->REG_REMAINING.all = key[2];
    }
    return CSK_DRIVER_OK;
}

int32_t
CRYPTO_TKIP_Michael(void *res, const uint8_t *data, uint32_t data_len, uint8_t is_end, uint32_t *result)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    if(data_len != 0 && data != NULL)
    {
        //hsu_source_addr_set(data);
        crypto->hsu_reg->REG_SOURCE_ADDR.all = (uint32_t)data;
        //hsu_length_set(data_len);
        crypto->hsu_reg->REG_LENGTH.all = data_len;
    }
    else
    {
        //hsu_source_addr_set(data);
        crypto->hsu_reg->REG_SOURCE_ADDR.all = 0;
        //hsu_length_set(data_len);
        crypto->hsu_reg->REG_LENGTH.all = 0;
    }

    //hsu_status_clear_set(HSU_DONE_CLEAR_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;
    //hsu_enable_crypto_irq();
    crypto->hsu_reg->REG_IRQ_CTRL_EN.bit.CRYPTO_IRQ_EN = 0;
    //hsu_control_set(ctrl);
    crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 0;
    if (is_end)
        crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 1;
    else
        crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 0;
    crypto->hsu_reg->REG_CONTROL.bit.MODE = HSU_MODE_TKIP_MIC;
    crypto->hsu_reg->REG_CONTROL.bit.START = 1;
    //hsu_wait_done(HSU_DONE_SET_TKIP_BIT, true);
    CRYPTO_HSU_WAIT_DONE(TKIP);

    if(result != NULL)
    {
        uint32_t *mic_tab = (uint32_t*)&crypto->hsu_reg->REG_MIC_TAB.all;
        result[0] = mic_tab[0];
        result[1] = mic_tab[1];
        result[2] = crypto->hsu_reg->REG_REMAINING.all;
    }

    return CSK_DRIVER_OK;
}

int32_t
CRYPTO_IP_Checksum(void *res, const uint8_t *addr, uint16_t len, uint16_t *checksum)
{
    CHECK_RESOURCES(res);

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if (((uint32_t)addr >= PSRAM_BASE_ADDRESS) && DCachePresent())
    {
        vPortEnterCritical();
        HAL_FlushDCache_by_Addr((uint32_t *)addr, len);
        vPortExitCritical();
    }
#endif

    //hsu_source_addr_chk_set(addr);
    crypto->hsu_reg->REG_SOURCE_ADDR_CHK.all = (uint32_t)addr;
    //hsu_length_chk_setf(len);
    crypto->hsu_reg->REG_LENGTH_CHK.all = len;
    //hsu_status_clear_chk_set(HSU_DONE_CLEAR_CHK_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR_CHK.bit.DONE_CLEAR_CHK = 1;
    //hsu_control_chk_set(ctrl);
    crypto->hsu_reg->REG_CONTROL_CHK.bit.FIRST_BUFFER_CHK = 1;
    crypto->hsu_reg->REG_CONTROL_CHK.bit.LAST_BUFFER_CHK = 1;
    crypto->hsu_reg->REG_CONTROL_CHK.bit.START_CHK = 1;
    //hsu_wait_done(HSU_DONE_SET_CHK_BIT, false);
    CRYPTO_HSU_WAIT_DONE(CHK);
    //*checksum = hsu_mic_chk_getf();
    *checksum = crypto->hsu_reg->REG_MIC_TAB_CHK.all;

    return CSK_DRIVER_OK;
}
