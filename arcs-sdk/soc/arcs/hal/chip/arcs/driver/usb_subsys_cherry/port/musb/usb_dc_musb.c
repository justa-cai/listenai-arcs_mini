/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usb_musb_reg.h"

/* Total FIFO size for all USB Endpoints (include EP0) */
#define USB_TOTAL_FIFO_SIZE  4096 // 4KB

/* DMA channel count available */
#define USB_DMA_CH_COUNT_AVAIL 6


#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#define USB_BASE (g_usbdev_bus[0].reg_base)

/* ============================================================
 * DMA Configuration
 * Define CONFIG_USB_MUSB_DMA_TX/RX to use hardware DMA for non-EP0 endpoints.
 * Undefine/comment out to fall back to PIO (CPU FIFO access) mode.
 * ============================================================ */
/* Fine-grained DMA control: enable/disable TX and RX DMA independently */
#define CONFIG_USB_MUSB_DMA_TX  1 //0 ///* DMA for TX (IN/write to host) */
#define CONFIG_USB_MUSB_DMA_RX  1 //0 ///* DMA for RX (OUT/read from host) */

/* DPB macros */
//#define CONFIG_MUSB_DPB_TX   	1 //0 //
//#define CONFIG_MUSB_DPB_RX   	1 //0 //
#ifndef CONFIG_MUSB_DPB
#define CONFIG_MUSB_DPB   	    0 //1 //
#endif

#if CONFIG_MUSB_DPB
#define USB_FIFOSZ_DPB_MASK         (0x10)
#define USB_FIFOSZ_DPB              (USB_FIFOSZ_DPB_MASK)

volatile uint32_t musb_dpb_flags = 0;
void SET_EP_DPB(uint8_t ep_addr) {
    uint8_t idx = USB_EP_GET_IDX(ep_addr);
    uint8_t dir = USB_EP_GET_DIR(ep_addr);
    musb_dpb_flags |= 0x1UL << (dir ? idx+16 : idx);
}
uint32_t GET_EP_DPB(uint8_t ep_addr) {
    uint8_t idx = USB_EP_GET_IDX(ep_addr);
    uint8_t dir = USB_EP_GET_DIR(ep_addr);
    return musb_dpb_flags & (0x1UL << (dir ? idx+16 : idx));
}
#endif // CONFIG_MUSB_DPB


/* DMA register offsets */
#define USB_DMA_INTR_OFFSET     0x200
#define USB_DMA_CTRL_OFFSET(ch) (0x204 + (ch) * 0x10)
#define USB_DMA_ADDR_OFFSET(ch) (0x208 + (ch) * 0x10)
#define USB_DMA_COUNT_OFFSET(ch) (0x20C + (ch) * 0x10)
//#define USB_DMA_EP_OFFSET(ch)   (0x204 + (ch) * 0x10 + 0x02) /* Assuming endpoint select in control reg */

#if defined(CONFIG_USB_MUSB_SUNXI)
#define MUSB_FADDR_OFFSET 0x98
#define MUSB_POWER_OFFSET 0x40
#define MUSB_TXIS_OFFSET  0x44
#define MUSB_RXIS_OFFSET  0x46
#define MUSB_TXIE_OFFSET  0x48
#define MUSB_RXIE_OFFSET  0x4A
#define MUSB_IS_OFFSET    0x4C
#define MUSB_IE_OFFSET    0x50
#define MUSB_EPIDX_OFFSET 0x42

#define MUSB_IND_TXMAP_OFFSET      0x80
#define MUSB_IND_TXCSRL_OFFSET     0x82
#define MUSB_IND_TXCSRH_OFFSET     0x83
#define MUSB_IND_RXMAP_OFFSET      0x84
#define MUSB_IND_RXCSRL_OFFSET     0x86
#define MUSB_IND_RXCSRH_OFFSET     0x87
#define MUSB_IND_RXCOUNT_OFFSET    0x88
#define MUSB_IND_TXTYPE_OFFSET     0x8C
#define MUSB_IND_TXINTERVAL_OFFSET 0x8D
#define MUSB_IND_RXTYPE_OFFSET     0x8E
#define MUSB_IND_RXINTERVAL_OFFSET 0x8F

#define MUSB_FIFO_OFFSET 0x00

#define MUSB_DEVCTL_OFFSET 0x41

#define MUSB_TXFIFOSZ_OFFSET  0x90
#define MUSB_RXFIFOSZ_OFFSET  0x94
#define MUSB_TXFIFOADD_OFFSET 0x92
#define MUSB_RXFIFOADD_OFFSET 0x96

#define MUSB_TXFUNCADDR0_OFFSET 0x98
#define MUSB_TXHUBADDR0_OFFSET  0x9A
#define MUSB_TXHUBPORT0_OFFSET  0x9B
#define MUSB_TXFUNCADDRx_OFFSET 0x98
#define MUSB_TXHUBADDRx_OFFSET  0x9A
#define MUSB_TXHUBPORTx_OFFSET  0x9B
#define MUSB_RXFUNCADDRx_OFFSET 0x9C
#define MUSB_RXHUBADDRx_OFFSET  0x9E
#define MUSB_RXHUBPORTx_OFFSET  0x9F

#define USB_TXMAP_BASE(ep_idx)      (USB_BASE + MUSB_IND_TXMAP_OFFSET)
#define USB_TXCSRL_BASE(ep_idx)     (USB_BASE + MUSB_IND_TXCSRL_OFFSET)
#define USB_TXCSRH_BASE(ep_idx)     (USB_BASE + MUSB_IND_TXCSRH_OFFSET)
#define USB_RXMAP_BASE(ep_idx)      (USB_BASE + MUSB_IND_RXMAP_OFFSET)
#define USB_RXCSRL_BASE(ep_idx)     (USB_BASE + MUSB_IND_RXCSRL_OFFSET)
#define USB_RXCSRH_BASE(ep_idx)     (USB_BASE + MUSB_IND_RXCSRH_OFFSET)
#define USB_RXCOUNT_BASE(ep_idx)    (USB_BASE + MUSB_IND_RXCOUNT_OFFSET)
#define USB_TXTYPE_BASE(ep_idx)     (USB_BASE + MUSB_IND_TXTYPE_OFFSET)
#define USB_TXINTERVAL_BASE(ep_idx) (USB_BASE + MUSB_IND_TXINTERVAL_OFFSET)
#define USB_RXTYPE_BASE(ep_idx)     (USB_BASE + MUSB_IND_RXTYPE_OFFSET)
#define USB_RXINTERVAL_BASE(ep_idx) (USB_BASE + MUSB_IND_RXINTERVAL_OFFSET)

#define USB_TXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDRx_OFFSET)
#define USB_TXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXHUBADDRx_OFFSET)
#define USB_TXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXHUBPORTx_OFFSET)
#define USB_RXADDR_BASE(ep_idx)    (USB_BASE + MUSB_RXFUNCADDRx_OFFSET)
#define USB_RXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_RXHUBADDRx_OFFSET)
#define USB_RXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_RXHUBPORTx_OFFSET)

