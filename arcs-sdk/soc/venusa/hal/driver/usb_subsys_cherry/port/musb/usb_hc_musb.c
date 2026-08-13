/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_musb_reg.h"

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#define USB_BASE (bus->hcd.reg_base)

#ifdef CONFIG_USB_MUSB_SUNXI
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

#define MUSB_TXFUNCADDR0_OFFSET 0x80
#define MUSB_TXHUBADDR0_OFFSET  0x82
#define MUSB_TXHUBPORT0_OFFSET  0x83
#define MUSB_TXFUNCADDRx_OFFSET 0x88
#define MUSB_TXHUBADDRx_OFFSET  0x8A
#define MUSB_TXHUBPORTx_OFFSET  0x8B
#define MUSB_RXFUNCADDRx_OFFSET 0x8C
#define MUSB_RXHUBADDRx_OFFSET  0x8E
#define MUSB_RXHUBPORTx_OFFSET  0x8F

#define USB_TXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx)
#define USB_TXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 2)
#define USB_TXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 3)
#define USB_RXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 4)
#define USB_RXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 6)
#define USB_RXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 7)
#endif

#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

/* ============================================================
 * Host-side DMA Mode 1 support.
 * Define CONFIG_USBH_MUSB_DMA_TX to use DMA for bulk OUT (TX) transfers.
 * Undefine to fall back to PIO (CPU FIFO write) mode.
 * ============================================================ */
#define CONFIG_USBH_MUSB_DMA_TX
#define CONFIG_USBH_MUSB_DMA_RX


#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
/* DMA register offsets (channels 1-based) */
#define MUSB_HC_DMA_INTR_OFFSET   0x200
#define MUSB_HC_DMA_CNTL(ch)      (0x204 + ((ch) - 1) * 0x10)
#define MUSB_HC_DMA_ADDR(ch)      (0x208 + ((ch) - 1) * 0x10)
#define MUSB_HC_DMA_COUNT(ch)     (0x20C + ((ch) - 1) * 0x10)

/* DMA CNTL register bits */
#define HC_DMA_CNTL_ENABLE        (1 << 0)
#define HC_DMA_CNTL_DIR_TX        (1 << 1)   /* 1=TX (mem->FIFO), 0=RX (FIFO->mem) */
#define HC_DMA_CNTL_DIR_RX        0
#define HC_DMA_CNTL_MODE1         (1 << 2)   /* 0=Mode0, 1=Mode1 */
#define HC_DMA_CNTL_IE            (1 << 3)   /* Interrupt Enable */
#define HC_DMA_CNTL_EP(n)         ((n) << 4) /* Endpoint number [7:4] */
#define HC_DMA_CNTL_ERR           (1 << 8)   /* Bus Error */
#define HC_DMA_CNTL_BURST_INCR16  (3 << 9)

/* ---- Dynamic DMA Channel Pool ----
 * MUSB has 4 internal DMA channels (1-4). We manage them dynamically so
 * the driver automatically adapts to any EP configuration without the
 * customer needing to modify macros.
 * Channel 0 is reserved (EP0 never uses DMA). */
#define MUSB_DMA_CH_NUM   6   /* Total hardware DMA channels */

static uint8_t g_hc_dma_ch_owner[MUSB_DMA_CH_NUM + 1]; /* index 1..4, 0=unused marker. Value = EP idx owning it, 0xFF = free */

static void musb_hc_dma_pool_init(void)
{
    for (int i = 1; i <= MUSB_DMA_CH_NUM; i++)
        g_hc_dma_ch_owner[i] = 0xFF;
}

/* Allocate a free DMA channel, returns channel number (1-4) or 0 on failure */
static uint8_t musb_hc_dma_ch_alloc(uint8_t ep_idx)
{
    for (int i = 1; i <= MUSB_DMA_CH_NUM; i++) {
        if (g_hc_dma_ch_owner[i] == 0xFF) {
            g_hc_dma_ch_owner[i] = ep_idx;
            return (uint8_t)i;
        }
    }
    return 0; /* No free channel */
}

/* Free a DMA channel back to the pool */
static void musb_hc_dma_ch_free(uint8_t ch)
{
    if (ch >= 1 && ch <= MUSB_DMA_CH_NUM)
        g_hc_dma_ch_owner[ch] = 0xFF;
}

/* Find which DMA channel is currently owned by a given EP (for a given direction).
 * dir: 0=RX, 1=TX. Returns channel number or 0 if none. */
static uint8_t musb_hc_dma_ch_find(uint8_t ep_idx)
{
    for (int i = 1; i <= MUSB_DMA_CH_NUM; i++) {
        if (g_hc_dma_ch_owner[i] == ep_idx)
            return (uint8_t)i;
    }
    return 0;
}
#endif

typedef enum {
    USB_EP0_STATE_SETUP = 0x0, /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA,     /**< IN DATA */
    USB_EP0_STATE_IN_STATUS,   /**< IN status*/
    USB_EP0_STATE_OUT_DATA,    /**< OUT DATA */
    USB_EP0_STATE_OUT_STATUS,  /**< OUT status */
} ep0_state_t;

struct musb_pipe {
    uint8_t chidx;
    bool inuse;
    uint32_t xfrd;
    volatile uint8_t ep0_state;
    usb_osal_sem_t waitsem;
    struct usbh_urb *urb;
};

struct musb_hcd {
    volatile bool port_csc;
    volatile bool port_pec;
    volatile bool port_pe;
    struct musb_pipe pipe_pool[CONFIG_USBHOST_PIPE_NUM];    /* TX (OUT) + EP0 */
    struct musb_pipe rx_pipe_pool[CONFIG_USBHOST_PIPE_NUM]; /* RX (IN) */
} g_musb_hcd[CONFIG_USBHOST_MAX_BUS];

