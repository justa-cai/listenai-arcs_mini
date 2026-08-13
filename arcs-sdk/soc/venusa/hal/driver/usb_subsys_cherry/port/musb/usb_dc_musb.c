/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usb_musb_reg.h"

/* Enable this macro to utilize Double Packet Buffering (DPB) for MSC Bulk IN endpoints 
 * running in DMA Mode 1, doubling the hardware pipeline depth and maximizing throughput. */
//#define CONFIG_USB_MUSB_TX_DPB
//#define CONFIG_USB_MUSB_RX_DPB


#include "ClockManager.h"
#include "venusa_ap.h"

extern void dcache_clean_range(unsigned long start, unsigned long end);
extern void dcache_invalidate_range(unsigned long start, unsigned long end);

#include "log_print.h"
#include <stdio.h>


#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#define USB_BASE (g_usbdev_bus[0].reg_base)

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

#define MUSB_IND_TXMAP_OFFSET   0x80
#define MUSB_IND_TXCSRL_OFFSET  0x82
#define MUSB_IND_TXCSRH_OFFSET  0x83
#define MUSB_IND_RXMAP_OFFSET   0x84
#define MUSB_IND_RXCSRL_OFFSET  0x86
#define MUSB_IND_RXCSRH_OFFSET  0x87
#define MUSB_IND_RXCOUNT_OFFSET 0x88

#define MUSB_FIFO_OFFSET 0x00

#define MUSB_DEVCTL_OFFSET 0x41

#define MUSB_TXFIFOSZ_OFFSET  0x90
#define MUSB_RXFIFOSZ_OFFSET  0x94
#define MUSB_TXFIFOADD_OFFSET 0x92
#define MUSB_RXFIFOADD_OFFSET 0x96

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

#define MUSB_IND_TXMAP_OFFSET   0x10
#define MUSB_IND_TXCSRL_OFFSET  0x12
#define MUSB_IND_TXCSRH_OFFSET  0x13
#define MUSB_IND_RXMAP_OFFSET   0x14
#define MUSB_IND_RXCSRL_OFFSET  0x16
#define MUSB_IND_RXCSRH_OFFSET  0x17
#define MUSB_IND_RXCOUNT_OFFSET 0x18

#define MUSB_FIFO_OFFSET 0x20

#define MUSB_DEVCTL_OFFSET 0x60

#define MUSB_TXFIFOSZ_OFFSET  0x62
#define MUSB_RXFIFOSZ_OFFSET  0x63
#define MUSB_TXFIFOADD_OFFSET 0x64
#define MUSB_RXFIFOADD_OFFSET 0x66

#endif // CONFIG_USB_MUSB_SUNXI

#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

/* ============================================================
 * DMA Configuration
 * Define CONFIG_USB_MUSB_DMA to use hardware DMA for non-EP0 endpoints.
 * Undefine/comment out to fall back to PIO (CPU FIFO access) mode.
 * ============================================================ */
#define CONFIG_USB_MUSB_DMA
/* Fine-grained DMA control: enable/disable TX and RX DMA independently */
#define CONFIG_USB_MUSB_DMA_TX   /* DMA for TX (IN/write to host) */
#define CONFIG_USB_MUSB_DMA_RX   /* DMA for RX (OUT/read from host) */

#ifdef CONFIG_USB_MUSB_DMA
/* DMA register offsets (4 built-in channels) */
#define MUSB_DMA_INTR_OFFSET    0x200
#define MUSB_DMA_CNTL(ch)       (0x204 + ((ch) - 1) * 0x10)
#define MUSB_DMA_ADDR(ch)       (0x208 + ((ch) - 1) * 0x10)
#define MUSB_DMA_COUNT(ch)      (0x20C + ((ch) - 1) * 0x10)

/* DMA CNTL register bits */
#define DMA_CNTL_ENABLE         (1 << 0)
#define DMA_CNTL_DIR_TX         (1 << 1)   /* 1=TX (mem->FIFO), 0=RX (FIFO->mem) */
#define DMA_CNTL_MODE1          (1 << 2)   /* 0=Mode0, 1=Mode1 */
#define DMA_CNTL_IE             (1 << 3)   /* Interrupt Enable */
#define DMA_CNTL_EP(n)          ((n) << 4) /* Endpoint number [7:4] */
#define DMA_CNTL_ERR            (1 << 8)   /* Bus Error */
#define DMA_CNTL_BURST_INCR4    (1 << 9)
#define DMA_CNTL_BURST_INCR8    (2 << 9)
#define DMA_CNTL_BURST_INCR16   (3 << 9)

