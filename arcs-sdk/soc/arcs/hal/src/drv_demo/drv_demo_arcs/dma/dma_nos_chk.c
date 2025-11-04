/*
 * dma_chk_nos.c
 *
 *  Created on: Jul 22, 2020
 *
 *  Revision:
 *
 */

#include "dma.h"
#include "nos_timer.h"
#include "log_print.h"
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define MASTER_SEL_CNT  4
#define SRC_MASTER_SEL  0 // only support Master 0 on AP (Options: 0 ~ 3)
#define DST_MASTER_SEL  0 // only support Master 0 on AP (Options: 0 ~ 3)

#define SMS_NO  SRC_MASTER_SEL
#define DMS_NO  DST_MASTER_SEL


// Event flag
static uint32_t volatile DMAEvent;

// Max. size of SRC/DST buffer
#define MaxLen          1024 // 1024*5

// SRC buffer size
#define SourceLen       (MaxLen-1) //100 //

// DST buffer size
#define ReceiveLen      (MaxLen-1) //100 //

// unit length
#define UnitLen         4 // 1

//static _DMA uint8_t SourceBuf[MaxLen] = {0};
static uint32_t SourceBuf[MaxLen] = {0};
//static _DMA uint8_t DestinBuf[MaxLen] = {0};
static uint32_t DestinBuf[MaxLen] = {0};


uint32_t TC_total = 0, TC_pass = 0, TC_fail = 0;

//=============================================================================
static void
DMA_Src_Buf_Gen(uint8_t * buffer, uint32_t size)
{
    //LOGD("[%s]: generate random buffer......\r\n", __func__);
    int i;
    for (i = 0; i < size; i++) {
        buffer[i] = i%256;
    }
}

static void
DMA_Dst_Buf_Gen(uint8_t * buffer, uint32_t size){
    //LOGD("[%s]: generate random buffer......\r\n", __func__);
    int i = 0;
    for (i = 0; i < size; i++) {
        buffer[i] = (i*i)%256;
    }
}

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    LOGD("[%s]: event = %d, channel = %d, xfer_bytes = %d\r\n", __func__,
            event_info & 0xFF, (event_info >> 8) & 0xFF, xfer_bytes);
    DMAEvent = event_info & 0xFF;
}

static void
DMA_Waiting(void)
{
    while(1){
        if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
    }
}

// return false if timeout
static bool
DMA_Waiting_timeout(uint32_t max_wait_ms)
{
    bool ret = false;

    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms){
        if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

//=============================================================================
void DMA_mem_int(uint32_t total_bytes)
{
    DMA_Src_Buf_Gen((uint8_t*)SourceBuf, total_bytes);
    DMA_Dst_Buf_Gen((uint8_t*)DestinBuf, total_bytes);
}

// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
bool DMA_configure_check(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                        uint32_t dst_width, uint32_t dst_bsize,
                        bool addr_dec, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    total_bytes = SourceLen * UnitLen;
    DMA_mem_int(total_bytes);

    //access SrcBuffer / DstBuffer before DMA ferries data
    SourceBuf[0] = 0xAA; SourceBuf[1] = 0xCC;
    DestinBuf[0] = 0x77; DestinBuf[1] = 0x99;

//    if (src_width > DMA_WIDTH_MAX)
//        src_width = DMA_WIDTH_MAX;
//    if (dst_width > DMA_WIDTH_MAX)
//        dst_width = DMA_WIDTH_MAX;

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);

    if (addr_dec) {
        src_addr += total_bytes - (0x1 << src_width);
        dst_addr += total_bytes - (0x1 << dst_width);
        control |= DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_DEC;
    } else {
        control |= DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC;
    }

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    //access DstBuffer while DMA is ferrying data
//    DestinBuf[SourceLen - 1] = 0xDD;

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    DMA_Waiting();

    if(memcmp(SourceBuf, DestinBuf, total_bytes) == 0){
        LOGD("[%s OK] channel %d, Memory compare success ~~~~~\r\n", __func__, ch);
        return true;
    }else{
        LOGD("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", __func__, ch);
        return false;
    }
}


// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
bool DMA_configure_polling_check(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                        uint32_t dst_width, uint32_t dst_bsize,
                        bool addr_dec, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    total_bytes = SourceLen * UnitLen;
    DMA_mem_int(total_bytes);

    //access SrcBuffer / DstBuffer before DMA ferries data
    SourceBuf[0] = 0xAA; SourceBuf[1] = 0xCC;
    DestinBuf[0] = 0x77; DestinBuf[1] = 0x99;

//    if (src_width > DMA_WIDTH_MAX)
//        src_width = DMA_WIDTH_MAX;
//    if (dst_width > DMA_WIDTH_MAX)
//        dst_width = DMA_WIDTH_MAX;

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);

    if (addr_dec) {
        src_addr += total_bytes - (0x1 << src_width);
        dst_addr += total_bytes - (0x1 << dst_width);
        control |= DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_DEC;
    } else {
        control |= DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC;
    }

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure_polling (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    //access DstBuffer while DMA is ferrying data
//    DestinBuf[SourceLen - 1] = 0xDD;

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    while (!dma_channel_xfer_complete(ch));
    dma_channel_clear_xfer_status(ch);

    if(memcmp(SourceBuf, DestinBuf, total_bytes) == 0){
        LOGD("[%s OK] channel %d, Memory compare success ~~~~~\r\n", __func__, ch);
        return true;
    }else{
        LOGD("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", __func__, ch);
        return false;
    }
}


// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
bool DMA_configure_check_SrcGather(uint8_t *pch, DMA_CACHE_SYNC cache_sync)
{
    int i, stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t src_gath = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count=2, interval=2

    uint32_t *p = SourceBuf;
    for (i=0; i<MaxLen/4; i++) {
        *p++ = 0x11111111;
        *p++ = 0x22222222;
        *p++ = 0x33333333;
        *p++ = 0x44444444;
    }
    memset(DestinBuf, 0, sizeof(DestinBuf));

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);
    control |= DMA_CH_CTLL_S_GATH_EN;

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, MaxLen/2,
                                control, config_low, config_high, src_gath, 0);

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    DMA_Waiting();

    bool bOK = false;

    p = DestinBuf;
    while (true) {
        for (i=0; i<MaxLen/4; i++) {
            if (*p++ != 0x11111111) break;
            if (*p++ != 0x22222222) break;
        }
        if (i < MaxLen/4)
            break;
        for (i=0; i<MaxLen/2; i++) {
            if (*p++ != 0x0)    break;
        }
        bOK = (i == MaxLen/2);
        break;
    } // end while

    if(bOK){
        LOGD("[%s OK] channel %d, success ~~~~~\r\n", __func__, ch);
    }else{
        LOGD("[%s FAILED] channel %d, error !!!!!\r\n", __func__, ch);
    }
    return bOK;
}


// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
bool DMA_configure_check_DstScatter(uint8_t *pch, DMA_CACHE_SYNC cache_sync)
{
    int i, stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t dst_scat = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count=2, interval=2

    uint32_t *p = SourceBuf;
    for (i=0; i<MaxLen/2; i++) {
        *p++ = 0x33333333;
        *p++ = 0x44444444;
    }
    memset(DestinBuf, 0, sizeof(DestinBuf));

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);
    control |= DMA_CH_CTLL_D_SCAT_EN;

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, MaxLen/2,
                                control, config_low, config_high, 0, dst_scat);

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    DMA_Waiting();

    bool bOK = false;

    p = DestinBuf;
    while (true) {
        for (i=0; i<MaxLen/4; i++) {
            if (*p++ != 0x33333333) break;
            if (*p++ != 0x44444444) break;
            if (*p++ != 0)  break;
            if (*p++ != 0)  break;
        }
        bOK = (i == MaxLen/4);
        break;
    } // end while

    if(bOK){
        LOGD("[%s OK] channel %d, success ~~~~~\r\n", __func__, ch);
    }else{
        LOGD("[%s FAILED] channel %d, error !!!!!\r\n", __func__, ch);
    }
    return bOK;
}


// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
DMA_LLI items[4];
bool DMA_configure_LLP_check_4Blk(uint8_t *pch, DMA_CACHE_SYNC cache_sync)
{
    int i, stat = 0;
    uint32_t control, config_low, config_high;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    memset(items, 0, sizeof(items));
    items[0].SAR = (uint32_t)(&SourceBuf[0] + MaxLen * 2 / 4); // 2#
    items[0].DAR = (uint32_t)(&DestinBuf[0] + MaxLen * 3 / 4); // 3#
    items[0].LLP = (uint32_t)(&items[1]);
    items[1].SAR = (uint32_t)(&SourceBuf[0] + MaxLen * 1 / 4); // 1#
    items[1].DAR = (uint32_t)(&DestinBuf[0] + MaxLen * 2 / 4); // 2#
    items[1].LLP = (uint32_t)(&items[2]);
    items[2].SAR = (uint32_t)(&SourceBuf[0] + MaxLen * 0 / 4); // 0#
    items[2].DAR = (uint32_t)(&DestinBuf[0] + MaxLen * 1 / 4); // 1#
    items[2].LLP = (uint32_t)(&items[3]);
    items[3].SAR = (uint32_t)(&SourceBuf[0] + MaxLen * 3 / 4); // 3#
    items[3].DAR = (uint32_t)(&DestinBuf[0] + MaxLen * 0 / 4); // 0#
    items[3].LLP = 0;

    uint8_t *p = (uint8_t*)SourceBuf;
    memset(p, 0x55, MaxLen); // 1KB 0x55
    p += MaxLen;
    memset(p, 0x66, MaxLen); // 1KB 0x66
    p += MaxLen;
    memset(p, 0x77, MaxLen); // 1KB 0x77
    p += MaxLen;
    memset(p, 0x88, MaxLen); // 1KB 0x88
    memset(DestinBuf, 0, sizeof(DestinBuf));


    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);
    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    for (i=0; i<4; i++) {
        items[i].CTL_LO = control;
        items[i].u.SIZE = MaxLen / 4;
    }

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure_LLP (ch, items, config_low, config_high, 0, 0);

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    DMA_Waiting();

    bool bOK = false;

    // Dst Buffer is supposed to be 0x88...0x55...0x66...0x77

    while (true) {
        p = (uint8_t*)DestinBuf;
        for (i=0; i<MaxLen; i++) {
            if (*p++ != 0x88)
                break;
        }
        if (i<MaxLen)   break;

        for (i=0; i<MaxLen; i++) {
            if (*p++ != 0x55)
                break;
        }
        if (i<MaxLen)   break;

        for (i=0; i<MaxLen; i++) {
            if (*p++ != 0x66)
                break;
        }
        if (i<MaxLen)   break;

        for (i=0; i<MaxLen; i++) {
            if (*p++ != 0x77)
                break;
        }

        bOK = (i == MaxLen);
        break;
    } // end while

    if(bOK){
        LOGD("[%s OK] channel %d, success ~~~~~\r\n", __func__, ch);
    }else{
        LOGD("[%s FAILED] channel %d, error !!!!!\r\n", __func__, ch);
    }
    return bOK;
}


// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// chk_susp_only = true, return true if really suspended, false if NOT suspended
// chk_susp_only = false, return the result of memory comparison: true = same, false = different
// first enable channel xfer with SUSP flag set, and then enable channel
bool DMA_configure_check_SuspEn(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_susp_only)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    total_bytes = SourceLen * UnitLen;
    DMA_mem_int(total_bytes);

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);

    config_low = DMA_CH_CFGL_CH_PRIOR(0) | DMA_CH_CFGL_CH_SUSP;
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

#if IC_BOARD // add some delay on ASIC
    volatile uint32_t i = 10;
    while (i-- > 0);