#elif defined(CONFIG_USB_MUSB_CUSTOM)
#include "musb_custom.h"
#else
#define MUSB_FADDR_OFFSET 0x00
#define MUSB_POWER_OFFSET 0x01
#define MUSB_TXIS_OFFSET  0x02
#define MUSB_RXIS_OFFSET  0x04
#define MUSB_TXIE_OFFSET  0x06
#define MUSB_RXIE_OFFSET  0x08
#define MUSB_IS_OFFSET    0x0A
#define MUSB_IE_OFFSET    0x0B

#define MUSB_EPIDX_OFFSET 0x0E

#define MUSB_IND_TXMAP_OFFSET      0x10
#define MUSB_IND_TXCSRL_OFFSET     0x12
#define MUSB_IND_TXCSRH_OFFSET     0x13
#define MUSB_IND_RXMAP_OFFSET      0x14
#define MUSB_IND_RXCSRL_OFFSET     0x16
#define MUSB_IND_RXCSRH_OFFSET     0x17
#define MUSB_IND_RXCOUNT_OFFSET    0x18
#define MUSB_IND_TXTYPE_OFFSET     0x1A
#define MUSB_IND_TXINTERVAL_OFFSET 0x1B
#define MUSB_IND_RXTYPE_OFFSET     0x1C
#define MUSB_IND_RXINTERVAL_OFFSET 0x1D

#define MUSB_FIFO_OFFSET 0x20

#define MUSB_DEVCTL_OFFSET 0x60

#define MUSB_TXFIFOSZ_OFFSET  0x62
#define MUSB_RXFIFOSZ_OFFSET  0x63
#define MUSB_TXFIFOADD_OFFSET 0x64
#define MUSB_RXFIFOADD_OFFSET 0x66

/*
#define MUSB_TXFUNCADDR0_OFFSET 0x80
#define MUSB_TXHUBADDR0_OFFSET  0x82
#define MUSB_TXHUBPORT0_OFFSET  0x83
#define MUSB_TXFUNCADDRx_OFFSET 0x88
#define MUSB_TXHUBADDRx_OFFSET  0x8A
#define MUSB_TXHUBPORTx_OFFSET  0x8B
#define MUSB_RXFUNCADDRx_OFFSET 0x8C
#define MUSB_RXHUBADDRx_OFFSET  0x8E
#define MUSB_RXHUBPORTx_OFFSET  0x8F

#define MUSB_TXMAP0_OFFSET          0x100

// do not use EPIDX
#define USB_TXMAP_BASE(ep_idx)      (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx)
#define USB_TXCSRL_BASE(ep_idx)     (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 2)
#define USB_TXCSRH_BASE(ep_idx)     (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 3)
#define USB_RXMAP_BASE(ep_idx)      (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 4)
#define USB_RXCSRL_BASE(ep_idx)     (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 6)
#define USB_RXCSRH_BASE(ep_idx)     (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 7)
#define USB_RXCOUNT_BASE(ep_idx)    (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 8)
#define USB_TXTYPE_BASE(ep_idx)     (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 0x0A)
#define USB_TXINTERVAL_BASE(ep_idx) (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 0x0B)
#define USB_RXTYPE_BASE(ep_idx)     (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 0x0C)
#define USB_RXINTERVAL_BASE(ep_idx) (USB_BASE + MUSB_TXMAP0_OFFSET + 0x10 * ep_idx + 0x0D)

#define USB_TXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx)
#define USB_TXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 2)
#define USB_TXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 3)
#define USB_RXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 4)
#define USB_RXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 6)
#define USB_RXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 7)
*/
#define USB_TXMAP_BASE(ep_idx)      (USB_BASE + MUSB_IND_TXMAP_OFFSET)
#define USB_TXCSRL_BASE(ep_idx)     (USB_BASE + MUSB_IND_TXCSRL_OFFSET)
#define USB_TXCSRH_BASE(ep_idx)     (USB_BASE + MUSB_IND_TXCSRH_OFFSET)
#define USB_RXMAP_BASE(ep_idx)      (USB_BASE + MUSB_IND_RXMAP_OFFSET)
#define USB_RXCSRL_BASE(ep_idx)     (USB_BASE + MUSB_IND_RXCSRL_OFFSET)
#define USB_RXCSRH_BASE(ep_idx)     (USB_BASE + MUSB_IND_RXCSRH_OFFSET)
#define USB_RXCOUNT_BASE(ep_idx)    (USB_BASE + MUSB_IND_RXCOUNT_OFFSET)
#define USB_TXTYPE_BASE(ep_idx)     (USB_BASE + MUSB_IND_TXTYPE_OFFSET)
#define USB_TXINTERVAL_BASE(ep_idx) (USB_BASE + MUSB_IND_TXINTERVAL_OFFSET)
#define USB_RXTYPE_BASE(ep_idx)     (USB_BASE + MUSB_IND_RXTYPE_OFFSET)
#define USB_RXINTERVAL_BASE(ep_idx) (USB_BASE + MUSB_IND_RXINTERVAL_OFFSET)
#endif

#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

typedef enum {
    USB_EP0_STATE_SETUP = 0x0,      /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA = 0x1,    /**< IN DATA */
    USB_EP0_STATE_OUT_DATA = 0x3,   /**< OUT DATA */
    USB_EP0_STATE_IN_STATUS = 0x4,  /**< IN status */
    USB_EP0_STATE_OUT_STATUS = 0x5, /**< OUT status */
    USB_EP0_STATE_IN_ZLP = 0x6,     /**< OUT status */
    USB_EP0_STATE_STALL = 0x7,      /**< STALL status */
} ep0_state_t;

/* Endpoint state */
struct musb_ep_state {
    uint16_t fifo_size; /* Endpoint FIFO size */
    uint16_t ep_mps;    /* Endpoint max packet size */

