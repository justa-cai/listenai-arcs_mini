/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *  Ported from MARS to VENUSA: Mar. 20, 2025
 *
 * @file dma.h
 * @brief DMA controller driver header file for Venusa platform
 *        This file contains all necessary definitions and interfaces
 *        for controlling the Direct Memory Access (DMA) peripheral.
 */

#ifndef __DMA_VENUSA_H
#define __DMA_VENUSA_H

#include <stdint.h>
#include <stdbool.h>

#include "venusa_ap.h"

/** @defgroup DMA
  * @brief DMA HAL module driver
  * @{
  */

/** @defgroup DMA_Exported_Macros DMA Exported Macros
  * @{
  */
/**
 * @brief Maximum supported number of DMA channels (hardware limit)
 */
#define DMA_MAX_NR_CHANNELS             ((uint8_t)8)

/**
 * @brief Actual available number of DMA channels in current configuration
 */
#define DMA_NUMBER_OF_CHANNELS          ((uint8_t) 6)

/**
 * @brief Special channel number indicating any available channel
 */
#define DMA_CHANNEL_ANY                 ((uint8_t)0xFF)

/**
 * @brief Block transfer size bit width (maximum 20 bits)
 *        Allows block sizes up to (1<<20)-1 = 1MB-1 items
 */
#define MAX_BLK_BITS    20

/**
 * @brief Maximum allowed block transfer size value
 */
#define MAX_BLK_TS      ((1 << MAX_BLK_BITS) - 1)

/**
 * @brief Bitmask for valid block transfer size values
 */
#define BLK_TS_MASK     ((1 << MAX_BLK_BITS) - 1)

/**
 * @brief Hardware Linked List Processing (LLP) support flag
 *        Set to 1 when HW LLP is implemented
 */
#define SUPPORT_HW_LLP          1 // 0

/**
 * @brief Use internal linked list items storage flag
 *        Set to 1 when using internal LLI storage mechanism
 */
#define USE_INTERNAL_LLITEMS    1 // 0

extern uint32_t calc_max_burst_size(uint32_t items);

/**
 * @brief Calculate byte width based on DMA width setting
 * @param dma_width DMA width parameter (see #DMA_WIDTH_* definitions)
 * @return Number of bytes per transfer unit
 */
#define WIDTH_BYTES(dma_width)      (1 << (dma_width))

/**
 * @brief Convert burst size to number of items
 * @param bsize Burst size code (see #DMA_BSIZE_* definitions)
 * @return Number of items per burst transaction
 */
#define ITEMS_FROM_BSIZE(bsize)     ((bsize) == 0 ? 1 : (2 << (bsize)))

/**
 * @brief Convert number of items to burst size code
 * @param items Number of items per burst
 * @return Corresponding burst size code
 */
#define ITEMS_TO_BSIZE(items)       (calc_max_burst_size(items) & 0xFF)

// Burst size configurations (must match hardware capabilities)
/** @{ */
#define DMA_BSIZE_1                     (0)  // Burst size = 1 item
#define DMA_BSIZE_4                     (1)  // Burst size = 4 items
#define DMA_BSIZE_8                     (2)  // Burst size = 8 items
#define DMA_BSIZE_16                    (3)  // Burst size = 16 items
#define DMA_BSIZE_32                    (4)  // Burst size = 32 items
#define DMA_BSIZE_64                    (5)  // Burst size = 64 items
#define DMA_BSIZE_128                   (6)  // Burst size = 128 items
#define DMA_BSIZE_256                   (7)  // Burst size = 256 items
/** @} */

// Data width configurations (in bytes)
/** @{ */
#define DMA_WIDTH_BYTE                  (0)  // Width = 1 byte (8 bits)
#define DMA_WIDTH_HALFWORD              (1)  // Width = 2 bytes (16 bits)
#define DMA_WIDTH_WORD                  (2)  // Width = 4 bytes (32 bits)
#define DMA_WIDTH_MAX                   (2)  // Maximum supported width (32 bits)
/** @} */