#endif

    if (dma_channel_get_status(ch) == 0) {
        LOGD("%s: CANNOT TEST for DMA has been done on channel %d!\n", __func__, ch);
        return false;
    }

    bool ret = DMA_Waiting_timeout(5000); // 5000 ms = 5s
    if (chk_susp_only) {
        dma_channel_disable(ch, true); // release the selected DMA channel
        //dma_channel_disable(ch, false); // now channel suspend, so channel FIFO can never be emptied...
        return !ret;
    }

    if (!ret) { // timeout
        dma_channel_enable(ch);
        DMA_Waiting();
    }

    if(memcmp(SourceBuf, DestinBuf, total_bytes) == 0){
        LOGD("[%s OK] channel %d, Memory compare success ~~~~~\r\n", __func__, ch);
        return true;
    }else{
        LOGD("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", __func__, ch);
        return false;
    }

}


// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// chk_susp_only = true, return true if really suspended, false if NOT suspended
// chk_susp_only = false, return the result of memory comparison: true = same, false = different
// first enable channel xfer, then suspend it, and resume it finally.
bool DMA_configure_check_SuspResm(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_susp_only)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    total_bytes = SourceLen * UnitLen;
    DMA_mem_int(total_bytes);

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

#if IC_BOARD // add some delay on ASIC
    volatile uint32_t i = 100;
    while (i-- > 0);
