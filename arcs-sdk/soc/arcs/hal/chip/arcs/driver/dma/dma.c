/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */

//#include "log_print.h"
#include "dma.h"
#include "ClockManager.h"
#include "Driver_Common.h"

#include "cache.h"
#include <string.h> // for memset
#include <assert.h>

#define HAS_CACHE_SYNC  0 // 1
#define WB_BLK_DONE		0 // 1: DONE bit is written back to LLItem

//--------------------------------------------------------------------------

// Per-channel hardware register definitions
typedef struct {
    // The first 6 DWORD(64bit) registers are same as DMA_LLI
    __IO DWORD_REG(SAR);    // Source Address Register
    __IO DWORD_REG(DAR);    // Destination Address Register
    __IO DWORD_REG(LLP);    // Linked List Pointer
    __IO uint32_t CTL_LO;   // Control Register Low WORD
    __IO uint32_t CTL_HI;   // Control Register High WORD
    __IO DWORD_REG(SSTAT);  // Source Status Register, unimplemented, set to 0
    __IO DWORD_REG(DSTAT);  // Destination Status Register, unimplemented, set to 0
    // The following registers are NOT in DMA_LLI
    __IO DWORD_REG(SSTATAR); // Source Status Address Register, unused
    __IO DWORD_REG(DSTATAR); // Destination Status Address Register, unused
    __IO uint32_t CFG_LO;   // Configuration Register Low WORD
    __IO uint32_t CFG_HI;   // Configuration Register High WORD
    __IO DWORD_REG(SGR);    // Source Gather Register
    __IO DWORD_REG(DSR);    // Destination Scatter Register
} DMA_CHANNEL_REG;

// Interrupt register definitions
typedef struct {
    __IO DWORD_REG(XFER);
    __IO DWORD_REG(BLOCK);
    __IO DWORD_REG(SRC_TRAN);
    __IO DWORD_REG(DST_TRAN);
    __IO DWORD_REG(ERROR);
} DMA_IRQ_REG;

// Overall register memory map
typedef struct {
    // 0x000 ~ 0x2b8 N Channels' Registers
    DMA_CHANNEL_REG   CHANNEL[DMA_MAX_NR_CHANNELS];

    DMA_IRQ_REG     RAW;    // [RO] 0x2c0 ~ 0x2e0 raw
    DMA_IRQ_REG     STATUS; // [RO] 0x2e8 ~ 0x308 (raw & mask)
    DMA_IRQ_REG     MASK;   // [RW] 0x310 ~ 0x330 (set = irq enabled)
    DMA_IRQ_REG     CLEAR;  // [WO] 0x338 ~ 0x358 (clear raw and status)

    // [RO] 0x360 Combined Interrupt Status Register
    __IO DWORD_REG(STA_INT);

    // 0x368 ~ 0x390 software handshaking
    __IO DWORD_REG(REQ_SRC);
    __IO DWORD_REG(REQ_DST);
    __IO DWORD_REG(SGL_REQ_SRC);
    __IO DWORD_REG(SGL_REQ_DST);
    __IO DWORD_REG(LAST_SRC);
    __IO DWORD_REG(LAST_DST);

    // 0x398 ~ 0x3b0 miscellaneous
    __IO DWORD_REG(CFG);
    __IO DWORD_REG(CH_EN);
    __IO DWORD_REG(ID);
    __IO DWORD_REG(TEST);

    // 0x3b8 ~ 0x3c0 reserved
    __IO DWORD_REG(__RSVD0);
    __IO DWORD_REG(__RSVD1);

    // 0x3c8 ~ 0x3f0 hardware configuration parameters
    __I uint64_t COMP_PARAMS[6];

    // 0x3f8 Component version register
    __I uint64_t COMP_VER;

} DMA_RegMap;

#define CSK_DMA              ((DMA_RegMap *) DMAC_BASE)
volatile DMA_RegMap *       gDmaReg = CSK_DMA;
//--------------------------------------------------------------------------

// There are two limitations in the original BLOCK transfer:
// 1. Max Block transfer size is 4095 data items (see bit[43:32] BLOCK_TS in CTLx)
// 2. DMA transfer does not pause between block transfers for Src/Dst LLP,
//  and therefore cache sync cannot be performed before new block transfer is started.
//
// So just use DMA transfer (single block) to simulate BLOCK transfer of multi-block in the DMAC driver.
// The mock BLOCK size can be any size (of uint32_t type), and its completion event can also be simulated.
typedef struct {
    DMA_SignalEvent_t   cb_event;
    uint32_t            usr_param;

    // cache sync operation
    uint8_t             cache_sync;

    // SRC width shift from CTLx.SRC_TR_WIDTH, for faster calculation
    uint8_t             width_shift;

    // DST width shift from CTLx.DST_TR_WIDTH, for faster calculation
    uint8_t             dst_wid_shift;

    // see details below.
    // bit[7]: indicate that the channel has been setup
    uint8_t             flags;

    // value of SGR (Source Gather)
    uint32_t            src_gath;
    // value of DSR (Destination Scatter)
    uint32_t            dst_scat;

    // Cnt, accumulated DMA transferred size (Change once DMA transfer is done)
    uint32_t            SizeXfered;
    // Size, remaining size of current LLI or single BLOCK
    uint32_t            SizeToXfer;
    // point to next LLI (real or mock BLOCK), and NULL if single or last LLI
    DMA_LLP             llp;

    // record last transferred LLI, usually used in BLOCK COMPLETE ISR
    DMA_LLP             last_llp;

    // SRC address for next time (one DMA transfer each time)
    uint32_t            SrcAddr;
    // DST address for next time (one DMA transfer each time)
    uint32_t            DstAddr;

#if HAS_CACHE_SYNC
    // only valid for DstBuffer, do post-dma invalidate operation
    // for current BLOCK (mock BLOCK) if necessary
    uint32_t            CacheSyncStart;
    uint32_t            CacheSyncBytes;
#endif

} DMA_Channel_Info;

#define DMA_FLAG_HW_LLP		(0x1 << 0) // bit[0] @ flags (use HW_LLP or Single/SW_LLP?)
#define DMA_FLAG_POLLING    	(0x1 << 1) // bit[1] @ flags (use polling, NO interrupts)
#define DMA_FLAG_PIPO       	(0x1 << 2) // bit[2] @ flags (use PingPong transfer)
//#define DMA_FLAG_PIPO_LAST  	(0x1 << 3) // bit[3] @ flags (last two blocks of PingPong transfer)
#define DMA_FLAG_SETUP      	(0x1 << 7) // bit[7] @ flags


#if (SUPPORT_HW_LLP && USE_INTERNAL_LLITEMS)

// max count of Link List Item implicitly supported by DMA driver
// it means max. (MAX_BLK_TS * DMA_MAX_LL_ITEMS) data can be transfered for 1 DMA interrupt.
// Those items are usually used in PingPong transfers for all dma channels.
#define DMA_MAX_LL_ITEMS                (DMA_NUMBER_OF_CHANNELS * 2) // 2 items each channel
#define DMA_MAX_LL_DATA                 (DMA_MAX_LL_ITEMS * MAX_BLK_TS)

DMA_LLI ll_items[DMA_MAX_LL_ITEMS]; // link list items
volatile uint32_t ll_item_bits = 0; // bit[x] = 1 indicates that item has been used, max. 32 items

#endif // SUPPORT_HW_LLP && USE_INTERNAL_LLITEMS


static uint32_t init_cnt       = 0U;
// channel active flag, set when channel is selected and cleared when DMA is completed or disabled
_FAST_BSS static uint32_t channel_active = 0U;
// channel reserved flag (if set, channel active flag is always kept until unreserved).
_FAST_BSS static uint32_t channel_reserved = 0U;
_FAST_BSS static DMA_Channel_Info channel_info[DMA_NUMBER_OF_CHANNELS];

#define DMA_CHANNEL(n)  ((DMA_CHANNEL_REG *)&(CSK_DMA->CHANNEL[n]))

void dma_irq_handler (void);


/**
  \fn          int32_t set_channel_active_flag (uint8_t ch)
  \brief       Protected set of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
_FAST_TEXT static int32_t set_channel_active_flag (uint8_t ch)
{
  uint8_t gie = GINT_enabled();

  if (gie) { disable_GINT(); }

  if (channel_active & (1U << ch)) {
    if (gie) { enable_GINT(); }
    return -1;
  }

  channel_active |= (1U << ch);

  if (gie) { enable_GINT(); }
  return 0;
}

/**
  \fn          void clear_channel_active_flag (uint8_t ch)
  \brief       Protected clear of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
*/
_FAST_TEXT static void clear_channel_active_flag (uint8_t ch)
{
  uint8_t gie = GINT_enabled();
  uint32_t ch_bit = 1U << ch;

  if (gie) { disable_GINT(); }

  //channel_active &= ~(1U << ch);
  //BSD: only NOT reserved channel can be clear active flay...
  if ((channel_reserved & ch_bit) == 0)
      channel_active &= ~ch_bit;
//  channel_info[ch].flags = 0; // clear to 0

  if (gie) { enable_GINT(); }
}

/**
  \fn          int32_t set_channel_reserved_flag (uint8_t ch)
  \brief       Protected set of channel reserved & active flag
  \param[in]   ch        Channel number (0..7 or 4)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
static int32_t set_channel_reserved_flag (uint8_t ch)
{
  uint8_t gie = GINT_enabled();
  uint32_t ch_bit = 1U << ch;

  if (gie) { disable_GINT(); }

  // return failure if already reserved or active
  if ((channel_reserved & ch_bit) || (channel_active & ch_bit)) {
    if (gie) { enable_GINT(); }
    return -1;
  }

  // set reserved flag and active flag
  channel_reserved |= ch_bit;
  channel_active |= ch_bit;

  if (gie) { enable_GINT(); }

  return 0;
}

/**
  \fn          void clear_channel_reserved_flag (uint8_t ch)
  \brief       Protected clear of channel reserved & active flag
  \param[in]   ch        Channel number (0..7 or 4)
*/
static void clear_channel_reserved_flag (uint8_t ch)
{
  uint8_t gie = GINT_enabled();
  uint32_t ch_bit = 1U << ch;

  if (gie) { disable_GINT(); }

  // clear reserved flag and active flag
  if (channel_reserved & ch_bit) {
      channel_reserved &= ~ch_bit;
      channel_active &= ~ch_bit;
  }

  if (gie) { enable_GINT(); }
}


static inline void dmac_clk_enable() {
#if 0
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_DMAC_CLK = 1;
#else
    __HAL_CRM_DMA_CLK_ENABLE();
#endif
}

static inline void dmac_clk_disable() {
#if 0
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_DMAC_CLK = 0;
#else
    __HAL_CRM_DMA_CLK_DISABLE();
#endif
}

/**
  \fn          int32_t dma_initialize (void)
  \brief       Initialize DMA peripheral
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t dma_initialize (void)
{
  init_cnt++;

  // Check if already initialized
  if (init_cnt > 1U) { return 0; }

  // Clear all DMA channel information
  memset(channel_info, 0, sizeof(channel_info));
  gDmaReg = CSK_DMA;

  // Enable DMA Peripheral Clock
  dmac_clk_enable();

  // Enable DMA Controller
  CSK_DMA->CFG = 0x1;

  // Disable all DMA channels
  CSK_DMA->CH_EN = 0xFF00;

  // Clear all DMA interrupt flags
  CSK_DMA->CLEAR.XFER = 0xFFFF;
  CSK_DMA->CLEAR.BLOCK = 0xFFFF;
  CSK_DMA->CLEAR.SRC_TRAN = 0xFFFF;
  CSK_DMA->CLEAR.DST_TRAN = 0xFFFF;
  CSK_DMA->CLEAR.ERROR = 0xFFFF;

  // Register DMA ISR
  register_ISR(IRQ_DMAC_VECTOR, (ISR)dma_irq_handler, NULL);

  // Enable DMA IRQ
  enable_IRQ(IRQ_DMAC_VECTOR);

  return 0;
}


/**
  \fn          int32_t dma_uninitialize (void)
  \brief       De-initialize DMA peripheral
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t dma_uninitialize (void) {

  // Check if DMA is initialized
  if (init_cnt == 0U) { return -1; }

  init_cnt--;
  if (init_cnt != 0U) { return 0; }

  // Disable all DMA channels
  CSK_DMA->CH_EN = 0xFF00;

  // Disable DMA Controller and wait until all transfers are over
  CSK_DMA->CFG = 0x0;
  while (CSK_DMA->CFG & 0x1) { }

  // Disable DMA Peripheral Clock
  dmac_clk_disable();

  // Disable DMA IRQ
  disable_IRQ(IRQ_DMAC_VECTOR);

  // Unregister DMA ISR
  register_ISR(IRQ_DMAC_VECTOR, NULL, NULL);

  return 0;
}


//dynamically allocate available free DMA channel
//return 0: function succeeded
//return -1: function failed
_FAST_TEXT static int32_t dma_get_free_channel(uint8_t *pch)
{

    uint8_t gie, i, found = 0;

    if (pch == NULL)
        return -1;

    gie = GINT_enabled();
    if (gie) { disable_GINT(); }
    for (i=0; i<DMA_NUMBER_OF_CHANNELS; i++) {
        if ((channel_active & (1U << i)) == 0) {
            *pch = i;
            found = 1;
            break;
        }
    }
    if (gie) { enable_GINT(); }
    return (found ? 0 : -1);
}


/**
   \fn          int32_t dma_channel_select (uint8_t      *pch,
                                        DMA_SignalEvent_t  cb_event,
                                        uint32_t           usr_param,
                                        DMA_CACHE_SYNC     cache_sync)
  \brief        Select specified channel or dynamically allocate a free channel
                before calling dma_channel_configure or dma_channel_configure_LLP.
                The channel will be released when DMA operation is completed or channel is disabled.
  \param[in, out]   pch     pointer to Channel number
                            the preferred dma channel as input parameter,
                            the actual dma channel as output parameter.
                            if NULL, Channel number is dynamically allocated.
  \param[in]   cb_event     Channel callback pointer
  \param[in]   usr_param    user-defined value, acts as last parameter of cb_event
  \param[in]   cache_sync   cache coherence or sync policy for source/destination RAM buffer
                            before DMA operation. see the definition of DMA_CACHE_SYNC.
  \returns
   - \b  the selected DMA channel number if successful, or DMA_CHANNEL_ANY (0xFF) if failed.
 */