/* get current active ep */
static uint8_t musb_get_active_ep(struct usbh_bus *bus)
{
    return HWREGB(USB_BASE + MUSB_EPIDX_OFFSET);
}

/* set the active ep */
static void musb_set_active_ep(struct usbh_bus *bus, uint8_t ep_index)
{
    HWREGB(USB_BASE + MUSB_EPIDX_OFFSET) = ep_index;
}

static void musb_fifo_flush(struct usbh_bus *bus, uint8_t ep)
{
    uint8_t ep_idx = ep & 0x7f;
    if (ep_idx == 0) {
        if ((HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & (USB_CSRL0_RXRDY | USB_CSRL0_TXRDY)) != 0)
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_CSRH0_FLUSH;
    } else {
        if (ep & 0x80) {
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_FLUSH;
        } else {
            if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_FLUSH;
        }
    }
}

static void musb_write_packet(struct usbh_bus *bus, uint8_t ep_idx, uint8_t *buffer, uint16_t len)
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

static void musb_read_packet(struct usbh_bus *bus, uint8_t ep_idx, uint8_t *buffer, uint16_t len)
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

static uint32_t musb_get_fifo_size(uint16_t mps, uint16_t *used)
{
    uint32_t size;

    for (uint8_t i = USB_TXFIFOSZ_SIZE_8; i <= USB_TXFIFOSZ_SIZE_2048; i++) {
        size = (8 << i);
        if (mps <= size) {
            *used = size;
            return i;
        }
    }

    *used = 0;
    return USB_TXFIFOSZ_SIZE_8;
}

static uint32_t usbh_musb_fifo_config(struct usbh_bus *bus, struct musb_fifo_cfg *cfg, uint32_t offset)
{
    uint16_t fifo_used;
    uint8_t c_size;
    uint16_t c_off;

    c_off = offset >> 3;
    c_size = musb_get_fifo_size(cfg->maxpacket, &fifo_used);

    musb_set_active_ep(bus, cfg->ep_num);

    switch (cfg->style) {
        case FIFO_TX:
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = c_size & 0x0f;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = c_off;
            break;
        case FIFO_RX:
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = c_size & 0x0f;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = c_off;
            break;
        case FIFO_TXRX:
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = c_size & 0x0f;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = c_off;
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = c_size & 0x0f;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = c_off;
            break;

        default:
            break;
    }

    return (offset + fifo_used);
}

void musb_control_urb_init(struct usbh_bus *bus, uint8_t chidx, struct usbh_urb *urb, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    uint8_t old_ep_index;
    uint8_t speed = USB_TXTYPE1_SPEED_FULL;

    old_ep_index = musb_get_active_ep(bus);
    musb_set_active_ep(bus, chidx);

    if (urb->hport->speed == USB_SPEED_HIGH) {
        speed = USB_TYPE0_SPEED_HIGH;
    } else if (urb->hport->speed == USB_SPEED_FULL) {
        speed = USB_TYPE0_SPEED_FULL;
    } else if (urb->hport->speed == USB_SPEED_LOW) {
        speed = USB_TYPE0_SPEED_LOW;
    }

    HWREGB(USB_TXADDR_BASE(chidx)) = urb->hport->dev_addr;
    HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = speed;
    HWREGB(USB_TXHUBADDR_BASE(chidx)) = 0;
    HWREGB(USB_TXHUBPORT_BASE(chidx)) = 0;

    musb_write_packet(bus, chidx, (uint8_t *)setup, 8);
    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY | USB_CSRL0_SETUP;
    musb_set_active_ep(bus, old_ep_index);
}

int musb_bulk_urb_init(struct usbh_bus *bus, uint8_t chidx, struct usbh_urb *urb, uint8_t *buffer, uint32_t buflen)
{
    uint8_t old_ep_index;
    uint8_t speed = USB_TXTYPE1_SPEED_FULL;

    old_ep_index = musb_get_active_ep(bus);
    musb_set_active_ep(bus, chidx);

    if (urb->hport->speed == USB_SPEED_HIGH) {
        speed = USB_TXTYPE1_SPEED_HIGH;
    } else if (urb->hport->speed == USB_SPEED_FULL) {
        speed = USB_TXTYPE1_SPEED_FULL;
    } else if (urb->hport->speed == USB_SPEED_LOW) {
        speed = USB_TXTYPE1_SPEED_LOW;
    }

    if (urb->ep->bEndpointAddress & 0x80) {
        if ((8 << HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET)) < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            USB_LOG_ERR("Ep %02x fifo is overflow\r\n", urb->ep->bEndpointAddress);
            return -USB_ERR_RANGE;
        }

        HWREGB(USB_RXADDR_BASE(chidx)) = urb->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = (urb->ep->bEndpointAddress & 0x0f) | speed | USB_TXTYPE1_PROTO_BULK;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = 0;
        HWREGB(USB_RXHUBADDR_BASE(chidx)) = 0;
        HWREGB(USB_RXHUBPORT_BASE(chidx)) = 0;
        HWREGH(USB_BASE + MUSB_IND_RXMAP_OFFSET) = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);