#endif

    if (dma_channel_get_status(ch) == 0) {
        LOGD("%s: CANNOT TEST for DMA has been done on channel %d!\n", __func__, ch);
        return false;
    }

    if (dma_channel_suspend(ch, true) == -1) {
        LOGD("[FAILED] dma_channel_suspend error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    bool ret = DMA_Waiting_timeout(5000); // 5000 ms = 5s
    if (chk_susp_only) {
        dma_channel_disable(ch, true); // release the selected DMA channel
        return !ret;
    }

    if (!ret) { // timeout
        dma_channel_resume(ch);
        ret = DMA_Waiting_timeout(5000); // 5000ms
        if (!ret) { // timeout
            dma_channel_disable(ch, true); // release the selected DMA channel
            return false;
        }
    }

    if(memcmp(SourceBuf, DestinBuf, total_bytes) == 0){
        LOGD("[%s OK] channel %d, Memory compare success ~~~~~\r\n", __func__, ch);
        return true;
    }else{
        LOGD("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", __func__, ch);
        return false;
    }

}

// DMA configure check for cache sync/coherence, src/dst width, addr inc/dec etc.
// chk_dis_only = true, return true if really suspended, false if NOT suspended
// chk_dis_only = false, return the result of memory comparison: true = same, false = different
bool DMA_configure_check_Dis(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_dis_only)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    total_bytes = SourceLen * UnitLen;
    DMA_mem_int(total_bytes);

    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        LOGD("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

#if IC_BOARD // add some delay on ASIC
    volatile uint32_t cycles = 10;
    while(cycles > 0) cycles--;
#endif

    if (dma_channel_get_status(ch) == 0) {
        LOGD("%s: CANNOT TEST for DMA has been done on channel %d!\n", __func__, ch);
        return false;
    }

    dma_channel_disable(ch, true);

    bool ret = DMA_Waiting_timeout(5000); // 5000 ms = 5s
    if (chk_dis_only) {
        //dma_channel_disable(ch, true);
        return !ret;
    }

    if (!ret) { // timeout
        dma_channel_enable(ch);
        ret = DMA_Waiting_timeout(5000); // 5000ms
        if (!ret) // timeout
            return false;
    }

    if(memcmp(SourceBuf, DestinBuf, total_bytes) == 0){
        LOGD("[%s OK] channel %d, Memory compare success ~~~~~\r\n", __func__, ch);
        return true;
    }else{
        LOGD("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", __func__, ch);
        return false;
    }

}


// DMA memcpy check for cache sync/coherence, different src/dst address start.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
bool DMA_memcpy_check(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;
    uint8_t *psrc, *pdst;

    //access SrcBuffer / DstBuffer before DMA ferries data
    psrc = (uint8_t *)src_addr;
    pdst = (uint8_t *)dst_addr;
    *psrc++ = 0xAA; *psrc++ = 0xBB; *psrc++ = 0xCC;
    *pdst++ = 0x77; *pdst++ = 0x88; *pdst++ = 0x99;

//    uint8_t ch = dma_channel_select(NULL, DMA_DrvEvent, 0, cache_sync);
//    if (stat == -1) {
//        LOGD("[FAILED] NO free DMA channel!!\r\n");
//        return false;
//    }

    uint8_t ch = 0;
    if (!dma_channel_is_reserved(ch)) {
        ch = dma_channel_reserve(ch, DMA_DrvEvent, 0, cache_sync);
    }

    if (ch == DMA_CHANNEL_ANY)
        ch = dma_channel_select(&ch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        LOGD("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }

    LOGD("%s: use DMA channel %d\r\n", __func__, ch);
    stat = dma_memcpy (ch, src_addr, dst_addr, total_bytes);

    //access DstBuffer while DMA is ferrying data
//    pdst = (uint8_t *)(dst_addr + total_bytes - 1);
//    *pdst = 0xDD;

    if(stat == -1){
        LOGD("[FAILED] dma_memcpy error.\r\n");
        return false;
    }

    DMA_Waiting();

    //if(memcmp(SourceBuf, DestinBuf, total_bytes) == 0){
    if(memcmp((uint8_t*)src_addr, (uint8_t*)dst_addr, total_bytes) == 0){
        LOGD("[%s OK] Memory compare success ~~~~~\r\n", __func__);
        return true;
    }else{
        LOGD("[%s FAILED] Memory compare error !!!!!\r\n", __func__);
        return false;
    }
}


void record_result(bool expected, bool returned, const char * hint)
{
    bool OK = (returned == expected);

    TC_total++;
    if (OK) {
        TC_pass++;
        LOGD("PASSED: %s\r\n\r\n", hint);
    } else {
        TC_fail++;
        LOGD("FAILED: %s\r\n\r\n", hint);
    }

}

// dma check task
void check_dma_configure_cases()
{

    //
    // DMA configure check
    //
    bool ret;
    uint8_t ch, ch_sel;

    for (ch = 0; ch < DMA_NUMBER_OF_CHANNELS; ch++) { // DMA_NUMBER_OF_CHANNELS
    LOGD("============= DMA channel %d =============\r\n\r\n", ch);

    // DMA configure 0: src/dst addr WORD aligned
    LOGD("Check DMA configure POLLING, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_polling_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "basic config. (burst16,WORD)");
    //continue;

    // DMA configure 1: src/dst addr WORD aligned
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "basic config. (burst16,WORD)");

    // DMA configure 2: src/dst addr HALFWORD aligned
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_HALFWORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_HALFWORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_HALFWORD, DMA_BSIZE_16, DMA_WIDTH_HALFWORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "HALFWORD");

    // DMA configure 3: src/dst addr BYTE aligned
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_BYTE, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_BYTE, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_BYTE, DMA_BSIZE_16, DMA_WIDTH_BYTE, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "BYTE");

/*
    // DMA configure 4: src/dst addr DWORD aligned
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_DOUBWORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_DOUBWORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_DOUBWORD, DMA_BSIZE_16, DMA_WIDTH_DOUBWORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "DWORD");

    // DMA configure 5: src/dst addr QWORD aligned
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_QUADWORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_QUADWORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_QUADWORD, DMA_BSIZE_16, DMA_WIDTH_QUADWORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "QWORD");

    // DMA configure 6: src/dst addr OWORD aligned
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_OCTUWORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_OCTUWORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_OCTUWORD, DMA_BSIZE_16, DMA_WIDTH_OCTUWORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "OWORD");
*/

    // DMA configure 7: dst BYTE aligned, burst_size = 64
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_BYTE, dst_burst = DMA_BSIZE_64, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_BYTE, DMA_BSIZE_64, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "dst burst64, BYTE");

    // DMA configure 8: src BYTE aligned, burst_size = 64
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_BYTE, src_burst = DMA_BSIZE_64, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_BYTE, DMA_BSIZE_64, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst64, BYTE");

    // DMA configure 9: src/dst addr decrease
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address DEC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_WORD, DMA_BSIZE_16, true, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "addr DEC");

    // DMA configure 10: burst size 1
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_1, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_1, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 1");

    // DMA configure 11: burst size 4
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_4, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_4, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 4");

    // DMA configure 12: burst size 8
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_8, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_8, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 8");

    // DMA configure 13: burst size 32
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_32, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_32, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 32");

    // DMA configure 14: burst size 64
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_64, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_64, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 64");

    // DMA configure 15: burst size 128
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_128, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_128, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 128");

    // DMA configure 16: burst size 256
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_256, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_256, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "src burst 256");


    // DMA configure 17: burst size 32 (dst)
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
            "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_32, address INC\r\n");
    ch_sel = ch;
    ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_WORD, DMA_BSIZE_32, false, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "dst burst 32");

    // DMA configure 18: Src Gather
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "Src Gather Count = Interval = 2\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_SrcGather(&ch_sel, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "SrcGather 2/2");

    // DMA configure 19: Dst Scatter
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "Dst Scatter Count = Interval = 2\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_DstScatter(&ch_sel, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "DstScatter 2/2");

    } // end for ch

}