/* DMA channel assignments for device mode */
#define USB_DMA_CH_TX  1  /* Channel 1 for TX (IN) endpoints */
#define USB_DMA_CH_RX  2  /* Channel 2 for RX (OUT) endpoints */
#endif /* CONFIG_USB_MUSB_DMA */

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
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct musb_udc {
    volatile uint8_t dev_addr;
    __attribute__((aligned(32))) struct usb_setup_packet setup;
    struct musb_ep_state in_ep[CONFIG_USBDEV_EP_NUM];  /*!< IN endpoint parameters*/
    struct musb_ep_state out_ep[CONFIG_USBDEV_EP_NUM]; /*!< OUT endpoint parameters */
} g_musb_udc;

static volatile uint8_t usb_ep0_state = USB_EP0_STATE_SETUP;

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

static void musb_write_packet(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
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

static void musb_read_packet(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
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

#ifdef CONFIG_USB_MUSB_DMA
/* Track RX DMA Mode 1 state: which ep owns the DMA RX channel and dma_len */
static uint8_t  g_dma_rx_ep_idx  = 0xFF; /* 0xFF = idle */
static uint32_t g_dma_rx_dma_len = 0;

/* Track TX DMA Mode 1 state: which ep owns the DMA TX channel, and amounts */
static uint8_t  g_dma_tx_ep_idx  = 0xFF;
static uint32_t g_dma_tx_dma_len = 0;
static uint32_t g_dma_tx_tail_len = 0;

/*
 * DMA polled wrapper functions.
 * These replace musb_write_packet / musb_read_packet for non-EP0 endpoints.
 * The DMA transfers data between memory and the endpoint FIFO via the
 * USB controller's internal DMA engine, freeing the CPU from byte-by-byte
 * FIFO access. Polling is used for simplicity (DMA completes very fast
 * for packet-sized transfers over the internal AHB bus).
 */
static void musb_dma_write_packet(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    uint32_t ep_mps = g_musb_udc.in_ep[ep_idx].ep_mps;
    uint32_t dma_len = (len / ep_mps) * ep_mps;
    uint32_t tail_len = len % ep_mps;

    /* Fallback to PIO if EP0, dma_len = 0, or memory is not 4-byte aligned */
    if (ep_idx == 0 || dma_len == 0 || ((uint32_t)buffer & 0x03) != 0) {
        musb_write_packet(ep_idx, buffer, len);
        return;
    }

    /* Clean cache before DMA reads from RAM */
    dcache_clean_range((unsigned long)buffer, (unsigned long)buffer + len);

    uint8_t old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    g_dma_tx_ep_idx = ep_idx;
    g_dma_tx_dma_len = dma_len;
    g_dma_tx_tail_len = tail_len;

    /* 
     * IMPORTANT: We DO NOT mask the standard TXIE interrupt here.
     * According to the MUSB manual ("MULTIPLE PACKETS: TX ENDPOINT"), IntrTxE should be 
     * left as 1 to detect physical bus errors. The MUSB hardware is smart enough:
     * as long as DMAEN=1, it will inherently suppress routine packet completion TXIS 
     * interrupts (preventing CPU storms when AUTOSET chunks hit the bus).
     */

    /* --- MODE 1 DMA for the large chunks --- */
    HWREG(USB_BASE + MUSB_DMA_ADDR(USB_DMA_CH_TX)) = (uint32_t)buffer;
    HWREG(USB_BASE + MUSB_DMA_COUNT(USB_DMA_CH_TX)) = dma_len;
    HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_TX)) =
        DMA_CNTL_ENABLE | DMA_CNTL_IE | DMA_CNTL_DIR_TX | DMA_CNTL_BURST_INCR16 | DMA_CNTL_MODE1 |
        DMA_CNTL_EP(ep_idx);

    /* Enable DMA Req, Mode 1, and AUTOSET */
    HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= (USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);

    /* ASYNC MODE: Return immediately. DMA_NINT (CH 1) will fire upon completion
     * of the RAM-to-FIFO transfers, at which point the tail will be pushed. */
    return;
}

