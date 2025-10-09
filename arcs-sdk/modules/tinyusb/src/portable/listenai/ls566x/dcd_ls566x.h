/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "usb_reg.h"

//BSD: decrease EP0 MPS to 16 so as to reduce bandwidth requirement
#define USB_MAX_CTRL_MPS    64 // 16   /**< maximum packet size (MPS) for EP 0 */
#define USB_CTRL_FIFO_SIZE  64   /**< FIFO size of EP 0, fixed in MUSB IP, CANNOT BE MODIFIED */

#define USB_MAX_FS_BULK_MPS 64   /**< full speed MPS for bulk EP */
#define USB_MAX_FS_INT_MPS  64   /**< full speed MPS for interrupt EP */
#define USB_MAX_FS_ISO_MPS  1023 /**< full speed MPS for isochronous EP */

//#define USB_RX_FIFO_BASE	0
//#define USB_TX_FIFO_BASE	1024

#define USB_EP_FIFO_BASE	0
//#define USB_EP_FIFO_BASE	(1024 * 2) // start from 2KB FIFO, TEST ONLY!!
//#define USB_EP_FIFO_BASE	(1024 * 3) // start from 3KB FIFO, TEST ONLY!!
#define USB_EP_FIFO_TOTAL_SIZE  (1024 * 4) // 4KB

#define REQTYPE_DIR_TO_DEVICE       DIR_IDX_OUT
#define REQTYPE_DIR_TO_HOST         DIR_IDX_IN


/**
 * USB endpoint direction and number.
 */
#define USB_EP_DIR_MASK     0x80
#define USB_EP_DIR_IN       0x80
#define USB_EP_DIR_OUT      0x00

/** Get endpoint index (number) from endpoint address */
#define USB_EP_GET_IDX(ep) ((ep) & 0x7F)

/** Get direction from endpoint address */
#define USB_EP_GET_DIR(ep)      ((ep) & USB_EP_DIR_MASK)
#define USB_EP_GET_DIR_IDX(ep)  (((ep) & USB_EP_DIR_MASK) >> 7)

/** Get endpoint address from endpoint index and direction */
#define USB_EP_GET_ADDR(idx, dir)       ((idx) | ((dir) & USB_EP_DIR_MASK))
#define USB_EP_GET_ADDR2(idx, dir_idx)  ((idx) | (((dir_idx) << 7) & USB_EP_DIR_MASK))

/** True if the endpoint is an IN endpoint */
#define USB_EP_DIR_IS_IN(ep) (USB_EP_GET_DIR(ep) == USB_EP_DIR_IN)
/** True if the endpoint is an OUT endpoint */
#define USB_EP_DIR_IS_OUT(ep) (USB_EP_GET_DIR(ep) == USB_EP_DIR_OUT)
#ifdef __cplusplus
extern "C" {
#endif


struct usb_ep_attr {
    uint16_t fifo_size;
    uint16_t mps;                   /* Max ep pkt size */

    //please refer to "tusb_desc_endpoint_t" definition for following fields...
    struct {
        uint8_t xfer_type: 2;   // 00b=CTRL, 01b=ISO, 10b=BULK, 11b=INT
        uint8_t sync_type: 2;   // VALID ONLY for ISO EP
        uint8_t usg_type: 2;    // VALID ONLY for ISO EP
        uint8_t rsvd1: 2;
    } attr_bits;

    uint8_t interval;       // VALID ONLY for ISO/INT EP

};

/*
 * USB endpoint private structure.
 */
struct usb_ep_ctrl_prv {
    struct usb_ep_attr fixed;

    uint8_t ep_ena : 1;
    uint8_t dma_ena : 1; // is DMA used to transfer data between RAM and TX/RX FIFO?
    //TODO: dma_reqmode (set in peripheral) & dma_mode (set in DMA controller, built-in or external)
    uint8_t dma_reqmode : 1; // DMA Request Mode 0 or 1, it relates to 1) when to send DMA request 2) whether EP interrupt is triggered
    uint8_t dma_mode : 1; // DMA (Transfer) Mode 0 or 1, it relates to when interrupt is triggered: a packet or whole transfer is done?
    uint8_t rsvd1 : 4;
//    uint8_t busy : 1; // exclusive access is guaranteed in upper USBD!
//    uint8_t dma_started : 1; // dma operation is ongoing or not?

    uint8_t dma_ch;    /* Endpoint dma channel number if dma_ena = 1 */
    uint8_t rsvd2;

//  uint16_t mps;                   /* Max ep pkt size */
//  uint16_t fifo_size;
//  usb_dc_ep_callback cb;          /* Endpoint callback function */  //BSD: TODO:
//  uint32_t data_len;              /*lenth for each transfer packet */ //BSD: always used in RX,  use "pkt_len" instead?
//    uint16_t pkt_len;               /*lenth of current received packet */ //BSD: always used in RX
    uint8_t term_early;     // whether transfer is terminated early?
    uint32_t last_len;       /* data length of data latest transferred or to transfer*/
    uint32_t xfer_len;       /* already transferred data total length for the request */
    uint32_t req_addr;       /* data buffer address requested by caller */
    uint32_t req_len;        /* data total length requested by caller */

//    usbd_ep_dma_cb dma_cb;   /* Endpoint dma transfer callback function if dma_ena = 1 */ //BSD: TODO:
//  struct usb_ep_dma_prv dma_info;
};


/*
 * USB controller private structure.
 */
struct usb_venus_ctrl_prv {
//  usb_dc_status_callback status_cb;  //BSD: TODO:
//  struct usb_ep_ctrl_prv in_ep_ctrl[USB_VENUS_IN_EP_NUM];     /* USB IN endpoint information */
//  struct usb_ep_ctrl_prv out_ep_ctrl[USB_VENUS_OUT_EP_NUM];   /* USB OUT endpoint information */
    struct usb_ep_ctrl_prv ep_info[2][USB_ARCS_IN_EP_NUM];     /* USB IN & OUT endpoint information, 0=OUT, 1=IN */

//    uint32_t txfifo_alloc_addr;                                 /* USB TxFifo address allocated */
//    uint32_t rxfifo_alloc_addr;                                 /* USB RxFifo address allocated */
    uint32_t fifo_alloc_addr;                                   /* USB endpoint FIFO address allocated */

    //uint8_t attached : 1;                                       /* USB device connected/attached to host? */
    uint8_t addressed : 1;                                      /* USB device's address allocated? */
    //uint8_t configured : 1;                                     /* USB device configured? */
    uint8_t reserved : 7;                                       /* reserved */

    uint8_t address;                                            /* USB device address */
    uint8_t channel_active;                                     /* USB dma channel active flag */
    uint8_t status;

    uint16_t ep0_data_len;           // data length required in DATA IN/OUT stage for USB Request
    uint16_t ep0_xfer_len;           // data length transfered in DATA IN/OUT stage for USB Request
    uint8_t ep0_req_dir;
    uint8_t ep0_request;

    uint16_t intr_txe;              // Interrupt enable register value of IntrTx
    uint16_t intr_rxe;              // Interrupt enable register value of IntrRx
    uint8_t intr_usbe;              // Interrupt enable register value of IntrUSB
};


#define CSK_USBC             IP_USBC

#ifdef __cplusplus
}
#endif
