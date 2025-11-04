/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */

#ifndef __DMA_ARCS_H
#define __DMA_ARCS_H

#include <stdint.h>
#include <stdbool.h>

#include "arcs_ap.h"


// max supported channel count, and actual count may be less (see DMA_NUMBER_OF_CHANNELS)
#define DMA_MAX_NR_CHANNELS     8

// any channel of DMA controller
#define DMA_CHANNEL_ANY                 ((uint8_t)0xFF)

#define MAX_BLK_BITS    20 // 19 //FIXME: MUST confirm it !!

//NOTE: original BLOCK_TS 12bits => 19/20bits @CTLx bit[50/51:32], 512K/1M-1
#define MAX_BLK_TS      ((1 << MAX_BLK_BITS) - 1)

// HW LLP (Linked list multi-block) is supported on ARCS...
#define SUPPORT_HW_LLP  1 //TODO: change to 1 since Linked list is supported!

#if SUPPORT_HW_LLP
// max count of Link List Item implicitly supported by DMA driver
// it means max. (MAX_BLK_TS * DMA_MAX_LL_ITEMS) data can be transfered for 1 DMA interrupt.
#define DMA_MAX_LL_ITEMS                4
//#define DMA_MAX_LL_DATA                 ((DMA_MAX_LL_ITEMS + 1) * MAX_BLK_TS)
#define DMA_MAX_LL_DATA                 (DMA_MAX_LL_ITEMS * MAX_BLK_TS)
#endif // SUPPORT_HW_LLP

extern uint32_t calc_max_burst_size(uint32_t items);

#define WIDTH_BYTES(dma_width)      (1 << (dma_width))
#define ITEMS_FROM_BSIZE(bsize)     ((bsize) == 0 ? 1 : (2 << (bsize)))
#define ITEMS_TO_BSIZE(items)       (calc_max_burst_size(items) & 0xFF)

// burst size of Source / Destination DMA transfer. (NO DMA_BSIZE_2!!)
#define DMA_BSIZE_1                     (0)  // Burst size = 1
#define DMA_BSIZE_4                     (1)  // Burst size = 4
#define DMA_BSIZE_8                     (2)  // Burst size = 8
#define DMA_BSIZE_16                    (3)  // Burst size = 16
#define DMA_BSIZE_32                    (4)  // Burst size = 32
#define DMA_BSIZE_64                    (5)  // Burst size = 64
#define DMA_BSIZE_128                   (6)  // Burst size = 128
#define DMA_BSIZE_256                   (7)  // Burst size = 256

// Width (in byte) of Source / Destination DMA transfer
#define DMA_WIDTH_BYTE                  (0)  // Width = 1 byte (8 bits)
#define DMA_WIDTH_HALFWORD              (1)  // Width = 2 bytes (16 bits)
#define DMA_WIDTH_WORD                  (2)  // Width = 4 bytes (32 bits)
#define DMA_WIDTH_MAX                   (2)  // DMAH_Mx_HDATA_WIDTH = 32bits
//#define DMA_WIDTH_DOUBWORD              (3)  // Width = 8 bytes (64 bits)
//#define DMA_WIDTH_QUADWORD              (4)  // Width = 16 bytes (128 bits)
//#define DMA_WIDTH_OCTUWORD              (5)  // Width = 32 bytes (256 bits)