// AHB master interface of memory that stores LLI (Linked List Item) for channel
#define DMAH_CH_LMS     0

// For DWORD (64bit) register, low WORD(32bit) is valid and high WORD(32bit) is not used.
#define DWORD_REG(name)     uint32_t name; uint32_t __pad_##name

//default or preferred DMA channel definition
#define DMA_CH_UART_TX_DEF          ((uint8_t) 2)   // UART transmit
#define DMA_CH_UART_RX_DEF          ((uint8_t) 3)   // UART receive
#define DMA_CH_SPI_TX_DEF           ((uint8_t) 4)   // SPI transmit
#define DMA_CH_SPI_RX_DEF           ((uint8_t) 5)   // SPI receive

#define DMA_HSID_COUNT  16
/**
  * @}
  */ /* End of group DMA_Exported_Macros */

/** @defgroup DMA_Exported_Constants DMA Exported Constants
  * @{
  */

// DMA Channel Control Register (Low 32bits) Field Definitions
/** @{ */
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
/** @} */

// DMA Channel Control Register (High 32bits) Field Definitions
/** @{ */
#define DMA_CH_CTLH_DONE        (0x1UL << MAX_BLK_BITS)   // (block transfer) Done bit, set by HW, cleared by SW
#define DMA_CH_CTLH_BLOCK_TS_MASK   (DMA_CH_CTLH_DONE - 1) // HIGH_WORD(CTLx) can be treated as "Block Transfer Size" register
                                            // (only low N bits, actually max (1<< N)-1 data items!)
/** @} */

// DMA Channel Configuration Register (Low 32bits) Field Definitions
/** @{ */
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
/** @} */

// DMA Channel Configuration Register (High 32bits) Field Definitions
/** @{ */
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
/** @} */

// Source Gather / Destination Scatter register definition
/** @{ */
#define SG_INTERVAL_POS     (0)             // Interval @ bit[19:0]
#define SG_INTERVAL_MASK    (0xFFFFF << 0)  // Interval mask
#define SG_COUNT_POS        (20)            // Count @ bit[31:20]
#define SG_COUNT_MASK       (0xFFF << 20)   // Count mask

#define SG_COUNT(n)         ((n >> SG_COUNT_POS) & 0xFFF)
#define SG_INTERVAL(n)      ((n >> SG_INTERVAL_POS) & 0xFFFFF)

#define PIPO_BLK_FLAG_STOP  (0x1 << 0)

/** @} */
/**
  * @}
  */ /* End of group DMA_Exported_Constants */

/** @defgroup DMA_Exported_Types DMA Exported Types
  * @{
  */
/**
 * @brief Linked List Item structure matching hardware register layout
 *        Contains source/destination addresses, control registers, and link pointers
 */
typedef struct _DMA_LINK_LIST_ITEM {
    uint32_t SAR;     ///< Source Address Register
    uint32_t DAR;     ///< Destination Address Register
    uint32_t LLP;     ///< Linked List Pointer (next item address)
    uint32_t CTL_LO;    ///< Control Register Low WORD
    union {
        uint32_t CTL_HI;    ///< Control Register High WORD (Transfer Size @ bit[x:0])
        uint32_t SIZE;      ///< Block Transfer Size (unlimited by 4095)
    } u;
    uint32_t SSTA;    ///< Source Status Register
    uint32_t DSTA;    ///< Destination Status Register
    struct _DMA_LINK_LIST_ITEM *preLLP; // point to previous Linked List Item (software only)
} DMA_LLI, *DMA_LLP;

// DMA Event Type, used by DMA callback DMA_SignalEvent_t (see below)
/** @{ */
enum {
    DMA_EVENT_TRANSFER_COMPLETE = 1, ///< Single/multi-block transfer completed
    DMA_EVENT_BLOCK_COMPLETE       = 2, ///< One block of multi-block completed
    DMA_EVENT_ERROR                = 4  ///< Error detected during transfer
};
/** @} */

/**
 * @brief DMA event callback function type
 * @param[in] event_info   Combined event type (lower 8 bits) and channel number (upper 8 bits)
 * @param[in] xfer_bytes   Total bytes transferred in this event
 * @param[in] usr_param    User-defined parameter passed through dma_channel_select()
 */
