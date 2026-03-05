#include "set_dma.h"

static uint32_t volatile DMAEvent;
uint32_t src_buf[MAX_LEN] = {0};
uint32_t dst_buf[MAX_LEN] = {0};

static void set_dma_src_buf(uint8_t * buffer, uint32_t size)
{
    // Generate random buffer
    for (int i = 0; i < size; i++) {
        buffer[i] = i % 256;
    }
}

static void set_dma_dst_buf(uint8_t * buffer, uint32_t size)
{
    // Generate random buffer
    for (int i = 0; i < size; i++) {
        buffer[i] = (i*i) % 256;
    }
}

void set_dma_mem_init(uint32_t total_bytes)
{
    set_dma_src_buf((uint8_t*)src_buf, total_bytes);
    set_dma_dst_buf((uint8_t*)dst_buf, total_bytes);
}

void set_dma_mem_uninit(uint32_t total_bytes)
{
    memset((uint8_t*)src_buf, 0, total_bytes);
    memset((uint8_t*)dst_buf, 0, total_bytes);
}

static void set_dma_callbackEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    DMAEvent = event_info & 0xFF;
}

static void set_dma_waiting(void)
{
    while(1)
    {
        if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
    }
}

bool set_dma_waiting_timeout(uint32_t max_wait_ms)
{
    bool ret = false;
    volatile uint32_t dummy = 0;
    uint32_t i, max_loop = max_wait_ms * 300000;

    for (i = 0; i < max_loop; i++) {
        if (DMAEvent & DMA_EVENT_TRANSFER_COMPLETE) {
            DMAEvent &= ~DMA_EVENT_TRANSFER_COMPLETE;
            ret = true;
            break;
        }
        dummy++; 
    }
    return ret;
}


bool set_dma_memcpy(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;

    uint8_t ch = 0;
    if (!dma_channel_is_reserved(ch)) {
        ch = dma_channel_reserve(ch, set_dma_callbackEvent, 0, cache_sync);
    }

    if (ch == DMA_CHANNEL_ANY)
        ch = dma_channel_select(&ch, set_dma_callbackEvent, 0, cache_sync);

    stat = dma_memcpy(ch, src_addr, dst_addr, total_bytes);

    if(stat == -1){
        return false;
    }

    set_dma_waiting();

    if(memcmp((uint32_t*)src_addr, (uint8_t*)dst_addr, total_bytes) == 0){
        return true;
    }else{
        return false;
    }
}

bool set_dma_configure(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                        uint32_t dst_width, uint32_t dst_bsize,
                        bool addr_dec, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    total_bytes = TOTAL_BYTES;
    set_dma_mem_init(total_bytes);

    //access SrcBuffer / DstBuffer before DMA ferries data
    src_buf[0] = 0xAA; src_buf[1] = 0xCC;
    dst_buf[0] = 0x77; dst_buf[1] = 0x99;

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

    if (addr_dec) {
        src_addr += total_bytes - (0x1 << src_width);
        dst_addr += total_bytes - (0x1 << dst_width);
        control |= DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_DEC;
    } else {
        control |= DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC;
    }

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    set_dma_waiting();

    if(memcmp(src_buf, dst_buf, total_bytes) == 0){
        return true;
    }else{
        return false;
    }
}

bool set_dma_configure_polling(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                                  uint32_t dst_width, uint32_t dst_bsize,
                                  bool addr_dec, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    total_bytes = TOTAL_BYTES;
    set_dma_mem_init(total_bytes);

    //access SrcBuffer / DstBuffer before DMA ferries data
    src_buf[0] = 0xAA; src_buf[1] = 0xCC;
    dst_buf[0] = 0x77; dst_buf[1] = 0x99;

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

    if (addr_dec) {
        src_addr += total_bytes - (0x1 << src_width);
        dst_addr += total_bytes - (0x1 << dst_width);
        control |= DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_DEC;
    } else {
        control |= DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC;
    }

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure_polling (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    while (!dma_channel_xfer_complete(ch));
    dma_channel_clear_xfer_status(ch);

    if(memcmp(src_buf, dst_buf, total_bytes) == 0){
        return true;
    }else{
        return false;
    }
}


bool set_dma_configure_srcgather(uint8_t *pch, DMA_CACHE_SYNC cache_sync)
{
    int i, stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t src_gath = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count=2, interval=2

    uint32_t *p = src_buf;
    for (i=0; i<MAX_LEN/4; i++) {
        *p++ = 0x11111111;
        *p++ = 0x22222222;
        *p++ = 0x33333333;
        *p++ = 0x44444444;
    }
    memset(dst_buf, 0, sizeof(dst_buf));

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    control |= DMA_CH_CTLL_S_GATH_EN;

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, MAX_LEN/2,
                                control, config_low, config_high, src_gath, 0);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    set_dma_waiting();

    bool bOK = false;

    p = dst_buf;
    while (true) {
        for (i=0; i<MAX_LEN/4; i++) {
            if (*p++ != 0x11111111) break;
            if (*p++ != 0x22222222) break;
        }
        if (i < MAX_LEN/4)
            break;
        for (i=0; i<MAX_LEN/2; i++) {
            if (*p++ != 0x0)    break;
        }
        bOK = (i == MAX_LEN/2);
        break;
    } // end while

    return bOK;
}