// DMA Channel Control register (Low 32bits) definition
#define DMA_CH_CTLL_INT_EN          (1 << 0)    // Interrupt Enable Bit
#define DMA_CH_CTLL_DST_WIDTH_POS   (1)         // Destination Transfer Width bit start index
#define DMA_CH_CTLL_DST_WIDTH_MASK  (0x7 << 1)  // Destination Transfer Width mask
#define DMA_CH_CTLL_DST_WIDTH(n)    ((n) << 1)  // Destination Transfer Width
#define DMA_CH_CTLL_SRC_WIDTH_POS   (4)         // Source Transfer Width bit start index
#define DMA_CH_CTLL_SRC_WIDTH_MASK  (0x7 << 4)  // Source Transfer Width mask
#define DMA_CH_CTLL_SRC_WIDTH(n)    ((n) << 4)  // Source Transfer Width
#define DMA_CH_CTLL_DSTADDRCTL_MASK (0x3 << 7)  // Destination Address Control mask
#define DMA_CH_CTLL_DST_INC     (0 << 7)    // Destination Address Increment @ bit[8:7]
#define DMA_CH_CTLL_DST_DEC     (1 << 7)    // 0 = Increment, 1 = Decrement,
#define DMA_CH_CTLL_DST_FIX     (2 << 7)    // 2,3 = No change
#define DMA_CH_CTLL_SRCADDRCTL_MASK (0x3 << 9)  // Source Address Control mask
#define DMA_CH_CTLL_SRC_INC     (0 << 9)    // Source Address Increment @ bit[10:9] !ERROR on Linux4.4!
#define DMA_CH_CTLL_SRC_DEC     (1 << 9)    // 0 = Increment, 1 = Decrement,
#define DMA_CH_CTLL_SRC_FIX     (2 << 9)    // 2,3 = No change
#define DMA_CH_CTLL_DST_BSIZE_POS   (11)    // Destination Burst Transaction Length bit start index
#define DMA_CH_CTLL_DST_BSIZE_MASK  (0x7 << 11) // Destination Burst Transaction Length mask
#define DMA_CH_CTLL_DST_BSIZE(n)    ((n) << 11) // Destination Burst Transaction Length @ bit[13:11]
#define DMA_CH_CTLL_SRC_BSIZE_POS   (14)        // Source Burst Transaction Length bit start index
#define DMA_CH_CTLL_SRC_BSIZE_MASK  (0x7 << 14) // Source Burst Transaction Length mask
#define DMA_CH_CTLL_SRC_BSIZE(n)    ((n) << 14) // Source Burst Transaction Length @ bit[16:14]
#define DMA_CH_CTLL_S_GATH_EN   (1 << 17)   // Source gather enable bit
#define DMA_CH_CTLL_D_SCAT_EN   (1 << 18)   // Destination scatter enable bit
#define DMA_CH_CTLL_TTFC_POS    (20)        // Transfer Type and Flow Control bit index
#define DMA_CH_CTLL_TTFC_MASK   (0x7 << 20) // Transfer Type and Flow Control mask
#define DMA_CH_CTLL_TTFC(n)     ((n) << 20) // Transfer Type and Flow Control @ bit [22:20]
#define DMA_CH_CTLL_TTFC_M2M    (0 << 20)   // Memory to Memory (DMAC as Flow Controller)
#define DMA_CH_CTLL_TTFC_M2P    (1 << 20)   // Memory to Peripheral (DMAC as Flow Controller)
#define DMA_CH_CTLL_TTFC_P2M    (2 << 20)   // Peripheral to Memory (DMAC as Flow Controller)
#define DMA_CH_CTLL_TTFC_P2P    (3 << 20)   // Peripheral to Peripheral (DMAC as Flow Controller)
#define DMA_CH_CTLL_DMS_MASK    (0x3 << 23) // Destination Master Select mask
#define DMA_CH_CTLL_DMS(n)      ((n) << 23) // Destination Master Select @ bit[24:23]
#define DMA_CH_CTLL_SMS_MASK    (0x3 << 25) // Source Master Select mask
#define DMA_CH_CTLL_SMS(n)      ((n) << 25) // Source Master Select @ bit[26:25]
#define DMA_CH_CTLL_LLP_D_EN    (1 << 27)   // dst block chain enable bit
#define DMA_CH_CTLL_LLP_S_EN    (1 << 28)   // src block chain enable bit
#define DMA_CH_CTLL_LLP_EN_MASK     (DMA_CH_CTLL_LLP_D_EN | DMA_CH_CTLL_LLP_S_EN)

// DMA Channel Control register (High 32bits) definition
#define DMA_CH_CTLH_DONE        (0x1UL << MAX_BLK_BITS)   // (block transfer) Done bit, set by HW, cleared by SW
#define DMA_CH_CTLH_BLOCK_TS_MASK   (DMA_CH_CTLH_DONE - 1) // HIGH_WORD(CTLx) can be treated as "Block Transfer Size" register
                                            // (only low N bits, actually max (1<< N)-1 data items!)