static void musb_dma_read_packet(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    /* Fallback to CPU PIO if EP0, length < 64, memory is not 4-byte aligned, or length is not multiple of 4 */
    if (ep_idx == 0 || len < 64 || ((uint32_t)buffer & 0x03) != 0 || (len & 0x03) != 0) {
        musb_read_packet(ep_idx, buffer, len);
        return;
    }

    uint16_t ep_mps = g_musb_udc.out_ep[ep_idx].ep_mps;
    /* DMA Mode 1 can only transfer full packets via AutoClear.
     * Round down to the nearest multiple of ep_mps. */
    /* Use 32-bit to prevent integer overflow on massive transfers like 256KB */
    uint32_t dma_len = (len / ep_mps) * ep_mps;

    uint8_t old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    /* HARDWARE BUG MITIGATION (MUSB Mode 1 Edge-Trigger Flaw):
     * If RxPktRdy is ALREADY 1 when we enable DMA Mode 1, the core will NOT generate
     * the DMA request edge. The endpoint will permanently deadlock. 
     * Solution: PIO pump the pre-filled packets to clear RxPktRdy to 0! 
     * We MUST use a 'while' loop here because with DPB enabled, there might be 
     * TWO packets sitting in the pipeline. Pumping only one will instantly reload 
     * the second packet's RxPktRdy, causing the edge to still be missed! */
    while (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
        uint16_t current_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
        /* Only pump if it is exactly a full packet. Short packets shouldn't arrive 
         * at the start of a massive DMA transfer, but if they do, PIO will handle it. */
        if (current_count == ep_mps && dma_len >= current_count) {
            musb_read_packet(ep_idx, buffer, current_count);
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;
            g_musb_udc.out_ep[ep_idx].actual_xfer_len += current_count;
            g_musb_udc.out_ep[ep_idx].xfer_buf += current_count;
            g_musb_udc.out_ep[ep_idx].xfer_len -= current_count;
            
            buffer += current_count;
            len -= current_count;
            dma_len -= current_count;
        } else {
            break; /* Force exit if we encounter a short packet */
        }
    }

    /* If total length < one full packet (or we just pumped the entirety of it),
     * we hand off to PIO or just exit cleanly with a notification callback! */
    if (dma_len == 0) {
        if (len > 0) {
            musb_read_packet(ep_idx, buffer, len);
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;
            g_musb_udc.out_ep[ep_idx].actual_xfer_len += len;
            g_musb_udc.out_ep[ep_idx].xfer_buf += len;
            g_musb_udc.out_ep[ep_idx].xfer_len -= len;
        }
        musb_set_active_ep(old_ep_idx);
        usbd_event_ep_out_complete_handler(0, ep_idx, g_musb_udc.out_ep[ep_idx].actual_xfer_len);
        return;
    }

    /* Per MUSB RX Mode 1 flow (known data block size):
     * 1. Set AutoClear, DMAReqEnab, DMAReqMode in RxCSRH
     * 2. Configure DMA: ADDR, COUNT=dma_len, Mode 1, RX direction
     * 3. DMA auto-receives full packets, AutoClear clears RxPktRdy for each
     * 4. Tail packet (< ep_mps) triggers RXIS, CPU reads via PIO */

    /* 1. Set IntrRxE.Dn = 0: DMA handles this, do not trigger standard CPU endpoint interrupt */
//    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);

    g_dma_rx_ep_idx = ep_idx;
    g_dma_rx_dma_len = dma_len;

    /* 1. Configure DMA controller */
    HWREG(USB_BASE + MUSB_DMA_ADDR(USB_DMA_CH_RX)) = (uint32_t)buffer;
    HWREG(USB_BASE + MUSB_DMA_COUNT(USB_DMA_CH_RX)) = dma_len;
    
    /* Hardware Synchronization Barrier: Wait for previous DMA state to fully clear */
    while (HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_RX)) & DMA_CNTL_ENABLE);

    /* 2. Enable DMA Engine FIRST so it is ready to capture the request edge */
    HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_RX)) =
        DMA_CNTL_ENABLE | DMA_CNTL_IE | DMA_CNTL_BURST_INCR16 | DMA_CNTL_MODE1 |
        DMA_CNTL_EP(ep_idx);

    /* 3. Tell MUSB Core to use DMA (Generates the DMA request to the now-active engine) */
    HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) |=
        (USB_RXCSRH1_AUTOCL | USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD);

    /* 4. DMA runs completely asynchronously now. 
     * CPU exits and hardware will trigger DMA_NINT when COUNT reaches 0. */
    musb_set_active_ep(old_ep_idx);
}
#endif /* CONFIG_USB_MUSB_DMA */

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

static uint32_t usbd_musb_fifo_config(struct musb_fifo_cfg *cfg, uint32_t offset)
{
    uint16_t fifo_used;
    uint8_t c_size;
    uint16_t c_off;

    c_off = offset >> 3;
    c_size = musb_get_fifo_size(cfg->maxpacket, &fifo_used);

    /* DPB for EP1 - disabled by default; enable via CONFIG_USB_MUSB_TX_DPB */
    uint8_t dpb_tx = 0;
#ifdef CONFIG_USB_MUSB_TX_DPB
    if (cfg->ep_num == 1) {
        dpb_tx = USB_TXFIFOSZ_DPB;
    }
#endif

    /* DPB for EP2 - disabled by default; enable via CONFIG_USB_MUSB_RX_DPB */
    uint8_t dpb_rx = 0;
#ifdef CONFIG_USB_MUSB_RX_DPB
    if (cfg->ep_num == 2) {
        dpb_rx = USB_RXFIFOSZ_DPB;
    }
#endif

    musb_set_active_ep(cfg->ep_num);

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
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = (c_size & 0x0f) | dpb_tx | dpb_rx;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = c_off;
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = (c_size & 0x0f) | dpb_tx | dpb_rx; /* Must equal TX when shared */
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = c_off;
            if (dpb_tx || dpb_rx) fifo_used *= 2;
            break;
        default:
            break;
    }

    return (offset + fifo_used);
}

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

    CLOGD("usb_dc_init");

    __HAL_CRM_USB_CLK_ENABLE();

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_SOFTCONN;

    // enable usb clock

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 1;   //set as device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 1;

    usb_dc_low_level_init();

