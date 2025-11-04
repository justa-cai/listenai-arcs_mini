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
//#define __aeabi_assert // for keil compiler

#define HAS_CACHE_SYNC      0 // 1

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
//#define IRQ_DMAC_VECTOR     IRQ_DMAC_VECTOR
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

    // bit[0]: HW LLP if 1; bit[1]: polling (NO interrupt) if 1
    uint8_t             flags;

    // value of SGR (Source Gather)
    uint32_t            src_gath;
    // value of DSR (Destination Scatter)
    uint32_t            dst_scat;

    // Cnt, accumulated DMA transferred size (Change once DMA transfer is done)
    uint32_t            SizeXfered;
    // Size, remaining size of current LLI or single BLOCK
    uint32_t            SizeToXfer;
    // point to next LLI (mock BLOCK), and NULL if single or last LLI
    DMA_LLP             llp;

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

#define DMA_FLAG_HW_LLP     0x1 // bit[0] @ flags
#define DMA_FLAG_POLLING    0x2 // bit[1] @ flags

#if SUPPORT_HW_LLP
_DMA DMA_LLI ll_items[DMA_MAX_LL_ITEMS]; // link list items
#endif // #if SUPPORT_HW_LLP

static uint32_t init_cnt       = 0U;
// channel active flag, set when channel is selected and cleared when DMA is completed or disabled
_DMA static uint32_t channel_active = 0U;
// channel reserved flag (if set, channel active flag is always kept until unreserved).
_DMA static uint32_t channel_reserved = 0U;
_DMA static DMA_Channel_Info channel_info[DMA_NUMBER_OF_CHANNELS];

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
_FAST_FUNC_RO static int32_t set_channel_active_flag (uint8_t ch)
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
_FAST_FUNC_RO static void clear_channel_active_flag (uint8_t ch)
{
  uint8_t gie = GINT_enabled();
  uint32_t ch_bit = 1U << ch;

  if (gie) { disable_GINT(); }

  //channel_active &= ~(1U << ch);
  //BSD: only NOT reserved channel can be clear active flay...
  if ((channel_reserved & ch_bit) == 0)
      channel_active &= ~ch_bit;

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
_FAST_FUNC_RO static int32_t dma_get_free_channel(uint8_t *pch)
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
_FAST_FUNC_RO uint8_t dma_channel_select(uint8_t *pch,
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
_FAST_FUNC_RO static void cache_sync_src(uint32_t control, uint32_t src_addr, uint32_t bytes, DMA_Channel_Info *ch_info)
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

_FAST_FUNC_SRAM static void cache_sync_dst(uint32_t control, uint32_t dst_addr, uint32_t bytes, DMA_Channel_Info *ch_info)
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
#endif

// Calculate how many bytes equal to "size" of data items on Source
_FAST_FUNC_SRAM static uint32_t calc_src_bytes(DMA_Channel_Info *ch_info, uint32_t control, uint32_t size)
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
_FAST_FUNC_SRAM static uint32_t calc_dst_bytes(DMA_Channel_Info *ch_info, uint32_t control, uint32_t size)
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
_FAST_FUNC_SRAM static void do_cache_sync(DMA_Channel_Info *ch_info, uint32_t control, uint32_t src_addr, uint32_t dst_addr, uint32_t size)
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

_FAST_FUNC_RO static void update_next_xfer_addr(DMA_Channel_Info *ch_info, uint32_t control,
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

_FAST_FUNC_SRAM __inline static void clear_all_interrupts(uint32_t ch_bits)
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
_FAST_FUNC_RO static int32_t dma_channel_configure_internal (
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
#if SUPPORT_HW_LLP
    //FIXME: set dma_ch->LLP register if HW_LLP!!
    if (ch_info->flags & DMA_FLAG_HW_LLP) // HW LLP
        ; //TODO: do something?
    else
#endif
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
_FAST_FUNC_RO static int32_t dma_channel_configure_internal_lite (
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
/*
    ch_info = &channel_info[ch];

    // Disable DMA interrupts
    disable_all_interrupts(ch_bit);
*/

    // Max block transfer size is MAX_BLK_TS (BLOCK_TS holds N bits)
    assert(size <= MAX_BLK_TS);
    // Set CTLx.BLOCK_TS = size and cTLx.Done = 0
    dma_ch->CTL_HI = (size & DMA_CH_CTLH_BLOCK_TS_MASK);

    // Set Source and Destination address
    dma_ch->SAR = src_addr;
    dma_ch->DAR = dst_addr;

/*
    // Write control & configuration etc. registers
    dma_ch->CTL_LO = control;
    dma_ch->CFG_LO = config_low;
    dma_ch->CFG_HI = config_high;

    // Reset LLP, if NOT use HW LLP (single block or SW LLP, a.k.a. mock BLOCK transfer
#if SUPPORT_HW_LLP
    if (!(ch_info->flags & DMA_FLAG_HW_LLP)) // HW LLP
#endif
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
*/

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

_FAST_FUNC_SRAM static bool dma_fill_ll_items(DMA_Channel_Info *ch_info,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      control,
                                            uint32_t      total_size)
{
    //JUST HERE!!
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

_FAST_FUNC_SRAM int32_t dma_channel_configure_wrapper (uint8_t      ch,
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
    if (total_size <= MAX_BLK_TS || total_size > DMA_MAX_LL_DATA) { // use single block or SW LLP
        size = total_size > MAX_BLK_TS ? MAX_BLK_TS : total_size;
        update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);

        ch_info->flags &= ~DMA_FLAG_HW_LLP;
        control &= ~DMA_CH_CTLL_LLP_EN_MASK;
    } else { // use HW LLP
        return -1; // return failure if use HW_LLP here...
/*
        ch_info->llp = &ll_items[0];
        dma_fill_ll_items(ch_info,
                        src_addr + (MAX_BLK_TS << ch_info->width_shift),
                        dst_addr + (MAX_BLK_TS << ch_info->width_shift),
                        control,
                        total_size - MAX_BLK_TS);
        size = MAX_BLK_TS;

        ch_info->flags |= DMA_FLAG_HW_LLP;
        control |= DMA_CH_CTLL_LLP_EN_MASK;
*/
    }

    // Trigger first DMA transfer
    return dma_channel_configure_internal(ch, en_int, src_addr, dst_addr, size, control, config_low, config_high);
}

#else  // !SUPPORT_HW_LLP

_FAST_FUNC_RO int32_t dma_channel_configure_wrapper (uint8_t      ch,
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
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;

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

/*
_FAST_FUNC_RO int32_t dma_channel_configure (uint8_t      ch,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      total_size,
                                            uint32_t      control,
                                            uint32_t      config_low,
                                            uint32_t      config_high,
                                            uint32_t      src_gath,
                                            uint32_t      dst_scat)
{
    return dma_channel_configure_wrapper(ch, 1, src_addr, dst_addr, total_size, control,
                                    config_low, config_high, src_gath, dst_scat);
}
*/


_FAST_FUNC_RO int32_t dma_channel_configure_polling (uint8_t      ch,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      total_size,
                                            uint32_t      control,
                                            uint32_t      config_low,
                                            uint32_t      config_high,
                                            uint32_t      src_gath,
                                            uint32_t      dst_scat)
{
#if SUPPORT_HW_LLP
    if (total_size > DMA_MAX_LL_DATA)
#else
    if (total_size > MAX_BLK_TS)
#endif
        return -1; //TODO: CSK_DRIVER_ERROR_PARAMETER;

    return dma_channel_configure_wrapper(ch, 0, src_addr, dst_addr, total_size, control,
                                    config_low, config_high, src_gath, dst_scat);
}

// Check the DMA channel has been configured for some peripheral as specified before and select if configured
// xfer_type    Memory to Peripheral (M2P) or Peripheral to Memory (P2M)
// hs_id        hardware handshaking interface # (SHOULD less than DMA_HSID_COUNT)
_FAST_FUNC_RO bool dma_channel_select_if_configured(uint8_t ch, uint8_t xfer_type, uint8_t hs_id)
{
    DMA_CHANNEL_REG * dma_ch;
    //uint32_t control, config_low, config_high;

    if (ch >= DMA_NUMBER_OF_CHANNELS || (channel_active & (0x1 << ch)) ||
        ((xfer_type != DMA_TT_M2P) && (xfer_type != DMA_TT_P2M)))
        return false;

    dma_ch = DMA_CHANNEL(ch);
    //control = dma_ch->CTL_LO;
    //config_low = dma_ch->CFG_LO;
    //config_high = dma_ch->CFG_HI;

    // check if transfer type meets requirement
    if ((dma_ch->CTL_LO & DMA_CH_CTLL_TTFC_MASK) >> DMA_CH_CTLL_TTFC_POS != xfer_type)
        return false;

    // check if handshaking interface # meets requirement
    if (xfer_type == DMA_TT_M2P) { // memory -> peripheral, hw handshake with DST
        if ((dma_ch->CFG_HI & DMA_CH_CFGH_DST_PER_MASK) >> DMA_CH_CFGH_DST_PER_POS == hs_id) {
            if (set_channel_active_flag(ch) == 0)
                return true;
        }
    } else { // peripheral -> memory, hw handshake with SRC
        if ((dma_ch->CFG_HI & DMA_CH_CFGH_SRC_PER_MASK) >> DMA_CH_CFGH_SRC_PER_POS == hs_id) {
            if (set_channel_active_flag(ch) == 0)
                return true;
        }
    }

    return false;
}

//BSD: Add dma_channel_configure_lite experimentally...
extern int32_t dma_channel_configure_lite (uint8_t      ch,
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
        return -1;

    ch_bit = 0x1U << ch;

    // return failure if channel is enabled or reserved flag is not set
    //if ((CSK_DMA->CH_EN & ch_bit) || !(channel_reserved & ch_bit))
    if (CSK_DMA->CH_EN & ch_bit)
        return -1;

    ch_info = &channel_info[ch];
    ch_info->SizeToXfer = total_size;
    ch_info->SizeXfered = 0;
    //ch_info->llp = 0;
    //ch_info->SrcAddr = 0;
    //ch_info->DstAddr = 0;
    //ch_info->CacheSyncStart = 0;
    //ch_info->CacheSyncBytes = 0;

    dma_ch = DMA_CHANNEL(ch);
    control = dma_ch->CTL_LO;
    if (total_size > MAX_BLK_TS) {
        size = MAX_BLK_TS;
        update_next_xfer_addr(ch_info, control, src_addr, dst_addr, size);
    } else {
        size = total_size;
    }

    // do cache sync operation before the mock BLOCK transfer
    if (ch_info->cache_sync != DMA_CACHE_SYNC_NOP)
        do_cache_sync(ch_info, control, src_addr, dst_addr, total_size);

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

    return 0;
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

_FAST_FUNC_RO int32_t dma_channel_configure_LLP_with_size (
                                       uint8_t      ch,
                                       DMA_LLP      llp,
                                       uint32_t     config_low,
                                       uint32_t     config_high,
                                       uint32_t     src_gath,
                                       uint32_t     dst_scat,
                                       uint32_t     total_size)
{
    uint32_t ch_bit, width, size;
    DMA_Channel_Info *ch_info;

    ch_bit = 0x1U << ch;
    ch_info = &channel_info[ch];

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;

    ch_info->llp = llp;

    ch_info->flags |= DMA_FLAG_HW_LLP;

    ch_info->SizeToXfer = total_size;

    DMA_CHANNEL_REG *dma_ch = DMA_CHANNEL(ch);
    dma_ch->LLP = (uint32_t)llp;
    dma_ch->CTL_LO = DMA_CH_CTLL_LLP_EN_MASK;
    dma_ch->CFG_LO = config_low;
    dma_ch->CFG_HI = config_high;

    dma_ch->DSR = ch_info->dst_scat;
    CSK_DMA->MASK.XFER = (ch_bit << 8) | ch_bit;
    CSK_DMA->MASK.ERROR = (ch_bit << 8) | ch_bit;

    CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

    return 0; // CSK_DRIVER_OK;
}

//FIXME: if HW LLP is used, DMA_CACHE_SYNC_DST may NOT take effect!!
_FAST_FUNC_RO int32_t dma_channel_configure_LLP (
                                       uint8_t      ch,
                                       DMA_LLP      llp,
                                       uint32_t     config_low,
                                       uint32_t     config_high,
                                       uint32_t     src_gath,
                                       uint32_t     dst_scat,
                                       uint32_t lli_count)
{
    // traverse all Linked List items
    DMA_LLP cur, next;

    // List Master Select, AHB layer/interface of memory device where LLI stores
    uint32_t ch_bit, lms, width, size;
    DMA_Channel_Info *ch_info;

    // SHOULD NOT set Auto Reload for Src/Dst
    if (llp == NULL || (config_low & DMA_CH_CFGL_RELOAD_MASK) != 0)
        return -1;

    // use "mock" BLOCK transfer (actually DMA transfer), SHOULD remove LLP_EN
//    llp->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

     lms = llp->LLP & 0x03;
   // LMS is hardcoded on ARCS?
//    if ( lms != DMAH_CH_LMS )
//        return -1;

    bool hw_llp = true; // assume HW LLP is supported

    //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
    cur = llp;
    // while (cur != NULL) {
    //     // Usually LMS bits SHOULD NOT change
    //     if (lms != (cur->LLP & 0x03))
    //         return -1;

    //     // check scatter/gather validity
    //     if (cur->CTL_LO & DMA_CH_CTLL_S_GATH_EN) {
    //         if (SG_COUNT(src_gath) == 0)
    //             return -1;
    //     }
    //     if (cur->CTL_LO & DMA_CH_CTLL_D_SCAT_EN) {
    //         if (SG_COUNT(dst_scat) == 0)
    //             return -1;
    //     }

    //     if (cur->u.SIZE > MAX_BLK_TS)
    //         //hw_llp = false;
    //         return -1;


    //     // Get next LLI (LLI address is 4bytes aligned)
    //     next = (DMA_LLP)(cur->LLP & ~0x3UL);

    //     // use "mock" BLOCK transfer (actually DMA transfer), SHOULD remove LLP_EN
    //     //cur->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

    //     cur = next; // next LLI
    // } //end while

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
//    ch_info->dst_wid_bytes = (1 << width); // dst width bytes

    width = (llp->CTL_LO & DMA_CH_CTLL_SRC_WIDTH_MASK) >> DMA_CH_CTLL_SRC_WIDTH_POS;
    if (width > DMA_WIDTH_MAX) {
        return -1;
        //width = DMA_WIDTH_MAX;
    }
    ch_info->width_shift = width; // src width shift
//    ch_info->width_bytes = (1 << width); // src width bytes

    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;
    ch_info->SizeXfered = 0;
//    ch_info->llp = (DMA_LLP)(llp->LLP);
    ch_info->llp = llp;
    ch_info->SrcAddr = 0;
    ch_info->DstAddr = 0;
#if HAS_CACHE_SYNC
    ch_info->CacheSyncStart = 0;
    ch_info->CacheSyncBytes = 0;
#endif

    //size = llp->u.SIZE > MAX_BLK_TS ? FIT_BLK_TS : llp->u.SIZE;
    size = llp->u.SIZE > MAX_BLK_TS ? MAX_BLK_TS : llp->u.SIZE;

/*    if (!hw_llp) { // SW LLP
        ch_info->flags &= ~DMA_FLAG_HW_LLP;
        ch_info->SizeToXfer = llp->u.SIZE;

        update_next_xfer_addr(ch_info, llp->CTL_LO, llp->SAR, llp->DAR, size);

        // do cache sync operation before the mock BLOCK transfer
        do_cache_sync(ch_info, llp->CTL_LO, llp->SAR, llp->DAR, llp->u.SIZE);

    } else { // HW LLP	*/
        ch_info->flags |= DMA_FLAG_HW_LLP;
        ch_info->SizeToXfer = 0;
        //cur = (DMA_LLP)(llp->LLP & ~0x3UL); // LLI address is 4bytes aligned
        cur = llp;
        ch_info->SizeToXfer += cur->u.SIZE*172;
        // while (cur != NULL) {
        //     ch_info->SizeToXfer += cur->u.SIZE;
        //     // do cache sync operation before the mock BLOCK transfer
        //     // do_cache_sync(ch_info, cur->CTL_LO, cur->SAR, cur->DAR, cur->u.SIZE);
        //     // Get next LLI (LLI address is 4bytes aligned)
        //     next = (DMA_LLP)(cur->LLP & ~0x3UL);
        //     // add LLP_EN if not last one
        //     if (next != NULL)   cur->CTL_LO |= DMA_CH_CTLL_LLP_EN_MASK;
		// 	else cur->CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;
        //     cur = next; // next LLI
        // } //end while

        //BSD0324
        DMA_CHANNEL_REG * dma_ch = DMA_CHANNEL(ch);
        dma_ch->LLP = (uint32_t)llp;
        dma_ch->CTL_LO = DMA_CH_CTLL_LLP_EN_MASK;
        dma_ch->CFG_LO = config_low;
        dma_ch->CFG_HI = config_high;
        dma_ch->SGR = src_gath;
        dma_ch->DSR = dst_scat;

        dma_ch->DSR = ch_info->dst_scat;

        enable_xfer_interrupts(ch_bit);
        enable_error_interrupts(ch_bit);

        // Enable DMA Channel to trigger data transfer
        CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;
        return 0; //CSK_DRIVER_OK;
/*    } */

/*
    // Disable DMA Channel
    //CSK_DMA->CH_EN = (ch_bit << 8);

    // Clear DMA interrupts
    clear_all_interrupts(ch_bit);

    // Trigger first DMA transfer
    return dma_channel_configure_internal(ch, 1, llp->SAR, llp->DAR, size, llp->CTL_LO,
                                          config_low, config_high);
*/
}

#else // !SUPPORT_HW_LLP

_FAST_FUNC_RO int32_t dma_channel_configure_LLP (
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
_FAST_FUNC_SRAM uint32_t dma_channel_get_count (uint8_t ch) {
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
    count += (DMA_CHANNEL(ch)->CTL_HI & DMA_CH_CTLH_BLOCK_TS_MASK);

    //if (int_en) enable_IRQ(IRQ_DMAC_VECTOR);
    if (gie) enable_GINT();

    return count;
}


/**
  \fn          void dma_irq_handler (void)
  \brief       DMA interrupt handler
*/
_FAST_FUNC_SRAM void dma_irq_handler (void) {
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

        // DMA transfer complete interrupt
        else if (CSK_DMA->STATUS.XFER & ch_bit) { // & CSK_DMA->MASK.XFER
            // Clear interrupt flag
            clear_xfer_interrupts(ch_bit);

            bool done = false;

        #if SUPPORT_HW_LLP
            if (ch_info->flags & DMA_FLAG_HW_LLP) { // HW LLP
                ch_info->SizeXfered = ch_info->SizeToXfer;
                ch_info->SizeToXfer = 0;
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
                #if HAS_CACHE_SYNC
                    // do post-dma invalidate operation if necessary
                    if (ch_info->CacheSyncBytes != 0) {
                        cache_dma_fast_inv_stage2(ch_info->CacheSyncStart,
                                ch_info->CacheSyncStart + ch_info->CacheSyncBytes);
                        ch_info->CacheSyncStart = 0;
                        ch_info->CacheSyncBytes = 0;
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

        // real BLOCK complete interrupt
        else if (CSK_DMA->STATUS.BLOCK & ch_bit) { // & CSK_DMA->MASK.BLOCK
            //TODO:
        }

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


/**
  \fn          int32_t dma_memcpy_SG ( uint8_t    ch,
                                uint32_t          src_addr,
                                uint32_t          dst_addr,
                                uint32_t          total_bytes,
                                uint32_t          src_gather,
                                uint32_t          dst_scatter,
                                uint8_t           src_width,
                                uint8_t           dst_width)
  \brief       Copy memory data *SCATTEREDLY* through some DMA channel.
  \param[in]   ch           The selected Channel number returned by dma_channel_select()
  \param[in]   src_addr     Source address
  \param[in]   dest_addr    Destination address
  \param[in]   total_bytes  The total bytes to be transfered.
  \param[in]   src_gather   Source Gather Register, including SG Interval & Count fields, see above.
  \param[in]   dst_scatter  Destination Scatter Register, including SG Interval & Count fields, see above.
  \param[in]   src_width    n in DMA_CH_CTLL_SRC_WIDTH(n), see DMA_WIDTH_XXX
  \param[in]   dst_width    n in DMA_CH_CTLL_DST_WIDTH(n), see DMA_WIDTH_XXX
*/
int32_t dma_memcpy_SG (uint8_t        ch,
                                  uint32_t          src_addr,
                                  uint32_t          dst_addr,
                                  uint32_t          total_bytes,
                                  uint32_t          src_gath,
                                  uint32_t          dst_scat,
                                  uint8_t           src_width, // see DMA_WIDTH_XXX
                                  uint8_t           dst_width) // see DMA_WIDTH_XXX
{
    uint32_t ch_bit, sw_bytes, dw_bytes, src_bsize, dst_bsize;
    uint32_t size, control, config_low, config_high;

    // return failure if channel is enabled or active flag is not set (indicates NOT selected before)
    ch_bit = 0x1U << ch;
    if ((CSK_DMA->CH_EN & ch_bit) || !(channel_active & ch_bit))
        return -1;

    // max value of src_width / dst_width is greater than DMA_WIDTH_MAX
    if (src_width > DMA_WIDTH_MAX || dst_width > DMA_WIDTH_MAX)
        return -1;

    sw_bytes = 1 << src_width;
    dw_bytes = 1 << dst_width;

    // src_addr / dst_addr address should be aligned to src_width / dst_width
    if ( (src_addr & (sw_bytes - 1)) || (dst_addr & (dw_bytes - 1)) )
        return -1;

    // total_bytes should be aligned to src_width & dst_width
    if ( (total_bytes & (sw_bytes - 1)) || (total_bytes & (dw_bytes - 1)) )
        return -1;

    DMA_Channel_Info *ch_info = &channel_info[ch];
    ch_info->width_shift = src_width;
    ch_info->dst_wid_shift = dst_width;
    ch_info->SizeToXfer = (total_bytes >> 2);
    ch_info->src_gath = src_gath;
    ch_info->dst_scat = dst_scat;

    src_bsize = calc_max_burst_size(DMA_CHANNELS_FIFO_DEPTH[ch] / sw_bytes);
    dst_bsize = calc_max_burst_size(DMA_CHANNELS_FIFO_DEPTH[ch] / dw_bytes);

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    if (src_gath != 0)
        control |= DMA_CH_CTLL_S_GATH_EN;
    if (dst_scat != 0)
        control |= DMA_CH_CTLL_D_SCAT_EN;

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
    return ret;
}