#ifdef CONFIG_USBH_MUSB_DMA_RX
        /* DMA Mode 1 RX: only for transfers >= 1 MaxPacketSize.
         * Sub-packet transfers (e.g. 13-byte CSW) MUST use PIO,
         * because DMA Mode 1 + AUTORQ will hang waiting for a
         * full packet that never arrives. */
        if (buflen >= USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            uint8_t dma_ch = musb_hc_dma_ch_alloc(chidx);
            if (dma_ch == 0) goto rx_pio_fallback; /* No free DMA channel, use PIO */

            /* Flush FIFO */
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_FLUSH;

            /* Set up DMA channel */
            HWREG(USB_BASE + MUSB_HC_DMA_ADDR(dma_ch)) = (uint32_t)buffer;
            HWREG(USB_BASE + MUSB_HC_DMA_COUNT(dma_ch)) = buflen;

            /* Enable RX interrupt for error detection */
            HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << chidx);

            /* 1. Start DMA Engine FIRST so it is ready to capture the request edge */
            HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) =
                HC_DMA_CNTL_ENABLE | HC_DMA_CNTL_DIR_RX | HC_DMA_CNTL_MODE1 |
                HC_DMA_CNTL_IE | HC_DMA_CNTL_BURST_INCR16 | HC_DMA_CNTL_EP(chidx);

            /* 2. Configure RXCSRH (Tells MUSB Core to use DMA and triggers AUTOREQ if empty) */
            HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) = USB_RXCSRH1_AUTOCL | USB_RXCSRH1_AUTORQ | USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD;

            /* 3. Manually trigger the first REQPKT to start the IN token train */
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
        } else {
            rx_pio_fallback:
            /* Small transfer: PIO fallback */
            HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
            HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << chidx);
        }
#else
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;

        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << chidx);
#endif /* CONFIG_USBH_MUSB_DMA_RX */
    } else {
        if ((8 << HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET)) < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            USB_LOG_ERR("Ep %02x fifo is overflow\r\n", urb->ep->bEndpointAddress);
            return -USB_ERR_RANGE;
        }

        HWREGB(USB_TXADDR_BASE(chidx)) = urb->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = (urb->ep->bEndpointAddress & 0x0f) | speed | USB_TXTYPE1_PROTO_BULK;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = 0;
        HWREGB(USB_TXHUBADDR_BASE(chidx)) = 0;
        HWREGB(USB_TXHUBPORT_BASE(chidx)) = 0;
        HWREGH(USB_BASE + MUSB_IND_TXMAP_OFFSET) = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);

#ifdef CONFIG_USBH_MUSB_DMA_TX
        /* DMA Mode 1 TX: only for transfers >= 1 MaxPacketSize.
         * Sub-packet transfers (e.g. 31-byte CBW) use PIO to avoid
         * AUTOSET issues with non-full last packets. */
        if (buflen >= USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            uint8_t dma_ch = musb_hc_dma_ch_alloc(chidx);
            if (dma_ch == 0) goto tx_pio_fallback; /* No free DMA channel, use PIO */

            /* Flush FIFO, ensure clean state */
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_FLUSH;

            /* Set up DMA channel */
            HWREG(USB_BASE + MUSB_HC_DMA_ADDR(dma_ch)) = (uint32_t)buffer;
            HWREG(USB_BASE + MUSB_HC_DMA_COUNT(dma_ch)) = buflen;

            /* Configure TXCSRH: Host mode + DMA Enable + DMA Mode 1 + AutoSet */
            HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) = USB_TXCSRH1_MODE | USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET;

            /* Enable TX interrupt for error detection */
            HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << chidx);

            /* Start DMA last */
            HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) =
                HC_DMA_CNTL_ENABLE | HC_DMA_CNTL_DIR_TX | HC_DMA_CNTL_MODE1 |
                HC_DMA_CNTL_IE | HC_DMA_CNTL_BURST_INCR16 | HC_DMA_CNTL_EP(chidx);
        } else {
            tx_pio_fallback:
            /* Small transfer: PIO fallback */
            musb_write_packet(bus, chidx, buffer, buflen);
            HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
            HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << chidx);
        }
#else
        /* PIO (CPU) mode: write first packet manually */
        if (buflen > USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            buflen = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);
        }

        musb_write_packet(bus, chidx, buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;

        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << chidx);
#endif /* CONFIG_USBH_MUSB_DMA_TX */
    }
    musb_set_active_ep(bus, old_ep_index);
    return 0;
}

int musb_intr_urb_init(struct usbh_bus *bus, uint8_t chidx, struct usbh_urb *urb, uint8_t *buffer, uint32_t buflen)
{
    uint8_t old_ep_index;
    uint8_t speed = USB_TXTYPE1_SPEED_FULL;

    old_ep_index = musb_get_active_ep(bus);
    musb_set_active_ep(bus, chidx);

    if (urb->hport->speed == USB_SPEED_HIGH) {
        speed = USB_TXTYPE1_SPEED_HIGH;
    } else if (urb->hport->speed == USB_SPEED_FULL) {
        speed = USB_TXTYPE1_SPEED_FULL;
    } else if (urb->hport->speed == USB_SPEED_LOW) {
        speed = USB_TXTYPE1_SPEED_LOW;
    }

    if (urb->ep->bEndpointAddress & 0x80) {
        if ((8 << HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET)) < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            USB_LOG_ERR("Ep %02x fifo is overflow\r\n", urb->ep->bEndpointAddress);
            return -USB_ERR_RANGE;
        }

        HWREGB(USB_RXADDR_BASE(chidx)) = urb->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = (urb->ep->bEndpointAddress & 0x0f) | speed | USB_TXTYPE1_PROTO_INT;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = urb->ep->bInterval;
        HWREGB(USB_RXHUBADDR_BASE(chidx)) = 0;
        HWREGB(USB_RXHUBPORT_BASE(chidx)) = 0;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;

        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << chidx);
    } else {
        if ((8 << HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET)) < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            USB_LOG_ERR("Ep %02x fifo is overflow\r\n", urb->ep->bEndpointAddress);
            return -USB_ERR_RANGE;
        }

        HWREGB(USB_TXADDR_BASE(chidx)) = urb->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = (urb->ep->bEndpointAddress & 0x0f) | speed | USB_TXTYPE1_PROTO_INT;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = urb->ep->bInterval;
        HWREGB(USB_TXHUBADDR_BASE(chidx)) = 0;
        HWREGB(USB_TXHUBPORT_BASE(chidx)) = 0;

        if (buflen > USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            buflen = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);
        }

        musb_write_packet(bus, chidx, buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;

        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << chidx);
    }
    musb_set_active_ep(bus, old_ep_index);
    return 0;
}