#ifdef CONFIG_USB_HS
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;
#else
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_HSENAB;
#endif

    musb_set_active_ep(0);
    HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;

//lyt debug
//    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) |= USB_DEVCTL_SESSION;

    cfg_num = usbd_get_musb_fifo_cfg(&cfg);

    for (uint8_t i = 0; i < cfg_num; i++) {
        offset = usbd_musb_fifo_config(&cfg[i], offset);
    }

    USB_ASSERT_MSG(offset <= usb_get_musb_ram_size(), "Your fifo config is overflow, please check");

    /* Enable USB interrupts */
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = USB_IE_RESET | USB_IE_SUSPND | USB_IE_RESUME| USB_INTRUSBE_DISCON;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;




    /* Register USB ISR */
    register_ISR(IRQ_USBC_VECTOR, USBD_IRQHandler, NULL);
    enable_IRQ(IRQ_USBC_VECTOR);

#ifdef CONFIG_USBDEV_SOF_ENABLE
    HWREGB(USB_BASE + MUSB_IE_OFFSET) |= USB_IE_SOF;
#endif

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_SOFTCONN;
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
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

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    uint8_t old_ep_idx;
    uint32_t ui32Flags = 0;
    uint16_t ui32Register = 0;

    if (ep_idx == 0) {
        g_musb_udc.out_ep[0].ep_mps = USB_CTRL_EP_MPS;
        g_musb_udc.out_ep[0].ep_type = 0x00;
        g_musb_udc.out_ep[0].ep_enable = true;
        g_musb_udc.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
        g_musb_udc.in_ep[0].ep_type = 0x00;
        g_musb_udc.in_ep[0].ep_enable = true;
        return 0;
    }

    USB_ASSERT_MSG(ep_idx < CONFIG_USBDEV_EP_NUM, "Ep addr %02x overflow", ep->bEndpointAddress);

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        g_musb_udc.out_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_musb_udc.out_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        g_musb_udc.out_ep[ep_idx].ep_enable = true;

        USB_ASSERT_MSG((8 << HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET)) >= g_musb_udc.out_ep[ep_idx].ep_mps,
                       "Ep %02x fifo is overflow", ep->bEndpointAddress);

        /* TODO: same as TxMaxP — bits [12:11] (MULT) stripped for now.
         * Restore full wMaxPacketSize write when high-bandwidth ISO RX is needed. */
        HWREGH(USB_BASE + MUSB_IND_RXMAP_OFFSET) = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);

        //
        // Allow auto clearing of RxPktRdy when packet of size max packet
        // has been unloaded from the FIFO.
        //
        if (ui32Flags & USB_EP_AUTO_CLEAR) {
            ui32Register = USB_RXCSRH1_AUTOCL;
        }
        //
        // Configure the DMA mode.
        //
        if (ui32Flags & USB_EP_DMA_MODE_1) {
            ui32Register |= USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD;
        } else if (ui32Flags & USB_EP_DMA_MODE_0) {
            ui32Register |= USB_RXCSRH1_DMAEN;
        }
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

        HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = (USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
        else
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_CLRDT;

        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
    } else {
        g_musb_udc.in_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_musb_udc.in_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        g_musb_udc.in_ep[ep_idx].ep_enable = true;

        USB_ASSERT_MSG((8 << HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET)) >= g_musb_udc.in_ep[ep_idx].ep_mps,
                       "Ep %02x fifo is overflow", ep->bEndpointAddress);

        /* TODO: bits [12:11] of TxMaxP are the MUSB high-bandwidth MULT field,
         * controlling how many packets the controller sends per microframe.
         * Currently stripped to bits [10:0] only, because AUTOSET threshold
         * misbehaves when MULT>0 with FIFO < MULT*MaxPacket.
         * Restore full wMaxPacketSize write once FIFO sizing is resolved. */
        HWREGH(USB_BASE + MUSB_IND_TXMAP_OFFSET) = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);

        //
        // Allow auto setting of TxPktRdy when max packet size has been loaded
        // into the FIFO.
        //
        if (ui32Flags & USB_EP_AUTO_SET) {
            ui32Register |= USB_TXCSRH1_AUTOSET;
        }

        //
        // Configure the DMA mode.
        //
        if (ui32Flags & USB_EP_DMA_MODE_1) {
            ui32Register |= USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD;
        } else if (ui32Flags & USB_EP_DMA_MODE_0) {
            ui32Register |= USB_TXCSRH1_DMAEN;
        }

        //
        // Enable isochronous mode if requested.
        //
        if (USB_GET_ENDPOINT_TYPE(ep->bmAttributes) == 0x01) {
            ui32Register |= USB_TXCSRH1_ISO;
        }

        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) = ui32Register | USB_TXCSRH1_MODE;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
        else
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_CLRDT;
    }

    musb_set_active_ep(old_ep_idx);

    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    if (ep_idx == 0) {
        return 0;
    }

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        g_musb_udc.out_ep[ep_idx].ep_enable = false;
        
        /* Flush hardware memory and halt internal RX DMA engine */
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_FLUSH;
        HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_RX)) &= ~DMA_CNTL_ENABLE;
    } else {
        g_musb_udc.in_ep[ep_idx].ep_enable = false;
        /* 1. Soft-stop new DMA bursts by disabling DMAReqEnab to the MUSB core */
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~(USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);
        
        /* 2. Wait for the PHY to finish draining the currently active FIFO packet onto the USB wire */
        volatile int timeout = 100000;
        while ((HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) && timeout) {
            timeout--;
        }
        
        /* 3. Forcibly flush the hardware TX pipeline for any trapped/residual bytes */
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_FLUSH;
        }
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_FLUSH;
        }
        
        /* 4. Halt the explicit SoC DMA channel logic */
        HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_TX)) &= ~DMA_CNTL_ENABLE;
        
        /* 5. Clear global driver DMA tracker safely */
        g_dma_tx_ep_idx = 0xFF;
    }

    musb_set_active_ep(old_ep_idx);

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
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_STALL;
        }
    } else {
        if (ep_idx == 0x00) {
            usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_STALL;
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
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an OUT endpoint.
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_STALL | USB_RXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_CLRDT;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an IN endpoint.
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~(USB_TXCSRL1_STALL | USB_TXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_CLRDT;
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
        if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_STALL) {
            *stalled = 1;
        } else {
            *stalled = 0;
        }
    } else {
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_STALL) {
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

    if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
        musb_set_active_ep(old_ep_idx);
        return -3;
    }

    g_musb_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_musb_udc.in_ep[ep_idx].xfer_len = data_len;
    g_musb_udc.in_ep[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        if (ep_idx == 0x00) {
            if (g_musb_udc.setup.wLength == 0) {
                usb_ep0_state = USB_EP0_STATE_IN_STATUS;
            } else {
                usb_ep0_state = USB_EP0_STATE_IN_ZLP;
            }
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
            HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);
        }
        musb_set_active_ep(old_ep_idx);
        return 0;
    }
    