    uint8_t ep_type : 2;    /* Endpoint type */
    uint8_t ep_enable : 1;  /* Endpoint enable */
    uint8_t ep_stalled : 1; /* Endpoint stall flag */
    uint8_t dma_reqmode : 1; /* (Endpoint) DMA Request Mode 0 or 1, it decides
                           1) when to send DMA request
                           2) whether EP interrupt is triggered */
    uint8_t dma_mode : 1; /* DMA (Channel Transfer) Mode 0 or 1, it decides
                            when DMA interrupt is triggered:
                            a packet (single packet, Mode 0) or
                            whole transfer (multiple packet, Mode 1) is done? */
    //uint8_t last_io : 2; // 0: NO IO, 1: DMA, 2: PIO
    uint8_t dma_ch; // 0xFF indicates no DMA is used
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct musb_udc {
    volatile uint16_t dev_addr : 8;
    volatile uint16_t fifo_alloc_addr;
    __attribute__((aligned(32))) struct usb_setup_packet setup;
    struct musb_ep_state in_ep[CONFIG_USBDEV_EP_NUM];  /*!< IN endpoint parameters*/
    struct musb_ep_state out_ep[CONFIG_USBDEV_EP_NUM]; /*!< OUT endpoint parameters */
} g_musb_udc;

static volatile uint8_t usb_ep0_state = USB_EP0_STATE_SETUP;


#if (CONFIG_USB_MUSB_DMA_TX || CONFIG_USB_MUSB_DMA_RX)

/* DMA channel allocation state: bit flags for 6 channels */
static volatile uint16_t dma_ch_allocated = 0;

/* DMA transfer tracking */
struct dma_transfer {
    uint8_t ep_idx;
    uint8_t direction;  // For USB device, 0 = RX, 1 = TX
    uint32_t len;
};
static struct dma_transfer dma_transfers[USB_DMA_CH_COUNT_AVAIL];

/* Allocate a DMA channel dynamically */
static uint8_t musb_dma_ch_alloc(void)
{
    for (int ch = 0; ch < USB_DMA_CH_COUNT_AVAIL; ch++) {
        if (!(dma_ch_allocated & (1 << ch))) {
            dma_ch_allocated |= (1 << ch);
            return ch;
        }
    }
    return 0xFF; /* No free channel */
}

/* Free a DMA channel */
static void musb_dma_ch_free(int ch)
{
    if (ch >= 0 && ch < USB_DMA_CH_COUNT_AVAIL) {
        dma_ch_allocated &= ~(1 << ch);
    }
}

#endif // (CONFIG_USB_MUSB_DMA_TX | CONFIG_USB_MUSB_DMA_RX)

/* get current active ep */
static uint8_t musb_get_active_ep(void)
{
    return HWREGB(USB_BASE + MUSB_EPIDX_OFFSET);
}

/* set the active ep */
static void musb_set_active_ep(uint8_t ep_index)
{
    HWREGB(USB_BASE + MUSB_EPIDX_OFFSET) = ep_index;
}

static void musb_write_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((uint32_t)buffer & 0x03) {
        buf8 = buffer;
        for (i = 0; i < len; i++) {
            HWREGB(USB_FIFO_BASE(ep_idx)) = *buf8++;
        }
    } else {
        count32 = len >> 2;
        count8 = len & 0x03;

        buf32 = (uint32_t *)buffer;

        while (count32--) {
            HWREG(USB_FIFO_BASE(ep_idx)) = *buf32++;
        }

        buf8 = (uint8_t *)buf32;

        while (count8--) {
            HWREGB(USB_FIFO_BASE(ep_idx)) = *buf8++;
        }
    }
}

static void musb_read_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((uint32_t)buffer & 0x03) {
        buf8 = buffer;
        for (i = 0; i < len; i++) {
            *buf8++ = HWREGB(USB_FIFO_BASE(ep_idx));
        }
    } else {
        count32 = len >> 2;
        count8 = len & 0x03;

        buf32 = (uint32_t *)buffer;

        while (count32--) {
            *buf32++ = HWREG(USB_FIFO_BASE(ep_idx));
        }

        buf8 = (uint8_t *)buf32;

        while (count8--) {
            *buf8++ = HWREGB(USB_FIFO_BASE(ep_idx));
        }
    }
}

#if CONFIG_USB_MUSB_DMA_TX
/* DMA TX functions for non-EP0 endpoints */
static bool musb_dma_write_packet(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    if (len == 0 || ((uint32_t)buffer & 0x03) != 0) {
        /* Fallback to CPU transfer */
        return false;
    }

    struct musb_ep_state* epp = &g_musb_udc.in_ep[ep_idx];
    int dma_ch = epp->dma_ch;
    if (dma_ch == 0xFF)
        dma_ch = musb_dma_ch_alloc();
    if (dma_ch == 0xFF) {
        /* Fallback to CPU transfer */
        return false;
    }

    /* Clean cache before DMA reads from RAM */
    //dcache_clean_range((unsigned long)buffer, (unsigned long)buffer + len); //FIXME:

//    uint8_t old_ep_idx = musb_get_active_ep();
//    musb_set_active_ep(ep_idx);

    /* Set transfer info for interrupt handling */
    dma_transfers[dma_ch].ep_idx = ep_idx;
    dma_transfers[dma_ch].direction = 1;  // For USB device, 0 = RX, 1 = TX
    dma_transfers[dma_ch].len = len;
    epp->dma_ch = dma_ch;

    //NOTE: Experiments shows that dma_reqmode 0 can decrease the USB IN/TX speed with DPB, so discard this branch...
    // And the other branch can improve the USB IN/TX speed without DPB, so use that branch in any case!
#if 0 //!CONFIG_MUSB_DPB_TX
    uint8_t tx_csrh = HWREG(USB_BASE + MUSB_IND_TXCSRH_OFFSET) | USB_TXCSRH1_DMAREQ_EN | USB_TXCSRH1_AUTOSET;
    uint16_t dma_ctrl = USB_DMACTL0_ENABLE | USB_DMACTL0_DIR_TX | USB_DMACTL0_IE | USB_DMACTL0_EP(ep_idx);

    // Use DMA MODE 1 generally, but MODE 0 for short packet (less than MPS)
    // USB_DMACTL3_BRSTM_ANY for ARCS, for other options have deficiency on PSRAM buffer!
    if (len < epp->ep_mps) {
        tx_csrh &= ~USB_TXCSRH1_DMAREQ_MOD;
        epp->dma_reqmode = 0; // DMA REQUEST MODE 0
        //dma_ctrl |= USB_DMACTL0_MODE0; // | USB_DMACTL0_BRSTM_INC16;
        epp->dma_mode = 0; // DMA MODE 0
    } else {
        tx_csrh |= USB_TXCSRH1_DMAREQ_MOD;
        epp->dma_reqmode = 1; // DMA REQUEST MODE 1
        dma_ctrl |= USB_DMACTL0_MODE1; // | USB_DMACTL0_BRSTM_INC16;
        epp->dma_mode = 1; // DMA MODE 1
    }
#else
    uint8_t tx_csrh = HWREG(USB_BASE + MUSB_IND_TXCSRH_OFFSET) | USB_TXCSRH1_DMAREQ_EN | USB_TXCSRH1_AUTOSET | USB_TXCSRH1_DMAREQ_MOD;
    uint16_t dma_ctrl = USB_DMACTL0_ENABLE | USB_DMACTL0_DIR_TX | USB_DMACTL0_IE | USB_DMACTL0_MODE1 | USB_DMACTL0_EP(ep_idx);
    epp->dma_reqmode = 1; // DMA REQUEST MODE 1
    epp->dma_mode = 1; // DMA MODE 1
#endif

    /* Enable DMA & INTRTX for TX endpoint */
    HWREG(USB_BASE + MUSB_IND_TXCSRH_OFFSET) = tx_csrh;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);

    /* Configure DMA registers for TX, USB_DMACTL3_BRSTM_ANY */
    HWREG(USB_BASE + USB_DMA_ADDR_OFFSET(dma_ch)) = (uint32_t)buffer;
    HWREG(USB_BASE + USB_DMA_COUNT_OFFSET(dma_ch)) = len;

    HWREG(USB_BASE + USB_DMA_CTRL_OFFSET(dma_ch)) = dma_ctrl;

//    musb_set_active_ep(old_ep_idx);
//    CLOGD("TO-DMA-TX Ch%d, EP%d, %dB\n", dma_ch, ep_idx, len);
    return true;
}
#endif