int musb_iso_urb_init(struct usbh_bus *bus, uint8_t chidx, struct usbh_urb *urb, uint8_t *buffer, uint32_t buflen)
{
    uint8_t old_ep_index;
    uint8_t speed = USB_TXTYPE1_SPEED_FULL;

    old_ep_index = musb_get_active_ep(bus);
    musb_set_active_ep(bus, chidx);

    if (urb->hport->speed == USB_SPEED_HIGH) {
        speed = USB_TXTYPE1_SPEED_HIGH;
    } else if (urb->hport->speed == USB_SPEED_FULL) {
        speed = USB_TXTYPE1_SPEED_FULL;
    } else if (urb->hport->speed == USB_SPEED_LOW) {
        speed = USB_TXTYPE1_SPEED_LOW;
    }

    if (urb->ep->bEndpointAddress & 0x80) {
        /* === ISO IN (RX) === */
        if ((8 << HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET)) < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            USB_LOG_ERR("Ep %02x fifo is overflow\r\n", urb->ep->bEndpointAddress);
            return -USB_ERR_RANGE;
        }

        HWREGB(USB_RXADDR_BASE(chidx)) = urb->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = (urb->ep->bEndpointAddress & 0x0f) | speed | USB_TXTYPE1_PROTO_ISOC;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = urb->ep->bInterval;
        HWREGB(USB_RXHUBADDR_BASE(chidx)) = 0;
        HWREGB(USB_RXHUBPORT_BASE(chidx)) = 0;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;

        /* PIO mode: AUTORQ for continuous ISO, REQPKT to start */
        HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) |= USB_RXCSRH1_AUTORQ;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << chidx);
    } else {
        /* === ISO OUT (TX) === */
        if ((8 << HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET)) < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            USB_LOG_ERR("Ep %02x fifo is overflow\r\n", urb->ep->bEndpointAddress);
            return -USB_ERR_RANGE;
        }

        HWREGB(USB_TXADDR_BASE(chidx)) = urb->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = (urb->ep->bEndpointAddress & 0x0f) | speed | USB_TXTYPE1_PROTO_ISOC;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = urb->ep->bInterval;
        HWREGB(USB_TXHUBADDR_BASE(chidx)) = 0;
        HWREGB(USB_TXHUBPORT_BASE(chidx)) = 0;

        if (buflen > USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
            buflen = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);
        }

        musb_write_packet(bus, chidx, buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;

        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << chidx);
    }
    musb_set_active_ep(bus, old_ep_index);
    return 0;
}

static int usbh_reset_port(struct usbh_bus *bus, const uint8_t port)
{
    g_musb_hcd[bus->hcd.hcd_id].port_pe = 0;
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_RESET;
    usb_osal_msleep(20);
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~(USB_POWER_RESET);
    usb_osal_msleep(20);
    g_musb_hcd[bus->hcd.hcd_id].port_pe = 1;
    return 0;
}

static uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port)
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

#if 0
static int musb_pipe_alloc(void)
{
    int chidx;

    for (chidx = 1; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        if (!g_musb_hcd[bus->hcd.hcd_id].pipe_pool[chidx].inuse) {
            g_musb_hcd[bus->hcd.hcd_id].pipe_pool[chidx].inuse = true;
            return chidx;
        }
    }

    return -1;
}
#endif

static void musb_pipe_free(struct musb_pipe *pipe)
{
#if 0
    pipe->inuse = false;
#endif
}

__WEAK void usb_hc_low_level_init(struct usbh_bus *bus)
{
    (void)bus;
}

__WEAK void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    (void)bus;
}

int usb_hc_init(struct usbh_bus *bus)
{
    uint8_t regval;
    uint16_t offset = 0;
    uint8_t cfg_num;
    struct musb_fifo_cfg *cfg;

    memset(&g_musb_hcd[bus->hcd.hcd_id], 0, sizeof(struct musb_hcd));

#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
    musb_hc_dma_pool_init();
#endif

    for (uint8_t i = 0; i < CONFIG_USBHOST_PIPE_NUM; i++) {
        g_musb_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem = usb_osal_sem_create(0);
        g_musb_hcd[bus->hcd.hcd_id].rx_pipe_pool[i].waitsem = usb_osal_sem_create(0);
    }

    usb_hc_low_level_init(bus);

    cfg_num = usbh_get_musb_fifo_cfg(&cfg);

    for (uint8_t i = 0; i < cfg_num; i++) {
        offset = usbh_musb_fifo_config(bus, &cfg[i], offset);
    }

    USB_ASSERT_MSG(offset <= usb_get_musb_ram_size(), "Your fifo config is overflow, please check");

    /* Enable USB interrupts */
    regval = USB_IE_RESET | USB_IE_CONN | USB_IE_DISCON |
             USB_IE_RESUME | USB_IE_SUSPND |
             USB_IE_BABBLE | USB_IE_SESREQ | USB_IE_VBUSERR;

    HWREGB(USB_BASE + MUSB_IE_OFFSET) = regval;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;

    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) |= USB_DEVCTL_SESSION;