#ifdef CONFIG_USB_MUSB_DMA_TX
    if (ep_idx == 0) {
        /* EP0 CANNOT use DMA! Must truncate to ep_mps and preserve xfer state for multi-packet continuation. */
        data_len = MIN(data_len, g_musb_udc.in_ep[ep_idx].ep_mps);
        musb_write_packet(ep_idx, (uint8_t *)data, data_len);
    } else {
        /* For Async DMA, if eligible, we dispatch and set state blindly as original.
         * xfer_len=0 informs TXIS that the host has extracted the final token seamlessly. */
        musb_dma_write_packet(ep_idx, (uint8_t *)data, data_len);
        g_musb_udc.in_ep[ep_idx].xfer_len = 0;
        g_musb_udc.in_ep[ep_idx].actual_xfer_len = data_len;
    }
#else
    data_len = MIN(data_len, g_musb_udc.in_ep[ep_idx].ep_mps);
    musb_write_packet(ep_idx, (uint8_t *)data, data_len);
#endif
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);

    if (ep_idx == 0x00) {
        usb_ep0_state = USB_EP0_STATE_IN_DATA;
        if (data_len < g_musb_udc.in_ep[ep_idx].ep_mps) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
        }
    } else {
        /* CRITICAL: ONLY set TXRDY manually if we are doing a PIO transfer!
         * If DMA Mode 1 is active (DMAEN=1), AUTOSET asserts TXRDY natively upon reaching MaxPacketSize.
         * Forcing TXRDY manually during an active DMA stream causes catastrophic short-packet corruption! */
        if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) & USB_TXCSRH1_DMAEN)) {
            if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_TXRDY;
            }
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
#ifdef CONFIG_USB_MUSB_DMA_RX
        /* RX DMA Mode 1 (async): dispatch to DMA and let hardware handle the rest */
        if (data_len >= g_musb_udc.out_ep[ep_idx].ep_mps) {
            musb_dma_read_packet(ep_idx, data, data_len);
            /* DMA started; return immediately. DMA_NINT will invoke completion. */
            return 0;
        }
#endif
        /* PIO path: short transfers (CBW 31B, etc.), ISR reads when data arrives */
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
    }
    musb_set_active_ep(old_ep_idx);
    return 0;
}