// DMA Channel Configuration register (Low 32bits) definition
#define DMA_CH_CFGL_CH_PRIOR_MASK   (0x7 << 5)  // channel priority mask
#define DMA_CH_CFGL_CH_PRIOR(x)     ((x) << 5)  // channel priority, lowest 0
#define DMA_CH_CFGL_CH_SUSP         (1 << 8)    // suspend transfer
#define DMA_CH_CFGL_FIFO_EMPTY      (1 << 9)    // [RO] data left in channel FIFO?
#define DMA_CH_CFGL_HS_HW_DST       (0 << 10)   // handshake w/dst, 0=hw
#define DMA_CH_CFGL_HS_HW_SRC       (0 << 11)   // handshake w/src, 0=hw
#define DMA_CH_CFGL_HS_SW_DST       (1 << 10)   // handshake w/dst, 1=sw
#define DMA_CH_CFGL_HS_SW_SRC       (1 << 11)   // handshake w/src, 1=sw
#define DMA_CH_CFGL_LOCK_CH_XFER    (0 << 12)   // Channel Lock Level @ bit[13:12]
#define DMA_CH_CFGL_LOCK_CH_BLOCK   (1 << 12)   // 0 = DMA transfer, 1 = DMA block transfer,
#define DMA_CH_CFGL_LOCK_CH_XACT    (2 << 12)   // 2,3 = DMA transaction
#define DMA_CH_CFGL_LOCK_BUS_XFER   (0 << 14)   // Bus Lock Level @ bit[15:14]
#define DMA_CH_CFGL_LOCK_BUS_BLOCK  (1 << 14)   // 0 = DMA transfer, 1 = DMA block transfer,
#define DMA_CH_CFGL_LOCK_BUS_XACT   (2 << 14)   // 2,3 = DMA transaction
#define DMA_CH_CFGL_LOCK_CH         (1 << 15)   // Channel Lock Bit
#define DMA_CH_CFGL_LOCK_BUS        (1 << 16)   // Bus Lock Bit
#define DMA_CH_CFGL_HS_DST_POL      (1 << 18)   // dst handshake, 1 = Active low
#define DMA_CH_CFGL_HS_SRC_POL      (1 << 19)   // src handshake, 1 = Active low
#define DMA_CH_CFGL_MAX_BURST_MASK  (0x3FF << 20) // Max AMBA Burst Length mask
#define DMA_CH_CFGL_MAX_BURST(x)    ((x) << 20) // Max AMBA Burst Length @ bit[29:20]
#define DMA_CH_CFGL_RELOAD_SAR      (1 << 30)   // Automatic Source Reload
#define DMA_CH_CFGL_RELOAD_DAR      (1 << 31)   // Automatic Destination Reload
#define DMA_CH_CFGL_RELOAD_MASK     (DMA_CH_CFGL_RELOAD_SAR | DMA_CH_CFGL_RELOAD_DAR)

// DMA Channel Configuration register (High 32bits) definition
#define DMA_CH_CFGH_FCMODE          (1 << 0)    // Flow Control Mode, default 0
#define DMA_CH_CFGH_FIFO_MODE       (1 << 1)    // FIFO Mode Select, default 0
#define DMA_CH_CFGH_PROTCTL_MASK    (0x7 << 2)  // Protection Control mask
#define DMA_CH_CFGH_PROTCTL(x)      ((x) << 2)  // Protection Control, mapped to HPROT[3:1], default 1
#define DMA_CH_CFGH_DS_UPD_EN       (1 << 5)    // Destination Status Update Enable, default disabled(0)
#define DMA_CH_CFGH_SS_UPD_EN       (1 << 6)    // Source Status Update Enable, default disabled(0)
#define DMA_CH_CFGH_SRC_PER_POS     (7)         // hardware handshaking interface # bit start index for source
#define DMA_CH_CFGH_SRC_PER_MASK    (0xF << 7)  // hardware handshaking interface # mask for source
#define DMA_CH_CFGH_SRC_PER(x)      ((x) << 7)  // hardware handshaking interface # of source peripheral
#define DMA_CH_CFGH_DST_PER_POS     (11)        // hardware handshaking interface # bit start index for destination
#define DMA_CH_CFGH_DST_PER_MASK    (0xF << 11) // hardware handshaking interface # mask for destination
#define DMA_CH_CFGH_DST_PER(x)      ((x) << 11) // hardware handshaking interface # of destination peripheral

// For DWORD (64bit) register, low WORD(32bit) is valid and high WORD(32bit) is not used.
#define DWORD_REG(name)     uint32_t name; uint32_t __pad_##name