#ifdef CONFIG_USB_MUSB_SUNXI
    musb_set_active_ep(bus, 0);
    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
#endif
    return 0;
}

int usb_hc_deinit(struct usbh_bus *bus)
{
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = 0;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = 0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_HSENAB;
    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) &= ~USB_DEVCTL_SESSION;

    for (uint8_t i = 0; i < CONFIG_USBHOST_PIPE_NUM; i++) {
        usb_osal_sem_delete(g_musb_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem);
        usb_osal_sem_delete(g_musb_hcd[bus->hcd.hcd_id].rx_pipe_pool[i].waitsem);
    }

    usb_hc_low_level_deinit(bus);
    return 0;
}

int usbh_roothub_control(struct usbh_bus *bus, struct usb_setup_packet *setup, uint8_t *buf)
{
    uint8_t nports;
    uint8_t port;
    uint32_t status;

    nports = CONFIG_USBHOST_MAX_RHPORTS;
    port = setup->wIndex;
    if (setup->bmRequestType & USB_REQUEST_RECIPIENT_DEVICE) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -USB_ERR_INVAL;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -USB_ERR_INVAL;
                }
                break;
            case HUB_REQUEST_GET_DESCRIPTOR:
                break;
            case HUB_REQUEST_GET_STATUS:
                memset(buf, 0, 4);
                break;
            default:
                break;
        }
    } else if (setup->bmRequestType & USB_REQUEST_RECIPIENT_OTHER) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_ENABLE:
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        g_musb_hcd[bus->hcd.hcd_id].port_csc = 0;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        g_musb_hcd[bus->hcd.hcd_id].port_pec = 0;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
                        break;
                    default:
                        return -USB_ERR_INVAL;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        usbh_reset_port(bus, port);
                        break;

                    default:
                        return -USB_ERR_INVAL;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
                }

                status = 0;
                if (g_musb_hcd[bus->hcd.hcd_id].port_csc) {
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                }
                if (g_musb_hcd[bus->hcd.hcd_id].port_pec) {
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }

                if (g_musb_hcd[bus->hcd.hcd_id].port_pe) {
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);
                    if (usbh_get_port_speed(bus, port) == USB_SPEED_LOW) {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    } else if (usbh_get_port_speed(bus, port) == USB_SPEED_HIGH) {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                }

                status |= (1 << HUB_PORT_FEATURE_POWER);
                memcpy(buf, &status, 4);
                break;
            default:
                break;
        }
    }
    return 0;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    struct musb_pipe *pipe;
    struct usbh_bus *bus;
    int chidx;
    size_t flags;
    int ret = 0;

    if (!urb || !urb->hport || !urb->ep || !urb->hport->bus) {
        return -USB_ERR_INVAL;
    }

    if (!urb->hport->connected) {
        return -USB_ERR_NOTCONN;
    }

    if (urb->errorcode == -USB_ERR_BUSY) {
        return -USB_ERR_BUSY;
    }

    bus = urb->hport->bus;

    if (USB_GET_ENDPOINT_TYPE(urb->ep->bmAttributes) == USB_ENDPOINT_TYPE_CONTROL) {
        chidx = 0;
    } else {
        chidx = (urb->ep->bEndpointAddress & 0x0f);

        if (chidx > (CONFIG_USBHOST_PIPE_NUM - 1)) {
            return -USB_ERR_RANGE;
        }
    }

    flags = usb_osal_enter_critical_section();

    if (urb->ep->bEndpointAddress & 0x80) {
        pipe = &g_musb_hcd[bus->hcd.hcd_id].rx_pipe_pool[chidx];
    } else {
        pipe = &g_musb_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
    }
    pipe->chidx = chidx;
    pipe->urb = urb;

    urb->hcpriv = pipe;
    urb->errorcode = -USB_ERR_BUSY;
    urb->actual_length = 0;

    switch (USB_GET_ENDPOINT_TYPE(urb->ep->bmAttributes)) {
        case USB_ENDPOINT_TYPE_CONTROL:
            pipe->ep0_state = USB_EP0_STATE_SETUP;
            musb_control_urb_init(bus, 0, urb, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_BULK:
            ret = musb_bulk_urb_init(bus, chidx, urb, urb->transfer_buffer, urb->transfer_buffer_length);
            if (ret < 0) {
                return ret;
            }
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            ret = musb_intr_urb_init(bus, chidx, urb, urb->transfer_buffer, urb->transfer_buffer_length);
            if (ret < 0) {
                return ret;
            }
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            ret = musb_iso_urb_init(bus, chidx, urb, urb->transfer_buffer, urb->transfer_buffer_length);
            if (ret < 0) {
                return ret;
            }
            break;
        default:
            break;
    }
    usb_osal_leave_critical_section(flags);

    if (urb->timeout > 0) {
        /* wait until timeout or sem give */
        ret = usb_osal_sem_take(pipe->waitsem, urb->timeout);
        if (ret < 0) {
            goto errout_timeout;
        }
        urb->timeout = 0;
        ret = urb->errorcode;
        /* we can free pipe when waitsem is done */
        musb_pipe_free(pipe);
    }
    return ret;
errout_timeout:
    urb->timeout = 0;
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    struct musb_pipe *pipe;
    struct usbh_bus *bus;
    size_t flags;

    if (!urb || !urb->hcpriv || !urb->hport->bus) {
        return -USB_ERR_INVAL;
    }

    bus = urb->hport->bus;

    ARG_UNUSED(bus);

    flags = usb_osal_enter_critical_section();

    pipe = (struct musb_pipe *)urb->hcpriv;
    urb->hcpriv = NULL;
    urb->errorcode = -USB_ERR_SHUTDOWN;
    pipe->urb = NULL;

    if (urb->ep->bEndpointAddress & 0x80) {
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << (urb->ep->bEndpointAddress & 0x0f));
        HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << (urb->ep->bEndpointAddress & 0x0f));
    } else {
        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << (urb->ep->bEndpointAddress & 0x0f));
        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << (urb->ep->bEndpointAddress & 0x0f));
    }

    musb_fifo_flush(bus, urb->ep->bEndpointAddress);

    if (urb->timeout) {
        usb_osal_sem_give(pipe->waitsem);
    } else {
        musb_pipe_free(pipe);
    }

    usb_osal_leave_critical_section(flags);
    return 0;
}