static void handle_ep0(void)
{
    uint8_t ep0_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);
    uint16_t read_count;

    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        usb_ep0_state = USB_EP0_STATE_SETUP;
        return;
    }

    if (ep0_status & USB_CSRL0_SETEND) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_SETENDC;
    }

    if (g_musb_udc.dev_addr > 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = g_musb_udc.dev_addr;
        g_musb_udc.dev_addr = 0;
    }

    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            if (ep0_status & USB_CSRL0_RXRDY) {
                read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                if (read_count != 8) {
                    return;
                }

                musb_read_packet(0, (uint8_t *)&g_musb_udc.setup, 8);
                if (g_musb_udc.setup.wLength) {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_RXRDYC;
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                }

                usbd_event_ep0_setup_complete_handler(0, (uint8_t *)&g_musb_udc.setup);
            }
            break;

        case USB_EP0_STATE_IN_DATA:
            if (g_musb_udc.in_ep[0].xfer_len > g_musb_udc.in_ep[0].ep_mps) {
                // Buffer the buffer pointer forward implicitly since xfer_buf wasn't incremented in start_write
                g_musb_udc.in_ep[0].xfer_buf += g_musb_udc.in_ep[0].ep_mps;
                g_musb_udc.in_ep[0].actual_xfer_len += g_musb_udc.in_ep[0].ep_mps;
                g_musb_udc.in_ep[0].xfer_len -= g_musb_udc.in_ep[0].ep_mps;
                
                // Pump the next chunk natively
                read_count = MIN(g_musb_udc.in_ep[0].xfer_len, g_musb_udc.in_ep[0].ep_mps);
                musb_write_packet(0, g_musb_udc.in_ep[0].xfer_buf, read_count);
                if (read_count < g_musb_udc.in_ep[0].ep_mps) {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
                }
            } else {
                g_musb_udc.in_ep[0].actual_xfer_len += g_musb_udc.in_ep[0].xfer_len;
                g_musb_udc.in_ep[0].xfer_len = 0;
                usbd_event_ep_in_complete_handler(0, 0x80, g_musb_udc.in_ep[0].actual_xfer_len);
            }
            break;
        case USB_EP0_STATE_OUT_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                musb_read_packet(0, g_musb_udc.out_ep[0].xfer_buf, read_count);
                g_musb_udc.out_ep[0].xfer_buf += read_count;
                g_musb_udc.out_ep[0].actual_xfer_len += read_count;

                if (read_count < g_musb_udc.out_ep[0].ep_mps) {
                    usbd_event_ep_out_complete_handler(0, 0x00, g_musb_udc.out_ep[0].actual_xfer_len);
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                    usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_RXRDYC;
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

void USBD_IRQHandler()
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint8_t old_ep_idx;
    uint8_t ep_idx;
    uint16_t write_count, read_count;


    is = HWREGB(USB_BASE + MUSB_IS_OFFSET);
    txis = HWREGH(USB_BASE + MUSB_TXIS_OFFSET);
    rxis = HWREGH(USB_BASE + MUSB_RXIS_OFFSET);
    //while(1)
    //CLOGD("USBD_IRQHandler");
    HWREGB(USB_BASE + MUSB_IS_OFFSET) = is;

    old_ep_idx = musb_get_active_ep();

#ifdef CONFIG_USB_MUSB_DMA
    /* Extract DMA hardware interrupt status */
    uint32_t dma_int_status = HWREG(USB_BASE + MUSB_DMA_INTR_OFFSET);
#endif

#ifdef CONFIG_USB_MUSB_DMA_RX
    /* Is COUNT = 0 reached in DMA hardware? (DMA_NINT for Channel 2 is bit 1) */
    if (dma_int_status & (1 << (USB_DMA_CH_RX - 1))) {
        if (g_dma_rx_ep_idx != 0xFF) {
            uint8_t ep_idx = g_dma_rx_ep_idx;
            musb_set_active_ep(ep_idx);

            /* Stop DMA engine explicitly */
            HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_RX)) &= ~DMA_CNTL_ENABLE;

            /* Disable AutoClear + DMAReqEnab + DMAReqMode */
            HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &= ~(USB_RXCSRH1_AUTOCL | USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD);

            uint32_t dma_transferred = g_dma_rx_dma_len;
            if (dma_transferred > 0) {
                dcache_invalidate_range((unsigned long)g_musb_udc.out_ep[ep_idx].xfer_buf,
                                        (unsigned long)g_musb_udc.out_ep[ep_idx].xfer_buf + dma_transferred);

                g_musb_udc.out_ep[ep_idx].xfer_buf += dma_transferred;
                g_musb_udc.out_ep[ep_idx].actual_xfer_len += dma_transferred;
                g_musb_udc.out_ep[ep_idx].xfer_len -= dma_transferred;
            }

            /* CRITICAL REENTRANCY FIX: clear global BEFORE callback to prevent clobbering! */
            g_dma_rx_ep_idx = 0xFF;

            /* CPU determines if tail packet processing is needed */
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
                /* There's lingering data. Inject RXIS into normal processing pipeline.
                 * Must re-enable RXIE so the logical bit survives masking later. */
                rxis |= (1 << ep_idx);
                HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
            } else if (g_musb_udc.out_ep[ep_idx].xfer_len == 0) {
                /* Clean transfer finished exactly at the chunk size. Hardware IDLE State reached! */
                usbd_event_ep_out_complete_handler(0, ep_idx, g_musb_udc.out_ep[ep_idx].actual_xfer_len);
            }

            musb_set_active_ep(old_ep_idx);
        }
    }