_FAST_TEXT uint8_t dma_channel_select(uint8_t *pch,
                                        DMA_SignalEvent_t  cb_event,
                                        uint32_t           usr_param,
                                        DMA_CACHE_SYNC     cache_sync)
{
    uint8_t ch, dynamic_ch = 0;
    DMA_Channel_Info *ch_info;

    // dynamic allocation of DMA channel
    if (pch == NULL || *pch == DMA_CHANNEL_ANY) {
        dynamic_ch = 1;
        if (dma_get_free_channel(&ch) != 0)
            return DMA_CHANNEL_ANY;
    } else {
        ch = *pch;
        *pch = DMA_CHANNEL_ANY; // No free channel
        // Check if channel is valid
        if (ch >= DMA_NUMBER_OF_CHANNELS)
            return DMA_CHANNEL_ANY;
    }

    // Set Channel active flag
    if (set_channel_active_flag(ch) == -1) {
        // already dynamically allocated
        if (dynamic_ch)
            return DMA_CHANNEL_ANY;
        // try dynamic allocation of DMA channel if NOT
        dynamic_ch = 1;
        if (dma_get_free_channel(&ch) != 0)
            return DMA_CHANNEL_ANY;
        // set Channel active flag again
        if (set_channel_active_flag(ch) == -1)
            return DMA_CHANNEL_ANY;
    }

    // write back DMA channel (maybe dynamically allocated)
    if (pch != NULL)
        *pch = ch;

    if (cache_sync >= DMA_CACHE_SYNC_COUNT)
        cache_sync = DMA_CACHE_SYNC_AUTO;

    // Initialize channel_info partially
    ch_info = &channel_info[ch];
    memset(ch_info, 0, sizeof(DMA_Channel_Info));
    ch_info->cb_event = cb_event;
    ch_info->usr_param = usr_param;
    ch_info->cache_sync = cache_sync;

    return ch;
}


/**
   \fn          int32_t dma_channel_reserve (uint8_t      ch,
                                        DMA_SignalEvent_t  cb_event,
                                        uint32_t           usr_param,
                                        DMA_CACHE_SYNC     cache_sync)
  \brief        Reserve specified channel for exclusive use
                before calling dma_channel_configure or dma_channel_configure_LLP.
                The channel will NOT be released when DMA operation is completed or channel is disabled.
                So this API function is NOT RECOMMENDED to use if NOT necessary!!
  \param[in]   ch          specified Channel number
  \param[in]   cb_event     Channel callback pointer
  \param[in]   usr_param    user-defined value, acts as last parameter of cb_event
  \param[in]   cache_sync   cache coherence or sync policy for source/destination RAM buffer
                            before DMA operation. see the definition of DMA_CACHE_SYNC.
  \returns
   - \b  the reserved DMA channel number if successful, or DMA_CHANNEL_ANY (0xFF) if failed.
 */
uint8_t dma_channel_reserve(uint8_t ch,
                            DMA_SignalEvent_t  cb_event,
                            uint32_t           usr_param,
                            DMA_CACHE_SYNC     cache_sync)
{
    DMA_Channel_Info *ch_info;

    // set channel reserved flag
    if (ch >= DMA_NUMBER_OF_CHANNELS ||
        set_channel_reserved_flag(ch) != 0)
        return DMA_CHANNEL_ANY;

    if (cache_sync >= DMA_CACHE_SYNC_COUNT)
        cache_sync = DMA_CACHE_SYNC_AUTO;

    // Initialize channel_info partially
    ch_info = &channel_info[ch];
    memset(ch_info, 0, sizeof(DMA_Channel_Info));
    ch_info->cb_event = cb_event;
    ch_info->usr_param = usr_param;
    ch_info->cache_sync = cache_sync;

    return ch;
}

/**
   \fn          int32_t dma_channel_unreserve (uint8_t      ch)
  \brief        Unreserve (or Release) specified channel exclusively used by some device.
  \param[in]   ch          specified dedicated Channel number
  \returns
   - \b  no return value.
 */
void dma_channel_unreserve(uint8_t ch)
{
    // clear channel reserved flag if any
    if (ch < DMA_NUMBER_OF_CHANNELS)
        clear_channel_reserved_flag(ch);
}

/**
   \fn          int32_t dma_channel_is_reserved (uint8_t      ch)
  \brief        Check whether specified channel is reserved (exclusively used by some device) or not.
  \param[in]   ch          specified dedicated Channel number
  \returns
   - \b  true indicates reserved, false indicates NOT reserved yet.
 */
bool dma_channel_is_reserved(uint8_t ch)
{
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return false;
    if (channel_reserved & (0x1 << ch))
        return true;
    return false;
}