// dma check task
void check_dma_misc_cases()
{

    bool ret;
    uint8_t ch, ch_sel;

    for (ch = 0; ch < DMA_NUMBER_OF_CHANNELS; ch++) { // DMA_NUMBER_OF_CHANNELS

    LOGD("************** DMA channel %d **************\r\n\r\n", ch);

    //FIXME: uncomment following lines when cache is enabled
    /*
        // DMA configure : cache sync Error
        // NOTE: dst memory is modified, and if not invalidated, the dst cache may be inconsistent with dst memory,
        // So src cache/memory may be different from dst cache!!
        LOGD("Check DMA configure, DMA_CACHE_SYNC_SRC: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
                "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
        ch_sel = ch;
        ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_SRC);
        //record_result(true, ret, "SYNC_SRC");
        record_result(false, ret, "SYNC_SRC");

        // DMA configure : cache sync Error
        // NOTE: src memory may be written back by system, so the test result can be true...
        LOGD("Check DMA configure, DMA_CACHE_SYNC_DST: src_width = DMA_WIDTH_WORD, src_burst = DMA_BSIZE_16, "
                "dst_width = DMA_WIDTH_WORD, dst_burst = DMA_BSIZE_16, address INC\r\n");
        ch_sel = ch;
        ret = DMA_configure_check(&ch_sel, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_WORD, DMA_BSIZE_16, false, DMA_CACHE_SYNC_DST);
        record_result(true, ret, "SYNC_DST");
    */

    // DMA configure_LLP 20:
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "4 blocks for Src & Dst\r\n");
    ch_sel = ch;
    ret = DMA_configure_LLP_check_4Blk(&ch_sel, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "4 Blks LLP");

    // DMA configure 21: initially SUSP only
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "init SUSP channel\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_SuspEn(&ch_sel, DMA_CACHE_SYNC_AUTO, true);
    record_result(true, ret, "init SUSP channel");

    // DMA configure 22: SUSP + Enable
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "init SUSP & Enable channel\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_SuspEn(&ch_sel, DMA_CACHE_SYNC_AUTO, false);
    record_result(true, ret, "init SUSP & Enable channel");

    // DMA configure 23: Disable channel only
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "Disable channel\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_Dis(&ch_sel, DMA_CACHE_SYNC_AUTO, true);
    record_result(true, ret, "Disable channel");

    // DMA configure 24: Start Xfer, and then SUSP only
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "post SUSP channel\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_SuspResm(&ch_sel, DMA_CACHE_SYNC_AUTO, true);
    record_result(true, ret, "post SUSP channel");

    // DMA configure 25: Start Xfer, and then SUSP & Resume
    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
            "post SUSP & Resume channel\r\n");
    ch_sel = ch;
    ret = DMA_configure_check_SuspResm(&ch_sel, DMA_CACHE_SYNC_AUTO, false);
    record_result(true, ret, "post SUSP & Resume channel");

    //NOTE: The case "Disable + Enable channel" make AP go into HardFault exception handler.
    //  The explanation is that the dma channel configuration is lost after disabled, and
    //  then it's in a unknown state when enabled without any new configuration, so crash occurs.