#if CONFIG_USB_MUSB_DMA_RX
static bool musb_dma_read_packet(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    if (((uint32_t)buffer & 0x03) != 0) {
        /* Fallback to CPU transfer */
        return false;
    }

    struct musb_ep_state* epp = &g_musb_udc.out_ep[ep_idx];
    int dma_ch = epp->dma_ch;
    if (dma_ch == 0xFF)
        dma_ch = musb_dma_ch_alloc();
    if (dma_ch == 0xFF) {
        /* Fallback to CPU transfer */
        return false;
    }

//    uint8_t old_ep_idx = musb_get_active_ep();
//    musb_set_active_ep(ep_idx);

    /* Set transfer info for interrupt handling */
    dma_transfers[dma_ch].ep_idx = ep_idx;
    dma_transfers[dma_ch].direction = 0; // For USB device, 0 = RX, 1 = TX
    dma_transfers[dma_ch].len = len;
    epp->dma_ch = dma_ch;

    //NOTE: the branch -- if (len < g_musb_udc.out_ep[ep_idx].ep_mps) COULD NOT WORK!!
    // According to the experiment Received data are all 0 when DMA interrupt is triggered...
#if 0
    uint8_t rx_csrh = HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) | USB_RXCSRH1_DMAREQ_EN | USB_RXCSRH1_AUTOCL;
    uint16_t dma_ctrl = USB_DMACTL0_ENABLE | USB_DMACTL0_DIR_RX | USB_DMACTL0_IE | USB_DMACTL0_EP(ep_idx); // | USB_DMACTL0_BRSTM_INC16;

    // Use DMA MODE 1 generally, but MODE 0 for short packet (less than MPS)
    // USB_DMACTL3_BRSTM_ANY for ARCS, for other options have deficiency on PSRAM buffer!
    if (len < g_musb_udc.out_ep[ep_idx].ep_mps) {
        // DMA Request Mode 0, AutoClear = 1, DMAReqEnab = 1, DMA Mode 0 (Single Packet DMA)
        rx_csrh &= ~(USB_RXCSRH1_DMAREQ_MOD);
        //dma_ctrl |= USB_DMACTL0_MODE0; // DMA Mode 0
        epp->dma_reqmode = 0;
        epp->dma_mode = 0;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
    } else {
        // DMA Request Mode 1, AutoClear = 1, DMAReqEnab = 1, DMA Mode 1 (Multiple Packet DMA)
        rx_csrh |= USB_RXCSRH1_DMAREQ_MOD;
        dma_ctrl |= USB_DMACTL0_MODE1; // DMA Mode 1
        epp->dma_reqmode = 1;
        epp->dma_mode = 1;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
    }
#else
    uint8_t rx_csrh = HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) | USB_RXCSRH1_DMAREQ_MOD | USB_RXCSRH1_AUTOCL | USB_RXCSRH1_DMAREQ_EN;
    uint16_t dma_ctrl = USB_DMACTL0_ENABLE | USB_DMACTL0_DIR_RX | USB_DMACTL0_IE | USB_DMACTL0_EP(ep_idx) | USB_DMACTL0_MODE1; // | USB_DMACTL0_BRSTM_INC16;
    epp->dma_reqmode = 1;
    epp->dma_mode = 1;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
#endif

    /* Enable DMA & INTRRX for RX endpoint */
    HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) = rx_csrh;
    //HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);

    /* Configure DMA registers for RX */
    HWREG(USB_BASE + USB_DMA_ADDR_OFFSET(dma_ch)) = (uint32_t)buffer;
    HWREG(USB_BASE + USB_DMA_COUNT_OFFSET(dma_ch)) = len;

    HWREGH(USB_BASE + USB_DMA_CTRL_OFFSET(dma_ch)) = dma_ctrl;

//    musb_set_active_ep(old_ep_idx);

    //CLOGD("TO-DMA-RX Ch%d, EP%d, %dB\n", dma_ch, ep_idx, len);
    return true;
}
#endif

/*
static uint32_t musb_get_fifo_size(uint16_t mps, uint16_t *used)
{
    uint32_t size;

    for (uint8_t i = USB_TXFIFOSZ_SIZE_8; i <= USB_TXFIFOSZ_SIZE_2048; i++) {
        size = (8 << i);
        if (mps <= size) {
            *used = size;
//#if (CONFIG_MUSB_DPB_TX || CONFIG_MUSB_DPB_RX)
//            *used *= 2;
//#endif
            return i;
        }
    }

    *used = 0;
    return USB_TXFIFOSZ_SIZE_8;
}

static uint32_t usbd_musb_fifo_config(struct musb_fifo_cfg *cfg, uint32_t offset)
{
    uint16_t fifo_used;
    uint8_t c_size, mask = 0x0f;
    uint16_t c_off;

    c_off = offset >> 3;
    c_size = musb_get_fifo_size(cfg->maxpacket, &fifo_used);

    //FIXME: TEST ONLY!!
#if (CONFIG_MUSB_DPB_TX || CONFIG_MUSB_DPB_RX)
    if(cfg->ep_num == 0x1 ||cfg->ep_num == 0x2) {
    //if(cfg->ep_num == 0x2) { //OUT, RX
    //if(cfg->ep_num == 0x1) { //IN, TX
        fifo_used <<= 1;
        c_size |= USB_FIFOSZ_DPB;
        mask = 0x1f;
    }
#else
    c_size &= ~USB_FIFOSZ_DPB;
#endif

    musb_set_active_ep(cfg->ep_num);

    switch (cfg->style) {
        case FIFO_TX:
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = c_size & mask;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = c_off;
            break;
        case FIFO_RX:
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = c_size & mask;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = c_off;
            break;
        case FIFO_TXRX:
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = c_size & mask;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = c_off;
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = c_size & mask;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = c_off;
            break;

        default:
            break;
    }

    return (offset + fifo_used);
}
*/

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(uint8_t busid)
{
    uint16_t offset = 0;
    uint8_t cfg_num;
    struct musb_fifo_cfg *cfg;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_SOFTCONN;
    usb_dc_low_level_init();

    memset(&g_musb_udc, 0, sizeof(struct musb_udc));

#if (CONFIG_USB_MUSB_DMA_TX || CONFIG_USB_MUSB_DMA_RX)
    /* Initialize DMA transfer tracking */
    memset(dma_transfers, 0xFF, sizeof(dma_transfers));
#endif

#ifdef CONFIG_USB_HS
#ifdef CONFIG_CHERRYUSB_DEVICE_MUSB_LISA
    /* ARCS MUSB: pulse SOFT_RST (0x7F) to unlock HS PHY, must be immediately before HSENAB */
    HWREGB(USB_BASE + 0x7F) |= (0x01 | 0x02);
#endif
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;
#else
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_HSENAB;
#endif

    musb_set_active_ep(0);
    HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) |= USB_DEVCTL_SESSION;

/*
    cfg_num = usbd_get_musb_fifo_cfg(&cfg);

    for (uint8_t i = 0; i < cfg_num; i++) {
        offset = usbd_musb_fifo_config(&cfg[i], offset);
    }

    USB_ASSERT_MSG(offset <= usb_get_musb_ram_size(), "Your fifo config is overflow, please check");
*/

    // Allocate FIFO for EP0. According to MUSB SPEC p115:
    // The Endpoint 0 FIFO has a fixed size (64 bytes) and a fixed location (start address 0)
    // BSD NOTE: in fact EP0 FIFO size can be changed...
    g_musb_udc.fifo_alloc_addr = (0 + CONFIG_USBDEV_REQUEST_BUFFER_LEN) >> 3; // next FIFO address except EP0

    /* Enable USB interrupts */
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = USB_IE_RESET | USB_IE_SUSPND | USB_IE_RESUME;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