bool set_dma_configure_dstscatter(uint8_t *pch, DMA_CACHE_SYNC cache_sync)
{
    int i, stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t dst_scat = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count=2, interval=2

    uint32_t *p = src_buf;
    for (i=0; i<MAX_LEN/2; i++) {
        *p++ = 0x33333333;
        *p++ = 0x44444444;
    }
    memset(dst_buf, 0, sizeof(dst_buf));

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    control |= DMA_CH_CTLL_D_SCAT_EN;

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, MAX_LEN/2,
                                control, config_low, config_high, 0, dst_scat);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    set_dma_waiting();

    bool bOK = false;

    p = dst_buf;
    while (true) {
        for (i=0; i<MAX_LEN/4; i++) {
            if (*p++ != 0x33333333) break;
            if (*p++ != 0x44444444) break;
            if (*p++ != 0)  break;
            if (*p++ != 0)  break;
        }
        bOK = (i == MAX_LEN/4);
        break;
    } // end while
    return bOK;
}

bool set_dma_configure_suspen(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_susp_only)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    total_bytes = TOTAL_BYTES;
    set_dma_mem_init(total_bytes);

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

    config_low = DMA_CH_CFGL_CH_PRIOR(0) | DMA_CH_CFGL_CH_SUSP;
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    if (dma_channel_get_status(ch) == 0) {
        return false;
    }

    bool ret = set_dma_waiting_timeout(1); 
    if (chk_susp_only) {
        dma_channel_disable(ch, true); // release the selected DMA channel
        //dma_channel_disable(ch, false); // now channel suspend, so channel FIFO can never be emptied...
        return !ret;
    }

    if (!ret) { // timeout
        dma_channel_enable(ch);
        set_dma_waiting();
    }

    if(memcmp(src_buf, dst_buf, total_bytes) == 0){
        return true;
    }else{
        return false;
    }
}

bool set_dma_configure_suspresm(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_susp_only)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    total_bytes = TOTAL_BYTES;
    set_dma_mem_init(total_bytes);

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

    // 配置时就设置CH_SUSP标志,确保通道启动时处于暂停状态
    config_low = DMA_CH_CFGL_CH_PRIOR(0) | DMA_CH_CFGL_CH_SUSP;
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    if (dma_channel_get_status(ch) == 0) {
        return false;
    }

    // 由于配置时已设置CH_SUSP标志,通道已处于暂停状态
    // 验证通道确实处于暂停状态(可选)

    bool ret = set_dma_waiting_timeout(1);
    if (chk_susp_only) {
        // 仅检查暂停功能:传输应该被暂停,不会完成
        dma_channel_disable(ch, true); // release the selected DMA channel
        return !ret;  // 期望超时(ret=false),返回true
    }

    // 测试恢复功能
    if (!ret) { // 超时,说明传输被正确暂停
        dma_channel_resume(ch);  // 恢复传输
        ret = set_dma_waiting_timeout(1);
        if (!ret) { // 恢复后仍然超时,说明失败
            dma_channel_disable(ch, true); // release the selected DMA channel
            return false;
        }
    } else {
        // 没有超时,说明暂停没有生效(不应该发生)
        dma_channel_disable(ch, true);
        return false;
    }

    if(memcmp(src_buf, dst_buf, total_bytes) == 0){
        return true;
    }else{
        return false;
    }
}

bool set_dma_configure_dis(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_dis_only)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes;

    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;

    total_bytes = TOTAL_BYTES;
    set_dma_mem_init(total_bytes);

    src_addr = (uint32_t)src_buf;
    dst_addr = (uint32_t)dst_buf;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    uint8_t ch = dma_channel_select(pch, set_dma_callbackEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY) {
        return false;
    }

    stat = dma_channel_configure (ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                control, config_low, config_high, 0, 0);

    if(stat == -1){
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    if (dma_channel_get_status(ch) == 0) {
        return false;
    }

    dma_channel_clear_xfer_status(ch);
    dma_channel_disable(ch, true);
    dma_channel_clear_xfer_status(ch);

    if (chk_dis_only) {
        return (dma_channel_get_status(ch) == 0);
    }

    bool ret = set_dma_waiting_timeout(1);

    if (!ret) { // timeout
        dma_channel_enable(ch);
        ret = set_dma_waiting_timeout(1); 
        if (!ret) // timeout
            return false;
    }

    if(memcmp(src_buf, dst_buf, total_bytes) == 0){
        return true;
    }else{
        return false;
    }
}