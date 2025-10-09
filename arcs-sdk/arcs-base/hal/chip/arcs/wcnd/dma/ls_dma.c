/*
 * ls_dma.c
 *
 *  dma functions
 */


/**
****************************************************************************************
* @addtogroup dma
* @ingroup 
* @brief dma function for ceva ip
*
* This is the driver block for ls chip
* @{
****************************************************************************************
*/

/**
 *****************************************************************************************
 * INCLUDE FILES
 *****************************************************************************************
 */

#include "bt_config.h"     // SW configuration

#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition

#include "dma.h"
#include "ble_drv.h"



#define LS_DMA_EN           (1)

/**
 ****************************************************************************************
 * FUNCTION INTERFACE
 ****************************************************************************************
 **/
#if (LS_DMA_EN)
static uint32_t ls_dma_init()
{
    return dma_initialize();
}

static uint32_t ls_dma_uninit()
{
    return dma_uninitialize();
}

static uint8_t volatile DMAEvent[DMA_MAX_NR_CHANNELS];

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    DMAEvent[(event_info & 0xFF00)>>8] = event_info & 0xFF;
}

static int32_t ls_dma_copy(uint8_t channel, void* p_dst_addr, const void* p_src_addr, uint32_t size)
{
    int32_t stat = 0;
    uint8_t ch = channel;

    if (DMAEvent[ch] != DMA_EVENT_TRANSFER_COMPLETE)
    {
        // last data trans error
        return DMAEvent[ch];
    }

    // Select channel
    dma_channel_select(&ch, DMA_DrvEvent, 0, DMA_CACHE_SYNC_AUTO);

    if (ch == DMA_CHANNEL_ANY)
    {
        // Channel error
        return -2;
    }

    stat = dma_channel_configure (ch, (uint32_t) p_src_addr, (uint32_t) p_dst_addr, size,
                DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_BYTE) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_BYTE) |\
                DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_8) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_8) |\
                DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |\
                DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                DMA_CH_CFGL_CH_PRIOR(1), // config_low
                DMA_CH_CFGH_FIFO_MODE, // config_high
                0, 0);

    return stat;
}
#else
static uint32_t ls_dma_init()
{
    return 0;
}

static uint32_t ls_dma_uninit()
{
    return 0;
}


static int32_t ls_dma_copy(uint8_t channel, void* p_dst_addr, const void* p_src_addr, uint32_t size)
{
    memcpy(p_dst_addr, p_src_addr, size);
    return 0;
}
#endif

void ls_dma_api_init(void *api)
{
    if (NULL == api)
    {
        return;
    }

    struct lsip_dma_api_str *dma_api = (struct lsip_dma_api_str *)api;

    dma_api->dma_ch = 0;
    dma_api->dma_init = ls_dma_init;
    dma_api->dma_uninit = ls_dma_uninit;
    dma_api->dma_copy = ls_dma_copy;

    return;
}