#ifdef CONFIG_USBDEV_SOF_ENABLE
    HWREGB(USB_BASE + MUSB_IE_OFFSET) |= USB_IE_SOF;
#endif

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_SOFTCONN;
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = 0;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = 0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_SOFTCONN;

    usb_dc_low_level_deinit();
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    if (addr == 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;
    }

    g_musb_udc.dev_addr = addr;
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_RESUME;
    usbd_musb_delay_ms(10);
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_RESUME;
    return 0;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    uint8_t speed = USB_SPEED_UNKNOWN;

    if (HWREGB(USB_BASE + MUSB_POWER_OFFSET) & USB_POWER_HSMODE)
        speed = USB_SPEED_HIGH;
    else if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_FSDEV)
        speed = USB_SPEED_FULL;
    else if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_LSDEV)
        speed = USB_SPEED_LOW;

    return speed;
}

static uint32_t calc_fifosz_val(uint16_t fifo_size)
{
    uint32_t size;
    for (uint8_t i = USB_TXFIFOSZ_SIZE_8; i <= USB_TXFIFOSZ_SIZE_2048; i++) {
        size = (8 << i);
        if (fifo_size <= size)
            return i;
    }
    return USB_TXFIFOSZ_SIZE_8;
}

static uint32_t get_remain_fifo_size()
{
    int32_t remained = USB_TOTAL_FIFO_SIZE - (g_musb_udc.fifo_alloc_addr << 3);
    return (remained > 0 ? remained : 0);
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    uint8_t ep_dir = USB_EP_GET_DIR(ep->bEndpointAddress);
    uint8_t old_ep_idx, val_u8;
    uint32_t ui32Flags = 0;
    uint16_t ui32Register = 0;
    uint16_t ep_mps, fifo_size;

    if (ep_idx == 0) {
        g_musb_udc.out_ep[0].ep_mps = USB_CTRL_EP_MPS;
        g_musb_udc.out_ep[0].ep_type = 0x00;
        g_musb_udc.out_ep[0].ep_enable = 1;
        g_musb_udc.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
        g_musb_udc.in_ep[0].ep_type = 0x00;
        g_musb_udc.in_ep[0].ep_enable = 1;
        return 0;
    }

    USB_ASSERT_MSG(ep_idx < CONFIG_USBDEV_EP_NUM, "Ep addr %02x overflow", ep->bEndpointAddress);

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    fifo_size = ep_dir ? g_musb_udc.in_ep[ep_idx].fifo_size : g_musb_udc.out_ep[ep_idx].fifo_size;

    // EP FIFO has been allocated and MPS > FIFO_SIZE
    if (fifo_size > 0 && ep_mps > fifo_size )
        return -1; //FIXME: other accurate value

    // Allocate EP's FIFO based on MPS if NOT yet
    if (fifo_size == 0) {
        val_u8 = 0;
#if CONFIG_MUSB_DPB
        if (GET_EP_DPB(ep->bEndpointAddress)) {
            //NOTE: it's calc_fifosz_val(ep_mps), NOT calc_fifosz_val(ep_mps << 1)!
            val_u8 = calc_fifosz_val(ep_mps) | USB_FIFOSZ_DPB;
            fifo_size = ((ep_mps << 1) + 7) & ~0x7;
        } else
#endif
        {
            val_u8 = calc_fifosz_val(ep_mps);
            fifo_size = (ep_mps + 7) & ~0x7;
        }
        // Check if remaining FIFO space is enough for the EP?
        if (get_remain_fifo_size() < fifo_size) {
            CLOG("Ep %02x fifo is overflow\r\n", ep->bEndpointAddress);
            return -1; //FIXME: other accurate value
        }

        if (ep_dir) { // IN EP
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = val_u8;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = g_musb_udc.fifo_alloc_addr;
        } else { // OUT EP
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = val_u8;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = g_musb_udc.fifo_alloc_addr;
        }
        g_musb_udc.fifo_alloc_addr += fifo_size >> 3; // 8bytes aligned address
    } // end if (fifo_size == 0)

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) { // OUT EP

        HWREGH(USB_RXMAP_BASE(ep_idx)) = ep_mps;
        g_musb_udc.out_ep[ep_idx].ep_mps = ep_mps;
        g_musb_udc.out_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        g_musb_udc.out_ep[ep_idx].ep_enable = 1;

        //
        // If requested, disable NYET responses for high-speed bulk and
        // interrupt endpoints.
        //
        if (ui32Flags & USB_EP_DIS_NYET) {
            ui32Register |= USB_RXCSRH1_DISNYET;
        }

        //
        // Enable isochronous mode if requested.
        //
        if (USB_GET_ENDPOINT_TYPE(ep->bmAttributes) == 0x01) {
            ui32Register |= USB_RXCSRH1_ISO;
        }

        HWREGB(USB_RXCSRH_BASE(ep_idx)) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_RXCSRL_BASE(ep_idx)) & USB_RXCSRL1_RXRDY)
            HWREGB(USB_RXCSRL_BASE(ep_idx)) = (USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
        else
            HWREGB(USB_RXCSRL_BASE(ep_idx)) = USB_RXCSRL1_CLRDT;

        HWREGB(USB_TXCSRH_BASE(ep_idx)) &= ~USB_TXCSRH1_MODE;

    } else { // IN EP

        HWREGH(USB_TXMAP_BASE(ep_idx)) = ep_mps;
        g_musb_udc.in_ep[ep_idx].ep_mps = ep_mps;
        g_musb_udc.in_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        g_musb_udc.in_ep[ep_idx].ep_enable = 1;

        //
        // Enable isochronous mode if requested.
        //
        if (USB_GET_ENDPOINT_TYPE(ep->bmAttributes) == 0x01) {
            ui32Register |= USB_TXCSRH1_ISO;
        }

        // Reset the Data toggle to zero.
        if (HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_TXCSRL1_TXRDY)
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = (USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
        else
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_TXCSRL1_CLRDT;
        HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~USB_TXCSRL1_CLRDT; //CSK NOTE:
    }

    musb_set_active_ep(old_ep_idx);

    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_TXCSRL_BASE(ep_idx)) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_RXCSRL_BASE(ep_idx)) |= USB_RXCSRL1_STALL;
        }
    } else {
        if (ep_idx == 0x00) {
            usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_TXCSRL_BASE(ep_idx)) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_TXCSRL_BASE(ep_idx)) |= USB_TXCSRL1_STALL;
        }
    }

    musb_set_active_ep(old_ep_idx);
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an OUT endpoint.
            HWREGB(USB_RXCSRL_BASE(ep_idx)) &= ~(USB_RXCSRL1_STALL | USB_RXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_RXCSRL_BASE(ep_idx)) |= USB_RXCSRL1_CLRDT;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an IN endpoint.
            HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~(USB_TXCSRL1_STALL | USB_TXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_TXCSRL_BASE(ep_idx)) |= USB_TXCSRL1_CLRDT;
            HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~USB_TXCSRL1_CLRDT; //CSK NOTE:
        }
    }

    musb_set_active_ep(old_ep_idx);
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (HWREGB(USB_RXCSRL_BASE(ep_idx)) & USB_RXCSRL1_STALL) {
            *stalled = 1;
        } else {
            *stalled = 0;
        }
    } else {
        if (HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_TXCSRL1_STALL) {
            *stalled = 1;
        } else {
            *stalled = 0;
        }
    }
    musb_set_active_ep(old_ep_idx);
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    if (!data && data_len) {
        return -1;
    }
    if (!g_musb_udc.in_ep[ep_idx].ep_enable) {
        return -2;
    }

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (ep_idx == 0x00) {
        if (HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_CSRL0_TXRDY) {
            musb_set_active_ep(old_ep_idx);
            CLOG("%s: ep%d busying\n", __func__, ep_idx);
            return -3;
        }
    }