static void musb_urb_waitup(struct usbh_urb *urb)
{
    struct musb_pipe *pipe;

    pipe = (struct musb_pipe *)urb->hcpriv;
    pipe->urb = NULL;
    urb->hcpriv = NULL;

    if (urb->timeout) {
        usb_osal_sem_give(pipe->waitsem);
    } else {
        musb_pipe_free(pipe);
    }

    if (urb->complete) {
        if (urb->errorcode < 0) {
            urb->complete(urb->arg, urb->errorcode);
        } else {
            urb->complete(urb->arg, urb->actual_length);
        }
    }
}

void handle_ep0(struct usbh_bus *bus)
{
    uint8_t ep0_status;
    struct musb_pipe *pipe;
    struct usbh_urb *urb;
    uint32_t size;

    pipe = (struct musb_pipe *)&g_musb_hcd[bus->hcd.hcd_id].pipe_pool[0];
    urb = pipe->urb;
    if (urb == NULL) {
        return;
    }

    musb_set_active_ep(bus, 0);
    ep0_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);
    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        pipe->ep0_state = USB_EP0_STATE_SETUP;
        urb->errorcode = -USB_ERR_STALL;
        musb_urb_waitup(urb);
        return;
    }
    if (ep0_status & USB_CSRL0_ERROR) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_ERROR;
        musb_fifo_flush(bus, 0);
        pipe->ep0_state = USB_EP0_STATE_SETUP;
        urb->errorcode = -USB_ERR_IO;
        musb_urb_waitup(urb);
        return;
    }
    if (ep0_status & USB_CSRL0_STALL) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALL;
        pipe->ep0_state = USB_EP0_STATE_SETUP;
        urb->errorcode = -USB_ERR_STALL;
        musb_urb_waitup(urb);
        return;
    }

    switch (pipe->ep0_state) {
        case USB_EP0_STATE_SETUP:
            urb->actual_length += 8;
            if (urb->transfer_buffer_length) {
                if (urb->setup->bmRequestType & 0x80) {
                    pipe->ep0_state = USB_EP0_STATE_IN_DATA;
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_REQPKT;
                } else {
                    pipe->ep0_state = USB_EP0_STATE_OUT_DATA;
                    size = urb->transfer_buffer_length;
                    if (size > USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
                        size = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);
                    }

                    musb_write_packet(bus, 0, urb->transfer_buffer, size);
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;

                    urb->transfer_buffer += size;
                    urb->transfer_buffer_length -= size;
                    urb->actual_length += size;
                }
            } else {
                pipe->ep0_state = USB_EP0_STATE_IN_STATUS;
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_REQPKT | USB_CSRL0_STATUS);
            }
            break;
        case USB_EP0_STATE_IN_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                size = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
                musb_read_packet(bus, 0, urb->transfer_buffer, size);
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_RXRDY;
                urb->transfer_buffer += size;
                urb->transfer_buffer_length -= size;
                urb->actual_length += size;

                if ((size < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) || (urb->transfer_buffer_length == 0)) {
                    pipe->ep0_state = USB_EP0_STATE_OUT_STATUS;
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_STATUS);
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_REQPKT;
                }
            }
            break;
        case USB_EP0_STATE_OUT_DATA:
            if (urb->transfer_buffer_length > 0) {
                size = urb->transfer_buffer_length;
                if (size > USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
                    size = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);
                }

                musb_write_packet(bus, 0, urb->transfer_buffer, size);
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;

                urb->transfer_buffer += size;
                urb->transfer_buffer_length -= size;
                urb->actual_length += size;
            } else {
                pipe->ep0_state = USB_EP0_STATE_IN_STATUS;
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_REQPKT | USB_CSRL0_STATUS);
            }
            break;
        case USB_EP0_STATE_OUT_STATUS:
            urb->errorcode = 0;
            musb_urb_waitup(urb);
            break;
        case USB_EP0_STATE_IN_STATUS:
            if (ep0_status & (USB_CSRL0_RXRDY | USB_CSRL0_STATUS)) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~(USB_CSRL0_RXRDY | USB_CSRL0_STATUS);
                urb->errorcode = 0;
                musb_urb_waitup(urb);
            }
            break;
    }
}