//  Link list item type, same as LLI registers
/*
typedef struct _DMA_LINK_LIST_ITEM {
    DWORD_REG(SAR);     // Source Address Register
    DWORD_REG(DAR);     // Destination Address Register
    DWORD_REG(LLP);     // Linked List Pointer
    uint32_t CTL_LO;    // Control Register Low WORD
    union {
    uint32_t CTL_HI;    // Control Register High WORD (Transfer Size @ bit[11:0])
    uint32_t SIZE;      // Block Transfer Size, not limited by 4095!
    } u;
    DWORD_REG(SSTAT);   // Source Status Register, unimplemented, set to 0
    DWORD_REG(DSTAT);   // Destination Status Register, unimplemented, set to 0
} DMA_LLI, *DMA_LLP;
*/

typedef struct _DMA_LINK_LIST_ITEM {
    uint32_t SAR;     // Source Address Register
    uint32_t DAR;     // Destination Address Register
    uint32_t LLP;     // Linked List Pointer
    uint32_t CTL_LO;    // Control Register Low WORD
    union {
    uint32_t CTL_HI;    // Control Register High WORD (Transfer Size @ bit[11:0])
    uint32_t SIZE;      // Block Transfer Size, not limited by 4095!
    } u;
    //uint32_t SSTAT;   // Source Status Register, unimplemented, set to 0
    uint32_t DSTAT;   // Destination Status Register, unimplemented, set to 0
} DMA_LLI, *DMA_LLP;

// Source Gather / Destination Scatter register definition
// Interval & Count are both in units of SRC_TR_WIDTH bits.
#define SG_INTERVAL_POS     (0)             // Interval @ bit[19:0]
#define SG_INTERVAL_MASK    (0xFFFFF << 0)  // Interval mask
#define SG_COUNT_POS        (20)            // Count @ bit[31:20]
#define SG_COUNT_MASK       (0xFFF << 20)   // Count mask

#define SG_COUNT(n)         ((n >> SG_COUNT_POS) & 0xFFF)
#define SG_INTERVAL(n)      ((n >> SG_INTERVAL_POS) & 0xFFFFF)


// DMA Event Type, used by DMA callback DMA_SignalEvent_t (see below)
// a DMA transfer (single-block or multi-block) is completed
#define DMA_EVENT_TRANSFER_COMPLETE     (1)
// one block of multi-block is completed
#define DMA_EVENT_BLOCK_COMPLETE        (2)
// an ERROR is found during a DMA transfer
#define DMA_EVENT_ERROR                 (4)

/**
  \fn          void DMA_SignalEvent_t (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param)
  \brief       Signal DMA Events.
  \param[in]   event_info   event and channel information
               bit[7:0] is event type, bit[15:8] is DMA channel number
  \param[in]   xfer_bytes   total bytes of transfered data
  \param[in]   usr_param    user parameter specified in dma_channel_select()
  \return      none
*/
typedef void (*DMA_SignalEvent_t) (uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param);

// BSD: cache coherence or cache sync operation for user buffer before DMA transfer
typedef enum _DMA_CACHE_SYNC {
    DMA_CACHE_SYNC_NOP = 0, // no cache sync operation, leave it to caller
    DMA_CACHE_SYNC_SRC = 1, // do cache sync for source buffer
    DMA_CACHE_SYNC_DST = 2, // do cache sync for destination buffer
    DMA_CACHE_SYNC_BOTH = 3, // do cache sync for both source and destination buffer
    DMA_CACHE_SYNC_AUTO = 4, // do cache sync for source and/or destination automatically by other parameters
    DMA_CACHE_SYNC_COUNT
} DMA_CACHE_SYNC;