#endif

#ifdef CONFIG_USB_MUSB_DMA_TX
     /* Is COUNT = 0 reached in DMA hardware? (DMA_NINT for Channel 1 is bit 0) */
     if (dma_int_status & (1 << (USB_DMA_CH_TX - 1))) {
         if (g_dma_tx_ep_idx != 0xFF) {
             uint8_t ep_idx = g_dma_tx_ep_idx;
             musb_set_active_ep(ep_idx);

             /* Stop DMA engine explicitly */
             HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_TX)) &= ~DMA_CNTL_ENABLE;

             /* Disable DMAEN, DMAMOD, AUTOSET */
             /* Safely clear DMAEN, DMAMOD, and AUTOSET now that the memory RAM pump has finished. 
              * The hardware will securely transmit any pending packets (TXRDY and FIFONE) 
              * and unmask the final TXIS interrupt once empty. */
             HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~(USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_AUTOSET);

             uint32_t dma_transferred = g_dma_tx_dma_len;
             if (dma_transferred > 0) {
                 g_musb_udc.in_ep[ep_idx].xfer_buf += dma_transferred;
             }

             /* Tail packet push via PIO mode. */
             if (g_dma_tx_tail_len > 0) {
                 musb_write_packet(ep_idx, g_musb_udc.in_ep[ep_idx].xfer_buf, g_dma_tx_tail_len);
                 g_musb_udc.in_ep[ep_idx].xfer_buf += g_dma_tx_tail_len;

                 /* Push TxPktRdy for the tail natively. */
                 if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)) {
                     HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_TXRDY;
                 }
             }

             /* CRITICAL RACE CONDITION FIX:
              * If the host ALREADY picked up the final packet BEFORE this IRQ ran, 
              * the read-to-clear MUSB_TXIS_OFFSET at the top of the ISR permanently consumed the event.
              * Since TXIE was off, `txis` local variable lost the bit. 
              * We MUST manually revive it into the local `txis` variable if the FIFO is empty! */
              if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)) {
                  txis |= (1 << ep_idx);
              }

             /* Re-enable TXIE securely without destructively reading TXIS. */
             HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);

             /* CRITICAL RACE CONDITION FIX:
              * If the host ALREADY picked up the final packet BEFORE this IRQ ran, 
              * the read-to-clear MUSB_TXIS_OFFSET at the top of the ISR permanently consumed the event.
              * Since TXIE was off, `txis` local variable lost the bit. 
              * We MUST manually revive it into the local `txis` variable if the FIFO is empty! */
              if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)) {
                  txis |= (1 << ep_idx);
              }

             musb_set_active_ep(old_ep_idx);
             g_dma_tx_ep_idx = 0xFF;
         }
     }
#endif

    /* Receive a reset signal from the USB bus */
    if (is & USB_IS_RESET) {
        memset(&g_musb_udc, 0, sizeof(struct musb_udc));
        usbd_event_reset_handler(0);
        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

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

    txis &= HWREGH(USB_BASE + MUSB_TXIE_OFFSET);
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
        musb_set_active_ep(0);
        handle_ep0();
        txis &= ~USB_TXIE_EP0;
    }

    ep_idx = 1;
    while (txis) {
        if (txis & (1 << ep_idx)) {
            musb_set_active_ep(ep_idx);
            HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_UNDRN) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_UNDRN;
            }

            if (g_musb_udc.in_ep[ep_idx].xfer_len > g_musb_udc.in_ep[ep_idx].ep_mps) {
                g_musb_udc.in_ep[ep_idx].xfer_buf += g_musb_udc.in_ep[ep_idx].ep_mps;
                g_musb_udc.in_ep[ep_idx].actual_xfer_len += g_musb_udc.in_ep[ep_idx].ep_mps;
                g_musb_udc.in_ep[ep_idx].xfer_len -= g_musb_udc.in_ep[ep_idx].ep_mps;
            } else {
                g_musb_udc.in_ep[ep_idx].actual_xfer_len += g_musb_udc.in_ep[ep_idx].xfer_len;
                g_musb_udc.in_ep[ep_idx].xfer_len = 0;
            }

            if (g_musb_udc.in_ep[ep_idx].xfer_len == 0) {
                uint8_t tx_csl = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);
                uint32_t dma_cntl = HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_TX));
                