void USBH_IRQHandler(uint8_t busid)
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint8_t ep_csrl_status;
    // uint8_t ep_csrh_status;
    struct musb_pipe *pipe;
    struct usbh_urb *urb;
    uint8_t ep_idx;
    uint8_t old_ep_idx;
    struct usbh_bus *bus;
    uint32_t size;

    bus = &g_usbhost_bus[busid];

    is = HWREGB(USB_BASE + MUSB_IS_OFFSET);
    txis = HWREGH(USB_BASE + MUSB_TXIS_OFFSET);
    rxis = HWREGH(USB_BASE + MUSB_RXIS_OFFSET);

    HWREGB(USB_BASE + MUSB_IS_OFFSET) = is;

    old_ep_idx = musb_get_active_ep(bus);

    if (is & USB_IS_CONN) {
        g_musb_hcd[bus->hcd.hcd_id].port_csc = 1;
        g_musb_hcd[bus->hcd.hcd_id].port_pec = 1;
        g_musb_hcd[bus->hcd.hcd_id].port_pe = 1;
        bus->hcd.roothub.int_buffer[0] = (1 << 1);
        usbh_hub_thread_wakeup(&bus->hcd.roothub);
    }

    if (is & USB_IS_DISCON) {
        g_musb_hcd[bus->hcd.hcd_id].port_csc = 1;
        g_musb_hcd[bus->hcd.hcd_id].port_pec = 1;
        g_musb_hcd[bus->hcd.hcd_id].port_pe = 0;
        bus->hcd.roothub.int_buffer[0] = (1 << 1);
        usbh_hub_thread_wakeup(&bus->hcd.roothub);
    }

    if (is & USB_IS_SOF) {
    }

    if (is & USB_IS_RESUME) {
    }

    if (is & USB_IS_SUSPEND) {
    }

    if (is & USB_IS_VBUSERR) {
    }

    if (is & USB_IS_SESREQ) {
    }

    if (is & USB_IS_BABBLE) {
    }

    txis &= HWREGH(USB_BASE + MUSB_TXIE_OFFSET);

#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
    {
        uint32_t dma_int = HWREG(USB_BASE + MUSB_HC_DMA_INTR_OFFSET);
        /* Scan all DMA channels for completion */
        for (int dma_ch = 1; dma_ch <= MUSB_DMA_CH_NUM; dma_ch++) {
            if (!(dma_int & (1 << (dma_ch - 1)))) continue;

            uint32_t cntl = HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch));
            uint8_t dma_ep = (cntl >> 4) & 0x0f;
            uint8_t is_tx = (cntl >> 1) & 0x01;

            /* Disable DMA channel */
            HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0;
            musb_hc_dma_ch_free(dma_ch);

            musb_set_active_ep(bus, dma_ep);

            if (is_tx) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~(USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);
                pipe = &g_musb_hcd[bus->hcd.hcd_id].pipe_pool[dma_ep];
                urb = pipe->urb;
                if (urb) {
                    urb->actual_length = urb->actual_length + urb->transfer_buffer_length;
                    urb->transfer_buffer_length = 0;
                    urb->errorcode = 0;
                    musb_urb_waitup(urb);
                }
                txis &= ~(1 << dma_ep);
            } else {
                HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &= ~(USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD | USB_RXCSRH1_AUTORQ | USB_RXCSRH1_AUTOCL);
                pipe = &g_musb_hcd[bus->hcd.hcd_id].rx_pipe_pool[dma_ep];
                urb = pipe->urb;
                if (urb) {
                    urb->actual_length = urb->actual_length + urb->transfer_buffer_length;
                    urb->transfer_buffer_length = 0;
                    urb->errorcode = 0;
                    musb_urb_waitup(urb);
                }
                rxis &= ~(1 << dma_ep);
            }
        }
    }
#endif

    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        txis &= ~USB_TXIE_EP0;
        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
        handle_ep0(bus);
    }

    for (ep_idx = 1; ep_idx < CONFIG_USBHOST_PIPE_NUM; ep_idx++) {
        if (txis & (1 << ep_idx)) {
            HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << ep_idx);

            pipe = &g_musb_hcd[bus->hcd.hcd_id].pipe_pool[ep_idx];
            urb = pipe->urb;
            musb_set_active_ep(bus, ep_idx);

            ep_csrl_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);

            if (!urb) {
                if (ep_csrl_status & USB_TXCSRL1_ERROR) HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_ERROR;
                if (ep_csrl_status & USB_TXCSRL1_NAKTO) HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_NAKTO;
                if (ep_csrl_status & USB_TXCSRL1_STALL) HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_STALL;
                continue;
            }

            if (ep_csrl_status & USB_TXCSRL1_ERROR) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_ERROR;
                urb->errorcode = -USB_ERR_IO;
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
                /* On error during DMA, stop DMA and clean up */
                { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                  if (dma_ch) { HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0; musb_hc_dma_ch_free(dma_ch); }
                }
                HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~(USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);
#endif
                musb_urb_waitup(urb);
            } else if (ep_csrl_status & USB_TXCSRL1_NAKTO) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_NAKTO;
                urb->errorcode = -USB_ERR_NAK;
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
                { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                  if (dma_ch) { HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0; musb_hc_dma_ch_free(dma_ch); }
                }
                HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~(USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);
#endif
                musb_urb_waitup(urb);
            } else if (ep_csrl_status & USB_TXCSRL1_STALL) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_STALL;
                urb->errorcode = -USB_ERR_STALL;
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
                { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                  if (dma_ch) { HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0; musb_hc_dma_ch_free(dma_ch); }
                }
                HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~(USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);
#endif
                musb_urb_waitup(urb);
            }
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
            else if (HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) & USB_TXCSRH1_DMAEN) {
                /* DMA active: ignore per-packet TXIS, DMA completion will handle it */
            }