/**
  \fn          int32_t dma_initialize (void)
  \brief       Initialize DMA peripheral
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t dma_initialize (void);


/**
  \fn          int32_t dma_uninitialize (void)
  \brief       De-initialize DMA peripheral
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t dma_uninitialize (void);


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
extern uint8_t dma_channel_select(uint8_t *pch,
                                DMA_SignalEvent_t  cb_event,
                                uint32_t           usr_param,
                                DMA_CACHE_SYNC     cache_sync);


/**
   \fn          int32_t dma_channel_reserve (uint8_t      ch,
                                        DMA_SignalEvent_t  cb_event,
                                        uint32_t           usr_param,
                                        DMA_CACHE_SYNC     cache_sync)
  \brief        Reserve specified channel for exclusive use
                before calling dma_channel_configure or dma_channel_configure_LLP.
                The channel will NOT be released when DMA operation is completed or channel is disabled,
                So this API function is NOT RECOMMENDED to use if NOT necessary!!
  \param[in]   ch          specified Channel number
  \param[in]   cb_event     Channel callback pointer
  \param[in]   usr_param    user-defined value, acts as last parameter of cb_event
  \param[in]   cache_sync   cache coherence or sync policy for source/destination RAM buffer
                            before DMA operation. see the definition of DMA_CACHE_SYNC.
  \returns
   - \b  the reserved DMA channel number if successful, or DMA_CHANNEL_ANY (0xFF) if failed.
 */
extern uint8_t dma_channel_reserve(uint8_t ch,
                                  DMA_SignalEvent_t  cb_event,
                                  uint32_t           usr_param,
                                  DMA_CACHE_SYNC     cache_sync);


/**
   \fn          int32_t dma_channel_unreserve (uint8_t      ch)
  \brief        Unreserve (or Release) specified channel exclusively used by some device.
  \param[in]   ch          specified dedicated Channel number
  \returns
   - \b  no return value.
 */
extern void dma_channel_unreserve(uint8_t ch);


/**
   \fn          int32_t dma_channel_is_reserved (uint8_t      ch)
  \brief        Check whether specified channel is reserved (exclusively used by some device) or not.
  \param[in]   ch          specified dedicated Channel number
  \returns
   - \b  true indicates reserved, false indicates NOT reserved yet.
 */
extern bool dma_channel_is_reserved(uint8_t ch);


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
extern int32_t dma_channel_configure_wrapper (uint8_t      ch,
                                            uint8_t       en_int,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      total_size,
                                            uint32_t      control,
                                            uint32_t      config_low,
                                            uint32_t      config_high,
                                            uint32_t      src_gath,
                                            uint32_t      dst_scat);


