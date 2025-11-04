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

/*
static uint32_t volatile DMAEvent = 0;

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    DMAEvent = event_info & 0xFF;
}
*/

// DMA channel event array
static uint32_t volatile s_DMAEvents[DMA_NUMBER_OF_CHANNELS] = { 0 };

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    uint8_t chn = (event_info >> 8) & 0xFF;
    assert(chn < DMA_NUMBER_OF_CHANNELS);

    s_DMAEvents[chn] = event_info & 0xFF;
}


/*
static int32_t psram_memcopy(uint8_t *ch, void* dst, void* src, uint32_t size, uint32_t bsize, uint32_t wsize){
    // size      :  size of sample

    // wsize = 0 :  byte
    //       = 1 :  half word
    //       = 2 :  word


    int32_t stat = 0;

    // clear interrupt status if necessary
    if (dma_channel_is_polling(*ch))
        dma_channel_clear_xfer_status(*ch);

    if (size <= MAX_BLK_TS) { // use polling if size <= Max block size

        // Select channel
        if (*ch != ALG_DMA_CH) // channel 0 has already been reserved in dma_init!!
            dma_channel_select(ch, NULL, 0, DMA_CACHE_SYNC_NOP);

        if (*ch == DMA_CHANNEL_ANY){
            // Channel error
            return -1;
        }

        stat = dma_channel_configure_polling (*ch,
                    (uint32_t) src,
                    (uint32_t) dst,
                    size,
                    DMA_CH_CTLL_DST_WIDTH(wsize) | DMA_CH_CTLL_SRC_WIDTH(wsize) |\
                  DMA_CH_CTLL_DST_BSIZE(bsize) | DMA_CH_CTLL_SRC_BSIZE(bsize) |\
                  DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |\
                  DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0), // control
                  //DMA_CH_CFGL_CH_PRIOR(1), // config_low
                  DMA_CH_CFGL_CH_PRIOR(0), // config_low
                  0, // config_high
                  0, 0);

    } else { // use interrupt if size > 4095
        // Select channel
        if (*ch != ALG_DMA_CH) // channel 0 has already been reserved in dma_init!!
            dma_channel_select(ch, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);

        if (*ch == DMA_CHANNEL_ANY){
            // Channel error
            return -1;
        }

        stat = dma_channel_configure (*ch,
                    (uint32_t) src,
                    (uint32_t) dst,
                    size,
                    DMA_CH_CTLL_DST_WIDTH(wsize) | DMA_CH_CTLL_SRC_WIDTH(wsize) |\
                  DMA_CH_CTLL_DST_BSIZE(bsize) | DMA_CH_CTLL_SRC_BSIZE(bsize) |\
                  DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |\
                  //DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(1) | DMA_CH_CTLL_INT_EN, // control
                  DMA_CH_CTLL_DMS(2) | DMA_CH_CTLL_SMS(3) | DMA_CH_CTLL_INT_EN, // control
                  //DMA_CH_CFGL_CH_PRIOR(1), // config_low
                  DMA_CH_CFGL_CH_PRIOR(0), // config_low
                  0, // config_high
                  0, 0);
    }

    if (stat == -1){
        return -2;
    }

    return 0;
}

#if (ALG_DMA_CH == 0)
//#define PSRAM_MEMCPY_16_W(ch, dst, src, size)      psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_32, DMA_WIDTH_WORD)
//#define PSRAM_MEMCPY_16_W(ch, dst, src, size)      psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_16, DMA_WIDTH_WORD)
#define PSRAM_MEMCPY_16_W(ch, dst, src, size)      psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_8, DMA_WIDTH_WORD)
#else
#define PSRAM_MEMCPY_16_W(ch, dst, src, size)      psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_16, DMA_WIDTH_WORD)
#endif
*/


/*
void dma_init()
{
//    // disable inst+data cache prefetch operation to avoid cache coherence issue
//    xthal_set_cache_prefetch(XTHAL_PREFETCH_DISABLE);

    // disable only data cache prefetch operation to avoid cache coherence issue
//    xthal_set_cache_prefetch(XTHAL_DCACHE_PREFETCH_OFF);

    dma_initialize();
    dma_channel_reserve(ALG_DMA_CH, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);
}
*/

bool dma_init(int chn)
{
    dma_initialize();

    if (chn == DMA_CHANNEL_ANY)
        chn = ALG_DMA_CH;

    // reserve a dedicated DMA channel for algorithm
    uint8_t ret_ch = dma_channel_reserve(chn, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);
    if (ret_ch == DMA_CHANNEL_ANY)
        return false; // failed to reserve specified DMA channel

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

    return true;
}


/*
void dma_wait_complete(int chn)
{
    if (dma_channel_is_polling(chn)) {
        while (!dma_channel_xfer_complete(chn) && !dma_channel_xfer_error(chn));
        dma_channel_clear_xfer_status(chn);
    } else {
        while((DMAEvent & (DMA_EVENT_TRANSFER_COMPLETE | DMA_EVENT_ERROR)) == 0);
        if (DMAEvent & DMA_EVENT_ERROR) { // Error
            assert(0);
        }
        DMAEvent = 0;
    }
}
*/

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


/*
void dma_cpy_async(int chn, void *dst, void *src, int32_t size)
{
	//size = (size >> 2) + 1;
	//PSRAM_MEMCPY_16_W((uint8_t *)&chn, ((uint32_t)dst & (uint32_t)0x480FFFFF), src, size);

    assert(size > 0 && !(size & 0x3));
	//size = (size >> 2);
	size = ((size + 3) >> 2);
	assert(!((uint32_t)src & 0x3) && !((uint32_t)dst & 0x3));
	if ((uint32_t)src >> 20 == 0x5fe)
	{
		src = (void *)((uint32_t)src & 0x480FFFFF);
	}
	if ((uint32_t)dst >> 20 == 0x5fe)
	{
		dst = (void *)((uint32_t)dst & 0x480FFFFF);
	}
	PSRAM_MEMCPY_16_W((uint8_t *)&chn, dst, src, size);
}
*/

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

    int32_t ret = dma_channel_start_block (chn,
                        DMACH_CFG_FLAG_BOTH_ADDR,
                        src_addr,
                        dst_addr,
                        size);
    assert(ret == 0);
}


void opi_psram_cpy_in(void *dst, void *src, int32_t size)
{
	//memcpy(dst, src, size);
	dma_cpy_async(ALG_DMA_CH, dst, src, size);
	dma_wait_complete(ALG_DMA_CH);
}

void opi_psram_cpy_out(void *dst, void *src, int32_t size)
{
	//memcpy(dst, src, size);
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