#if !CONFIG_USB_MUSB_DMA_TX
    else {
        if (HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_TXCSRL1_TXRDY) {
            musb_set_active_ep(old_ep_idx);
            CLOG("%s: ep%d busying\n", __func__, ep_idx);
            return -3;
        }
    }
#endif

    g_musb_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_musb_udc.in_ep[ep_idx].xfer_len = data_len;
    g_musb_udc.in_ep[ep_idx].actual_xfer_len = 0;
    g_musb_udc.in_ep[ep_idx].dma_ch = 0xFF; //CSK NOTE:

    if (data_len == 0) {
        if (ep_idx == 0x00) {
            if (g_musb_udc.setup.wLength == 0) {
                usb_ep0_state = USB_EP0_STATE_IN_STATUS;
            } else {
                usb_ep0_state = USB_EP0_STATE_IN_ZLP;
            }
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
        } else { //TODO: Check if TXRDY is set when CONFIG_USB_MUSB_DMA_TX...
#if CONFIG_USB_MUSB_DMA_TX
        if ((HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_TXCSRL1_TXRDY)) {
            musb_set_active_ep(old_ep_idx);
            CLOG("%s: ZLP TX failed and ep%d busy!!\n", __func__, ep_idx);
            return -3;
        }
#endif
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_TXCSRL1_TXRDY;
            HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);
        }

        musb_set_active_ep(old_ep_idx);
        return 0;
    }
    //data_len = MIN(data_len, g_musb_udc.in_ep[ep_idx].ep_mps);

    if (ep_idx == 0x00) {
        data_len = MIN(data_len, g_musb_udc.in_ep[ep_idx].ep_mps);
        musb_write_packet(ep_idx, (uint8_t *)data, data_len);
        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);
        usb_ep0_state = USB_EP0_STATE_IN_DATA;
        if (data_len < g_musb_udc.in_ep[ep_idx].ep_mps) {
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
        } else {
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_CSRL0_TXRDY;
        }
    } else {
        bool ret = false;
#if CONFIG_USB_MUSB_DMA_TX
        ret = musb_dma_write_packet(ep_idx, (uint8_t *)data, data_len);
        if (!ret && (HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_TXCSRL1_TXRDY)) {
            musb_set_active_ep(old_ep_idx);
            CLOG("%s: DMA TX failed and ep%d busy!!\n", __func__, ep_idx);
            return -3;
        }
#endif
        if (!ret) { // Fallback to CPU transfer
            data_len = MIN(data_len, g_musb_udc.in_ep[ep_idx].ep_mps);
            //CLOGD("TO-PIO-TX EP%d, %dB\n", ep_idx, data_len);
            musb_write_packet(ep_idx, (uint8_t *)data, data_len);
            HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);
            HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_TXCSRL1_TXRDY;
        }
    }

    musb_set_active_ep(old_ep_idx);
    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    if (!data && data_len) {
        return -1;
    }
    if (!g_musb_udc.out_ep[ep_idx].ep_enable) {
        return -2;
    }

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    g_musb_udc.out_ep[ep_idx].xfer_buf = data;
    g_musb_udc.out_ep[ep_idx].xfer_len = data_len;
    g_musb_udc.out_ep[ep_idx].actual_xfer_len = 0;
    g_musb_udc.out_ep[ep_idx].dma_ch = 0xFF; //CSK NOTE:

    if (data_len == 0) {
        if (ep_idx == 0) {
            usb_ep0_state = USB_EP0_STATE_SETUP;
        }
        musb_set_active_ep(old_ep_idx);
        return 0;
    }
    if (ep_idx == 0) {
        usb_ep0_state = USB_EP0_STATE_OUT_DATA;
    } else {
        bool ret;
#if CONFIG_USB_MUSB_DMA_RX
        ret = musb_dma_read_packet(ep_idx, data, data_len);
#else
        ret = false;
#endif
        if (!ret) // Fallback to CPU transfer
            HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
    }
    musb_set_active_ep(old_ep_idx);
    return 0;
}

static void handle_ep0(void)
{
    uint8_t ep_idx = 0; // EP0 index is always 0
    uint8_t ep0_status = HWREGB(USB_TXCSRL_BASE(ep_idx));
    uint16_t read_count;

    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~USB_CSRL0_STALLED;
        usb_ep0_state = USB_EP0_STATE_SETUP;
        return;
    }

    if (ep0_status & USB_CSRL0_SETEND) {
        HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_CSRL0_SETENDC;
    }

    if (g_musb_udc.dev_addr > 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = g_musb_udc.dev_addr;
        g_musb_udc.dev_addr = 0;
    }

    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            if (ep0_status & USB_CSRL0_RXRDY) {
                read_count = HWREGH(USB_RXCOUNT_BASE(ep_idx));

                if (read_count != 8) {
                    return;
                }

                musb_read_packet(0, (uint8_t *)&g_musb_udc.setup, 8);
                if (g_musb_udc.setup.wLength) {
                    HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_CSRL0_RXRDYC;
                } else {
                    HWREGB(USB_TXCSRL_BASE(ep_idx)) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                }

                usbd_event_ep0_setup_complete_handler(0, (uint8_t *)&g_musb_udc.setup);
            }
            break;

        case USB_EP0_STATE_IN_DATA:
            if (g_musb_udc.in_ep[0].xfer_len > g_musb_udc.in_ep[0].ep_mps) {
                g_musb_udc.in_ep[0].actual_xfer_len += g_musb_udc.in_ep[0].ep_mps;
                g_musb_udc.in_ep[0].xfer_len -= g_musb_udc.in_ep[0].ep_mps;
            } else {
                g_musb_udc.in_ep[0].actual_xfer_len += g_musb_udc.in_ep[0].xfer_len;
                g_musb_udc.in_ep[0].xfer_len = 0;
            }

            usbd_event_ep_in_complete_handler(0, 0x80, g_musb_udc.in_ep[0].actual_xfer_len);

            break;
        case USB_EP0_STATE_OUT_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                read_count = HWREGH(USB_RXCOUNT_BASE(ep_idx));

                musb_read_packet(0, g_musb_udc.out_ep[0].xfer_buf, read_count);
                g_musb_udc.out_ep[0].xfer_buf += read_count;
                g_musb_udc.out_ep[0].actual_xfer_len += read_count;

                if (read_count < g_musb_udc.out_ep[0].ep_mps) {
                    usbd_event_ep_out_complete_handler(0, 0x00, g_musb_udc.out_ep[0].actual_xfer_len);
                    HWREGB(USB_TXCSRL_BASE(ep_idx)) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                    usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                } else {
                    HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_CSRL0_RXRDYC;
                }
            }
            break;
        case USB_EP0_STATE_IN_STATUS:
        case USB_EP0_STATE_IN_ZLP:
            usb_ep0_state = USB_EP0_STATE_SETUP;
            usbd_event_ep_in_complete_handler(0, 0x80, 0);
            break;
    }
}