typedef void (*DMA_SignalEvent_t) (uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param);

/* cache coherence or cache sync operation for user buffer before DMA transfer */

typedef enum _DMA_CACHE_SYNC {
    DMA_CACHE_SYNC_NOP = 0,     ///< No cache sync operation
    DMA_CACHE_SYNC_SRC = 1,     ///< Sync source buffer cache
    DMA_CACHE_SYNC_DST = 2,     ///< Sync destination buffer cache
    DMA_CACHE_SYNC_BOTH = 3,    ///< Sync both source and destination caches
    DMA_CACHE_SYNC_AUTO = 4,    ///< Auto-sync based on other parameters
    DMA_CACHE_SYNC_COUNT        ///< Number of cache sync options
} DMA_CACHE_SYNC;

typedef enum {
    DMA_TT_M2M = 0, // Memory to Memory transfer
    DMA_TT_M2P,     // Memory to Peripheral transfer
    DMA_TT_P2M,     // Peripheral to Memory transfer
    DMA_TT_COUNT    // Number of transfer types
} DMA_XFER_TYPE;

// Mask/unmask channel interrupts (mask = true means disable that interrupt source)
extern void dma_channel_mask_xfer_interrupt(uint8_t ch, bool mask);
extern void dma_channel_mask_block_interrupt(uint8_t ch, bool mask);
extern void dma_channel_mask_error_interrupt(uint8_t ch, bool mask);

/**
  * @}
  */ /* End of group DMA_Exported_Types */

/** @defgroup DMA_Exported_Functions DMA Exported Functions
  * @{
  */
/**
 * @brief Initialize DMA peripheral
 * @return Negative error code on failure, 0 on success
 */
extern int32_t dma_initialize (void);

/**
 * @brief Deinitialize DMA peripheral
 * @return Negative error code on failure, 0 on success
 */
extern int32_t dma_uninitialize (void);

/**
 * @brief Select or allocate a DMA channel
 * @param[in,out] pch     Pointer to preferred channel number (input) and actual assigned channel (output)
 *                         Pass NULL for automatic allocation
 * @param[in] cb_event    Channel completion callback function
 * @param[in] usr_param    User parameter passed to callback
 * @param[in] cache_sync   Cache synchronization policy for buffers
 * @return Assigned channel number on success, DMA_CHANNEL_ANY (0xFF) on failure
 */
extern uint8_t dma_channel_select(uint8_t *pch,
                                DMA_SignalEvent_t  cb_event,
                                uint32_t           usr_param,
                                DMA_CACHE_SYNC     cache_sync);

/**
 * @brief Reserve a specific DMA channel for exclusive use
 * @note Unlike dma_channel_select(), reserved channels are NOT automatically released
 * @param[in] ch          Channel number to reserve
 * @param[in] cb_event    Channel completion callback function
 * @param[in] usr_param    User parameter passed to callback
 * @param[in] cache_sync   Cache synchronization policy for buffers
 * @return Reserved channel number on success, DMA_CHANNEL_ANY (0xFF) on failure
 */
extern uint8_t dma_channel_reserve(uint8_t ch,
                                  DMA_SignalEvent_t  cb_event,
                                  uint32_t           usr_param,
                                  DMA_CACHE_SYNC     cache_sync);

/**
 * @brief Release a previously reserved DMA channel
 * @param[in] ch      Reserved channel number to release
 */
extern void dma_channel_unreserve(uint8_t ch);

/**
 * @brief Check if a channel is currently reserved
 * @param[in] ch      Channel number to check
 * @return true if reserved, false otherwise
 */
extern bool dma_channel_is_reserved(uint8_t ch);