/**
  \fn          int32_t dma_channel_configure (uint8_t      ch,
                                    uint32_t        src_addr,
                                    uint32_t        dst_addr,
                                    uint32_t        total_size,
                                    uint32_t        control,
                                    uint32_t        config_low,
                                    uint32_t        config_high,
                                    uint32_t        src_gath,
                                    uint32_t        dst_scat);
*/
#define dma_channel_configure(ch, ...)  \
    dma_channel_configure_wrapper(ch, 1, ##__VA_ARGS__)


/**
  \fn          int32_t dma_channel_configure_polling (uint8_t      ch,
                                    uint32_t        src_addr,
                                    uint32_t        dst_addr,
                                    uint32_t        total_size,
                                    uint32_t        control,
                                    uint32_t        config_low,
                                    uint32_t        config_high,
                                    uint32_t        src_gath,
                                    uint32_t        dst_scat);
*/
//#define dma_channel_configure_polling(ch, ...)  \
//    dma_channel_configure_wrapper(ch, 0, ##__VA_ARGS__)

extern int32_t dma_channel_configure_polling (uint8_t      ch,
                                            uint32_t      src_addr,
                                            uint32_t      dst_addr,
                                            uint32_t      total_size,
                                            uint32_t      control,
                                            uint32_t      config_low,
                                            uint32_t      config_high,
                                            uint32_t      src_gath,
                                            uint32_t      dst_scat);


#define DMA_HSID_COUNT  16
typedef enum {
    DMA_TT_M2M = 0, // DMA_CH_CTLL_TTFC_M2M
    DMA_TT_M2P,     // DMA_CH_CTLL_TTFC_M2P
    DMA_TT_P2M,     // DMA_CH_CTLL_TTFC_P2M
    DMA_TT_COUNT
} DMA_XFER_TYPE;

// Check the DMA channel has been configured for some peripheral as specified before and select if configured
// xfer_type    Memory to Peripheral (M2P) or Peripheral to Memory (P2M)
// hs_id        hardware handshaking interface # (SHOULD less than DMA_HSID_COUNT)
extern bool dma_channel_select_if_configured(uint8_t ch, uint8_t xfer_type, uint8_t hs_id);

// ONLY used for "RESERVED" or unchanged DMA channel!!
// NOTE: dma_channel_configure OR dma_channel_configure_polling is invoked
//  for the initial configuration of DMA channel, and dma_channel_configure_lite
//  may be invoked for later configurations for the sake of efficiency...
//
// The parameter cfg_flags is used to specified which items should be updated:
#define DMACH_CFG_FLAG_SRC_ADDR     0x1 // bit[0]
#define DMACH_CFG_FLAG_DST_ADDR     0x2 // bit[1]
#define DMACH_CFG_FLAG_BOTH_ADDR    0x3 // bit[1:0]

extern int32_t dma_channel_configure_lite (uint8_t      ch,
                                           uint8_t      cfg_flags,
                                           uint32_t     src_addr,
                                           uint32_t     dst_addr,
                                           uint32_t     total_size);

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
extern int32_t dma_channel_configure_LLP (
                                       uint8_t      ch,
                                       DMA_LLP      llp,
                                       uint32_t     config_low,
                                       uint32_t     config_high,
                                       uint32_t     src_gath,
                                       uint32_t     dst_scat,
                                       uint32_t lli_count);


/**
  \fn          int32_t dma_channel_suspend (uint8_t ch, uint8_t wait_done)
  \brief       Suspend channel transfer (and can be resumed later)
  \param[in]   ch   Channel number
  \param[in]   wait_done    Whether to wait for suspend operation is done
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t dma_channel_suspend (uint8_t ch, uint8_t wait_done);


/**
  \fn          int32_t dma_channel_resume (uint8_t ch)
  \brief       Resume channel transfer (ever suspended before)
  \param[in]   ch           Channel number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t dma_channel_resume (uint8_t ch);


/**
  \fn          int32_t dma_channel_enable (uint8_t ch)
  \brief       Enable DMA channel (and Resume transfer if suspended)
  \param[in]   ch   Channel number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t dma_channel_enable (uint8_t ch);


/**
  \fn          int32_t dma_channel_disable (uint8_t ch, uint8_t wait_done)
  \brief       Abort transfer and then Disable DMA channel
  \param[in]   ch           Channel number
  \param[in]   wait_done    Whether to wait for disable operation is done
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t dma_channel_disable (uint8_t ch, uint8_t wait_done);


extern bool dma_channel_is_enabled(uint8_t ch);
extern bool dma_channel_is_polling(uint8_t ch);

extern bool dma_channel_xfer_error(uint8_t ch);
extern bool dma_channel_xfer_complete(uint8_t ch);
extern void dma_channel_clear_xfer_status(uint8_t ch);

/**
  \fn          uint32_t dma_channel_get_status (uint8_t ch)
  \brief       Check if DMA channel is enabled or disabled [discarded]
  \param[in]   ch Channel number
  \returns     Channel status
   - \b  1: channel enabled
   - \b  0: channel disabled
*/
extern uint32_t dma_channel_get_status (uint8_t ch);

/**
  \fn          uint32_t dma_channel_get_count (uint8_t ch)
  \brief       Get number of transferred data items
  \param[in]   ch Channel number
  \returns     Number of transferred data items
*/
extern uint32_t dma_channel_get_count (uint8_t ch);


/**
  \fn          uint32_t dma_channel_abort (uint8_t ch, uint8_t wait_done)
  \brief       Abort transfer on the channel
  \param[in]   ch         Channel number
  \param[in]   wait_done  Whether to wait for disable operation is done
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
//extern int32_t dma_channel_abort (uint8_t ch, uint8_t wait_done);


/*
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
extern int32_t dma_memcpy ( uint8_t            ch,
                            uint32_t           src_addr,
                            uint32_t           dst_addr,
                            uint32_t           total_bytes);



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
extern  int32_t dma_memcpy_SG ( uint8_t       ch,
                            uint32_t          src_addr,
                            uint32_t          dst_addr,
                            uint32_t          total_bytes,
                            uint32_t          src_gath,
                            uint32_t          dst_scat,
                            uint8_t           src_width,
                            uint8_t           dst_width);

int32_t dma_channel_configure_LLP_with_size (
                                       uint8_t      ch,
                                       DMA_LLP      llp,
                                       uint32_t     config_low,
                                       uint32_t     config_high,
                                       uint32_t     src_gath,
                                       uint32_t     dst_scat,
                                       uint32_t     total_size);

#endif /* __DMA_ARCS_H */