static inline bool TX_IS_IDLE(uint8_t ep_idx)
{
    uint8_t reg_val = HWREGB(USB_TXCSRL_BASE(ep_idx));
    return (reg_val & (USB_TXCSRL1_TXRDY | USB_TXCSRL1_FIFONE)) == 0;
}

void USBD_IRQHandler(uint8_t busid)
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint16_t write_count, read_count;
    uint8_t old_ep_idx;
    uint8_t ep_idx;
    bool ret;

    if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_HOST) {
        return;
    }

    is = HWREGB(USB_BASE + MUSB_IS_OFFSET);
    txis = HWREGH(USB_BASE + MUSB_TXIS_OFFSET);
    rxis = HWREGH(USB_BASE + MUSB_RXIS_OFFSET);

//    HWREGB(USB_BASE + MUSB_IS_OFFSET) = is;

    old_ep_idx = musb_get_active_ep();

    /* Receive a reset signal from the USB bus */
    if (is & USB_IS_RESET) {
        usbd_event_reset_handler(0);
        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

#if defined(CONFIG_USB_HS) && defined(CONFIG_CHERRYUSB_DEVICE_MUSB_LISA)
        /* ARCS MUSB: pulse SOFT_RST + re-enable HSENAB after bus reset */
        HWREGB(USB_BASE + 0x7F) |= (0x01 | 0x02);
        HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;
#endif

        usb_ep0_state = USB_EP0_STATE_SETUP;
    }

#ifdef CONFIG_USBDEV_SOF_ENABLE
    if (is & USB_IS_SOF) {
        usbd_event_sof_handler(0);
    }
#endif

    if (is & USB_IS_RESUME) {
        usbd_event_resume_handler(0);
    }

    if (is & USB_IS_SUSPEND) {
        usbd_event_suspend_handler(0);
    }

#if (CONFIG_USB_MUSB_DMA_TX || CONFIG_USB_MUSB_DMA_RX)
    /* Handle DMA interrupts */
    uint8_t dma_intr = HWREGB(USB_BASE + USB_DMA_INTR_OFFSET);
    if (dma_intr) {
        for (int ch = 0; ch < USB_DMA_CH_COUNT_AVAIL; ch++) {
            if (dma_intr & (1 << ch)) {
                struct dma_transfer *transfer = &dma_transfers[ch];
                ep_idx = transfer->ep_idx;
                //CLOGD("DMA ch%d INT, ep_idx = %d, len = %d\n", ch, ep_idx, transfer->len);
                if (ep_idx != 0xFF) { // && g_musb_udc.in_ep[ep_idx].dma_ch == ch
                    musb_set_active_ep(ep_idx); //CSK NOTE:
                    if (transfer->direction == 1) { // For USB device, 0 = RX, 1 = TX
#if CONFIG_USB_MUSB_DMA_TX
                        /* Clear DMA interrupt, NO NEED CLEAR for Read = Clear */
                        //HWREGB(USB_BASE + USB_DMA_INTR_OFFSET) = (1 << ch);

                        struct musb_ep_state *epp = &g_musb_udc.in_ep[ep_idx];
                        //uint8_t odd_data = 0;

                        //CSK NOTE: set TXRDY if necessary
                        if (transfer->len % epp->ep_mps) { // && transfer->len > 0
                            //odd_data = 1;
                            uint8_t tx_csrl = HWREGB(USB_TXCSRL_BASE(ep_idx));
                            if (!(tx_csrl & USB_TXCSRL1_TXRDY)) {
                                HWREGB(USB_TXCSRL_BASE(ep_idx)) = tx_csrl | USB_TXCSRL1_TXRDY;
                                //CLOGD("Set TXRDY!\n");
                            }
                        }

                        /* Disable DMA for endpoint */
                        HWREGH(USB_BASE + USB_DMA_CTRL_OFFSET(ch)) &= ~USB_DMACTL0_ENABLE;
                    #if CONFIG_MUSB_DPB
                        if (GET_EP_DPB(USB_EP_DIR_IN | ep_idx)) {
                            HWREGB(USB_TXCSRH_BASE(ep_idx)) &= ~(USB_TXCSRH1_DMAREQ_EN | USB_TXCSRH1_DMAREQ_MOD); // dma_reqmode = 0
                            epp->dma_reqmode = 0;
                        } else
                    #endif
                        {
                            HWREGB(USB_TXCSRH_BASE(ep_idx)) &= ~(USB_TXCSRH1_DMAREQ_EN); // | USB_TXCSRH1_DMAREQ_MOD
                        }

                        /* Free channel */
                        // DON'T set epp->dma_ch to 0xFF here, it can act as a flag of DMA operation
                        // which may be used in the handler of endpoint interrupt...
                        //epp->dma_ch = 0xFF;
                        transfer->ep_idx = 0xFF;
                        musb_dma_ch_free(ch);

                        /* Update transfer state */
                        epp->actual_xfer_len += transfer->len;
                        epp->xfer_len -= transfer->len;

                        // TX (IN), DMA Request Mode 1 (NO IN EP interrupt)
                        // Only first part of the whole TX process (RAM => IN EP FIFO => USB bus) is done
                        // OR All the data in the RX FIFO have been sent out...
                        if (epp->dma_reqmode == 1) { // CAN WORK
                        //if (epp->dma_reqmode == 1 || TX_IS_IDLE(ep_idx)) { // CAN WORK
                            // Call completion handler if transfer complete
                            if (epp->xfer_len == 0) {
                                HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
                                usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, epp->actual_xfer_len);
                            }
                        }
#endif // CONFIG_USB_MUSB_DMA_TX

                    } else { // RX

#if CONFIG_USB_MUSB_DMA_RX
                        /* Clear DMA interrupt, NO NEED CLEAR for Read = Clear */
                        //HWREGB(USB_BASE + USB_DMA_INTR_OFFSET) = (1 << ch);

                        struct musb_ep_state *epp = &g_musb_udc.out_ep[ep_idx];

                        /* Disable DMA for endpoint */
                        HWREGB(USB_RXCSRH_BASE(ep_idx)) &= ~(USB_RXCSRH1_DMAREQ_EN); // | USB_RXCSRH1_DMAREQ_MOD
                        HWREGH(USB_BASE + USB_DMA_CTRL_OFFSET(ch)) &= ~USB_DMACTL0_ENABLE; //CSK NOTE:

                        /* Free channel */
                        // DON'T set epp->dma_ch to 0xFF here, it can act as a flag of DMA operation
                        // which may be used in the handler of endpoint interrupt...
                        //epp->dma_ch = 0xFF;
                        transfer->ep_idx = 0xFF;
                        musb_dma_ch_free(ch);

                        //CLOGD("DMA RX: xfer_len = %d, DMA_CNT = %d\n", epp->xfer_len, HWREG(USB_BASE + USB_DMA_COUNT_OFFSET(ch)));

                        //CSK NOTE: clear RXRDY if necessary
                        uint8_t odd_data = 0;
                        if (transfer->len % epp->ep_mps && transfer->len > 0)
                            odd_data = 1;
                        if ((HWREGB(USB_RXCSRL_BASE(ep_idx)) & USB_RXCSRL1_RXRDY) && odd_data) { //
                            HWREGB(USB_RXCSRL_BASE(ep_idx)) &= ~(USB_RXCSRL1_RXRDY);
                            //CLOGD("CLR RXRDY!\n");
                        }

                        /* Update transfer state */
                        epp->actual_xfer_len += transfer->len;
                        epp->xfer_len -= transfer->len;

                        /* Call completion handler if transfer complete */
                        if (transfer->len < epp->ep_mps || epp->xfer_len == 0) {
//                            HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                            usbd_event_ep_out_complete_handler(0, ep_idx, epp->actual_xfer_len);
                        }
#endif // CONFIG_USB_MUSB_DMA_RX
                    }
                }
            }
        }
    }