#ifdef CONFIG_USB_MUSB_TX_DPB
                if ((dma_cntl & DMA_CNTL_ENABLE) || (tx_csl & USB_TXCSRL1_TXRDY) || (tx_csl & USB_TXCSRL1_FIFONE)) {
                    /* Ultimate DPB hardware barrier: 
                     * 1. DMA CNTL: SoC DMA controller is actively pumping RAM.
                     * 2. TXRDY=1: CPU side FIFO staging pending.
                     * 3. FIFONE=1: Physical prep area buffer flushing to wire. */
#else
                if ((dma_cntl & DMA_CNTL_ENABLE) || (tx_csl & USB_TXCSRL1_TXRDY)) {
                    /* Standard single-buffer barrier */
#endif
                } else {
                    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
                    usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, g_musb_udc.in_ep[ep_idx].actual_xfer_len);
                }
            } else {
                write_count = MIN(g_musb_udc.in_ep[ep_idx].xfer_len, g_musb_udc.in_ep[ep_idx].ep_mps);
#ifdef CONFIG_USB_MUSB_DMA_TX
                musb_dma_write_packet(ep_idx, g_musb_udc.in_ep[ep_idx].xfer_buf, write_count);
#else
                musb_write_packet(ep_idx, g_musb_udc.in_ep[ep_idx].xfer_buf, write_count);
#endif
                if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)) {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_TXRDY;
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
            musb_set_active_ep(ep_idx);
            HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << ep_idx);

#ifdef CONFIG_USB_MUSB_DMA_RX
            /* Check if DMA Mode 1 was active for this endpoint.
             * Short transfers (CBW 31B, etc.) don't start DMA and must
             * be handled by the PIO path below. */
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) & USB_RXCSRH1_DMAEN) {
                /* RX DMA Mode 1 completion:
                 * RXIS fires when a short packet arrives (DMA can't handle it)
                 * or when DMA COUNT reaches 0 for exact-multiple transfers. */

                /* Stop DMA engine if still running */
                HWREG(USB_BASE + MUSB_DMA_CNTL(USB_DMA_CH_RX)) &= ~DMA_CNTL_ENABLE;

                /* Disable AutoClear + DMAReqEnab + DMAReqMode */
                HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) &=
                    ~(USB_RXCSRH1_AUTOCL | USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD);

                /* Clear DMA interrupt status */
                (void)HWREG(USB_BASE + MUSB_DMA_INTR_OFFSET);

                /* Calculate how much DMA transferred */
                uint16_t ep_mps_rx = g_musb_udc.out_ep[ep_idx].ep_mps;
                uint32_t original_dma_len = (g_musb_udc.out_ep[ep_idx].xfer_len / ep_mps_rx) * ep_mps_rx;
                uint32_t dma_remaining = HWREG(USB_BASE + MUSB_DMA_COUNT(USB_DMA_CH_RX));
                uint32_t dma_transferred = original_dma_len - dma_remaining;

                /* Invalidate dcache for the DMA-written region */
                if (dma_transferred > 0) {
                    dcache_invalidate_range(
                        (unsigned long)g_musb_udc.out_ep[ep_idx].xfer_buf,
                        (unsigned long)g_musb_udc.out_ep[ep_idx].xfer_buf + dma_transferred);
                }

                g_musb_udc.out_ep[ep_idx].xfer_buf += dma_transferred;
                g_musb_udc.out_ep[ep_idx].actual_xfer_len += dma_transferred;
                g_musb_udc.out_ep[ep_idx].xfer_len -= dma_transferred;

                /* If there's a tail packet in FIFO (short packet), read via PIO */
                if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
                    read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
                    musb_read_packet(ep_idx, g_musb_udc.out_ep[ep_idx].xfer_buf, read_count);
                    HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;

                    g_musb_udc.out_ep[ep_idx].xfer_buf += read_count;
                    g_musb_udc.out_ep[ep_idx].actual_xfer_len += read_count;
                    g_musb_udc.out_ep[ep_idx].xfer_len -= read_count;
                }

                /* Transfer complete */
                HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                usbd_event_ep_out_complete_handler(0, ep_idx, g_musb_udc.out_ep[ep_idx].actual_xfer_len);
            } else
#endif
            /* PIO path: for short transfers (CBW, etc.) or non-DMA config */
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
                read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                musb_read_packet(ep_idx, g_musb_udc.out_ep[ep_idx].xfer_buf, read_count);

                if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
                    HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_RXRDY);
                }

                g_musb_udc.out_ep[ep_idx].xfer_buf += read_count;
                g_musb_udc.out_ep[ep_idx].actual_xfer_len += read_count;
                g_musb_udc.out_ep[ep_idx].xfer_len -= read_count;

                if ((read_count < g_musb_udc.out_ep[ep_idx].ep_mps) || (g_musb_udc.out_ep[ep_idx].xfer_len == 0)) {
                    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                    usbd_event_ep_out_complete_handler(0, ep_idx, g_musb_udc.out_ep[ep_idx].actual_xfer_len);
                } else {
                }
            }

            rxis &= ~(1 << ep_idx);
        }
        ep_idx++;
    }

    musb_set_active_ep(old_ep_idx);
}