/**
 * @brief Configure DMA channel for block transfer (implicitly enables channel)
 * @param[in] ch          Selected channel number
 * @param[in] en_int      Non-zero to enable interrupts
 * @param[in] src_addr    Source memory address
 * @param[in] dst_addr    Destination memory address
 * @param[in] total_size  Total number of data items to transfer
 * @param[in] control     Channel control register value
 * @param[in] config_low  Lower 32 bits of channel configuration
 * @param[in] config_high Upper 32 bits of channel configuration
 * @param[in] src_gath    Source gather register value
 * @param[in] dst_scat    Destination scatter register value
 * @return 0 on success, -1 on failure
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

#define dma_channel_configure(ch, ...)  \
    dma_channel_configure_wrapper(ch, 1, ##__VA_ARGS__)

//#define dma_channel_configure_polling(ch, ...)  \
//    dma_channel_configure_wrapper(ch, 0, ##__VA_ARGS__)

// en_bits options
#define DMA_CH_EN_XFER_INT      (0x1 << 0) // enable xfer & error interrupt (clear for polling)
#define DMA_CH_EN_BLK_INT       (0x1 << 1) // enable block interrupt (set if PingPong transfer)
#define DMA_CH_EN_PIPO          (0x1 << 7) // enable PingPong transfer

extern int32_t dma_channel_setup (uint8_t       ch,
                                  uint8_t       en_bits, //en_int
                                  uint32_t      control,
                                  uint32_t      config_low,
                                  uint32_t      config_high,
                                  uint32_t      src_gath,
                                  uint32_t      dst_scat);

extern int32_t dma_channel_start (uint8_t      ch,
                                  uint32_t      src_addr,
                                  uint32_t      dst_addr,
                                  uint32_t      total_size);



/**
 * @brief Check if channel is configured for specified peripheral interface
 * @param[in] ch         Channel number
 * @param[in] xfer_flag  Transfer type flag (M2P/P2M) @ bit[1:0], PingPong flag @ bit[7]
 * @param[in] hs_id      Hardware handshake interface number
 * @return true if configured correctly, false otherwise
 */
#define DMA_XFER_FLAG_TT_MASK      (0x3 << 0)
#define DMA_XFER_FLAG_PIPO_MASK    (0x1 << 7)
#define DMA_XFER_FLAG_PIPO         DMA_XFER_FLAG_PIPO_MASK
extern bool dma_channel_check_select(uint8_t ch, uint8_t xfer_flag, uint8_t hs_id);

//#define dma_channel_select_if_configured    dma_channel_check_select

// ONLY used for "RESERVED" or unchanged DMA channel!!
#define DMACH_CFG_FLAG_SRC_ADDR     0x1 // bit[0]
#define DMACH_CFG_FLAG_DST_ADDR     0x2 // bit[1]
#define DMACH_CFG_FLAG_BOTH_ADDR    0x3 // bit[1:0]
extern int32_t dma_channel_start_block (uint8_t      ch,
                                       uint8_t      cfg_flags,
                                       uint32_t     src_addr,
                                       uint32_t     dst_addr,
                                       uint32_t     total_size);
//#define dma_channel_configure_lite  dma_channel_start_block

/**
 * @brief Ping-Pong transfer block configuration
 * @var flags Bit flags controlling behavior (stop after this block)
 */
typedef struct {
    void *src;     ///< Source Address
    void *dst;     ///< Destination Address
    uint32_t size;    ///< Block Size (in data width units)
    uint32_t flags; ///< Control flags
} DMA_PIPO_BLK;

/**
 * @brief Start Ping-Pong mode transfer with block array
 * @param[in] ch       Channel number
 * @param[in] blk_array Array of blocks to transfer cyclically
 * @param[in,out] blk_cnt_p Input: max block count, Output: actual blocks loaded
 * @return 0 on success, negative on failure
 */
extern int32_t dma_channel_start_pipo (uint8_t ch, DMA_PIPO_BLK *blk_array, uint8_t *blk_cnt_p);

/**
 * @brief Cancel ongoing Ping-Pong transfer
 * @param[in] ch Channel number
 * @return 0 on success, negative on failure
 */
extern int32_t dma_channel_cancel_pipo (uint8_t ch);

