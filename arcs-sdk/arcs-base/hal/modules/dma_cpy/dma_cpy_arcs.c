#if 1 // use DMA copy

/*
 * Modified on Apr. 8, 2024 for MARS
 *
 * NOTE: Two changes compared with previous version (VENUS)
 *      1. use dma_channel_setup (called once) + dma_channel_start_block (called repeatedly)
 *         instead of the over-burdened function dma_channel_configure (called repeatedly)
 *      2. Remove POLLING functions which were used to DMA copy small amounts of data
 *         and wait infinitely for DMA complete instead of DMA interrupt.
 *      3. Support multi-threaded context
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dma.h"
#include "dma_cpy.h"

#define ALG_DMA_BSIZE   DMA_BSIZE_8 //DMA_BSIZE_16 //
#define ALG_DMA_WIDTH   DMA_WIDTH_WORD //DMA_WIDTH_HALFWORD //DMA_WIDTH_BYTE //

// use new DMA API dma_channel_setup
#define USE_DMA_CHANNEL_SETUP   0

// DMA channel event array
static uint32_t volatile s_DMAEvents[DMA_NUMBER_OF_CHANNELS] = { 0 };

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    uint8_t chn = (event_info >> 8) & 0xFF;
    assert(chn < DMA_NUMBER_OF_CHANNELS);

    s_DMAEvents[chn] = event_info & 0xFF;
}


bool dma_init(int chn)
{
    dma_initialize();

    if (chn == DMA_CHANNEL_ANY)
        chn = ALG_DMA_CH;

    // reserve a dedicated DMA channel for algorithm
    uint8_t ret_ch = dma_channel_reserve(chn, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);
    if (ret_ch == DMA_CHANNEL_ANY)
        return false; // failed to reserve specified DMA channel

#if USE_DMA_CHANNEL_SETUP
    // setup DMA channel of algorithm
    uint32_t control, config_low, config_high;
    control = DMA_CH_CTLL_DST_WIDTH(ALG_DMA_BSIZE) | DMA_CH_CTLL_SRC_WIDTH(ALG_DMA_BSIZE) |
            DMA_CH_CTLL_DST_BSIZE(ALG_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(ALG_DMA_BSIZE) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |
            DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;
    config_low = DMA_CH_CFGL_CH_PRIOR(0); // set priority of algorithm DMA channel to 0 (LOW)
    config_high = DMA_CH_CFGH_FIFO_MODE;// DMA_CH_CFGH_PROTCTL(1) |

    int32_t nret = dma_channel_setup (chn, 0x1, control, config_low, config_high, 0, 0);
    if (nret < 0) {
        //LOGD("%s: Failed to call dma_channel_setup!!\r\n", __func__);
        dma_channel_disable(chn, false);
        dma_channel_unreserve(chn);
        return false;
    }
#endif

    return true;
}


void dma_wait_complete(int chn)
{
    uint8_t end_event = DMA_EVENT_TRANSFER_COMPLETE | DMA_EVENT_ERROR;
    assert(chn < DMA_NUMBER_OF_CHANNELS);

    while((s_DMAEvents[chn] & end_event) == 0);

    if (s_DMAEvents[chn] & DMA_EVENT_ERROR) { // DMA Error
        assert(0);
    }

    // clear DMAEvent
    s_DMAEvents[chn] = 0;
}


void dma_cpy_async(int chn, void *dst, void *src, int32_t size)
{
    uint32_t dst_addr = (uint32_t)dst;
    uint32_t src_addr = (uint32_t)src;

#if (ALG_DMA_WIDTH == DMA_WIDTH_WORD)
    assert(size > 0 && !(size & 0x3));
    assert(!(src_addr & 0x3) && !(dst_addr & 0x3));
    //size = (size >> 2);
    size = ((size + 3) >> 2);
#elif (ALG_DMA_WIDTH == DMA_WIDTH_HALFWORD)
    assert(size > 0 && !(size & 0x1));
    assert(!(src_addr & 0x1) && !(dst_addr & 0x1));
    //size = (size >> 1);
    size = ((size + 1) >> 1);
#endif

    // setup DMA channel of algorithm
#if USE_DMA_CHANNEL_SETUP
    int32_t ret = dma_channel_start_block (chn,
                        DMACH_CFG_FLAG_BOTH_ADDR,
                        src_addr,
                        dst_addr,
                        size);
#else // !USE_DMA_CHANNEL_SETUP
    uint32_t control, config_low, config_high;
    control = DMA_CH_CTLL_DST_WIDTH(ALG_DMA_BSIZE) | DMA_CH_CTLL_SRC_WIDTH(ALG_DMA_BSIZE) |
            DMA_CH_CTLL_DST_BSIZE(ALG_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(ALG_DMA_BSIZE) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |
            DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;
    config_low = DMA_CH_CFGL_CH_PRIOR(0); // set priority of algorithm DMA channel to 0 (LOW)
    config_high = DMA_CH_CFGH_FIFO_MODE;// DMA_CH_CFGH_PROTCTL(1) |

    int32_t ret = dma_channel_configure (chn, (uint32_t) src, (uint32_t) dst, size,
                                           control, config_low, config_high, 0, 0);
#endif  // !USE_DMA_CHANNEL_SETUP
    assert(ret == 0);
}


void opi_psram_cpy_in(void *dst, void *src, int32_t size)
{
	dma_cpy_async(ALG_DMA_CH, dst, src, size);
	dma_wait_complete(ALG_DMA_CH);
}

void opi_psram_cpy_out(void *dst, void *src, int32_t size)
{
	dma_cpy_async(ALG_DMA_CH, dst, src, size);
	dma_wait_complete(ALG_DMA_CH);
}

void dma_uninit(int chn)
{
    if (chn < DMA_NUMBER_OF_CHANNELS) {
        dma_channel_disable(chn, false);
        dma_channel_unreserve(chn);
        s_DMAEvents[chn] = 0;
    }
}

/*
void opi_psram_cpy_in_pro(void *dst, void *src, int32_t size, dma_sync_call_func func, void* param)
{
	if (NULL != func)
	{
		func(param);
	}
	memcpy(dst, src, size);
}

void opi_psram_cpy_out_pro(void *dst, void *src, int32_t size, dma_sync_call_func func, void* param)
{
	if (NULL != func)
	{
		func(param);
	}
	memcpy(dst, src, size);
}
*/


#else // use memcpy instead of DMA copy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opi_psram_cpy.h"

//bool dma_init()
bool dma_init(int chn)
{
    return true;
}

int32_t count_dam_cpy = 0;
void dma_wait_complete(int chn)
{

}

void dma_cpy_async(int chn, void *dst, void *src, int32_t size)
{
	memcpy(dst, src, size);

	count_dam_cpy += 1;
	//printf("count_dam_cpy:%d, dst:%p, src:%p, size:%d--%x \n", count_dam_cpy, dst, src, size, size);
}


void opi_psram_cpy_in(void *dst, void *src, int32_t size)
{
	memcpy(dst, src, size);
}

void opi_psram_cpy_out(void *dst, void *src, int32_t size)
{
	memcpy(dst, src, size);
}

/*
void opi_psram_cpy_in_pro(void *dst, void *src, int32_t size, dma_sync_call_func func, void* param)
{
	if (NULL != func)
	{
		func(param);
	}
	memcpy(dst, src, size);
}

void opi_psram_cpy_out_pro(void *dst, void *src, int32_t size, dma_sync_call_func func, void* param)
{
	if (NULL != func)
	{
		func(param);
	}
	memcpy(dst, src, size);
}
*/

void dma_uninit(int chn)
{
}

#endif 