//    // DMA configure 26: Disable + Enable channel--Expected: timeout even if enabled later...
//    LOGD("Check DMA configure, DMA_CACHE_SYNC_AUTO: src/dst_width = DMA_WIDTH_WORD, src/dst_burst = DMA_BSIZE_16, "
//            "Disable & Enable channel\r\n");
//    ch_sel = ch;
//    ret = DMA_configure_check_Dis(&ch_sel, DMA_CACHE_SYNC_AUTO, false);
//    record_result(false, ret, "Dis & En channel");

    } // end for ch

}


// dma check task
void check_dma_memcpy_cases()
{
    bool ret;
    uint32_t src_addr = (uint32_t)SourceBuf;
    uint32_t dst_addr = (uint32_t)DestinBuf;
    uint32_t total_bytes = SourceLen * UnitLen;

    //
    // DMA memcpy check
    //

    // DMA memcpy 1: src/dst addr WORD aligned
    LOGD("Check DMA memcpy: src = 0x%08X, dst = 0x%08X, size = 0x%08X\r\n",
            src_addr, dst_addr, total_bytes);
    DMA_mem_int(total_bytes);
    ret = DMA_memcpy_check(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "Check DMA memcpy: addr WORD aligned");

    // DMA memcpy 2: src/dst addr HALFWORD aligned
    LOGD("Check DMA memcpy: src = 0x%08X, dst = 0x%08X, size = 0x%08X\r\n",
            src_addr+2, dst_addr+2, total_bytes-2);
    DMA_mem_int(total_bytes);
    ret = DMA_memcpy_check(src_addr + 2, dst_addr + 2, total_bytes - 2, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "Check DMA memcpy: addr HALFWORD aligned");

    // DMA memcpy 3: src/dst addr BYTE aligned
    LOGD("Check DMA memcpy: src = 0x%08X, dst = 0x%08X, size = 0x%08X\r\n",
            src_addr+1, dst_addr+1, total_bytes-1);
    DMA_mem_int(total_bytes);
    ret = DMA_memcpy_check(src_addr + 1, dst_addr + 1, total_bytes - 1, DMA_CACHE_SYNC_AUTO);
    record_result(true, ret, "Check DMA memcpy: addr BYTE aligned");

    //FIXME: uncomment following lines when cache is enabled
	/*
        // DMA memcpy 4: cache sync only for src
        LOGD("Check DMA memcpy, DMA_CACHE_SYNC_SRC: src = 0x%08X, dst = 0x%08X, size = 0x%08X\r\n",
                src_addr, dst_addr, total_bytes);
        DMA_mem_int(total_bytes);
        ret = DMA_memcpy_check(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_SRC);
        record_result(false, ret, "Check DMA memcpy: cache sync only for src");

        // DMA memcpy 5: cache sync only for dst
        // NOTE: src memory may be written back by system, so DMA_memcpy_check may succeed...
        LOGD("Check DMA memcpy, DMA_CACHE_SYNC_DST: src = 0x%08X, dst = 0x%08X, size = 0x%08X\r\n",
                src_addr, dst_addr, total_bytes);
        DMA_mem_int(total_bytes);
        ret = DMA_memcpy_check(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_DST);
        //record_result(false, ret, "Check DMA memcpy: cache sync only for dst");
        record_result(true, ret, "Check DMA memcpy: cache sync only for dst");
    */
}


int main()
{
    logInit(0, 115200); // uart0, baudrate=115200

    //init nos_timer
    nos_timer_init();

    //init dma
    dma_initialize();
    TC_total = TC_pass = TC_fail = 0;

    // normal DMA data transfer (dma_channel_configure)
    check_dma_configure_cases();

    //miscellaneous DMA operations(cache, LLP, SUSP, enable/disable etc.)
    check_dma_misc_cases();

    // DMA memcpy operations
    //TODO: dont't check in IP verification phase
    check_dma_memcpy_cases();

    // show test statistics
    LOGD("\r\n===============================================\r\n");
    LOGD("===============================================\r\n");
    LOGD("===============================================\r\n");
    LOGD("DMA Test cases: total = %d, passed = %d, failed = %d\r\n", TC_total, TC_pass, TC_fail);

    return 0;
}