#if HAS_CACHE_SYNC
_FAST_TEXT static void cache_sync_src(uint32_t control, uint32_t src_addr, uint32_t bytes, DMA_Channel_Info *ch_info)
{
    uint32_t start, end;
    uint32_t addr_ctrl = control & DMA_CH_CTLL_SRCADDRCTL_MASK;
    // calculate start & end address
    if (addr_ctrl == DMA_CH_CTLL_SRC_INC) {
        start = src_addr;
        end = src_addr + bytes;
    } else if (addr_ctrl == DMA_CH_CTLL_SRC_DEC) {
        if (ch_info == NULL) {
            uint32_t src_width = (control & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
            //if (src_width > DMA_WIDTH_MAX)
            //    src_width = DMA_WIDTH_MAX;
            end = src_addr + (1 << src_width);
        } else {
            end = src_addr + (1 << ch_info->width_shift);
        }
        assert(end >= bytes);
        start = end - bytes;
    } else {
        return; // fixed source address
    }

    // if NOT cacheable, do nothing
    if (!range_is_cacheable(start, bytes))
        return;

    // write back from cache to source memory
    dcache_clean_range(start, end);
}

_FAST_TEXT static void cache_sync_dst(uint32_t control, uint32_t dst_addr, uint32_t bytes, DMA_Channel_Info *ch_info)
{
    uint32_t start, end;
    uint32_t addr_ctrl = control & DMA_CH_CTLL_DSTADDRCTL_MASK;
    // calculate start & end address
    if (addr_ctrl == DMA_CH_CTLL_DST_INC) {
        start = dst_addr;
        end = dst_addr + bytes;
    } else if (addr_ctrl == DMA_CH_CTLL_DST_DEC) {

        if (ch_info == NULL) {
            uint32_t dst_width = (control & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
            //if (dst_width > DMA_WIDTH_MAX)
            //    dst_width = DMA_WIDTH_MAX;
            end = dst_addr + (1 << dst_width);
        } else {
            end = dst_addr + (1 << ch_info->dst_wid_shift);
        }
        assert(end >= bytes);
        start = end - bytes;
    } else {
        return; // fixed destination address
    }

    // if NOT cacheable, do nothing
    if (!range_is_cacheable(start, bytes))
        return;

    // invalidate cache for destination memory
    //nds32_dcache_invalidate_range(start, end);
    if (ch_info != NULL) {
        uint32_t line_size_mask = CACHE_LINE_SIZE(DCACHE) - 1;
        // NEED do post-dma invalidate operation if either start or bytes doesn't align with cache line size
        if ( (start & line_size_mask) || (bytes & line_size_mask) ) {
            ch_info->CacheSyncStart = start;
            ch_info->CacheSyncBytes = bytes;
        }
    }
    if (ch_info->dst_scat == 0) // NO DST SCATTER, just call fast invalidate operation
        cache_dma_fast_inv_stage1(start, end); //BSD: start & end may not align with CACHE_LINE_SIZE!
    else // with DST SCATTER, there may be some memory holes which should be synchronized with cache
        dcache_flush_range(start, end);
}
#endif // HAS_CACHE_SYNC

// Calculate how many bytes equal to "size" of data items on Source
_FAST_TEXT static uint32_t calc_src_bytes(DMA_Channel_Info *ch_info, uint32_t control, uint32_t size)
{
    uint32_t bytes, sg_cnt, sg_int;

    // support Source Gather
    if (control & DMA_CH_CTLL_S_GATH_EN) {
        sg_cnt = SG_COUNT(ch_info->src_gath);
        sg_int = SG_INTERVAL(ch_info->src_gath);
        bytes = ( size / sg_cnt * (sg_cnt + sg_int) + size % sg_cnt ) << ch_info->width_shift;
    } else {
        bytes = size << ch_info->width_shift;
    }
    return bytes;
}


// Calculate how many bytes equal to "size" of data items on Destination
_FAST_TEXT static uint32_t calc_dst_bytes(DMA_Channel_Info *ch_info, uint32_t control, uint32_t size)
{
    uint32_t bytes, dst_size, sg_cnt, sg_int;

    // support Destination Scatter
    if (control & DMA_CH_CTLL_D_SCAT_EN) {
        dst_size = (size << ch_info->width_shift) >> ch_info->dst_wid_shift;
        sg_cnt = SG_COUNT(ch_info->dst_scat);
        sg_int = SG_INTERVAL(ch_info->dst_scat);
        bytes = ( dst_size / sg_cnt * (sg_cnt + sg_int) + dst_size % sg_cnt ) << ch_info->dst_wid_shift;
    } else {
        bytes = size << ch_info->width_shift;
    }
    return bytes;
}

#if HAS_CACHE_SYNC
// do cache sync operation on src/dst
_FAST_TEXT static void do_cache_sync(DMA_Channel_Info *ch_info, uint32_t control, uint32_t src_addr, uint32_t dst_addr, uint32_t size)
{
    uint32_t src_bytes, dst_bytes;
    assert(ch_info != NULL && size != 0);
    //assert(ch_info->width_bytes > 0);
    if (ch_info == NULL || size == 0)
        return;

    ch_info->CacheSyncStart = 0; // reset default 0
    ch_info->CacheSyncBytes = 0; // reset default 0

    // cache coherence or cache sync operation
    switch (ch_info->cache_sync) {
    case DMA_CACHE_SYNC_SRC:
        src_bytes = calc_src_bytes(ch_info, control, size);
        cache_sync_src(control, src_addr, src_bytes, ch_info);
        break;
    case DMA_CACHE_SYNC_DST:
        dst_bytes = calc_dst_bytes(ch_info, control, size);
        cache_sync_dst(control, dst_addr, dst_bytes, ch_info);
        break;
    case DMA_CACHE_SYNC_BOTH:
        src_bytes = calc_src_bytes(ch_info, control, size);
        cache_sync_src(control, src_addr, src_bytes, ch_info);
        dst_bytes = calc_dst_bytes(ch_info, control, size);
        cache_sync_dst(control, dst_addr, dst_bytes, ch_info);
        break;
    default:
        break;
    }

    if (!(control & DMA_CH_CTLL_SRC_FIX)) {
        //BSD: assume source memory need do cache sync now that source address is not fixed
        if (ch_info->cache_sync == DMA_CACHE_SYNC_AUTO) {
            src_bytes = calc_src_bytes(ch_info, control, size);
            cache_sync_src(control, src_addr, src_bytes, ch_info);
        }
    }

    if (!(control & DMA_CH_CTLL_DST_FIX)) {
        //BSD: assume destination memory need do cache sync now that destination address is not fixed
        if (ch_info->cache_sync == DMA_CACHE_SYNC_AUTO) {
            dst_bytes = calc_dst_bytes(ch_info, control, size);
            cache_sync_dst(control, dst_addr, dst_bytes, ch_info);
        }
    }
}

#else

#define do_cache_sync(...)   ((void)0)

#endif // HAS_CACHE_SYNC

_FAST_TEXT static void update_next_xfer_addr(DMA_Channel_Info *ch_info, uint32_t control,
                                uint32_t src_addr, uint32_t dst_addr, uint32_t size)
{
    assert(ch_info != NULL && size != 0);
    if (ch_info == NULL || size == 0)
        return;

    // calculate total bytes
    uint32_t bytes;

    if (!(control & DMA_CH_CTLL_SRC_FIX)) {
        // size => bytes
        bytes = calc_src_bytes(ch_info, control, size);

        // Source address decrement
        if (control & DMA_CH_CTLL_SRC_DEC) {
            src_addr -=  bytes;
        } else {// Source address increment
            src_addr += bytes;
        }
    } // source address for next DMA transfer

    if (!(control & DMA_CH_CTLL_DST_FIX)) {
        // size => bytes
        bytes = calc_dst_bytes(ch_info, control, size);

        // Destination address decrement
        if (control & DMA_CH_CTLL_DST_DEC)
            dst_addr -= bytes;
        else // Destination address increment
            dst_addr += bytes;
    } // destination address for next DMA transfer

    // Save channel information
    ch_info->SrcAddr = src_addr;
    ch_info->DstAddr = dst_addr;
}

__inline static void clear_all_interrupts(uint32_t ch_bits)
{
    // Clear all DMA interrupt flags of specified channels
    CSK_DMA->CLEAR.XFER = ch_bits;
    CSK_DMA->CLEAR.BLOCK = ch_bits;
    CSK_DMA->CLEAR.SRC_TRAN = ch_bits;
    CSK_DMA->CLEAR.DST_TRAN = ch_bits;
    CSK_DMA->CLEAR.ERROR = ch_bits;
}

__inline static void clear_xfer_interrupts(uint32_t ch_bits)
{
    CSK_DMA->CLEAR.XFER = ch_bits;
}

__inline static void clear_block_interrupts(uint32_t ch_bits)
{
    CSK_DMA->CLEAR.BLOCK = ch_bits;
}

__inline static void clear_error_interrupts(uint32_t ch_bits)
{
    CSK_DMA->CLEAR.ERROR = ch_bits;
}

__inline static void disable_all_interrupts(uint32_t ch_bits)
{
    // Disable/Mask all DMA interrupt flags of specified channels
    CSK_DMA->MASK.XFER = (ch_bits << 8);
    CSK_DMA->MASK.BLOCK = (ch_bits << 8);
    CSK_DMA->MASK.SRC_TRAN = (ch_bits << 8);
    CSK_DMA->MASK.DST_TRAN = (ch_bits << 8);
    CSK_DMA->MASK.ERROR = (ch_bits << 8);
}

//#define DECL_ENABLE_INTERRUPTS(name)     \
//    __inline static void enable_##name_interrupts(uint32_t ch_bits) \
//    { CSK_DMA->MASK.##name = ((ch_bits << 8) | ch_bits); }
//
//#define DECL_DISABLE_INTERRUPTS(name)     \
//    __inline static void disable_##name_interrupts(uint32_t ch_bits) \
//    { CSK_DMA->MASK.##name = (ch_bits << 8); }

__inline static void enable_xfer_interrupts(uint32_t ch_bits)
{
    CSK_DMA->MASK.XFER = (ch_bits << 8) | ch_bits;
}

__inline static void enable_block_interrupts(uint32_t ch_bits)
{
    CSK_DMA->MASK.BLOCK = (ch_bits << 8) | ch_bits;
}

__inline static void enable_error_interrupts(uint32_t ch_bits)
{
    CSK_DMA->MASK.ERROR = (ch_bits << 8) | ch_bits;
}

__inline static void disable_xfer_interrupts(uint32_t ch_bits)
{
    CSK_DMA->MASK.XFER = (ch_bits << 8);
}

__inline static void disable_block_interrupts(uint32_t ch_bits)
{
    CSK_DMA->MASK.BLOCK = (ch_bits << 8);
}

__inline static void disable_error_interrupts(uint32_t ch_bits)
{
    CSK_DMA->MASK.ERROR = (ch_bits << 8);
}

void dma_channel_mask_xfer_interrupt(uint8_t ch, bool mask)
{
    if (ch >= DMA_NUMBER_OF_CHANNELS) return;
    uint32_t ch_bit = 1U << ch;
    if (mask) disable_xfer_interrupts(ch_bit);
    else      enable_xfer_interrupts(ch_bit);
}

void dma_channel_mask_block_interrupt(uint8_t ch, bool mask)
{
    if (ch >= DMA_NUMBER_OF_CHANNELS) return;
    uint32_t ch_bit = 1U << ch;
    if (mask) disable_block_interrupts(ch_bit);
    else      enable_block_interrupts(ch_bit);
}

void dma_channel_mask_error_interrupt(uint8_t ch, bool mask)
{
    if (ch >= DMA_NUMBER_OF_CHANNELS) return;
    uint32_t ch_bit = 1U << ch;
    if (mask) disable_error_interrupts(ch_bit);
    else      enable_error_interrupts(ch_bit);
}

__inline static uint32_t calc_burst_bytes(uint32_t width, uint32_t bsize)
{
    uint32_t count;
    if (width > DMA_WIDTH_MAX)  width = DMA_WIDTH_MAX;
    count = (bsize == 0 ? 1 : (2 << bsize));
    count *= (1 << width);
    return count;
}

static int32_t check_burst_bytes(uint8_t ch, uint32_t control)
{
    uint32_t width, bsize, fifo_depth;

    assert (ch < DMA_NUMBER_OF_CHANNELS);
    fifo_depth = DMA_CHANNELS_FIFO_DEPTH[ch];

    width = (control & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    bsize = (control & DMA_CH_CTLL_SRC_BSIZE_MASK) >> DMA_CH_CTLL_SRC_BSIZE_POS;
    if (calc_burst_bytes(width, bsize) > fifo_depth)
        return -1;

    width = (control & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
    bsize = (control & DMA_CH_CTLL_DST_BSIZE_MASK) >> DMA_CH_CTLL_DST_BSIZE_POS;
    if (calc_burst_bytes(width, bsize) > fifo_depth)
        return -1;

    return 0;
}

/**
  \fn  int32_t dma_channel_configure_internal (uint8_t     ch,
                                        uint8_t            en_int,
                                        uint32_t           src_addr,
                                        uint32_t           dst_addr,
                                        uint32_t           size,
                                        uint32_t           control,
                                        uint32_t           config_low,
                                        uint32_t           config_high)
  \brief       Configure DMA channel for Single Block transfer
  \param[in]   ch           Channel number
  \param[in]   src_addr     Source address
  \param[in]   dest_addr    Destination address
  \param[in]   size         Amount of data items to transfer (SHOULD be less then 4096)
                            The transferred bytes is (size * SrcWidth).
  \param[in]   control      Channel control
  \param[in]   config_low   Channel configuration's low WORD(32bit)
  \param[in]   config_high  Channel configuration's high WORD(32bit)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
_FAST_TEXT static int32_t dma_channel_configure_internal (
                                uint8_t            ch,
                                uint8_t            en_int,
                                uint32_t           src_addr,
                                uint32_t           dst_addr,
                                uint32_t           size,
                                uint32_t           control,
                                uint32_t           config_low,
                                uint32_t           config_high)
{
    DMA_CHANNEL_REG *dma_ch;
    DMA_Channel_Info *ch_info;
    uint32_t ch_bit;

    ch_bit = 0x1U << ch;
    dma_ch = DMA_CHANNEL(ch);
    ch_info = &channel_info[ch];

    if (en_int == 0) {
        ch_info->flags |= DMA_FLAG_POLLING; // polling, no interrupt
        control &= ~DMA_CH_CTLL_INT_EN; // Disable interrupt
    } else {
        ch_info->flags &= ~DMA_FLAG_POLLING; // use interrupt
        control |= DMA_CH_CTLL_INT_EN; // Enable interrupt
    }

    // Disable DMA interrupts
    disable_all_interrupts(ch_bit);

    // Max block transfer size is MAX_BLK_TS (BLOCK_TS holds n bits)
    assert(size <= MAX_BLK_TS);
    // Set CTLx.BLOCK_TS = size and cTLx.Done = 0
    dma_ch->CTL_HI = (size & DMA_CH_CTLH_BLOCK_TS_MASK);

    // Set Source and Destination address
    dma_ch->SAR = src_addr;
    dma_ch->DAR = dst_addr;

    // Write control & configuration etc. registers
    dma_ch->CTL_LO = control;
    dma_ch->CFG_LO = config_low;
    dma_ch->CFG_HI = config_high;

    // Reset LLP, if NOT use HW LLP (single block or SW LLP, a.k.a. mock BLOCK transfer
    // Comment out following lines -- NO NEED to handle it!!
/*
#if SUPPORT_HW_LLP
    //FIXME: set dma_ch->LLP register if HW_LLP!!
    if (ch_info->flags & DMA_FLAG_HW_LLP) // HW LLP
        ; //TODO: do something?
    else
#endif
*/
        dma_ch->LLP = 0;

    // Reset scatter/gather etc. registers
    dma_ch->SGR = (control & DMA_CH_CTLL_S_GATH_EN ? ch_info->src_gath : 0);
    dma_ch->DSR = (control & DMA_CH_CTLL_D_SCAT_EN ? ch_info->dst_scat : 0);

    // Enable DMA XFER and ERROR interrupts (no BLOCK & TRANS interrupts)
    if (en_int == 0) {
        disable_xfer_interrupts(ch_bit);
        disable_error_interrupts(ch_bit);
    } else {
        enable_xfer_interrupts(ch_bit);
        enable_error_interrupts(ch_bit);
    }

    // Enable DMA Channel to trigger data transfer
    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    // Enable DMA Controller
    //CSK_DMA->CFG = 0x1;

    return 0;
}

//BSD: Add dma_channel_configure_internal_lite experimentally...
_FAST_TEXT static int32_t dma_channel_configure_internal_lite (
                                uint8_t            ch,
                                uint32_t           src_addr,
                                uint32_t           dst_addr,
                                uint32_t           size)
{
    DMA_CHANNEL_REG *dma_ch;
//    DMA_Channel_Info *ch_info;
    uint32_t ch_bit;

    ch_bit = 0x1U << ch;
    dma_ch = DMA_CHANNEL(ch);

    // Max block transfer size is MAX_BLK_TS (BLOCK_TS holds N bits)
    assert(size <= MAX_BLK_TS);
    // Set CTLx.BLOCK_TS = size and cTLx.Done = 0
    dma_ch->CTL_HI = (size & DMA_CH_CTLH_BLOCK_TS_MASK);

    // Set Source and Destination address
    dma_ch->SAR = src_addr;
    dma_ch->DAR = dst_addr;

    // Enable DMA Channel to trigger data transfer
    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    // Enable DMA Controller
    //CSK_DMA->CFG = 0x1;

    return 0;
}

/**
  \fn          int32_t dma_channel_configure_wrapper (uint8_t      ch,
                                    uint8_t         en_int,
                                    uint32_t        src_addr,
                                    uint32_t        dst_addr,
                                    uint32_t        total_size,
                                    uint32_t        control,
                                    uint32_t        config_low,
                                    uint32_t        config_high,
                                    uint32_t        src_gath,
                                    uint32_t        dst_scat);
  \brief       Configure DMA channel for block transfer (and enable DMA channel implicitly)
  \param[in]   ch           The selected Channel number returned by dma_channel_select()
  \param[in]   en_int       Whether to enable DMA interrupts. enable interrupts if non-zero.
  \param[in]   src_addr     Source address
  \param[in]   dest_addr    Destination address
  \param[in]   total_size   Amount of data items to transfer from source (maybe greater than 4095)
                            The total number of transferred bytes is (total_size * SrcWidth).
  \param[in]   control      Channel control
  \param[in]   config_low   Channel configuration's low WORD(32bit)
  \param[in]   config_high  Channel configuration's high WORD(32bit)
  \param[in]   src_gath     Value of Source Gather register (see above macros of SG_XXX)
  \param[in]   dst_scat     Value of Destination Scatter register (see above macros of SG_XXX)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/

#if SUPPORT_HW_LLP
/*
_FAST_TEXT static bool dma_fill_ll_items(DMA_Channel_Info *ch_info,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      control,
                                            uint32_t      total_size)
{
    uint32_t i, count, size;
    DMA_LLP pi = &ll_items[0];

    if (ch_info == NULL || total_size == 0)
        return false;

    count = (total_size + MAX_BLK_TS) >> MAX_BLK_BITS;
    size = (count << MAX_BLK_BITS) - count;
    if (total_size > size)
        count++;

    if (count > DMA_MAX_LL_ITEMS)
        return false;

    size = total_size;

    // item 0 ~ count-2
    for (i = 0; i < count - 1; i++, pi++) {
        pi->SAR = src_addr;
        pi->DAR = dst_addr;
        pi->LLP = (uint32_t) &ll_items[i+1];
        pi->CTL_LO = control | DMA_CH_CTLL_LLP_EN_MASK;
        pi->u.CTL_HI = MAX_BLK_TS;
        src_addr += MAX_BLK_TS << ch_info->width_shift;
        dst_addr += MAX_BLK_TS << ch_info->width_shift;
        size -= MAX_BLK_TS;
    } // end for

    if (size > 0) {
        pi->SAR = src_addr;
        pi->DAR = dst_addr;
        pi->LLP = 0;
        pi->CTL_LO = control & ~DMA_CH_CTLL_LLP_EN_MASK;
        pi->u.CTL_HI = size;
    }

    return true;
}
*/

_FAST_TEXT int32_t dma_channel_configure_wrapper (uint8_t      ch,
                                            uint8_t       en_int,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      total_size,
                                            uint32_t      control,
                                            uint32_t      config_low,
                                            uint32_t      config_high,
                                            uint32_t      src_gath,
                                            uint32_t      dst_scat)
{
    uint32_t ch_bit, width, size;
    DMA_Channel_Info *ch_info;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return -1;

    // check if src/dst burst size & xfer width are legal
    //if (check_burst_bytes(ch, control) != 0)
    //    return -1;

    // check scatter/gather validity
    if (control & DMA_CH_CTLL_S_GATH_EN) {
        if (SG_COUNT(src_gath) == 0)
            return -1;
    }
    if (control & DMA_CH_CTLL_D_SCAT_EN) {
        if (SG_COUNT(dst_scat) == 0)
            return -1;
    }

    ch_info = &channel_info[ch];

    width = (control & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->dst_wid_shift = width; // dst width shift

    width = (control & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->width_shift = width; // src width shift

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;

    ch_info->SizeToXfer = total_size;
    ch_info->SizeXfered = 0;
    ch_info->llp = 0;
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

//    size = total_size > MAX_BLK_TS ? FIT_BLK_TS : total_size;
//    update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);

    // do cache sync operation before the mock BLOCK transfer
    do_cache_sync(ch_info, control, src_addr, dst_addr, total_size);

    // Disable DMA Channel
    //CSK_DMA->CH_EN = (ch_bit << 8);

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

    // make sure remove LLP_EN
    // NOT use HW LLP is less than MAX_BLK_TS or greater than DMA_MAX_LL_DATA
//    if (total_size <= MAX_BLK_TS || total_size > DMA_MAX_LL_DATA) { // use single block or SW LLP
        size = total_size > MAX_BLK_TS ? MAX_BLK_TS : total_size;
        update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);

        ch_info->flags &= ~DMA_FLAG_HW_LLP;
        control &= ~DMA_CH_CTLL_LLP_EN_MASK;
/*    } else { // use HW LLP
        ch_info->llp = &ll_items[0];
        dma_fill_ll_items(ch_info,
                        src_addr + (MAX_BLK_TS << ch_info->width_shift),
                        dst_addr + (MAX_BLK_TS << ch_info->width_shift),
                        control,
                        total_size - MAX_BLK_TS);
        size = MAX_BLK_TS;

        ch_info->flags |= DMA_FLAG_HW_LLP;
        control |= DMA_CH_CTLL_LLP_EN_MASK;
    }
*/

    // Trigger first DMA transfer
    return dma_channel_configure_internal(ch, en_int, src_addr, dst_addr, size, control, config_low, config_high);
}

#else  // !SUPPORT_HW_LLP

_FAST_TEXT int32_t dma_channel_configure_wrapper (uint8_t      ch,
                                            uint8_t       en_int,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      total_size,
                                            uint32_t      control,
                                            uint32_t      config_low,
                                            uint32_t      config_high,
                                            uint32_t      src_gath,
                                            uint32_t      dst_scat)
{
    uint32_t ch_bit, width, size;
    DMA_Channel_Info *ch_info;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return -1;

    // check if src/dst burst size & xfer width are legal
    //if (check_burst_bytes(ch, control) != 0)
    //    return -1;

    // check scatter/gather validity
    if (control & DMA_CH_CTLL_S_GATH_EN) {
        if (SG_COUNT(src_gath) == 0)
            return -1;
    }
    if (control & DMA_CH_CTLL_D_SCAT_EN) {
        if (SG_COUNT(dst_scat) == 0)
            return -1;
    }

    ch_info = &channel_info[ch];

    width = (control & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->dst_wid_shift = width; // dst width shift

    width = (control & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->width_shift = width; // src width shift

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;

    ch_info->SizeToXfer = total_size;
    ch_info->SizeXfered = 0;
    ch_info->llp = 0;
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

    size = total_size > MAX_BLK_TS ? MAX_BLK_TS : total_size;
    update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);

    // do cache sync operation before the mock BLOCK transfer
    do_cache_sync(ch_info, control, src_addr, dst_addr, total_size);

    // Disable DMA Channel
    //CSK_DMA->CH_EN = (ch_bit << 8);

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

    // make sure remove LLP_EN
    control &= ~DMA_CH_CTLL_LLP_EN_MASK;

    // Trigger first DMA transfer
    return dma_channel_configure_internal(ch, en_int, src_addr, dst_addr, size, control, config_low, config_high);
}

#endif // SUPPORT_HW_LLP

//[NEW]
int32_t dma_channel_setup (uint8_t      ch,
                           uint8_t       en_bits, //en_int
                           uint32_t      control,
                           uint32_t      config_low,
                           uint32_t      config_high,
                           uint32_t      src_gath,
                           uint32_t      dst_scat)
{
    uint32_t ch_bit, width; //, size
    DMA_Channel_Info *ch_info;
    uint8_t en_int, en_blk, en_pipo;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return CSK_DRIVER_ERROR_PARAMETER;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return CSK_DRIVER_ERROR;

    // check if src/dst burst size & xfer width are legal
    //if (check_burst_bytes(ch, control) != 0)
    //    return -1;

    // check scatter/gather validity
    if (control & DMA_CH_CTLL_S_GATH_EN) {
        if (SG_COUNT(src_gath) == 0)
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (control & DMA_CH_CTLL_D_SCAT_EN) {
        if (SG_COUNT(dst_scat) == 0)
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    en_blk = en_bits & DMA_CH_EN_BLK_INT;
    en_pipo = en_bits & DMA_CH_EN_PIPO;
    // block interrupt SHOULD be enabled for PingPong transfer
    if (en_pipo && !en_blk)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Disable DMA interrupts
    disable_all_interrupts(ch_bit);

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

    ch_info = &channel_info[ch];
    ch_info->flags = 0;

    width = (control & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
    if (width > DMA_WIDTH_MAX)
        return CSK_DRIVER_ERROR_PARAMETER;
    ch_info->dst_wid_shift = width; // dst width shift

    width = (control & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    if (width > DMA_WIDTH_MAX)
        return CSK_DRIVER_ERROR_PARAMETER;
    ch_info->width_shift = width; // src width shift

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;

    en_int = en_bits & DMA_CH_EN_XFER_INT;
    if (en_int == 0) {
        ch_info->flags |= DMA_FLAG_POLLING; // polling, no interrupt
        control &= ~DMA_CH_CTLL_INT_EN; // Disable interrupt
    } else {
        ch_info->flags &= ~DMA_FLAG_POLLING; // use interrupt
        control |= DMA_CH_CTLL_INT_EN; // Enable interrupt
    }

    // make sure remove LLP_EN currently
    if (en_pipo) {
        // HW_LLP is necessary for PingPong transfer
        ch_info->flags |= DMA_FLAG_PIPO | DMA_FLAG_HW_LLP;
        control |= DMA_CH_CTLL_LLP_EN_MASK;
    } else {
        ch_info->flags &= ~DMA_FLAG_PIPO; // Non-PingPong transfer
        control &= ~DMA_CH_CTLL_LLP_EN_MASK;
    }

    DMA_CHANNEL_REG * dma_ch;
    dma_ch = DMA_CHANNEL(ch);

    // Write control & configuration etc. registers
    dma_ch->CTL_LO = control;
    dma_ch->CFG_LO = config_low;
    dma_ch->CFG_HI = config_high;
    dma_ch->LLP = 0; // Reset LLP when setup

    // Reset scatter/gather etc. registers
    dma_ch->SGR = (control & DMA_CH_CTLL_S_GATH_EN ? ch_info->src_gath : 0);
    dma_ch->DSR = (control & DMA_CH_CTLL_D_SCAT_EN ? ch_info->dst_scat : 0);

    // Enable DMA XFER and ERROR interrupts (no BLOCK & TRANS interrupts)
    if (en_int != 0) {
        enable_xfer_interrupts(ch_bit);
        enable_error_interrupts(ch_bit);
    }
    if (en_blk != 0) {
        enable_block_interrupts(ch_bit);
    }

    ch_info->flags |= DMA_FLAG_SETUP;
    return CSK_DRIVER_OK;
}


//[NEW]
_FAST_TEXT int32_t dma_channel_start (uint8_t      ch,
                                  uint32_t      src_addr,
                                  uint32_t      dst_addr,
                                  uint32_t      total_size)
{
    return dma_channel_start_block(ch, DMACH_CFG_FLAG_BOTH_ADDR,
                            src_addr, dst_addr, total_size);
}


// Check the DMA channel has been configured for some peripheral as specified before and select if configured
// xfer_type    Memory to Peripheral (M2P) or Peripheral to Memory (P2M)
// hs_id        hardware handshaking interface # (SHOULD less than DMA_HSID_COUNT)
/*_FAST_TEXT*/ bool dma_channel_check_select(uint8_t ch, uint8_t xfer_flag, uint8_t hs_id)
{
    DMA_CHANNEL_REG * dma_ch;
    bool in_pipo_xfer;
    uint8_t xfer_type = DMA_TT_MASK(xfer_flag);
    uint8_t req_pipo = DMA_PIPO_MASK(xfer_flag);
    //uint32_t control, config_low, config_high;

    if (ch >= DMA_NUMBER_OF_CHANNELS || // (channel_active & (0x1 << ch)) ||
        ((xfer_type != DMA_TT_M2P) && (xfer_type != DMA_TT_P2M)))
        return false;

    dma_ch = DMA_CHANNEL(ch);
    //control = dma_ch->CTL_LO;
    //config_low = dma_ch->CFG_LO;
    //config_high = dma_ch->CFG_HI;

    // check the channel has been setup, return if NOT yet
    if (!(channel_info[ch].flags & DMA_FLAG_SETUP))
        return false;

    // don't check if active or not if in_pipo_xfer
    in_pipo_xfer = (channel_info[ch].flags & DMA_FLAG_PIPO) != 0;
    if (!in_pipo_xfer) {// already active or request PIPO xfer
        if((channel_active & (0x1 << ch)) || req_pipo)
            return false;
    }

    // check if transfer type meets requirement
    if ((dma_ch->CTL_LO & DMA_CH_CTLL_TTFC_MASK) >> DMA_CH_CTLL_TTFC_POS != xfer_type)
        return false;

    // check if handshaking interface # meets requirement
    if (xfer_type == DMA_TT_M2P) { // memory -> peripheral, hw handshake with DST
        if ((dma_ch->CFG_HI & DMA_CH_CFGH_DST_PER_MASK) >> DMA_CH_CFGH_DST_PER_POS == hs_id) {
            if (in_pipo_xfer || set_channel_active_flag(ch) == 0)
                return true;
        }
    } else { // peripheral -> memory, hw handshake with SRC
        if ((dma_ch->CFG_HI & DMA_CH_CFGH_SRC_PER_MASK) >> DMA_CH_CFGH_SRC_PER_POS == hs_id) {
            if (in_pipo_xfer || set_channel_active_flag(ch) == 0)
                return true;
        }
    }

    return false;
}


//BSD: Add dma_channel_configure_lite experimentally...
/*
extern int32_t dma_channel_configure_lite (uint8_t      ch,
                                           uint8_t      cfg_flags,
                                           uint32_t     src_addr,
                                           uint32_t     dst_addr,
                                           uint32_t     total_size)
*/
/*_FAST_TEXT*/ int32_t dma_channel_start_block (uint8_t      ch,
                                                      uint8_t      cfg_flags,
                                                      uint32_t     src_addr,
                                                      uint32_t     dst_addr,
                                                      uint32_t     total_size)
{
    uint32_t ch_bit, control, size;
    DMA_Channel_Info *ch_info;
    DMA_CHANNEL_REG * dma_ch;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return CSK_DRIVER_ERROR_PARAMETER;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit)) // !(channel_reserved & ch_bit)
        return CSK_DRIVER_ERROR;

    ch_info = &channel_info[ch];
    ch_info->SizeToXfer = total_size;
    ch_info->SizeXfered = 0;
    ch_info->llp = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

    dma_ch = DMA_CHANNEL(ch);
    control = dma_ch->CTL_LO;
    if (total_size > MAX_BLK_TS) {
        size = MAX_BLK_TS;
        update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);
    } else {
        size = total_size;
        ch_info->SrcAddr = 0;
        ch_info->DstAddr = 0;
    }

    // do cache sync operation before the mock BLOCK transfer
    if (ch_info->cache_sync != DMA_CACHE_SYNC_NOP)
        do_cache_sync(ch_info, control, src_addr, dst_addr, total_size);

    // Set CTLx.BLOCK_TS = size and cTLx.Done = 0
    dma_ch->CTL_HI = (size & DMA_CH_CTLH_BLOCK_TS_MASK);

    if (cfg_flags & DMACH_CFG_FLAG_SRC_ADDR)
        dma_ch->SAR = src_addr;
    if (cfg_flags & DMACH_CFG_FLAG_DST_ADDR)
        dma_ch->DAR = dst_addr;

    // Reset LLP
    dma_ch->LLP = 0;

    // Enable DMA Channel to trigger data transfer
    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    // Enable DMA Controller
    //CSK_DMA->CFG = 0x1;

    return CSK_DRIVER_OK;
}


//[IN] blk_array    blocks by which data are transfered in Ping-Pong mode endlessly
//[IN/OUT] blk_cnt_p    indicate count of blocks in blk_arry when input, and
//                  count of blocks (start from head) set successfully into DMAC when output.
int32_t dma_channel_start_pipo (uint8_t ch, DMA_PIPO_BLK *blk_array, uint8_t *blk_cnt_p)
{
    uint32_t ch_bit, reg_val;
    uint8_t blks, i, gie, first_start;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS || blk_array == NULL || blk_cnt_p == NULL) // || *blk_cnt_p < 2
        return CSK_DRIVER_ERROR_PARAMETER;

    for (i = 0; i < *blk_cnt_p; i++) {
        if (blk_array[i].size > MAX_BLK_TS)
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    ch_bit = 0x1U << ch;

    // return failure if channel active flag is not set (indicates NOT selected before)
    if (!(channel_active & ch_bit)) //(CSK_DMA->CH_EN & ch_bit)
        return CSK_DRIVER_ERROR;

    // first start_pipo call after dma_channel_setup
    first_start = !(CSK_DMA->CH_EN & ch_bit);
    if (first_start && *blk_cnt_p < 2)
        return CSK_DRIVER_ERROR_PARAMETER;

    DMA_Channel_Info *ch_info;
    DMA_CHANNEL_REG * dma_ch;
    DMA_LLP mark, cur, pre;

    ch_info = &channel_info[ch];
    dma_ch = DMA_CHANNEL(ch);

    if (first_start) { // dma_ch->LLP == NULL
        uint32_t used_bits = 0;

        // initialize all 0 for PingPong transfer
        ch_info->SizeToXfer = 0;
        ch_info->SizeXfered = 0;
        ch_info->SrcAddr = ch_info->DstAddr = 0;
        ch_info->last_llp = NULL;

        //dma_ch->LLP = NULL;
        mark = cur = NULL;
        gie = GINT_enabled();
        if (gie) { disable_GINT(); }
        for (i = 0, blks = 0; i < DMA_MAX_LL_ITEMS; i++) {
            if (ll_item_bits & (0x1 << i)) // used
                continue;
            used_bits |= (0x1 << i);
            memset(&ll_items[i], 0, sizeof(DMA_LLI));

            if (mark == NULL) {
                cur = mark = &ll_items[i];
            } else {
                cur->LLP = (uint32_t)&ll_items[i];
                ll_items[i].preLLP = cur;
                cur = &ll_items[i];
            }
            blks++; // increase count of allocated block
            if (blks == *blk_cnt_p)
                break;
        } // end for i

        if (blks == *blk_cnt_p || blks >= 2) {
            cur->LLP = (uint32_t)mark;
            mark->preLLP = cur;
            ch_info->llp = mark; // saved in channel info
            //*blk_cnt_p = blks;
            ll_item_bits |= used_bits;
            dma_ch->LLP = (uint32_t)mark;
        }
        if (gie) { enable_GINT(); }

        if (blks < 2) { // no enough LL_ITEM memory
            *blk_cnt_p = 0;
            return CSK_DRIVER_ERROR;
        }
    } // first start_pipo call

    // fill in LLI items
    i = 0;
#if WB_BLK_DONE // DONE bit is written back to LLItem
    cur = mark = (DMA_LLP)dma_ch->LLP; // next LLI
    assert(cur != NULL);
    do {
        reg_val = cur->u.CTL_HI;
        // check if current LLI item is available
        if (reg_val == 0 || (reg_val & DMA_CH_CTLH_DONE)) {
            reg_val = (uint32_t)blk_array[i].src;
            cur->SAR = reg_val ? reg_val : dma_ch->SAR; // keep current value if reg_val == 0
            reg_val = (uint32_t)blk_array[i].dst;
            cur->DAR = reg_val ? reg_val : dma_ch->DAR; // keep current value if reg_val == 0
            cur->CTL_LO = dma_ch->CTL_LO; // use current DMA channel control
            cur->u.SIZE = blk_array[i].size;
            i++;

            // do cache sync operation before BLOCK transfer
            if (ch_info->cache_sync != DMA_CACHE_SYNC_NOP)
                do_cache_sync(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, cur->u.SIZE);
        }
        cur = (DMA_LLP)cur->LLP;
    } while (i < *blk_cnt_p && cur != mark);

#else // IP bug: DONE bit is NOT written back to LLItem (ONLY 2 blocks PingPong)
    // Currently DONE bit is NOT set correctly, so we CANNOT know which blocks
    // have been transferred! We just assume that the other block is finished
    // when there are only 2 blocks in the linked list!

//    volatile int dly_cnt = 10000;
//    while (dly_cnt -- > 0);

    cur = mark = (DMA_LLP)dma_ch->LLP;

    do {
        reg_val = (uint32_t)blk_array[i].src;
        cur->SAR = reg_val ? reg_val : dma_ch->SAR; // keep current value if 0
        reg_val = (uint32_t)blk_array[i].dst;
        cur->DAR = reg_val ? reg_val : dma_ch->DAR; // keep current value if 0
        cur->CTL_LO = dma_ch->CTL_LO; // use current DMA channel control
        cur->u.SIZE = blk_array[i].size;

        // do cache sync operation before BLOCK transfer
        if (ch_info->cache_sync != DMA_CACHE_SYNC_NOP)
            do_cache_sync(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, cur->u.SIZE);

        pre = cur;
        cur = (DMA_LLP)cur->LLP;

        // Stop PingPong after this block
        if (blk_array[i].flags & PIPO_BLK_FLAG_STOP) {
            // clean EN_LLP flags of src & dst in CTL_LO
            pre->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;
            pre->LLP = 0;

            // set DONE bit, clear BLOCK_TS;
            //cur->u.CTL_HI = DMA_CH_CTLH_BLOCK_TS_MASK;
            //cur->u.CTL_HI = 0;
            //cur->LLP = 0;
        }

        i++;
    } while (i < *blk_cnt_p && cur != mark);

#endif

    *blk_cnt_p = i; // return count of LLI item actually filled

    // Enable DMA Channel to trigger data transfer
    if (first_start) {
        //dma_ch->LLP = (uint32_t)ch_info->llp;
        CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;
    }

    // Enable DMA Controller
    //CSK_DMA->CFG = 0x1;

#if 0
    DMA_LLP old_cur = mark;
    if (!first_start) {
        if(old_cur == &ll_items[0])
            logDbg("Fblock 0!\n");
        else if(old_cur == &ll_items[1])
            logDbg("Fblock #1!\n");
    } else {
        if(old_cur == &ll_items[0])
            logDbg("Init Fblock 0!\n");
        else if(old_cur == &ll_items[1])
            logDbg("Init Fblock #1!\n");
    }
#endif

    return CSK_DRIVER_OK;
}


// cancel the circular Ping/Ping operation, that is, break  the circular chain,
// usually called in BLOCK COMPLETE ISR.
int32_t dma_channel_cancel_pipo (uint8_t ch)
{
    uint32_t ch_bit, reg_val;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return CSK_DRIVER_ERROR_PARAMETER;

    ch_bit = 0x1U << ch;

    DMA_Channel_Info *ch_info;
    DMA_CHANNEL_REG * dma_ch;
    DMA_LLP cur, first;

    ch_info = &channel_info[ch];
    if (!(ch_info->flags & DMA_FLAG_PIPO))
        return CSK_DRIVER_OK; //CSK_DRIVER_ERROR;

    dma_ch = DMA_CHANNEL(ch);
    first = (DMA_LLP)dma_ch->LLP;
    if (first == NULL)
        return CSK_DRIVER_OK;

#if WB_BLK_DONE
    dma_ch->LLP = 0;
    cur = first;
    do {
        reg_val = cur->u.CTL_HI;
        // check if current LLI item is intact or done
        if (reg_val == 0 || (reg_val & DMA_CH_CTLH_DONE)) {
            //if (cur == first)
            //    dma_ch->LLP = 0;
            cur->preLLP->LLP = 0;
            return CSK_DRIVER_OK;
        }
        cur = (DMA_LLP)cur->LLP;
    } while (cur != first);

#else // !WB_BLK_DONE
    // There are only 2 blocks in the linked list!
    dma_ch->LLP = 0;
    first->preLLP->LLP = 0;

#endif // !WB_BLK_DONE

    return CSK_DRIVER_OK;
}

/**
  \fn          int32_t dma_channel_configure_LLP (
                                   uint8_t      ch,
                                   DMA_LLP      llp,
                                   uint32_t     config_low,
                                   uint32_t     config_high,
                                   uint32_t     src_gath,
                                   uint32_t     dst_scat);
  \brief       Configure DMA channel for Multi-Block transfer with linked list (block chaining)
               (and enable DMA channel implicitly)
  \param[in]   ch           The selected Channel number returned by dma_channel_select()
  \param[in]   llp          pointer to the fisrt LLI (linked list item)
                            NOTE: memory of all LLIs SHOULD be kept until DMA is finished!
  \param[in]   config_low   Channel configuration's low WORD(32bit)
  \param[in]   config_high  Channel configuration's high WORD(32bit)
  \param[in]   src_gath     Value of Source Gather register (see above macros of SG_XXX)
  \param[in]   dst_scat     Value of Destination Scatter register (see above macros of SG_XXX)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/

#if SUPPORT_HW_LLP

/*_FAST_TEXT*/ int32_t dma_channel_start_LLP (uint8_t ch, DMA_LLP llp)
{
    // traverse all Linked List items
    DMA_LLP cur, next;

    // List Master Select, AHB layer/interface of memory device where LLI stores
    uint32_t ch_bit, lms, control; // width, size
    DMA_Channel_Info *ch_info;
    DMA_CHANNEL_REG * dma_ch;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS || llp == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return CSK_DRIVER_ERROR;

    ch_info = &channel_info[ch];
    dma_ch = DMA_CHANNEL(ch);

    control = dma_ch->CTL_LO;
    lms = llp->LLP & 0x03;

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    while (cur != NULL) {
        // size doesn't exceed MAX_BLK_TS
        if (cur->u.SIZE > MAX_BLK_TS)
            return CSK_DRIVER_ERROR_PARAMETER;

        // use current CTL if NOT set
        if (cur->CTL_LO == 0)
            cur->CTL_LO = control;

        // Usually LMS bits SHOULD NOT change
        if (lms != (cur->LLP & 0x03))
            return CSK_DRIVER_ERROR_PARAMETER;

        // check scatter/gather validity
        if (cur->CTL_LO & DMA_CH_CTLL_S_GATH_EN) {
            if (SG_COUNT(ch_info->src_gath) == 0)
                return CSK_DRIVER_ERROR_PARAMETER;
        }
        if (cur->CTL_LO & DMA_CH_CTLL_D_SCAT_EN) {
            if (SG_COUNT(ch_info->dst_scat) == 0)
                return CSK_DRIVER_ERROR_PARAMETER;
        }

        // Get next LLI (LLI address is 4bytes aligned)
        cur = (DMA_LLP)(cur->LLP & ~0x3UL);

    } //end while

    ch_info->SizeXfered = 0;
    ch_info->llp = llp; //(DMA_LLP)(llp->LLP);
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

    ch_info->flags |= DMA_FLAG_HW_LLP;
    ch_info->SizeToXfer = 0;

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    while (cur != NULL) {
        ch_info->SizeToXfer += cur->u.SIZE;
        // do cache sync operation before the mock BLOCK transfer
        if (ch_info->cache_sync != DMA_CACHE_SYNC_NOP)
            do_cache_sync(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, cur->u.SIZE);
        // Get next LLI (LLI address is 4bytes aligned)
        next = (DMA_LLP)(cur->LLP & ~0x3UL);
        // add LLP_EN if not last one
        if (next != NULL)
            cur->CTL_LO |= DMA_CH_CTLL_LLP_EN_MASK;
        else
            cur->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;
        cur = next; // next LLI
    } //end while

    dma_ch->LLP = (uint32_t)llp;
    dma_ch->CTL_LO = DMA_CH_CTLL_LLP_EN_MASK;

    // Enable DMA Channel to trigger data transfer
    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    // Enable DMA Controller
    //CSK_DMA->CFG = 0x1;

    return CSK_DRIVER_OK;
}

//FIXME: if HW LLP is used, DMA_CACHE_SYNC_DST may NOT take effect!!
/*_FAST_TEXT*/ int32_t dma_channel_configure_LLP (
                                       uint8_t      ch,
                                       DMA_LLP      llp,
                                       uint32_t     config_low,
                                       uint32_t     config_high,
                                       uint32_t     src_gath,
                                       uint32_t     dst_scat)
{
    // traverse all Linked List items
    DMA_LLP cur, next;

    // List Master Select, AHB layer/interface of memory device where LLI stores
    uint32_t ch_bit, lms, width; //, size
    DMA_Channel_Info *ch_info;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return CSK_DRIVER_ERROR_PARAMETER;

    // SHOULD NOT set Auto Reload for Src/Dst
    if (llp == NULL || (config_low & DMA_CH_CFGL_RELOAD_MASK) != 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return CSK_DRIVER_ERROR;

     lms = llp->LLP & 0x03;
    //LMS is hardcoded on ARCS?
    //if ( lms != DMAH_CH_LMS )
    //    return CSK_DRIVER_ERROR;

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    while (cur != NULL) {
        // Usually LMS bits SHOULD NOT change
        if (lms != (cur->LLP & 0x03))
            return CSK_DRIVER_ERROR_PARAMETER;

        // check scatter/gather validity
        if (cur->CTL_LO & DMA_CH_CTLL_S_GATH_EN) {
            if (SG_COUNT(src_gath) == 0)
                return CSK_DRIVER_ERROR_PARAMETER;
        }
        if (cur->CTL_LO & DMA_CH_CTLL_D_SCAT_EN) {
            if (SG_COUNT(dst_scat) == 0)
                return CSK_DRIVER_ERROR_PARAMETER;
        }

        if (cur->u.SIZE > MAX_BLK_TS)
            return CSK_DRIVER_ERROR_PARAMETER;

        // Get next LLI (LLI address is 4bytes aligned)
        cur = (DMA_LLP)(cur->LLP & ~0x3UL);
    } //end while

    // check if src/dst burst size & xfer width are legal
    //if (check_burst_bytes(ch, llp->CTL_LO) != 0)
    //    return -1;

    ch_info = &channel_info[ch];

    width = (llp->CTL_LO & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return CSK_DRIVER_ERROR;
    }
    ch_info->dst_wid_shift = width; // dst width shift

    width = (llp->CTL_LO & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return CSK_DRIVER_ERROR;
    }
    ch_info->width_shift = width; // src width shift

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;
    ch_info->SizeXfered = 0;
    ch_info->llp = llp; //(DMA_LLP)(llp->LLP);
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

     ch_info->flags |= DMA_FLAG_HW_LLP;
     ch_info->SizeToXfer = 0;

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    while (cur != NULL) {
        ch_info->SizeToXfer += cur->u.SIZE;
        // do cache sync operation before the mock BLOCK transfer
        do_cache_sync(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, cur->u.SIZE);
        // Get next LLI (LLI address is 4bytes aligned)
        next = (DMA_LLP)(cur->LLP & ~0x3UL);
        // add LLP_EN if not last one
        if (next != NULL)   cur->CTL_LO |= DMA_CH_CTLL_LLP_EN_MASK;
        else cur->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;
        cur = next; // next LLI
    } //end while

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

    DMA_CHANNEL_REG * dma_ch = DMA_CHANNEL(ch);
    dma_ch->LLP = (uint32_t)llp;
    dma_ch->CTL_LO = DMA_CH_CTLL_LLP_EN_MASK;
    dma_ch->CFG_LO = config_low;
    dma_ch->CFG_HI = config_high;

    enable_xfer_interrupts(ch_bit);
    enable_error_interrupts(ch_bit);

    // Enable DMA Channel to trigger data transfer
    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    return CSK_DRIVER_OK;
}

#else // !SUPPORT_HW_LLP

/*_FAST_TEXT*/ int32_t dma_channel_start_LLP (uint8_t ch, DMA_LLP llp)
{
    // traverse all Linked List items
    DMA_LLP cur, next;
    uint32_t ch_bit, lms, control, size; //width
    DMA_Channel_Info *ch_info;
    DMA_CHANNEL_REG * dma_ch;

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS || llp == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return CSK_DRIVER_ERROR;

    lms = llp->LLP & 0x03;

    ch_info = &channel_info[ch];
    dma_ch = DMA_CHANNEL(ch);
    control = dma_ch->CTL_LO;

    if (llp->CTL_LO == 0)
        llp->CTL_LO = control;
    //else
    //    assert(control == llp->CTL_LO);

    // use "mock" BLOCK transfer (actually DMA transfer), SHOULD remove LLP_EN
    llp->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    while (cur != NULL) {
/*
        // Usually LMS bits SHOULD NOT change
        if (lms != (cur->LLP & 0x03))
            return -1;

        // check scatter/gather validity
        if (cur->CTL_LO & DMA_CH_CTLL_S_GATH_EN) {
            if (SG_COUNT(src_gath) == 0)
                return -1;
        }
        if (cur->CTL_LO & DMA_CH_CTLL_D_SCAT_EN) {
            if (SG_COUNT(dst_scat) == 0)
                return -1;
        }
*/

        // Get next LLI (LLI address is 4bytes aligned)
        next = (DMA_LLP)(cur->LLP & ~0x3UL);

        //value = cur->CTL_LO & DMA_CH_CTLL_LLP_EN_MASK;
        //if (next == NULL) {
        //    // LLP enabled bits SHOULD of last LLI should be 0
        //    if (value != 0)
        //        return -1;
        //} else if (llp_en != value) {
        //    // LLP enabled bits SHOULD NOT change between blocks
        //    return -1;
        //}

        if (cur->CTL_LO == 0)
            cur->CTL_LO = control;
        //else
        //    assert(cur->CTL_LO = control);

        // use "mock" BLOCK transfer (actually DMA transfer), SHOULD remove LLP_EN
        cur->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

        cur = next; // next LLI
    } //end while


    ch_info->SizeToXfer = llp->u.SIZE;
    ch_info->SizeXfered = 0;
    ch_info->llp = (DMA_LLP)(llp->LLP);
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif
    size = llp->u.SIZE > MAX_BLK_TS ? MAX_BLK_TS : llp->u.SIZE;
    update_next_xfer_addr(ch_info, control, llp->SAR, llp->DAR, size); // llp->CTL_LO

    // do cache sync operation before the mock BLOCK transfer
    if (ch_info->cache_sync != DMA_CACHE_SYNC_NOP)
        do_cache_sync(ch_info, control, llp->SAR, llp->DAR, llp->u.SIZE); // llp->CTL_LO

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

//    // Trigger first DMA transfer
//    return dma_channel_configure_internal(ch, 1, llp->SAR, llp->DAR, size, llp->CTL_LO,
//                                          config_low, config_high);

    // Set CTLx.BLOCK_TS = size and cTLx.Done = 0
    dma_ch->CTL_HI = (size & DMA_CH_CTLH_BLOCK_TS_MASK);
    //assert(dma_ch->CTL_LO == llp->CTL_LO);
    dma_ch->CTL_LO = llp->CTL_LO;
    dma_ch->SAR = llp->SAR;
    dma_ch->DAR = llp->DAR;

    // Reset LLP
    dma_ch->LLP = 0;

    // Enable DMA Channel to trigger data transfer
    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    // Enable DMA Controller
    //CSK_DMA->CFG = 0x1;

    return CSK_DRIVER_OK;
}

/*_FAST_TEXT*/ int32_t dma_channel_configure_LLP (
                                       uint8_t      ch,
                                       DMA_LLP      llp,
                                       uint32_t     config_low,
                                       uint32_t     config_high,
                                       uint32_t     src_gath,
                                       uint32_t     dst_scat)
{
    // traverse all Linked List items
    DMA_LLP cur, next;
    // LLP enabled bits (LLP_SRC/DST_EN bits in CTLx)
    //uint32_t llp_en, value;
    // List Master Select, AHB layer/interface of memory device where LLI stores
    uint32_t ch_bit, lms, width, size;
    DMA_Channel_Info *ch_info;

    // SHOULD NOT set Auto Reload for Src/Dst
    if (llp == NULL || (config_low & DMA_CH_CFGL_RELOAD_MASK) != 0)
        return -1;

    lms = llp->LLP & 0x03;
    //llp_en = llp->CTL_LO & DMA_CH_CTLL_LLP_EN_MASK;

    // use "mock" BLOCK transfer (actually DMA transfer), SHOULD remove LLP_EN
    llp->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

    // LMS is hardcoded on ARCS?
//    if ( lms != DMAH_CH_LMS )
//        return -1;

    ////if (llp_en == 0) // no LLP_EN set for Src or Dst
    //if (llp_en != DMA_CH_CTLL_LLP_EN_MASK) // SHOULD set LLP_EN for both Src and Dst!
    //    return -1;

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    while (cur != NULL) {
        // Usually LMS bits SHOULD NOT change
        if (lms != (cur->LLP & 0x03))
            return -1;

        // check scatter/gather validity
        if (cur->CTL_LO & DMA_CH_CTLL_S_GATH_EN) {
            if (SG_COUNT(src_gath) == 0)
                return -1;
        }
        if (cur->CTL_LO & DMA_CH_CTLL_D_SCAT_EN) {
            if (SG_COUNT(dst_scat) == 0)
                return -1;
        }

        // Get next LLI (LLI address is 4bytes aligned)
        next = (DMA_LLP)(cur->LLP & ~0x3UL);

        //value = cur->CTL_LO & DMA_CH_CTLL_LLP_EN_MASK;
        //if (next == NULL) {
        //    // LLP enabled bits SHOULD of last LLI should be 0
        //    if (value != 0)
        //        return -1;
        //} else if (llp_en != value) {
        //    // LLP enabled bits SHOULD NOT change between blocks
        //    return -1;
        //}

        // use "mock" BLOCK transfer (actually DMA transfer), SHOULD remove LLP_EN
        cur->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

        cur = next; // next LLI
    } //end while

    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return -1;

    // check if src/dst burst size & xfer width are legal
    //if (check_burst_bytes(ch, llp->CTL_LO) != 0)
    //    return -1;

    ch_info = &channel_info[ch];

    width = (llp->CTL_LO & DMA_CH_CTLL_DST_WIDTH_MASK) >> DMA_CH_CTLL_DST_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->dst_wid_shift = width; // dst width shift

    width = (llp->CTL_LO & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->width_shift = width; // src width shift

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;

    ch_info->SizeToXfer = llp->u.SIZE;
    ch_info->SizeXfered = 0;
    ch_info->llp = (DMA_LLP)(llp->LLP);
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

    //size = llp->u.SIZE > MAX_BLK_TS ? FIT_BLK_TS : llp->u.SIZE;
    size = llp->u.SIZE > MAX_BLK_TS ? MAX_BLK_TS : llp->u.SIZE;
    update_next_xfer_addr(ch_info, llp->CTL_LO, llp->SAR, llp->DAR, size);

    // do cache sync operation before the mock BLOCK transfer
    do_cache_sync(ch_info, llp->CTL_LO, llp->SAR, llp->DAR, llp->u.SIZE);

    // Disable DMA Channel
    //CSK_DMA->CH_EN = (ch_bit << 8);

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

    // Trigger first DMA transfer
    return dma_channel_configure_internal(ch, 1, llp->SAR, llp->DAR, size, llp->CTL_LO,
                                          config_low, config_high);
}

#endif // SUPPORT_HW_LLP


/**
  \fn          int32_t dma_channel_suspend (uint8_t ch, uint8_t wait_done)
  \brief       Suspend channel transfer (and can be resumed later)
  \param[in]   ch   Channel number
  \param[in]   wait_done    Whether to wait for suspend operation is done
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t dma_channel_suspend (uint8_t ch, uint8_t wait_done)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    uint32_t value, ch_bit = 1U << ch;

    // Check if channel is enabled
    if ((CSK_DMA->CH_EN & ch_bit) == 0)
        return -1;

    value = DMA_CHANNEL(ch)->CFG_LO;
    if ((value & DMA_CH_CFGL_CH_SUSP) == 0) {
        value |= DMA_CH_CFGL_CH_SUSP;
        DMA_CHANNEL(ch)->CFG_LO = value;
        if (wait_done) {
            // wait for the channel FIFO is empty
            while ((DMA_CHANNEL(ch)->CFG_LO & DMA_CH_CFGL_FIFO_EMPTY) == 0) { }
        }
    }

    return 0;
}


/**
  \fn          int32_t dma_channel_resume (uint8_t ch)
  \brief       Resume channel transfer (ever suspended before)
  \param[in]   ch           Channel number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t dma_channel_resume (uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    uint32_t value, ch_bit = 1U << ch;

    // Check if channel is enabled
    if ((CSK_DMA->CH_EN & ch_bit) == 0)
        return -1;

    value = DMA_CHANNEL(ch)->CFG_LO;
    if (value & DMA_CH_CFGL_CH_SUSP) {
        value &= ~DMA_CH_CFGL_CH_SUSP;
        DMA_CHANNEL(ch)->CFG_LO = value;
    }

    return 0;
}


/**
  \fn          int32_t dma_channel_enable (uint8_t ch)
  \brief       Enable DMA channel (and Resume transfer if necessary)
               Event callback can also be replaced with new one.
  \param[in]   ch   Channel number
  \param[in]   cb_event_new     new event callback if NOT NULL.
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
//int32_t dma_channel_enable (uint8_t ch, DMA_SignalEvent_t cb_event_new)
int32_t dma_channel_enable (uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    uint32_t value;
    uint32_t enabled = 0, started = 0;
    uint32_t ch_bit = 1U << ch;

    if (CSK_DMA->CH_EN & ch_bit)
        enabled = 1;

    value = DMA_CHANNEL(ch)->CFG_LO;
    if ((value & DMA_CH_CFGL_CH_SUSP) == 0)
        started = 1;

    // return failure if channel has already been enabled & started
    if (enabled && started)
        return -1;

    // Set channel active flag if not set
    if ((channel_active & ch_bit) == 0) {
        if (set_channel_active_flag (ch) == -1)
            return -1;
    }

//    if (cb_event_new != NULL)
//        channel_info[ch].cb_event = cb_event_new;

    // Start the channel
    if (!started) {
        value &= ~DMA_CH_CFGL_CH_SUSP;
        DMA_CHANNEL(ch)->CFG_LO = value;
    }

    // Set ChEnReg bit for the channel
    if (!enabled)
        CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit ;

    return 0;
}


static void dma_channel_release_pipo(DMA_Channel_Info * pchi)
{
    assert(pchi != NULL);

    // release LLI items only if PingPong transfer
    if (!(pchi->flags & DMA_FLAG_PIPO))
        return;

    // PIPO cleanup
    DMA_LLP cur, pre, first = pchi->llp;
    pchi->llp = NULL;

    if (first == NULL)
        return;

    uint8_t gie = GINT_enabled();
    if (gie) { disable_GINT(); }

    cur = first;
    do {
        uint32_t i = ((uint32_t)cur - (uint32_t)&ll_items[0]) / sizeof (DMA_LLI);
        assert(i < 32);
        ll_item_bits &= ~(0x1 << i);
        cur->LLP = 0;

        //NOTE: use preLLP to traverse all items used by a DMA channel
        pre = cur->preLLP;
        cur->preLLP = NULL;
        cur = pre;
    } while (cur != NULL && cur != first);

    if (gie) { enable_GINT(); }

    pchi->flags &= ~DMA_FLAG_PIPO;
}

/**
  \fn          int32_t dma_channel_disable (uint8_t ch, uint8_t wait_done)
  \brief       Abort transfer and then Disable DMA channel
  \param[in]   ch           Channel number
  \param[in]   wait_done    Whether to wait for disable operation is done
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t dma_channel_disable (uint8_t ch, uint8_t wait_done)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return -1;

    uint32_t ch_bit = 1U << ch;

    // Suspend channel and then clear ChEnReg bit
    if (CSK_DMA->CH_EN & ch_bit) {
        // suspend channel if not yet
        uint32_t value = DMA_CHANNEL(ch)->CFG_LO;
        if ((value & DMA_CH_CFGL_CH_SUSP) == 0) {
             value |= DMA_CH_CFGL_CH_SUSP;
            DMA_CHANNEL(ch)->CFG_LO = value;
            if (wait_done) {
                // wait for the channel FIFO is empty
                while ((DMA_CHANNEL(ch)->CFG_LO & DMA_CH_CFGL_FIFO_EMPTY) == 0) { }
            }
        }
        // disable channel
        CSK_DMA->CH_EN = (ch_bit << 8);
        if (wait_done) {
            while (CSK_DMA->CH_EN & ch_bit) { }
        }
    }

    // Clear the channel information
    //memset(&channel_info[ch], 0, sizeof(DMA_Channel_Info));

    // Clear Channel active flag if set
    if (channel_active & ch_bit) {
        // release pipo items if any
        dma_channel_release_pipo(&channel_info[ch]);

        clear_channel_active_flag (ch);
    }

    return 0;
}


/**
  \fn          uint32_t dma_channel_abort (uint8_t ch, uint8_t wait_done)
  \brief       Abort transfer on the channel
  \param[in]   ch         Channel number
  \param[in]   wait_done  Whether to wait for disable operation is done
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
/*
int32_t dma_channel_abort (uint8_t ch, uint8_t wait_done)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS) { return -1; }

    uint32_t ch_bit = 1U << ch;

    // Suspend channel and then disable it
    if (CSK_DMA->CH_EN & ch_bit) {
        // suspend channel
        uint32_t value = DMA_CHANNEL(ch)->CFG_LO;
        value |= DMA_CH_CFGL_CH_SUSP;
        DMA_CHANNEL(ch)->CFG_LO = value;
        if (wait_done) {
            // wait for the channel FIFO is empty
            while ((DMA_CHANNEL(ch)->CFG_LO & DMA_CH_CFGL_FIFO_EMPTY) == 0) { }
        }
        // disable channel
        CSK_DMA->CH_EN = (ch_bit << 8);
        if (wait_done) {
            while (CSK_DMA->CH_EN & ch_bit) { }
        }
    }

    // Clear the channel information
    memset(&channel_info[ch], 0, sizeof(DMA_Channel_Info));

    // Clear Channel active flag if set
    if (channel_active & ch_bit) {
        clear_channel_active_flag (ch);
    }

    return 0;
}
*/


/**
  \fn          uint32_t dma_channel_get_status (uint8_t ch)
  \brief       Check if DMA channel is enabled or disabled
  \param[in]   ch Channel number
  \returns     Channel status
   - \b  1: channel enabled
   - \b  0: channel disabled
*/
/*
uint32_t dma_channel_get_status (uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return 0U;

    return ( (CSK_DMA->CH_EN & (1U << ch)) ? 1 : 0 );
}
*/
uint32_t dma_channel_get_status (uint8_t ch)
{
    return dma_channel_is_enabled(ch) ? 1 : 0;
}


bool dma_channel_is_enabled (uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return false;

    return ( (CSK_DMA->CH_EN & (1U << ch)) ? true : false );
}

bool dma_channel_is_polling(uint8_t ch)
{
    // Check if channel is valid, use interrupt by default
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return false;

    DMA_Channel_Info *ch_info = &channel_info[ch];
    return (ch_info->flags & DMA_FLAG_POLLING);
}

bool dma_channel_xfer_error(uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return false;

    return (CSK_DMA->RAW.ERROR & (1U << ch));
}

bool dma_channel_xfer_complete(uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return false;
    return (CSK_DMA->RAW.XFER & (1U << ch));
}

void dma_channel_clear_xfer_status(uint8_t ch)
{
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS)
        return;

    uint32_t ch_bit, size;
    DMA_CHANNEL_REG * dma_ch;
    DMA_Channel_Info *ch_info;
    bool clear_status = false;

    ch_bit = 0x1 << ch;
    dma_ch = DMA_CHANNEL(ch);
    ch_info = &channel_info[ch];

    // Clear polling flag
    ch_info->flags &= ~DMA_FLAG_POLLING; // use interrupt

    // Error interrupt (raw set, but mask not set)
    if ((CSK_DMA->RAW.ERROR & ch_bit) && !(CSK_DMA->MASK.ERROR & ch_bit)) {
        clear_error_interrupts(ch_bit);
        clear_status = true;
    }

    // Block interrupt (raw set, but mask not set)
    if ((CSK_DMA->RAW.BLOCK & ch_bit) && !(CSK_DMA->MASK.BLOCK & ch_bit)) {
        clear_block_interrupts(ch_bit);
    }

    // Xfer interrupt (raw set, but mask not set)
    if ((CSK_DMA->RAW.XFER & ch_bit) && !(CSK_DMA->MASK.XFER & ch_bit)) {
        clear_xfer_interrupts(ch_bit);
        clear_status = true;

        // update transferred size for the channel
        size = dma_ch->CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
        ch_info->SizeXfered += size;
        if (ch_info->SizeToXfer > size)
            ch_info->SizeToXfer -= size;
        else
            ch_info->SizeToXfer = 0;
    }

    if (clear_status) {
        dma_ch->CTL_LO = 0U;
        dma_ch->CTL_HI = 0U;
        dma_ch->CFG_LO = 0U;
        dma_ch->CFG_HI = 0U;
        dma_ch->LLP = 0U;

        // Clear Channel active flag
        clear_channel_active_flag (ch);
    }
}

/**
  \fn          uint32_t dma_channel_get_count (uint8_t ch)
  \brief       Get number of transferred data
  \param[in]   ch Channel number
  \returns     Number of transferred data items
*/
_FAST_TEXT uint32_t dma_channel_get_count (uint8_t ch) {
    // Check if channel is valid
    if (ch >= DMA_NUMBER_OF_CHANNELS) return 0;

    //// disable DMAC interrupt during calculation of count
    //uint8_t int_en = IRQ_enabled(IRQ_DMAC_VECTOR);
    //if (int_en) disable_IRQ(IRQ_DMAC_VECTOR);

    // disable DMAC interrupt & task switching interrupt (SWI)
    uint8_t gie = GINT_enabled();
    if (gie) disable_GINT();

    uint32_t count = channel_info[ch].SizeXfered;
    //if (CSK_DMA->CH_EN & (1U << ch)) // DMA channel transfer is ongoing
    //if (!(channel_info[ch].flags & DMA_FLAG_PIPO))
    count += (DMA_CHANNEL(ch)->CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK);

    //if (int_en) enable_IRQ(IRQ_DMAC_VECTOR);
    if (gie) enable_GINT();

    return count;
}


// called in BLOCK COMPLETE ISR for PingPong transfer
static uint32_t update_pipo_block_xferred_count(uint8_t ch)
{
    uint32_t xfer_cnt = 0;
    DMA_Channel_Info *pchi;
    DMA_CHANNEL_REG * dma_ch;
    DMA_LLP mark, cur;
    bool last_two = false, last_blk = false;

    assert (ch < DMA_NUMBER_OF_CHANNELS);
    pchi = &channel_info[ch];
    if (!(pchi->flags & DMA_FLAG_PIPO))
        return 0;

    dma_ch = DMA_CHANNEL(ch);
    mark = (DMA_LLP)dma_ch->LLP;

    uint8_t gie = GINT_enabled();
    if (gie) { disable_GINT(); }

#if WB_BLK_DONE // DONE bit is written back to LLItem
    //the second to last or the last
    if (mark == NULL) {
        cur = pchi->llp;
        while (cur->LLP != 0)
            cur = (DMA_LLP)cur->LLP;
        mark = cur->preLLP;
    }

    cur = mark;
    //mark = mark->preLLP; // ignore the ongoing LLI in the DMAC REG
    do {
        if (cur->u.CTL_HI & DMA_CH_CTLH_DONE) {
            pchi->last_llp = cur;
            xfer_cnt += cur->u.CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
        }
        cur = (DMA_LLP)cur->LLP; // use LLP to traverse all items
    } while (cur != NULL && cur != mark);

#else // IP bug: DONE bit is NOT written back to LLItem (ONLY 2 blocks PingPong)

    //the second to last or the last
    if (mark == NULL) {
        last_two = true;
        cur = pchi->llp;
        while (cur->LLP != 0)
            cur = (DMA_LLP)cur->LLP;
        mark = cur->preLLP; // second to last by default

        if (pchi->last_llp == cur->preLLP) {
            mark = (DMA_LLP)mark->LLP; // last one
            last_blk = true;
        }
    }

    pchi->last_llp = mark;
    xfer_cnt += mark->u.CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
    if (last_blk)
        dma_ch->CTL_HI &= ~DMA_CH_CTLH_BLOCK_TS_MASK; // clear BLOCK_TS

#endif // WB_BLK_DONE

    if (gie) { enable_GINT(); }
    return xfer_cnt;
}


// called in BLOCK COMPLETE ISR for PingPong transfer
#if WB_BLK_DONE // DONE bit is written back to LLItem
static void clr_pipo_block_done_count(uint8_t ch)
{
    DMA_Channel_Info *pchi;
    DMA_CHANNEL_REG * dma_ch;
    DMA_LLP mark, cur;

    assert (ch < DMA_NUMBER_OF_CHANNELS);
    pchi = &channel_info[ch];
    if (!(pchi->flags & DMA_FLAG_PIPO))
        return;

    dma_ch = DMA_CHANNEL(ch);
    mark = (DMA_LLP)dma_ch->LLP;
    if (mark == NULL)
        return;

    uint8_t gie = GINT_enabled();
    if (gie) { disable_GINT(); }

    cur = mark;
    mark = mark->preLLP;
    do {
        if (cur->u.CTL_HI & DMA_CH_CTLH_DONE)
            cur->u.CTL_HI &= ~(DMA_CH_CTLH_DONE | DMA_CH_CTLH_BLOCK_TS_MASK);
        cur = (DMA_LLP)cur->LLP; // use LLP to traverse all items
    } while (cur != NULL && cur != mark);

    if (gie) { enable_GINT(); }
}
#endif // WB_BLK_DONE


// return count of transferred block, called in BLOCK COMPLETE ISR for PingPong transfer
int32_t dma_channel_get_pipo_blks(uint8_t ch, DMA_PIPO_BLK *blk_array, uint8_t blk_cnt)
{
    uint32_t cnt = 0;
    DMA_Channel_Info *pchi;

    assert (ch < DMA_NUMBER_OF_CHANNELS);
    pchi = &channel_info[ch];
    if (!(pchi->flags & DMA_FLAG_PIPO))
        return CSK_DRIVER_ERROR;

#if WB_BLK_DONE // DONE bit is written back to LLItem
    DMA_LLP mark, cur;
    DMA_CHANNEL_REG * dma_ch;

    dma_ch = DMA_CHANNEL(ch);
    mark = (DMA_LLP)dma_ch->LLP;

    //the second to last or the last
    if (mark == NULL) {
        cur = pchi->llp;
        while (cur->LLP != 0)
            cur = (DMA_LLP)cur->LLP;
        mark = cur->preLLP;
    }

    uint8_t gie = GINT_enabled();
    if (gie) { disable_GINT(); }

    cur = mark;
    //mark = mark->preLLP; // ignore the ongoing LLI in the DMAC REG
    do {
        if (cur->u.CTL_HI & DMA_CH_CTLH_DONE) {
            if (cnt < blk_cnt) {
                blk_array[cnt].src = (void *)cur->SAR;
                blk_array[cnt].dst = (void *)cur->DAR;
                blk_array[cnt].size = cur->u.CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
                blk_array[cnt].flags = 0;
            }
            cnt++;
        }
        cur = (DMA_LLP)cur->LLP; // use LLP to traverse all items
    } while (cur != NULL && cur != mark);

    if (gie) { enable_GINT(); }

#else // IP bug: DONE bit is NOT written back to LLItem (ONLY 2 blocks PingPong)

    assert(pchi->last_llp != NULL);

    if (cnt < blk_cnt) {
        blk_array[cnt].src = (void *)pchi->last_llp->SAR;
        blk_array[cnt].dst = (void *)pchi->last_llp->DAR;
        blk_array[cnt].size = pchi->last_llp->u.CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
        blk_array[cnt].flags = 0;
    }
    cnt++;

#endif // WB_BLK_DONE

    return cnt;
}

/**
  \fn          void dma_irq_handler (void)
  \brief       DMA interrupt handler
*/
_FAST_TEXT void dma_irq_handler (void) {
    uint16_t i, ch;
    uint32_t ch_bit, size;
    DMA_CHANNEL_REG * dma_ch;
    DMA_Channel_Info *ch_info;

    // travers all dma channelfrom the largest no. to the smallest one
    for (i = 0; i < DMA_NUMBER_OF_CHANNELS; i++) {
        ch = DMA_NUMBER_OF_CHANNELS - 1 - i;
        dma_ch = DMA_CHANNEL(ch);
        ch_bit = 0x1 << ch;
        ch_info = &channel_info[ch];

        if (!(ch_bit & channel_active)) {
            continue;
        }

        // Error interrupt
        if (CSK_DMA->STATUS.ERROR & ch_bit) { // & CSK_DMA->MASK.ERROR
            // Clear interrupt flag
            clear_error_interrupts(ch_bit);
            if (CSK_DMA->STATUS.XFER & ch_bit)
                clear_xfer_interrupts(ch_bit);
            if (CSK_DMA->STATUS.BLOCK & ch_bit)
                clear_block_interrupts(ch_bit);

            // update transferred size for the channel
            size = dma_ch->CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
            ch_info->SizeXfered += size;
            if (ch_info->SizeToXfer > size)
                ch_info->SizeToXfer -= size;
            else
                ch_info->SizeToXfer = 0;

            dma_ch->CTL_LO = 0U;
            dma_ch->CTL_HI = 0U;
            dma_ch->CFG_LO = 0U;
            dma_ch->CFG_HI = 0U;
            //dma_ch->LLP = 0U; //FIXME:

            // Clear Channel active flag
            clear_channel_active_flag (ch);

            // Signal Event
            if (ch_info->cb_event) {
                ch_info->cb_event(
                        (ch << 8) | DMA_EVENT_ERROR,
                        ch_info->SizeXfered << ch_info->width_shift,
                        ch_info->usr_param);
            }

        } // end Error interrupt

        // real BLOCK complete interrupt
        if (CSK_DMA->STATUS.BLOCK & ch_bit) { // & CSK_DMA->MASK.BLOCK
            //TODO:
            // Clear interrupt flag
            clear_block_interrupts(ch_bit);

            // accumulate transferred count of block
            if (ch_info->flags & DMA_FLAG_PIPO)
                ch_info->SizeXfered += update_pipo_block_xferred_count(ch);

            // Signal Event
            if (ch_info->cb_event) {
                ch_info->cb_event(
                        (ch << 8) | DMA_EVENT_BLOCK_COMPLETE,
                        0, //(dma_ch->CTL_HI & MAX_BLK_TS) << ch_info->width_shift,
                        ch_info->usr_param);
            }

            //FIXME: clean all DONE bits and size in the block chain?
        #if WB_BLK_DONE // DONE bit is written back to LLItem
            clr_pipo_block_done_count(ch);
        #endif
        }

        // DMA transfer complete interrupt
        if (CSK_DMA->STATUS.XFER & ch_bit) { // & CSK_DMA->MASK.XFER
            // Clear interrupt flag
            clear_xfer_interrupts(ch_bit);

            bool done = false;

        #if SUPPORT_HW_LLP
            if (ch_info->flags & DMA_FLAG_HW_LLP) { // HW LLP
                //FIXME:
                if (!(ch_info->flags & DMA_FLAG_PIPO)) { // non-PingPong
                    ch_info->SizeXfered = ch_info->SizeToXfer;
                    ch_info->SizeToXfer = 0;
                }

                dma_ch->CTL_HI &= ~DMA_CH_CTLH_BLOCK_TS_MASK; // clear BLOCK_TS
                done = true;
            } else { // SW LLP or single block
        #endif
                // update transferred size for the channel
                size = dma_ch->CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK;
                ch_info->SizeXfered += size;
                dma_ch->CTL_HI &= ~DMA_CH_CTLH_BLOCK_TS_MASK; // clear BLOCK_TS
                if (ch_info->SizeToXfer > size)
                    ch_info->SizeToXfer -= size;
                else
                    ch_info->SizeToXfer = 0;

                uint32_t control, config_low, config_high;
                control = dma_ch->CTL_LO;
                config_low = dma_ch->CFG_LO;
                config_high = dma_ch->CFG_HI;

                //size = ch_info->SizeToXfer > MAX_BLK_TS ? FIT_BLK_TS : ch_info->SizeToXfer;
                size = ch_info->SizeToXfer > MAX_BLK_TS ? MAX_BLK_TS : ch_info->SizeToXfer;

                if (size > 0) { // current LLI/block is not done
                    uint32_t src_addr, dst_addr;
                    src_addr = ch_info->SrcAddr;
                    dst_addr = ch_info->DstAddr;
                    update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);

                    // Trigger next DMA transfer
                    //dma_channel_configure_internal(ch, 1, src_addr, dst_addr, size,
                    //                               control, config_low, config_high);
                    // use dma_channel_configure_internal_lite instead of dma_channel_configure_internal?
                    dma_channel_configure_internal_lite(ch, src_addr, dst_addr, size);

                } else { // current LLI/block is done
                    // do post-dma invalidate operation if necessary
                #if HAS_CACHE_SYNC
                    if (ch_info->CacheSyncBytes != 0) {
//                        cache_dma_fast_inv_stage2(ch_info->CacheSyncStart,
//                                ch_info->CacheSyncStart + ch_info->CacheSyncBytes);
//                        ch_info->CacheSyncStart = 0;
//                        ch_info->CacheSyncBytes = 0;
                    }
                #endif
                    if (ch_info->llp != NULL) { // next LLI/block
                        DMA_LLP cur = ch_info->llp;
                        ch_info->llp = (DMA_LLP)cur->LLP;
                        ch_info->SizeToXfer = cur->u.SIZE;

                        //size = cur->u.SIZE > MAX_BLK_TS ? FIT_BLK_TS : cur->u.SIZE;
                        size = cur->u.SIZE > MAX_BLK_TS ? MAX_BLK_TS : cur->u.SIZE;
                        update_next_xfer_addr(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, size);

                        // do cache sync operation before the mock BLOCK transfer
                        do_cache_sync(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, cur->u.SIZE);

                        // Trigger first DMA transfer
                       dma_channel_configure_internal(ch, 1, cur->SAR, cur->DAR, size,
                                                   cur->CTL_LO, config_low, config_high);
                    } else { // all LLIs/blocks are done
                        done = true;
                    } // end all LLI/block is done
                } // end current LLI/block is done
        #if SUPPORT_HW_LLP
            } // end !SUPPORT_HW_LLP (SW_LLP or single block)
        #endif

            if (done) {
                // release pipo items if any
                dma_channel_release_pipo(ch_info);

                // Clear Channel active flag
                clear_channel_active_flag (ch);

                // Signal Event
                if (ch_info->cb_event) {
                    ch_info->cb_event(
                            (ch << 8) | DMA_EVENT_TRANSFER_COMPLETE,
                            //ch_info->SizeXfered * ch_info->width_bytes,
                            ch_info->SizeXfered << ch_info->width_shift,
                            ch_info->usr_param);
                }
            } // end done

        } // end DMA transfer complete interrupt

    } // end for each channel
}


// count = channel_fifo_depth (bytes) / width_bytes
uint32_t calc_max_burst_size(uint32_t items)
{
    uint32_t i;
    if (items < 4)
        return DMA_BSIZE_1;
    if (items > 256)
        return DMA_BSIZE_256;
    for (i=7; i>=2; i--) { // 255 ~ 4
        if (items >> i)
            break;
    }
    return i-1;
}


/**
  \fn          int32_t dma_memcpy ( uint8_t    ch,
                            uint32_t           src_addr,
                            uint32_t           dst_addr,
                            uint32_t           total_bytes)
  \brief       Copy memory data through specified DMA channel.
  \param[in]   ch           The selected Channel number returned by dma_channel_select()
  \param[in]   src_addr     Source address
  \param[in]   dest_addr    Destination address
  \param[in]   total_bytes  The total bytes to be transfered
*/

//#define DMA_CHANNEL_FIFO_DEPTH_MAX       64 // bytes

int32_t dma_memcpy (uint8_t        ch,
                              uint32_t           src_addr,
                              uint32_t           dst_addr,
                              uint32_t           total_bytes)
{
    uint32_t cpy_bytes, i, ch_bit;
    uint32_t bytes_left = total_bytes;

    // move data bytes prior to DMA operation block
    cpy_bytes = (src_addr & 0x3UL);
    if (cpy_bytes > 0) {
        cpy_bytes = 4 - cpy_bytes;
        if (cpy_bytes > bytes_left)
            cpy_bytes = bytes_left;
        for (i=0; i<cpy_bytes; i++) {
            *(uint8_t*)dst_addr = *(uint8_t*)src_addr;
            src_addr++; dst_addr++;
        }
        bytes_left -= cpy_bytes;
    }

    // move data bytes post DMA operation block
    cpy_bytes = bytes_left & 0x3UL;
    bytes_left &= ~0x3UL; // word (4 bytes) aligned
    if (cpy_bytes > 0) {
        uint32_t src_addr2 = src_addr + bytes_left;
        uint32_t dst_addr2 = dst_addr + bytes_left;
        for (i=0; i<cpy_bytes; i++) {
            *(uint8_t*)dst_addr2 = *(uint8_t*)src_addr2;
            src_addr2++; dst_addr2++;
        }
    }

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    ch_bit = 0x1U << ch;
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return -1;

    DMA_Channel_Info *ch_info = &channel_info[ch];
    if (bytes_left == 0) { // no DMA operation
        // Clear Channel active flag
        clear_channel_active_flag (ch);

        // Signal Event even if no DMA operation
        if (ch_info->cb_event) {
            ch_info->cb_event(
                    (ch << 8) | DMA_EVENT_TRANSFER_COMPLETE,
                    total_bytes,
                    ch_info->usr_param);
        }

    } else { // bytes_left > 0, trigger DMA transfer
        uint32_t src_width, src_bsize, dst_width, dst_bsize;
        uint32_t size, control, config_low, config_high;

        ch_info->width_shift = 2; // shift
        ch_info->SizeToXfer = (bytes_left >> 2);

        src_width = DMA_WIDTH_WORD;
        src_bsize = DMA_BSIZE_1; // default, modified later
        src_bsize = calc_max_burst_size(DMA_CHANNELS_FIFO_DEPTH[ch] >> 2); // /4
//        i = DMA_CHANNELS_FIFO_DEPTH[ch];
//        if (i >= 64)
//            src_bsize = DMA_BSIZE_16; // mostly here
//        else if (i >= 16)
//            src_bsize = DMA_BSIZE_4; // could run here on AP

        dst_bsize = DMA_BSIZE_1; // default, modified later
        if ((dst_addr & 0x3) == 0) { // word (4 bytes) aligned
            dst_width = DMA_WIDTH_WORD;
            ch_info->dst_wid_shift = 2; //word shift
//            dst_bsize = src_bsize;
        } else if ((dst_addr & 0x1) == 0) { // halfword (2 bytes) aligned
            dst_width = DMA_WIDTH_HALFWORD;
            ch_info->dst_wid_shift = 1; //halfword shift
//            if (i >= 64)
//                dst_bsize = DMA_BSIZE_32;
//            else if (i >= 16)
//                dst_bsize = DMA_BSIZE_8;
        } else { // byte aligned
            dst_width = DMA_WIDTH_BYTE;
            ch_info->dst_wid_shift = 0; //byte shift
//            if (i >= 64)
//                dst_bsize = DMA_BSIZE_64;
//            else if (i >= 16)
//                dst_bsize = DMA_BSIZE_16;
        }
        dst_bsize = calc_max_burst_size(DMA_CHANNELS_FIFO_DEPTH[ch] >> ch_info->dst_wid_shift);

        control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
                DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
                DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

        config_low = DMA_CH_CFGL_CH_PRIOR(0);
        config_high = DMA_CH_CFGH_FIFO_MODE; // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

        //size = ch_info->SizeToXfer > MAX_BLK_TS ? FIT_BLK_TS : ch_info->SizeToXfer;
        size = ch_info->SizeToXfer > MAX_BLK_TS ? MAX_BLK_TS : ch_info->SizeToXfer;
        update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);

        // do cache sync operation before the mock BLOCK transfer
        do_cache_sync(ch_info, control, src_addr, dst_addr, ch_info->SizeToXfer);

        // Clear DMA interrupts
        clear_all_interrupts(ch_bit);

        // Trigger first DMA transfer
        int32_t ret = dma_channel_configure_internal(ch, 1, src_addr, dst_addr, size, control, config_low, config_high);
        if (ret < 0)
            return ret;

    } //end bytes_left > 0

    return 0;
}

