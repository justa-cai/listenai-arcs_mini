/*
 * usb_controller.h
 *
 *  Created on: Sep 7, 2020
 *
 */

#ifndef __USB_CONTROLLER_H
#define __USB_CONTROLLER_H

#include "venusa_ap.h"

// #define __NAME__                ({const char *_f=__FILE__, *_p=strrchr(_f,'/'); _p?_p+1:_f;})
#define USBD_BIT(b)             (1 << (b))
#define USBD_PRINT(fmt, ...)    // printf(fmt, ##__VA_ARGS__)
#define USBD_BITS(h, l)         (USBD_BIT(h + 1) - USBD_BIT(l))
#define USBD_LOG(fmt, ...)      USBD_PRINT(fmt "\r\n", ##__VA_ARGS__)
#define USBD_LOGE(fmt, ...)     USBD_LOG("\e[31m[E/%s:%d] " fmt "\e[0m", __func__, __LINE__, ##__VA_ARGS__)
#define USBD_LOGW(fmt, ...)     USBD_LOG("\e[35m[W/%s:%d] " fmt "\e[0m", __func__, __LINE__, ##__VA_ARGS__)
#define USBD_LOGI(fmt, ...)     USBD_LOG("\e[33m[I/%s:%d] " fmt "\e[0m", __func__, __LINE__, ##__VA_ARGS__)
#define USBD_LOGD(fmt, ...)     USBD_LOG("\e[34m[D/%s:%d] " fmt "\e[0m", __func__, __LINE__, ##__VA_ARGS__)
#define USBD_TRACE(fmt, ...)    USBD_LOG("\e[3;92m" fmt "\e[0m", ##__VA_ARGS__)

#define USBD_ASSERT(cond, fmt, ...) do { if (!(cond)) { USBD_LOGE("[assert] " fmt, ##__VA_ARGS__); while(1); } } while(0)

//BSD: decrease EP0 MPS to 16 so as to reduce bandwidth requirement
#define USB_MAX_CTRL_MPS        64  // 16   /**< maximum packet size (MPS) for EP 0 */
#define USB_CTRL_FIFO_SIZE      64  /**< FIFO size of EP 0, fixed in MUSB IP, CANNOT BE MODIFIED */
// #define USB_MAX_BUCK_MPS        1024

#define EP_FIFO_TOTAL           (4 * 1024) // 4KB

#define EP_GET_IDIR(ep)         (((ep) & USB_EP_DIR_MASK) >> 7)
#define EP_GET_ADDR(ep,dir)     ((ep) | (((dir) << 7) & USB_EP_DIR_MASK))

#define DMA_XFER_EN             (1)
#define DMA_NOT_ASSIGNED        (0xff)

/**
 * DMA_REQ_MODE(DMA Request Modes): mode0 or mode1
 * (see section 16.0 of the datasheet):
 *
 * DMA Request Line Behaviors:
 *  > For Rx Endpoints:
 *   DMA becomes active when a data packet is available in the EP FIFO.
 *   [mode0] DMA becomes inactive when the transfer completed, or the CPU clears the RxPktRdy bit.
 *   [mode1] DMA becomes inactive only when the received size equals to MPS.
 *  > For Tx Endpoints:
 *   DMA becomes active when the EP FIFO is able to accept a data packet.
 *   DMA becomes inactive when the MPS bytes have been loaded into FIFO, or CPU sets the TxPktRdy bit
 *
 * EP Interrupts(if enabled) Behaviors:
 *   [mode0] No interrupt will be generated when packets are received, but the appropriate EP interrupt will be 
 *           generated to prompt the loading of all packets. 
 *   [mode1] The EP interrupt is suppressed except the packet is a SLP(short length packet, size less than MPS).
 * 
 *   The conditions under which Tx/Rx EPs interrupts are generated are summarized in the following tables.
 *   /-------------------------------------------------\/------------------------------------------------------\
 *   | EPInterrupt associated with RxPktRdy being set  ||  EP Interrupt associated with TxPktRdy being cleared |
 *   |-------------------------------------------------||------------------------------------------------------|
 *   | DMAReqEnable | DMAReqMode | InterruptGenerated  ||   DMAReqEnable  |  DMAReqMode  |  InterruptGenerated |
 *   | ------------ | ---------- | ------------------- ||   ------------  |  ----------  |  ----------------   |
 *   |       0      |     X      |         Yes         ||         0       |       X      |         Yes         |
 *   |       1      |     0      |         No          ||         1       |       0      |         Yes         |
 *   |       1      |     1      |    Only when SLP    ||         1       |       1      |         No          |
 *   \-------------------------------------------------/\------------------------------------------------------/
 */
#define DMA_REQ_MODE            (1)

/**
 * DMA_BULK_MODE(DMA Transfer Modes): mode0 or mode1
 * (see section 17.0 of the datasheet):
 *
 * [mode0]
 *  > Valid for any transactions(CTRL/BULK/ISO/INT).
 *  > DMA can be only programmed to load/unload one packet each time.
 *  > CPU should do intervention for each packet transferred.
 * 
 * [mode1]
 *  > Valid only for BULK transactions.
 *  > DMA can be programmed to load/unload a complete bulk transfer(which can be many packets), 
 *    once setup, DMA will load/unload all packets of the whole transfer.
 *  > Interrupting asserted only when the whole transfer completed.
 */
// #define DMA_BULK_MODE           (1)

#define EP0IDX                  (0)
#define EPoIDX                  (0)
#define EPiIDX                  (1)
#define EPSEL(epidx)            ({ uint8_t _o=IP_USBC->INDEX; IP_USBC->INDEX=(epidx)&0x7F; _o; })

struct usb_ep_info {
    struct {
        uint16_t addr;          // fifo address
        uint16_t size;          // fifo size
    } fifo;
    uint16_t ep_mps;            // max ep packet size
    uint8_t  ep_en;             // is endpoint enabled?
    uint8_t  dma_en;            // is DMA used to transfer data between RAM and TX/RX FIFO?
    uint8_t  dma_ch;            // Endpoint dma channel number if dma_en = 1
    uint8_t  termin;            // whether transfer is terminated early?
    uint32_t szxfer;            // data length of data latest transferred or to transfer
    uint32_t szdone;            // already transferred data total length for the request
    uint32_t reqsize;           // data total length requested by caller
    uint32_t reqaddr;           // data buffer address requested by caller
};
struct usb_ctrl_priv {
    struct usb_setup_packet __attribute__((aligned(32))) setup;
    struct usb_ep_info ep_info[2][USB_IN_EP_NUM]; /* USB IN & OUT endpoint information, 0=OUT, 1=IN */
    uint32_t fifo_htop;         // USB endpoint FIFO address allocated
    uint8_t dev_addr;           // USB device function address, bit7 stands for address allocated
    uint8_t dma_mask;           // USB dma channel active flag
    uint8_t ep0stat;            // ep0 status
    struct {
        uint16_t txe;           // Interrupt enable register value of IntrTx
        uint16_t rxe;           // Interrupt enable register value of IntrRx
        uint8_t usbe;           // Interrupt enable register value of IntrUSB
    } intr;
};
#define sz sizeof(struct usb_ep_info)+ sizeof(struct usb_ctrl_priv)

#endif /* __USB_CONTROLLER_H */