#endif
            else {
                uint32_t size = urb->transfer_buffer_length;

                if (size > USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
                    size = USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize);
                }

                urb->transfer_buffer += size;
                urb->transfer_buffer_length -= size;
                urb->actual_length += size;

                if (urb->transfer_buffer_length == 0) {
                    //HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
                    urb->errorcode = 0;
                    musb_urb_waitup(urb);
                } else {
                    musb_write_packet(bus, ep_idx, urb->transfer_buffer, MIN(urb->transfer_buffer_length, USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)));
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
                }
            }
        }
    }

    rxis &= HWREGH(USB_BASE + MUSB_RXIE_OFFSET);
    for (ep_idx = 1; ep_idx < CONFIG_USBHOST_PIPE_NUM; ep_idx++) {
        if (rxis & (1 << ep_idx)) {
            HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << ep_idx); // clear isr flag

            pipe = &g_musb_hcd[bus->hcd.hcd_id].rx_pipe_pool[ep_idx];
            urb = pipe->urb;
            musb_set_active_ep(bus, ep_idx);

            ep_csrl_status = HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET);
            //ep_csrh_status = HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET); // todo:for iso transfer

            if (!urb) {
                if (ep_csrl_status & USB_RXCSRL1_RXRDY) HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;
                if (ep_csrl_status & USB_RXCSRL1_ERROR) HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_ERROR;
                if (ep_csrl_status & USB_RXCSRL1_NAKTO) HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_NAKTO;
                if (ep_csrl_status & USB_RXCSRL1_STALL) HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_STALL;
                continue;
            }

            if (ep_csrl_status & USB_RXCSRL1_ERROR) {
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_ERROR;
                urb->errorcode = -USB_ERR_IO;
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
                { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                  if (dma_ch) { HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0; musb_hc_dma_ch_free(dma_ch); }
                }
                HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &= ~(USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD | USB_RXCSRH1_AUTORQ | USB_RXCSRH1_AUTOCL);
#endif
                musb_urb_waitup(urb);
            } else if (ep_csrl_status & USB_RXCSRL1_NAKTO) {
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_NAKTO;
                urb->errorcode = -USB_ERR_NAK;
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
                { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                  if (dma_ch) { HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0; musb_hc_dma_ch_free(dma_ch); }
                }
                HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &= ~(USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD | USB_RXCSRH1_AUTORQ | USB_RXCSRH1_AUTOCL);
#endif
                musb_urb_waitup(urb);
            } else if (ep_csrl_status & USB_RXCSRL1_STALL) {
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_STALL;
                urb->errorcode = -USB_ERR_STALL;
#if defined(CONFIG_USBH_MUSB_DMA_TX) || defined(CONFIG_USBH_MUSB_DMA_RX)
                { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                  if (dma_ch) { HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0; musb_hc_dma_ch_free(dma_ch); }
                }
                HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &= ~(USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD | USB_RXCSRH1_AUTORQ | USB_RXCSRH1_AUTOCL);
#endif
                musb_urb_waitup(urb);
            }
#ifdef CONFIG_USBH_MUSB_DMA_RX
            else if (HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) & USB_RXCSRH1_DMAEN) {
                /* DMA active: wait for DMA completion, UNLESS it's a short packet!
                 * AUTOCLR does not clear RXRDY for short packets, so RXIS will fire. */
                if (ep_csrl_status & USB_RXCSRL1_RXRDY) {
                    uint16_t rx_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
                    if (rx_count < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) {
                        /* Short packet! Stop DMA. */
                        { uint8_t dma_ch = musb_hc_dma_ch_find(ep_idx);
                          uint32_t dma_transferred = 0;
                          if (dma_ch) {
                            HWREG(USB_BASE + MUSB_HC_DMA_CNTL(dma_ch)) = 0;
                            /* Calculate how much DMA already moved */
                            uint32_t dma_remain = HWREG(USB_BASE + MUSB_HC_DMA_COUNT(dma_ch)) & 0xFFFFFF;
                            musb_hc_dma_ch_free(dma_ch);
                            if (dma_remain > urb->transfer_buffer_length) dma_remain = urb->transfer_buffer_length;
                            dma_transferred = urb->transfer_buffer_length - dma_remain;
                          }
                          HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &= ~(USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD | USB_RXCSRH1_AUTORQ | USB_RXCSRH1_AUTOCL);

                          urb->transfer_buffer += dma_transferred;
                          urb->transfer_buffer_length -= dma_transferred;
                          urb->actual_length += dma_transferred;

                          /* Read the short packet via PIO */
                          musb_read_packet(bus, ep_idx, urb->transfer_buffer, rx_count);
                          HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;

                          urb->transfer_buffer += rx_count;
                          urb->transfer_buffer_length -= rx_count;
                          urb->actual_length += rx_count;

                          /* It's a short packet, so we complete the URB even if transfer_buffer_length > 0 */
                          urb->errorcode = 0;
                          musb_urb_waitup(urb);
                        }
                    }
                }
            }
#endif
            else if (ep_csrl_status & USB_RXCSRL1_RXRDY) {
                size = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                musb_read_packet(bus, ep_idx, urb->transfer_buffer, size);

                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;

                urb->transfer_buffer += size;
                urb->transfer_buffer_length -= size;
                urb->actual_length += size;

                if ((size < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize)) || (urb->transfer_buffer_length == 0)) {
                    //HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                    urb->errorcode = 0;
                    musb_urb_waitup(urb);
                } else {
                    HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
                }
            }
        }
    }
    musb_set_active_ep(bus, old_ep_idx);
}