#endif // (CONFIG_USB_MUSB_DMA_TX || CONFIG_USB_MUSB_DMA_RX)

    txis &= HWREGH(USB_BASE + MUSB_TXIE_OFFSET);
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
//        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
        musb_set_active_ep(0);
        handle_ep0();
        txis &= ~USB_TXIE_EP0;
    }

    ep_idx = 1;
    while (txis) {
        if (txis & (1 << ep_idx)) {
            struct musb_ep_state *epp = &g_musb_udc.in_ep[ep_idx];
            musb_set_active_ep(ep_idx);
//            HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_TXCSRL_BASE(ep_idx)) & USB_TXCSRL1_UNDRN) {
                HWREGB(USB_TXCSRL_BASE(ep_idx)) &= ~USB_TXCSRL1_UNDRN;
            }

            /* For DMA enabled endpoints, transfer is complete */
            //HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
            //usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, epp->actual_xfer_len);
            //CLOGD("TX EP%d: CANNOT come here when DMA TX!\n", ep_idx);

            if (epp->dma_ch == 0xFF) { // PIO (NOT DMA)
                if (epp->xfer_len > epp->ep_mps) {
                    epp->xfer_buf += epp->ep_mps;
                    epp->actual_xfer_len += epp->ep_mps;
                    epp->xfer_len -= epp->ep_mps;
                } else {
                    epp->xfer_buf += epp->xfer_len;
                    epp->actual_xfer_len += epp->xfer_len;
                    epp->xfer_len = 0;
                }
                //int a = epp->actual_xfer_len, b = ep_idx | 0x80; //TEST ONLY!!
                //CLOGD("PIO TX %dB @0x%x, xfer_len = %d\n", a, b, epp->xfer_len);

                if (epp->xfer_len == 0) {
                    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
                    usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, epp->actual_xfer_len);
                } else {
                    write_count = MIN(epp->xfer_len, epp->ep_mps);

                    musb_write_packet(ep_idx, epp->xfer_buf, write_count);
                    HWREGB(USB_TXCSRL_BASE(ep_idx)) = USB_TXCSRL1_TXRDY;
                }
            } else if (epp->dma_reqmode == 0) { // DMA, with DMA Request Mode 0
                /* Update transfer state */
                // Now dma_transfers[dma_ch] may be dynamically used by other EP,
                // So DON'T update EP transfer state here!!
                //epp->actual_xfer_len += dma_transfers[dma_ch].len;
                //epp->xfer_len -= dma_transfers[dma_ch].len;

                /* Call completion handler if transfer complete */
                if (epp->xfer_len == 0) { //odd_data ||
                    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
                    usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, epp->actual_xfer_len);
                }

            }

            txis &= ~(1 << ep_idx);
        }
        ep_idx++;
    }

    rxis &= HWREGH(USB_BASE + MUSB_RXIE_OFFSET);
    ep_idx = 1;
    while (rxis) {
        if (rxis & (1 << ep_idx)) {
            struct musb_ep_state *epp = &g_musb_udc.out_ep[ep_idx];
            musb_set_active_ep(ep_idx);
//            HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_RXCSRL_BASE(ep_idx)) & USB_RXCSRL1_RXRDY) {
                read_count = HWREGH(USB_RXCOUNT_BASE(ep_idx));
                //CLOGD("EP%d RXCNT: %d\n", ep_idx, read_count);
                //ret = false;
#if CONFIG_USB_MUSB_DMA_RX
                uint8_t dma_ch = epp->dma_ch;
                if (dma_ch != 0xFF) {
                    if (read_count > 0) { // there's some data in the RX FIFO
                        //NOTE: Experiments show that Restarting DMA is much faster than CPU reading!
                    #if 1 // //read short packet by restarting DMA with read_count, DMA Mode 0
                        uint32_t xfered = dma_transfers[dma_ch].len - HWREGH(USB_BASE + USB_DMA_COUNT_OFFSET(dma_ch));
                        if (xfered > 0)
                            epp->actual_xfer_len += xfered;
                        epp->xfer_len = read_count;
                        dma_transfers[dma_ch].len = read_count;

                        // Restart DMA transfer after DMA_COUNT is changed and DMA mode is set to 0
                        HWREG(USB_BASE + USB_DMA_COUNT_OFFSET(dma_ch)) = read_count;
                        HWREGH(USB_BASE + USB_DMA_CTRL_OFFSET(dma_ch)) &= ~USB_DMACTL0_MODE1;
                    #else //read short packet via CPU
                        musb_read_packet(ep_idx, epp->xfer_buf, read_count);
                        epp->xfer_buf += read_count;
                        epp->actual_xfer_len += read_count;
                        epp->xfer_len -= read_count;
                        HWREGB(USB_RXCSRL_BASE(ep_idx)) &= ~(USB_RXCSRL1_RXRDY);
                        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                        usbd_event_ep_out_complete_handler(0, ep_idx, epp->actual_xfer_len);
                    #endif
                    } else { // NO data in the RX FIFO, and it is ZLP!
                        HWREGB(USB_RXCSRL_BASE(ep_idx)) &= ~(USB_RXCSRL1_RXRDY);
                        //CLOGD("CLR RXRDY for ZLP!\n");
                        if (epp->xfer_len > 0) { // NO enough data indicates "terminate early"!
                            HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                            usbd_event_ep_out_complete_handler(0, ep_idx, epp->actual_xfer_len);
                        }
                    }
                } else
#endif
                { // Fallback to CPU transfer
                    if (read_count > 0) {
                        musb_read_packet(ep_idx, epp->xfer_buf, read_count);
                        epp->xfer_buf += read_count;
                        epp->actual_xfer_len += read_count;
                        epp->xfer_len -= read_count;
                    }
                    HWREGB(USB_RXCSRL_BASE(ep_idx)) &= ~(USB_RXCSRL1_RXRDY);
                    // it indicates ZLP when read_count == 0, and it's one case of "read_count < epp->ep_mps"
                    if ((read_count < epp->ep_mps) || (epp->xfer_len == 0)) {
                        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                        usbd_event_ep_out_complete_handler(0, ep_idx, epp->actual_xfer_len);
                    }
                }
            }
            rxis &= ~(1 << ep_idx);
        }
        ep_idx++;
    }

    musb_set_active_ep(old_ep_idx);
}