/**
 * @brief Get completed Ping-Pong blocks information
 * @param[in] ch       Channel number
 * @param[out] blk_array Array to store completed block info
 * @param[in] blk_cnt  Number of blocks to retrieve
 * @return Number of blocks retrieved
 */
extern int32_t dma_channel_get_pipo_blks(uint8_t ch, DMA_PIPO_BLK *blk_array, uint8_t blk_cnt);

/**
 * @brief Configure DMA channel for Multi-Block transfer with linked list
 * @param[in] ch          Selected channel number
 * @param[in] llp         First Linked List Item pointer
 * @param[in] config_low  Lower 32 bits of channel configuration
 * @param[in] config_high Upper 32 bits of channel configuration
 * @param[in] src_gath    Source gather register value
 * @param[in] dst_scat    Destination scatter register value
 * @return 0 on success, -1 on failure
 */
extern int32_t dma_channel_configure_LLP (
                                   uint8_t      ch,
                                   DMA_LLP      llp,
                                   uint32_t     config_low,
                                   uint32_t     config_high,
                                   uint32_t     src_gath,
                                   uint32_t     dst_scat);

extern int32_t dma_channel_start_LLP (uint8_t ch, DMA_LLP llp);

/**
 * @brief Suspend DMA channel transfer
 * @param[in] ch         Channel number
 * @param[in] wait_done  Non-zero to wait for suspension completion
 * @return 0 on success, -1 on failure
 */
extern int32_t dma_channel_suspend (uint8_t ch, uint8_t wait_done);

/**
 * @brief Resume suspended DMA channel transfer
 * @param[in] ch Channel number
 * @return 0 on success, -1 on failure
 */
extern int32_t dma_channel_resume (uint8_t ch);

/**
 * @brief Enable DMA channel (resume if suspended)
 * @param[in] ch Channel number
 * @return 0 on success, -1 on failure
 */
extern int32_t dma_channel_enable (uint8_t ch);

/**
 * @brief Disable DMA channel (abort transfer first if required)
 * @param[in] ch         Channel number
 * @param[in] wait_done  Non-zero to wait for disable completion
 * @return 0 on success, -1 on failure
 */
extern int32_t dma_channel_disable (uint8_t ch, uint8_t wait_done);

extern bool dma_channel_is_enabled(uint8_t ch);
//extern bool dma_channel_is_polling(uint8_t ch);

extern bool dma_channel_xfer_error(uint8_t ch);
extern bool dma_channel_xfer_complete(uint8_t ch);
extern void dma_channel_clear_xfer_status(uint8_t ch);

/**
 * @brief Get DMA channel enabled status
 * @param[in] ch Channel number
 * @return 1 if enabled, 0 if disabled
 */
extern uint32_t dma_channel_get_status (uint8_t ch);

/**
 * @brief Get number of transferred data items
 * @param[in] ch Channel number
 * @return Number of items transferred
 */
extern uint32_t dma_channel_get_count (uint8_t ch);

/**
 * @brief Abort ongoing DMA transfer
 * @param[in] ch         Channel number
 * @param[in] wait_done  Non-zero to wait for abort completion
 * @return 0 on success, -1 on failure
 */
//extern int32_t dma_channel_abort (uint8_t ch, uint8_t wait_done);

/**
 * @brief Perform memory copy using DMA
 * @param[in] ch         Channel number
 * @param[in] src_addr   Source memory address
 * @param[in] dst_addr   Destination memory address
 * @param[in] total_bytes Total bytes to copy
 * @return 0 on success, -1 on failure
 */
extern int32_t dma_memcpy ( uint8_t            ch,
                            uint32_t           src_addr,
                            uint32_t           dst_addr,
                            uint32_t           total_bytes);


// workaround API functions for polling corresponding interrupts' status
// in case that ERROR/BLOCK/SRCTRANS/DSTTRANS interrupts don't work
extern void poll_dma_channel_interrupt(uint8_t ch);
extern void poll_all_dma_channel_interrupts(void);

/**
  * @}
  */ /* End of group DMA_Exported_Functions */
/** @} */ /* End of DMA group */

#endif /* __DMA_VENUSA_H */
