/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "tusb_option.h"
#include "cache.h"
#include "dcd_ls566x.h"
#include "log_print.h"

#include "Driver_Common.h"
#include <common/tusb_common.h>
#include <device/dcd.h>
#include <assert.h>
#include "arcs_ap.h"

#if CFG_TUD_ENABLED && CFG_TUSB_MCU == OPT_MCU_LS566X

#if CFG_TUSB_DEBUG == 0
#define LOG_DBG(fmt, ...)   ((void)0)
#define LOG_ERR(fmt, ...)   ((void)0)
#elif CFG_TUSB_DEBUG == 1
#define LOG_DBG(fmt, ...)   ((void)0)
#define LOG_ERR(fmt, ...)   TU_LOG1(fmt, ##__VA_ARGS__);TU_LOG1("\r\n")
#elif CFG_TUSB_DEBUG >= 2
#define LOG_DBG(fmt, ...)   TU_LOG3(fmt, ##__VA_ARGS__);TU_LOG3("\r\n")
#define LOG_ERR(fmt, ...)   TU_LOG1(fmt, ##__VA_ARGS__);TU_LOG1("\r\n")
#endif

#include "tusb.h"
#include "device/usbd_pvt.h"

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif


#define CONFIG_SOF_CNT  0 // 1
#define SOF_CNT_OVERFLOW_THRESHOLD    32 // 8 // *125um

enum {
  EP_ISO_NUM = 8, // Endpoint number is fixed (8) for ISOOUT and ISOIN
  EP_CBI_COUNT = 8  // Control Bulk Interrupt endpoints count
};

typedef union {
  volatile uint8_t   u8;
  volatile uint16_t  u16;
  volatile uint32_t  u32;
} hw_fifo_t;

// Transfer Descriptor
typedef struct {
  uint8_t* buffer;
  uint16_t total_len;
  volatile uint16_t actual_len;
  uint16_t mps; // max packet size

  // nRF will auto accept OUT packet after DMA is done
  // indicate packet is already ACK
  volatile bool data_received;
  volatile bool started;

  // Set to true when data was transferred from RAM to ISO IN output buffer.
  // New data can be put in ISO IN output buffer after SOF.
  bool iso_in_transfer_ready;

} xfer_td_t;

static struct usb_venus_ctrl_prv usb_venus_ctrl;

static void usbd_reset_internal_state(void);

static void usb_dma_clear_channel_active_flag (uint8_t ch);

static void control_epx_interrupt(int dir_idx, int ep_idx, bool enable);


#if CONFIG_SOF_CNT

// set overflow threshold of SOF packet counting, return updated threshold
static uint32_t set_sof_cnt_ovf_thr(uint32_t thr)
{
#define MAX_SOF_CNT     ((0x1 << 14) - 1)
    if (thr <= MAX_SOF_CNT)
        IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_CNT = thr;
    return IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_CNT;
}

// get current count of SOF packet since last clear
static inline uint32_t get_sof_cnt(void)
{ return IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_CNT; }

// clear current count of SOF packet, and overflow flag
static inline void clr_sof_frame_cnt(void)
{ IP_CMN_SYS->REG_USB_SOF_CNT1.bit.SOF_FRAME_CNT_CLR = 1; }

// enable/disable SOF_CNT (and its interrupt)
static inline void enable_sof_cnt(bool ena)
{
    uint8_t ena_val = ena ? 1 : 0;
    IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_INT_ENABLE = ena_val;
    IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_CNT_ENABLE = ena_val;
}

// ISR of SOF_CNT overflow interrupt
static void usb_sof_cnt_isr_handler(void)
{
    // uint32_t sof_cnt = get_sof_cnt();
    clr_sof_frame_cnt();
//    LOG_DBG("ISR: sof_cnt = %d\r\n", sof_cnt);

    //TODO: ADD handler here...
}

#endif //CONFIG_SOF_CNT

static inline void usbd_ep_pio_write(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    uint32_t i;
    volatile uint8_t *pFifo_addr;

    pFifo_addr = (volatile uint8_t*)(&CSK_USBC->FIFOX[ep_idx]);

    volatile hw_fifo_t *reg = (volatile hw_fifo_t*)pFifo_addr;
    uintptr_t addr = (uintptr_t)buffer;
    while (len >= 4) {
        reg->u32 = *(uint32_t const *)addr;
        addr += 4;
        len  -= 4;
    }
    if (len >= 2) {
        reg->u16 = *(uint16_t const *)addr;
        addr += 2;
        len  -= 2;
    }
    if (len) {
        reg->u8 = *(uint8_t const *)addr;
    }

}

static void usbd_ep_pio_read(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    uint32_t i, data_word, len1;
    //uint8_t const ep_idx = USB_EP_GET_IDX(ep_addr);
    //uint8_t const dir   = USB_EP_GET_DIR(ep_addr);

    //assert(dir == USB_EP_DIR_OUT);
    assert(buffer != NULL && len != 0);

    // Data in the FIFOs to be read per 32-bit words
    len1 = len & ~0x3;
    if (len1 > 0) {
        if (!((uint32_t)buffer & 0x3)) { // buffer address is multiple of 4
            for (i = 0U; i < len1; i += 4U, buffer += 4U) {
                *(uint32_t *)buffer = CSK_USBC->FIFOX[ep_idx];
            }
        } else { // buffer address is NOT multiple of 4
            for (i = 0U; i < len1; i += 4U) {
                data_word = CSK_USBC->FIFOX[ep_idx];
                //TODO: Assume that CPU's byte order is little endian
                *buffer++ = data_word & 0xFF;
                *buffer++ = (data_word >> 8) & 0xFF;
                *buffer++ = (data_word >> 16) & 0xFF;
                *buffer++ = (data_word >> 24) & 0xFF;
            }
        }
    } // end if (len1 > 0)

    // Data bytes to be read when remaining length is less than 4
    len1 = len & 0x3;
    if (len1 > 0) {
        data_word = CSK_USBC->FIFOX[ep_idx];
        for (i = 0U; i < len1; i++) {
            //TODO: Assume that CPU's byte order is little endian
            *buffer++ = data_word & 0xFF;
            data_word >>= 8;
        }
    }
}



//BSD: Cancel the ongoing transfer on some endpoint (internal)
static void dcd_edpt_cancel_xfer_internal (uint8_t rhport, uint8_t ep_idx, uint8_t dir_idx)
{
    ARG_UNUSED(rhport);

    uint16_t val;
    struct usb_ep_ctrl_prv *epp;

    // reset transfer state
}


static inline int32_t usb_ep_get_xfer_bytes(struct usb_ep_ctrl_prv *epp)
{
    assert(epp != NULL);
    int32_t bytes = 0;
    uint8_t dma_ch = epp->dma_ch;
    if (dma_ch != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED &&
        (CSK_USBC->USB_DMA[dma_ch].CNTL & USB_ARCS_DMA_CNTL_DMA_ENAB) != 0) {
        bytes = epp->last_len - CSK_USBC->USB_DMA[dma_ch].COUNT;
        assert(bytes >= 0);
    }
    return (epp->xfer_len + bytes);
}


// reset internal state of the controller driver
static void usbd_reset_internal_state(void)
{
    uint8_t i, flag, irq_en;

    // if NOT addressed yet, already in initial state, do nothing...
    if (usb_venus_ctrl.addressed == 0 && usb_venus_ctrl.address == 0) {
        LOG_DBG("usbd_reset_internal_state: no need to reset");
        return;
    }

    LOG_DBG("usbd_reset_internal_state");

    irq_en = IRQ_enabled(IRQ_USBC_VECTOR);
    if (irq_en) disable_IRQ(IRQ_USBC_VECTOR);

    // stop DMA operation if any
    flag = usb_venus_ctrl.channel_active;
    for (i=0; i<USB_DMA_CH_COUNT_AVAIL; i++) {
        if ((flag & (1U << i)) != 0) {
            CSK_USBC->USB_DMA[i].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
            CSK_USBC->USB_DMA[i].COUNT = 0;
            CSK_USBC->USB_DMA[i].ADDR = 0;
        }
    }
    usb_venus_ctrl.channel_active = 0;

    //NOTE: USB_ARCS_IN_EP_NUM == USB_ARCS_OUT_EP_NUM
    // flush RX FIFO
    for (i = 1U; i < USB_ARCS_IN_EP_NUM; i++) {
        EDPxReg_SEL(i);
        CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO; //TODO: check USB_ARCS_RXCSRL_RXPKTRDY bit?
        CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO; //TODO: check USB_ARCS_TXCSRL_TXPKTRDY bit?
    }
    EDPxReg_SEL(0);

    // clear private data of all EPs except EP0
    (void)memset(&usb_venus_ctrl.ep_info[0][1], 0,
            sizeof(struct usb_ep_ctrl_prv) * (USB_ARCS_IN_EP_NUM -1));
    (void)memset(&usb_venus_ctrl.ep_info[1][1], 0,
            sizeof(struct usb_ep_ctrl_prv) * (USB_ARCS_IN_EP_NUM -1));

    // clear global USB status
    //BSD: MUST reserve USB_CTRL_FIFO_SIZE bytes (Starting from 0) for EP0 FIFO
    usb_venus_ctrl.fifo_alloc_addr = (USB_EP_FIFO_BASE + USB_CTRL_FIFO_SIZE) >> 3;
    usb_venus_ctrl.addressed = 0; // restore to NOT addressed
    usb_venus_ctrl.address = 0;
    //usb_venus_ctrl.configured = 0; // restore to NOT configured

    LOG_DBG("%s, change status to USB_ARCS_STS_SETUP", __func__);
    usb_venus_ctrl.status = USB_ARCS_STS_SETUP;
    usb_venus_ctrl.ep0_data_len = 0;

    if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
}


// Read Endpoint (NOT EP0) when RX data ready
// return read bytes, and -1 if error.
static int32_t usbd_epx_read(uint8_t ep_idx)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t bytes_to_read;
    uint16_t pkt_len = 0;

    if (ep_idx == 0 || ep_idx >= USB_ARCS_OUT_EP_NUM)
        return -1;

    epp = &usb_venus_ctrl.ep_info[0][ep_idx];
    bytes_to_read = epp->req_len - epp->xfer_len;

    EDPxReg_SEL(ep_idx);

    if( !(CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) )
        return -1;
    pkt_len = CSK_USBC->RXCOUNT & USB_ARCS_RXCOUNT_MASK;

    // NO buffer to hold data (usually dcd_edpt_xfer has NOT been called)
    if (bytes_to_read == 0 && pkt_len > 0)
        return -1;

    if (pkt_len > 0) {
        if (bytes_to_read > pkt_len)
            bytes_to_read = pkt_len;

        //TODO: should remaining DATA in RX FIFO should be flushed if bytes_to_read < pkt_len?
        if (bytes_to_read > 0) {
            epp->last_len = bytes_to_read;
            usbd_ep_pio_read(ep_idx, (uint8_t *)(epp->req_addr + epp->xfer_len), bytes_to_read);
            epp->xfer_len += bytes_to_read;
        }
    }

    return bytes_to_read;
}

// Write Endpoint (NOT EP0)
// return written bytes, and -1 if error.
static int32_t usbd_epx_write(uint8_t ep_idx, bool send_zlp_if_none)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t bytes_to_write;
    int32_t ret = 0;

    if (ep_idx == 0 || ep_idx >= USB_ARCS_IN_EP_NUM)
        return -1;

    epp = &usb_venus_ctrl.ep_info[1][ep_idx];
    bytes_to_write = epp->req_len - epp->xfer_len;

    dcd_int_disable(0);

    EDPxReg_SEL(ep_idx);

    if (bytes_to_write > 0) {
        usbd_ep_pio_write(ep_idx, (uint8_t *)(epp->req_addr + epp->xfer_len), bytes_to_write);
        epp->xfer_len += bytes_to_write;
    }
    epp->last_len = bytes_to_write; // bytes_to_write may be 0...

    CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;

    ret = bytes_to_write;

EXIT:
    dcd_int_enable(0);

    return ret;
}

static void update_ep_tx_status(uint8_t ep_idx)
{
    struct usb_ep_ctrl_prv *epp;
    epp = &usb_venus_ctrl.ep_info[1][ep_idx];

    uint32_t xferred_bytes = epp->xfer_len;

    LOG_DBG("%s:%d, xferred_bytes: %d, before dcd_event_xfer_complete", __FUNCTION__, __LINE__, xferred_bytes);
    
    dcd_event_xfer_complete(0, USB_EP_GET_ADDR(0, USB_EP_DIR_IN),
                            xferred_bytes, XFER_RESULT_SUCCESS, true);

}
static void update_ep_rx_status(uint8_t ep_idx)
{
    struct usb_ep_ctrl_prv *epp;
    epp = &usb_venus_ctrl.ep_info[0][ep_idx];
    if (epp->req_len > 0) {

            // cleanup last read operation?
            uint32_t xferred_bytes = epp->xfer_len;
            epp->req_addr = 0;
            epp->req_len = epp->xfer_len = epp->last_len = 0;
            epp->term_early = 0;

            // notify upper USBD driver
            dcd_event_xfer_complete(0, USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_OUT),
                                    xferred_bytes, XFER_RESULT_SUCCESS, true);
    }
}

static bool usbd_ep_is_valid(uint8_t ep_idx, uint8_t dir_idx)
{
    //assert(dir_idx == 0 || dir_idx == 1);
    if (dir_idx == DIR_IDX_OUT) {
        if (ep_idx >= USB_ARCS_OUT_EP_NUM)
            return false;
    } else {
        if (ep_idx >= USB_ARCS_IN_EP_NUM)
            return false;
    }

    return true;
}




//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
    (void) rhport;
    (void) rh_init;

    /* Clear private data */
    (void)memset(&usb_venus_ctrl, 0, sizeof(usb_venus_ctrl));

    /* Initial usb_venus_ctrl */
    LOG_DBG("%s, change status to USB_ARCS_STS_SETUP", __func__);
    usb_venus_ctrl.status = USB_ARCS_STS_SETUP;

    /* Endpoint0 is fifo size is fixed to 16 bytes */
    usb_venus_ctrl.ep_info[0][0].fixed.mps = USB_MAX_CTRL_MPS;
    usb_venus_ctrl.ep_info[0][0].fixed.fifo_size = USB_CTRL_FIFO_SIZE;
    usb_venus_ctrl.ep_info[1][0].fixed.mps = USB_MAX_CTRL_MPS;
    usb_venus_ctrl.ep_info[1][0].fixed.fifo_size = USB_CTRL_FIFO_SIZE;

    usb_venus_ctrl.ep_info[0][0].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
    usb_venus_ctrl.ep_info[1][0].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;

    /* Set RX/TX FIFO space base address */
//    usb_venus_ctrl.rxfifo_alloc_addr = USB_RX_FIFO_BASE;
//    usb_venus_ctrl.txfifo_alloc_addr = USB_TX_FIFO_BASE;

    //BSD: MUST reserve USB_CTRL_FIFO_SIZE bytes (Starting from 0) for EP0 FIFO
    usb_venus_ctrl.fifo_alloc_addr = (USB_EP_FIFO_BASE + USB_CTRL_FIFO_SIZE) >> 3;

    /* USB_ARCS_IN_EP_NUM == USB_ARCS_OUT_EP_NUM */
    uint32_t i;
    for(i = 1; i < USB_ARCS_IN_EP_NUM; i++){
        usb_venus_ctrl.ep_info[0][i].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        usb_venus_ctrl.ep_info[1][i].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        // enable EP DMA by default except EP0
        usb_venus_ctrl.ep_info[0][i].dma_ena = 0;
        usb_venus_ctrl.ep_info[1][i].dma_ena = 0;
    }

#if TUD_OPT_HIGH_SPEED
    /* Set device speed to High Speed */
    CSK_USBC->POWER |= USB_ARCS_POWER_HSENABLE;   //high speed enable
#else
    /* Set device speed to Full Speed */
    CSK_USBC->POWER &= ~USB_ARCS_POWER_HSENABLE;   //Full speed enable
#endif

    /* Register USB ISR */
    register_ISR(IRQ_USBC_VECTOR, (ISR)dcd_int_handler, NULL);

    // Initialize usb interrupt enable
    //TODO: enable SOF interrupt if necessary...
    usb_venus_ctrl.intr_usbe = USB_ARCS_INTRUSBE_RESET | USB_ARCS_INTRUSBE_SUSPEND |
                               USB_ARCS_INTRUSBE_RESUME | USB_ARCS_INTRUSBE_DISCON; //USB_ARCS_INTRUSBE_CONN |
    usb_venus_ctrl.intr_txe = USB_ARCS_INTRTX_EP0;
    usb_venus_ctrl.intr_rxe = 0;

#if CONFIG_SOF_CNT
    //IP_CMN_SYS->REG_USB_SOF_CNT1.bit.SOF_FRAME_CNT_CLR = 1; // clear SOF_CNT (cnt & overflow fields)
    //IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_CNT = 8; //default, 8 * 125us (USB 2.0) = 1ms
    //IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_INT_ENABLE = 1;
    //IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_CNT_ENABLE = 1;
    clr_sof_frame_cnt();
    set_sof_cnt_ovf_thr(SOF_CNT_OVERFLOW_THRESHOLD);
    enable_sof_cnt(true);
    register_ISR(IRQ_SOF_CNT_VECTOR, (ISR)usb_sof_cnt_isr_handler, NULL);
#endif // CONFIG_SOF_CNT

    return true;
}

void dcd_int_enable(uint8_t rhport) {
    ARG_UNUSED(rhport);

    CSK_USBC->INTRUSBE = usb_venus_ctrl.intr_usbe;
    CSK_USBC->INTRTXE = usb_venus_ctrl.intr_txe;
    CSK_USBC->INTRRXE = usb_venus_ctrl.intr_rxe;


    // Enable global interrupt
    enable_IRQ(IRQ_USBC_VECTOR);

#if CONFIG_SOF_CNT
    enable_IRQ(IRQ_SOF_CNT_VECTOR);
#endif
}

void dcd_int_disable(uint8_t rhport) {
    ARG_UNUSED(rhport);

    CSK_USBC->INTRUSBE = 0x0;
    CSK_USBC->INTRTXE = 0x0;
    CSK_USBC->INTRRXE = 0x0;

    // Disable global interrupt
    disable_IRQ(IRQ_USBC_VECTOR);

#if CONFIG_SOF_CNT
    disable_IRQ(IRQ_SOF_CNT_VECTOR);
#endif
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
    (void) dev_addr;
    
    dcd_int_disable(0);
    EDPxReg_SEL(0);
    usb_venus_ctrl.status = USB_ARCS_STS_STATUS;
    LOG_DBG("%s: %d, change to STS_STATUS status", __func__, __LINE__);
    CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY | USB_ARCS_CSR0L_DATAEND;
    dcd_int_enable(0);
}

void dcd_remote_wakeup(uint8_t rhport) {
    (void) rhport;

    // TODO: Add Remote_Wakeup function...
    // please refer to P72, Chap 3.11.2. LPM_CNTRL @musbmhdrc_pspg.pdf
}

// disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport) {
    ARG_UNUSED(rhport);

    // Enable soft disconnect
    CSK_USBC->POWER &= ~USB_ARCS_POWER_SOFTCONN;
}

// connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport) {
    ARG_UNUSED(rhport);

    /* Enable soft connect */
    CSK_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
    (void) rhport;
    (void) en;

  // TODO implement later
}


static void control_epx_interrupt(int dir_idx, int ep_idx, bool enable) {
    uint16_t val;

    if (ep_idx == 0) {
        val = USB_ARCS_DAINT_IN_EP_INT(ep_idx);
        if (enable) {
            CSK_USBC->INTRTXE |= val;
            usb_venus_ctrl.intr_txe |= val;
        } else {
            CSK_USBC->INTRTXE &= ~val;
            usb_venus_ctrl.intr_txe &= ~val;
        }
    } else {
        val = USB_ARCS_DAINT_OUT_EP_INT(ep_idx);
        if (dir_idx == DIR_IDX_IN) {
            if (enable){
                CSK_USBC->INTRTXE |= val;
                usb_venus_ctrl.intr_txe |= val;
            } else {
                CSK_USBC->INTRTXE &= ~val;
                usb_venus_ctrl.intr_txe &= ~val;
            }
        } else if (dir_idx == DIR_IDX_OUT) {
            if (enable) {
                CSK_USBC->INTRRXE |= val;
                usb_venus_ctrl.intr_rxe |= val;
            } else {
                CSK_USBC->INTRRXE &= ~val;
                usb_venus_ctrl.intr_rxe &= ~val;
            }
        }
    }
    LOG_DBG("intr %d: %d", ep_idx, enable);
}


//static int usb_venus_ep_set(uint8_t ep, uint32_t ep_mps, enum usb_dc_ep_transfer_type ep_type)
static int usbd_ep_set(uint8_t ep_idx, uint8_t dir_idx, uint16_t ep_mps, tusb_xfer_type_t ep_type)
{
    LOG_DBG("%s ep %d (dir=%d), mps %d, type %d", __func__, ep_idx, dir_idx, ep_mps, ep_type);

    if (ep_idx == 0) { // EP0, ignore dir_idx
        if (ep_type != TUSB_XFER_CONTROL || ep_mps > USB_CTRL_FIFO_SIZE)
            return CSK_DRIVER_ERROR_PARAMETER;
        usb_venus_ctrl.ep_info[0][0].fixed.mps = usb_venus_ctrl.ep_info[1][0].fixed.mps = ep_mps;
        return CSK_DRIVER_OK;
    }

    if ((dir_idx && ep_idx >= USB_ARCS_OUT_EP_NUM) || (!dir_idx && ep_idx >= USB_ARCS_IN_EP_NUM))
        return CSK_DRIVER_ERROR_PARAMETER;

    if (ep_mps > USB_ARCS_MAXP_MASK || ep_type > TUSB_XFER_INTERRUPT)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint16_t fifo_size, val;
    struct usb_ep_ctrl_prv *epp;

    epp = &usb_venus_ctrl.ep_info[dir_idx][ep_idx];
    fifo_size = epp->fixed.fifo_size;
    if (fifo_size > 0 && ep_mps > fifo_size )
        return CSK_DRIVER_ERROR_PARAMETER;

    dcd_int_disable(0);

    // Set max packet size
    EDPxReg_SEL(ep_idx);
    if (dir_idx == DIR_IDX_OUT)
        CSK_USBC->RXMAXP = ep_mps;
    else
        CSK_USBC->TXMAXP = ep_mps;

    EDPxReg_SEL(ep_idx);

    dcd_int_enable(0);

    /* Set endpoint type and FIFO size for EP1-15 */

    if (ep_type == TUSB_XFER_ISOCHRONOUS) { // ISO
        // Set DMA Request Mode & DMA Mode to 0
        epp->dma_reqmode = 1; //0;
        epp->dma_mode = 1; //0

        // Set ISO type bit
        if (dir_idx == DIR_IDX_OUT)
            CSK_USBC->RXCSRH |= USB_ARCS_RXCSRH_ISO;
        else
            CSK_USBC->TXCSRH |= USB_ARCS_TXCSRH_ISO;
    } else { // BULK or INT
        //BSD2013.1.22 HID button experiment (INT IN EP):
        // dma_mode = 1, dma_reqmode = 0 or 1, it's OK;
        // dma_mode = 0, dma_reqmode = 0, it's OK;
        // dma_mode = 0, dma_reqmode = 1, it doesn't work!
        // Many reports are uploaded in one interrupt transfer!!

        // Set DMA Request Mode & DMA Mode to 0
        // dma_reqmode decides 1) When to raise DMA request for RX 2) Whether to trigger EP interrupt
        epp->dma_reqmode = 1; //0; // RX = 1, TX = 0? It seems no difference for INT when dma_mode=1
        //dma_mode = 1: one EP interrupt (optional) for one xfer (1 or more packets)
        //dma_mode = 0: one EP interrupt (optional) for one packet
        epp->dma_mode = 0; //0;

        if (dir_idx == DIR_IDX_OUT) {
            /* Clear ISO type bit */
            CSK_USBC->RXCSRH &= (~USB_ARCS_RXCSRH_ISO_MASK);
            /* force the endpoint data toggle */
            if (ep_type == TUSB_XFER_BULK)
                CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_CLRDATATOG;
        } else {
            /* Clear ISO type bit */
            CSK_USBC->TXCSRH &= (~USB_ARCS_TXCSRH_ISO_MASK);
            /* force the endpoint data toggle */
            if (ep_type == TUSB_XFER_BULK)
                CSK_USBC->TXCSRH |= USB_ARCS_TXCSRH_FRCDATATOG;
        }
    }


    val = GET_FIFOSZ_CFG(ep_mps); // mps => FIFOSZ
    if (dir_idx == DIR_IDX_OUT) {
        /* set endpoint fifo size and fifo address */
        CSK_USBC->RXFIFOSZ &= ~(USB_ARCS_FIFOSZ_DPB_MASK | USB_ARCS_FIFOSZ_SZ_MASK);
        CSK_USBC->RXFIFOSZ |= val; // GET_FIFOSZ_CFG(ep_mps);

        usb_venus_ctrl.ep_info[0][ep_idx].fixed.mps = ep_mps;
        LOG_DBG("%s: %d, ep_mps = %d, fifo_size = %d, RXFIFOSZ = 0x%x", __FUNCTION__, __LINE__, ep_mps, fifo_size, CSK_USBC->RXFIFOSZ);
        if (fifo_size == 0) {
            //BSD: the actual fifo size can be smaller (no less than ep_mps) to save FIFO space,
            // and make sure that it is an integral multiple of 8!!
            //fifo_size = FIFOSZ_CFG_TO_BYTES(val);
            fifo_size = (ep_mps + 7) & ~0x7;
            usb_venus_ctrl.ep_info[0][ep_idx].fixed.fifo_size = fifo_size; // ep_mps;

            CSK_USBC->RXFIFOADD = usb_venus_ctrl.fifo_alloc_addr;
            usb_venus_ctrl.fifo_alloc_addr += fifo_size >> 3; // ep_mps >> 3;  // 8bytes aligned address
        }

        if(usb_venus_ctrl.ep_info[0][ep_idx].dma_ena) {
            CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
            CSK_USBC->RXCSRH |= (USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_RXCSRH_DMAREQMODE_POS));
        }else{
            /* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
            CSK_USBC->RXCSRH &= ~USB_ARCS_RXCSRH_DMAREQENAB_MASK;
        }
    } else {
        /* set endpoint fifo size and fifo address */
        CSK_USBC->TXFIFOSZ &= ~(USB_ARCS_FIFOSZ_DPB_MASK | USB_ARCS_FIFOSZ_SZ_MASK);
        CSK_USBC->TXFIFOSZ |= val; // GET_FIFOSZ_CFG(ep_mps);
        if(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_FIFONOTEMPTY) {
            //The CPU write 1 to this bit to flush the latest packet from endpoint TX FIFO
            CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO;
        }

        usb_venus_ctrl.ep_info[1][ep_idx].fixed.mps = ep_mps;
        if (fifo_size == 0) {
            //BSD: the actual fifo size can be smaller (no less than ep_mps) to save FIFO space,
            // and make sure that it is an integral multiple of 8!!
            //fifo_size = FIFOSZ_CFG_TO_BYTES(val);
            fifo_size = (ep_mps + 7) & ~0x7;
            usb_venus_ctrl.ep_info[1][ep_idx].fixed.fifo_size = fifo_size; // ep_mps;

            CLOGI("EP%x (dir=%d) TX FIFO address is 0x%x, size is 0x%x", ep_idx, dir_idx,
                    usb_venus_ctrl.fifo_alloc_addr, fifo_size);

            CSK_USBC->TXFIFOADD = usb_venus_ctrl.fifo_alloc_addr;
            usb_venus_ctrl.fifo_alloc_addr += fifo_size >> 3; // ep_mps >> 3;  // 8bytes aligned address
        }

        if(usb_venus_ctrl.ep_info[1][ep_idx].dma_ena) {
            CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_1);
            //BSD NOTE: USB_ARCS_TXCSRH_DMAREQMODE_0 had better be used if we need IN EP interrupt (usb_venus_int_iep_handler) together with DMA interrupt!!
            CSK_USBC->TXCSRH |= (USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_TXCSRH_DMAREQMODE_POS)); //USB_ARCS_TXCSRH_DMAREQMODE_1
        }else{
            /* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
            CSK_USBC->TXCSRH &= ~USB_ARCS_TXCSRH_DMAREQENAB_MASK;
        }
    }

    #ifdef DOUBLE_PACKET_ENABLE
        CSK_USBC->TXFIFOSZ |= USB_ARCS_FIFOSZ_DPB;
    #endif

    return CSK_DRIVER_OK;
}



//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
    LOG_DBG("dcd_edpt0_status_complete, recipient=%d, type=%d, request=%d",
        request->bmRequestType_bit.recipient, request->bmRequestType_bit.type, request->bRequest);
    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
        request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
            request->bRequest == TUSB_REQ_SET_ADDRESS )
    {
        dcd_int_disable(rhport);
        EDPxReg_SEL(0);
        uint8_t const dev_addr = (uint8_t) request->wValue;

        CSK_USBC->FADDR = (dev_addr) & USB_ARCS_FADDR_ADDR_MASK;
        dcd_int_enable(rhport);
    }
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const* p_endpoint_desc) {
  (void) rhport;
    uint8_t ep_idx, dir_idx;
    uint16_t val;

    ARG_UNUSED(rhport);
    if (p_endpoint_desc == NULL)
        return false;

    ep_idx = USB_EP_GET_IDX(p_endpoint_desc->bEndpointAddress);
    dir_idx = USB_EP_GET_DIR_IDX(p_endpoint_desc->bEndpointAddress);

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return false;

    EDPxReg_SEL(ep_idx);

    // enable EP interrupts etc.
    if (dir_idx == DIR_IDX_IN || ep_idx == 0) { //TX (IN) EP or EP0
        if (CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_TXPKTRDY) { // TX packet ready
            CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO; // flush TX FIFO
        }
        val = USB_ARCS_DAINT_IN_EP_INT(ep_idx);
        usb_venus_ctrl.intr_txe |= val;
        CSK_USBC->INTRTXE |= val;
    } else { //RX (OUT) EP
        if (CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) { // RX packet ready
            CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO; // flush RX FIFO
            //CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
        }
        val = USB_ARCS_DAINT_OUT_EP_INT(ep_idx);
        usb_venus_ctrl.intr_rxe |= val;
        LOG_DBG("%s: intr_rxe = 0x%x", __func__, CSK_USBC->INTRRXE);
        CSK_USBC->INTRRXE |= val;
    }

    EDPxReg_SEL(0);

    memcpy(&usb_venus_ctrl.ep_info[dir_idx][ep_idx].fixed.attr_bits,
           &p_endpoint_desc->bmAttributes, 1);

    usbd_ep_set(ep_idx, dir_idx,
            p_endpoint_desc->wMaxPacketSize,
            (tusb_xfer_type_t)p_endpoint_desc->bmAttributes.xfer);

    usb_venus_ctrl.ep_info[dir_idx][ep_idx].ep_ena = 1U;
    if (ep_idx == 0)
        usb_venus_ctrl.ep_info[1][ep_idx].ep_ena = 1U;

    return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  // disable interrupt to prevent race condition
  dcd_int_disable(rhport);


  dcd_int_enable(rhport);
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
    (void) rhport;

    uint8_t ep_idx, dir_idx;
    uint16_t val;

    ARG_UNUSED(rhport);
    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return;

    EDPxReg_SEL(ep_idx);
    dcd_edpt_cancel_xfer_internal(rhport, ep_idx, dir_idx);

    //usbd_ep_flush(ep_addr); // it seems useless...

    /* Disable EP interrupts */
    if (dir_idx == DIR_IDX_IN || ep_idx == 0) { //TX (IN) EP or EP0
        val = USB_ARCS_DAINT_IN_EP_INT(ep_idx);
        usb_venus_ctrl.intr_txe &= ~val;
        LOG_DBG("%s: intr_txe = 0x%x", __func__, CSK_USBC->INTRTXE);
        CSK_USBC->INTRTXE &= ~val;

    } else { //RX (OUT) EP
        val = USB_ARCS_DAINT_OUT_EP_INT(ep_idx);
        usb_venus_ctrl.intr_rxe &= ~val;
        CSK_USBC->INTRRXE &= ~val;
    }

    /* release fifo */
    //TODO: CANNOT release the FIFO space of one single EP only!!

    /* De-activate, disable and set NAK for EP */
    usb_venus_ctrl.ep_info[dir_idx][ep_idx].ep_ena = 0;
}

static bool usbd_ep0_write(struct usb_ep_ctrl_prv *epp)
{
    bool bret = false;

    dcd_int_disable(0);
    EDPxReg_SEL(0);

    // Wait for previous TXPKTRDY to be cleared
    while (CSK_USBC->CSR0L & USB_ARCS_CSR0L_TXPKTRDY);

    if (epp->req_len > 0) {
        usbd_ep_pio_write(0, (uint8_t *)(epp->req_addr), epp->req_len);
    }

    // 如果这是一个短包或ZLP，设置DATAEND
    if (epp->req_len < epp->fixed.mps) {
        usb_venus_ctrl.status = USB_ARCS_STS_STATUS;
        LOG_DBG("before set TXPKTRDY and DATAEND");
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_TXPKTRDY | USB_ARCS_CSR0L_DATAEND;
    } else {
        LOG_DBG("before set TXPKTRDY");
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_TXPKTRDY;
    }
    dcd_int_enable(0);
    
    return true;  // 总是返回true，让TinyUSB继续处理后续包
}

static bool is_request_of_status_phase_in_setup(uint16_t data_len) {
    return (data_len == 0);
}

static bool edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes) {
    uint8_t const ep_idx = USB_EP_GET_IDX(ep_addr);
    uint8_t const dir_idx = USB_EP_GET_DIR_IDX(ep_addr);
    struct usb_ep_ctrl_prv *epp;

    epp = &usb_venus_ctrl.ep_info[dir_idx][ep_idx];

    EDPxReg_SEL(0);

    // Current only support up to 64 bytes
    TU_ASSERT(total_bytes <= 64);

    LOG_DBG("%s: status: %d, ep_addr = 0x%x, total_bytes = %d, buf_addr: %p",
         __FUNCTION__, usb_venus_ctrl.status, ep_addr, total_bytes, buffer);

    switch (usb_venus_ctrl.status)
    {
    case USB_ARCS_STS_SETUP:
        epp->req_addr = (uint32_t)buffer;
        epp->req_len = total_bytes;
        epp->xfer_len = 0;
        epp->last_len = 0;
        epp->term_early = 0;

        break;
    case USB_ARCS_STS_IN:
        epp->req_addr = (uint32_t)buffer;
        epp->req_len = total_bytes;
        epp->xfer_len = 0;
        epp->last_len = 0;
        epp->term_early = 0;
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
        if (usbd_ep0_write(epp) == false) {
            LOG_DBG("%s: usbd_ep0_write failed, ep_addr = 0x%x, ep_idx = %d, dir_idx = %d", __FUNCTION__, ep_addr, ep_idx, dir_idx);
        }
        break;
    case USB_ARCS_STS_OUT:
        epp->req_addr = (uint32_t)buffer;
        epp->req_len = total_bytes;
        epp->xfer_len = 0;
        epp->last_len = 0;
        epp->term_early = 0;
        LOG_DBG("set buffer(addr: %p) for OUT data\n", epp->req_addr);
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
        
        // control_epx_interrupt(dir_idx, ep_idx, true);
        break;
    case USB_ARCS_STS_STATUS:
        // Ignore TinyUSB status phase, Because USB hardware handled it automatically.
        if (is_request_of_status_phase_in_setup(total_bytes)) {
            dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, false);

            LOG_DBG("TinyUSB status phase, Clear RXPKTRDY, set DATAEND");
            CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY | USB_ARCS_CSR0L_DATAEND;

        }
        break;
    default:
        break;
    }

}


bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes) {
    struct usb_ep_ctrl_prv *epp;
    uint32_t addr = (uint32_t)buffer;
    uint8_t const ep_idx = USB_EP_GET_IDX(ep_addr);
    uint8_t const dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    LOG_DBG("%s: ep_addr = 0x%X, total_bytes = %d, ep_idx = %d, dir_idx = %d",
        __func__, ep_addr, total_bytes, ep_idx, dir_idx);

    ARG_UNUSED(rhport);
    epp = &usb_venus_ctrl.ep_info[dir_idx][ep_idx];
    epp->req_addr = addr;
    epp->req_len = total_bytes;
    epp->xfer_len = 0;
    epp->last_len = 0;
    epp->term_early = 0;


    if (ep_idx == 0) {
        dcd_int_disable(0);
        edpt0_xfer(rhport, ep_addr, buffer, total_bytes);
        dcd_int_enable(0);
        goto MY_EXIT;
    }

    // EP PIO Read / Write (other than EP0)
    if (dir_idx == DIR_IDX_OUT) {
        dcd_int_disable(0);
        EDPxReg_SEL(ep_idx);
        // We must clear the last RXPKTRDY until the new read buffer is ready
        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
        dcd_int_enable(0);
        // control_epx_interrupt(dir_idx, ep_idx, true);
    } else if (dir_idx == DIR_IDX_IN) {
        int32_t result = usbd_epx_write(ep_idx, (total_bytes==0));
        if (result < 0) {
            LOG_DBG("%s: IN transfer failed: ep=%d, error=%d", __func__, ep_idx, result);
            return false;
        }
    }

MY_EXIT:
    /* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
    EDPxReg_SEL(0);

    return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  
    uint8_t ep_idx, dir_idx;

    ARG_UNUSED(rhport);
    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return;

    EDPxReg_SEL(ep_idx);
    if(ep_idx == 0) { //endpoint0
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SENDSTALL;
    } else {        //endpoint1-15
        if (dir_idx == DIR_IDX_OUT) {
            CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_SENDSTALL;
        } else {
            CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_SENDSTALL;
        }
    }
    LOG_DBG("SENDSTALL is set on EP(0x%02x) by SW!\n", ep_addr);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
 
     uint8_t ep_idx, dir_idx;

    ARG_UNUSED(rhport);
    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (!usbd_ep_is_valid(ep_idx, dir_idx)) {
        return;
    }

    if (ep_idx == 0) {
        /* Not possible to clear stall for EP0 */
        return;
    }

    EDPxReg_SEL(ep_idx);
    //BSD: modified as same as the commit 09a57593c78f573f805a90e055d5b5f1aeae2c06
    //NOTE: it's SEN'T'STALL_MASK, NOT SEN'D'STALL_MASK!!
    if (dir_idx == DIR_IDX_OUT) {
        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_SENTSTALL_MASK;
    } else {
        CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_SENTSTALL_MASK;
    }
}

static inline void usb_venus_int_oep_handler(uint32_t intsr)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t ep_int_status;
    uint16_t rxcnt;
    uint8_t ep_idx;

    for (ep_idx = 1U; ep_idx < USB_ARCS_OUT_EP_NUM; ep_idx++) {
        if (intsr & (USB_ARCS_INTRRX_EP_POS << ep_idx)) {
            /* Read OUT RX EP interrupt status */
            EDPxReg_SEL(ep_idx);
            ep_int_status = CSK_USBC->RXCSRL;
            rxcnt = CSK_USBC->RXCOUNT;

            // check all EP interrupts, including RxPktRdy, OverRun, SentStall etc.
            if (ep_int_status & USB_ARCS_RXCSRL_SENTSTALL) { // SentStall
                CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_SENTSTALL;
                LOG_DBG("%s: SentStall is set on EP%d, clear it", __func__, ep_idx);
            }

            if (ep_int_status & USB_ARCS_RXCSRL_OVERRUN) { // Overrun
                if (ep_int_status & USB_ARCS_RXCSRL_RXPKTRDY)
                    CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO;

                CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_OVERRUN;
                LOG_DBG("%s: Overrun is set on EP%d, clear it", __func__, ep_idx);
            }

            if (!(ep_int_status & USB_ARCS_RXCSRL_RXPKTRDY))
                continue;

            epp = &usb_venus_ctrl.ep_info[0][ep_idx];
            if (rxcnt == 0) { // no data arrived
                //TODO: wait for OUT data by DMA, but ZLP arrives...
                // NO user RX or RX is pending, but NONE has been received (it always indicates END of last transfer)
                //if (epp->req_len == 0) {
                //if (epp->req_len == 0 || (epp->req_len > 0 && epp->xfer_len == 0)) {
                if (epp->req_len == 0 || (epp->req_len > 0 && usb_ep_get_xfer_bytes(epp) == 0)) {
                    LOG_DBG("%s: RXCOUNT is 0 on EP%d, skip it", __func__, ep_idx);
                    CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
                    EDPxReg_SEL(0);
                    continue; // check next EP
                }
            }

            if (epp->req_addr == 0 || epp->req_len == 0) {  // NO user RX
                LOG_DBG("%s: NO dcd_edpt_xfer (RX) for EP%d is called!, just flush RX FIFO!", __func__, ep_idx);
                // CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO;
                break;

            } else {
                usbd_epx_read(ep_idx);
                update_ep_rx_status(ep_idx);
            }

            /* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
            EDPxReg_SEL(0);
        }
    }
    /* Clear interrupt. */
}


static inline void usb_venus_int_iep_handler(uint32_t intsr)
{
    uint32_t ep_int_status;
    uint8_t ep_idx;
    struct usb_ep_ctrl_prv *epp;
    uint32_t xferred_bytes;

    for (ep_idx = 1U; ep_idx < USB_ARCS_IN_EP_NUM; ep_idx++) {
        if (intsr & (USB_ARCS_INTRTX_EP_POS << ep_idx)) {
            /* Read IN TX EP interrupt status */
            EDPxReg_SEL(ep_idx);
            ep_int_status = CSK_USBC->TXCSRL;

            LOG_DBG("USB IN EP%u interrupt status: 0x%x", ep_idx, ep_int_status);

            // check all EP interrupts, including TxPktRdy, UnderRun, SentStall etc.
            if (ep_int_status & USB_ARCS_TXCSRL_SENTSTALL) { // SentStall
                CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_SENTSTALL;
                LOG_DBG("%s: SentStall! clear it", __func__);
                continue;
            }

            if (!(ep_int_status & USB_ARCS_TXCSRL_TXPKTRDY)) { // TX done
                epp = &usb_venus_ctrl.ep_info[1][ep_idx];
                LOG_DBG("EP%d TX done, xfer_len=%d", ep_idx, epp->xfer_len);
                dcd_event_xfer_complete(0, USB_EP_GET_ADDR2(ep_idx, 1),
                                    epp->xfer_len, XFER_RESULT_SUCCESS, true);
            }

        }
    }

}

static void read_ep0_data(uint32_t buf[2]) {
    buf[0] = CSK_USBC->FIFOX[0];
    buf[1] = CSK_USBC->FIFOX[0];
}

static bool is_ep0_write_last_packet(uint8_t csr0l) {
    return (usb_venus_ctrl.ep0_data_len != 0) && ((csr0l & USB_ARCS_CSR0L_TXPKTRDY) == 0);
}

/* Handle interrupts on a control endpoint */
static void usb_venus_ep0_isr(void)
{
    uint8_t ep_idx = 0;
    uint16_t pkt_len;
    struct usb_ep_ctrl_prv *epp;

    //select endpoint0
    EDPxReg_SEL(ep_idx);
    uint8_t csr0l = CSK_USBC->CSR0L;
    uint8_t count = CSK_USBC->COUNT0 & USB_ARCS_COUNT0_MASK;

    
    LOG_DBG("CSR0L: 0x%X, received count: %d", csr0l & 0xFF, count);

    //endpoint0 setupend interrupt
    if(csr0l & USB_ARCS_CSR0L_SETUPEND){
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDSETUPEND;
        LOG_DBG("Endpoint0 setupend interrupt generated");
        // Clear any pending TXPKTRDY
        if (csr0l & USB_ARCS_CSR0L_TXPKTRDY) {
            CSK_USBC->CSR0L &= ~USB_ARCS_CSR0L_TXPKTRDY;
        }
        usb_venus_ctrl.status = USB_ARCS_STS_SETUP;
        return;
    }

    if(csr0l & USB_ARCS_CSR0L_SENTSTALL){
        CSK_USBC->CSR0L &= ~USB_ARCS_CSR0L_SENTSTALL;
        //BSD: NO SentStall interrupt according to MUSB datasheet!!
        usb_venus_ctrl.status = USB_ARCS_STS_SETUP;
        LOG_DBG("STALL has been sent!");
        LOG_DBG("Endpoint0 sentstall interrupt generated");
        return;
    }

    switch (usb_venus_ctrl.status) {
        case USB_ARCS_STS_SETUP:
        if(csr0l & USB_ARCS_CSR0L_RXPKTRDY) {
            pkt_len = count;
            if(pkt_len >= sizeof(tusb_control_request_t)) {
                uint32_t data[2];

                read_ep0_data(data);

                tusb_control_request_t *setup_tmp = (tusb_control_request_t *)data;

                #if CFG_TUSB_DEBUG >= CFG_TUD_LOG_LEVEL
                TU_LOG_USBD("Setup packet data:\r\n");
                // Hex dump data as uint8_t array
                for (int i = 0; i < sizeof(tusb_control_request_t); i++) {
                    TU_LOG_USBD("%02X ", ((uint8_t *)(setup_tmp))[i]);
                }
                TU_LOG_USBD("\r\n");
                #endif
                
                usb_venus_ctrl.ep0_req_dir = setup_tmp->bmRequestType_bit.direction;
                usb_venus_ctrl.ep0_request = setup_tmp->bRequest;

                if ((setup_tmp->bmRequestType_bit.direction == REQTYPE_DIR_TO_DEVICE) && (setup_tmp->wLength == 0)) {

                    if (setup_tmp->bRequest == TUSB_REQ_SET_ADDRESS) {
                        usb_venus_ctrl.address = setup_tmp->wValue;
                    } else {                   
                        usb_venus_ctrl.status = USB_ARCS_STS_STATUS;
                    }

                } else if (setup_tmp->bmRequestType_bit.direction == REQTYPE_DIR_TO_DEVICE) {
                    usb_venus_ctrl.status = USB_ARCS_STS_OUT;
                    usb_venus_ctrl.ep0_data_len = 0;
                    usb_venus_ctrl.ep0_xfer_len = 0; // initialize to 0 once new request arrives
                    LOG_DBG("%s: %d, change to STS_OUT phase", __func__, __LINE__);
                } else if (setup_tmp->bmRequestType_bit.direction == REQTYPE_DIR_TO_HOST) {
                    usb_venus_ctrl.status = USB_ARCS_STS_IN;
                    usb_venus_ctrl.ep0_data_len = setup_tmp->wLength;
                    usb_venus_ctrl.ep0_xfer_len = 0; // initialize to 0 once new request arrives
                    LOG_DBG("%s: %d, change to STS_IN phase", __func__, __LINE__);
                } else {
                    LOG_ERR("Unrecognized request type: %d, request: %d", 
                        setup_tmp->bmRequestType_bit.direction, setup_tmp->bRequest);
                }


                dcd_event_setup_received(0, (uint8_t const *)data, true);

            } else { 
                LOG_ERR("The received setup packet len must be %d, but is %d", sizeof(tusb_control_request_t), pkt_len);
            }
        }
        break;

        case USB_ARCS_STS_IN:
        {
            epp = &usb_venus_ctrl.ep_info[DIR_IDX_IN][ep_idx];
            LOG_DBG("USB_ARCS_STS_IN, ep0_data_len: %d, ep0_xfer_len: %d, epp->req_len: %d",
                 usb_venus_ctrl.ep0_data_len, usb_venus_ctrl.ep0_xfer_len, epp->req_len);
            usb_venus_ctrl.ep0_xfer_len += epp->req_len;

            dcd_event_xfer_complete(0, USB_EP_GET_ADDR(0, USB_EP_DIR_IN),
                                    epp->req_len, XFER_RESULT_SUCCESS, true);
            epp->req_addr = NULL;
            epp->req_len = 0;

            break;
        }
        case USB_ARCS_STS_OUT:
            if(csr0l & USB_ARCS_CSR0L_RXPKTRDY) {
                epp = &usb_venus_ctrl.ep_info[DIR_IDX_OUT][ep_idx];
                if (epp->req_addr == NULL) {
                    LOG_DBG("USB_ARCS_STS_OUT request addr is NULL");
                    return;
                }
                read_ep0_data(epp->req_addr);
                epp->xfer_len = count;
                usb_venus_ctrl.ep0_xfer_len += count;
                LOG_DBG("USB_ARCS_STS_OUT, count: %d, ep0_xfer_len: %d", count, usb_venus_ctrl.ep0_xfer_len);
                
                dcd_event_xfer_complete(0, USB_EP_GET_ADDR(0, USB_EP_DIR_OUT),
                    count, XFER_RESULT_SUCCESS, true);

                if (usb_venus_ctrl.ep0_xfer_len >= usb_venus_ctrl.ep0_data_len) {
                    LOG_DBG("change to STATUS phase from OUT phase");
                    usb_venus_ctrl.status = USB_ARCS_STS_STATUS;
                    // LOG_DBG("change to SETUP phase, ep0_xfer_len: %d, ep0_data_len: %d", usb_venus_ctrl.ep0_xfer_len, usb_venus_ctrl.ep0_data_len);
                    // usb_venus_ctrl.status = USB_ARCS_STS_SETUP;
                    // CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY | USB_ARCS_CSR0L_DATAEND;

                } else {
                    // CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
                }
                break;
            }
        case USB_ARCS_STS_STATUS:
            /* When CSRL0 is zero, it means that completion of sending a any length packet
            * or receiving a zero length packet. */

            if (usb_venus_ctrl.ep0_request == TUSB_REQ_SET_ADDRESS) {
                LOG_DBG("SET_ADDRESS, dev_addr: %d", usb_venus_ctrl.address);
                uint16_t dev_addr = usb_venus_ctrl.address;
                CSK_USBC->FADDR = (dev_addr) & USB_ARCS_FADDR_ADDR_MASK;

                LOG_DBG("Status stage complete, change status to SETUP");
                usb_venus_ctrl.status = USB_ARCS_STS_SETUP;

                dcd_event_xfer_complete(0, USB_EP_GET_ADDR(0, USB_EP_DIR_IN),
                    0, XFER_RESULT_SUCCESS, true);
            } else {
                if (usb_venus_ctrl.ep0_req_dir == REQTYPE_DIR_TO_HOST) {
                    LOG_DBG("Received status(out) packet");
                    LOG_DBG("Change to SETUP phase from STATUS phase");
                    usb_venus_ctrl.status = USB_ARCS_STS_SETUP;
                } else if (usb_venus_ctrl.ep0_req_dir == REQTYPE_DIR_TO_DEVICE) {
                    usb_venus_ctrl.status = USB_ARCS_STS_SETUP;
                    LOG_DBG("Sent status(in) packet");
                }
            }
            
        break;
    default:
        break;
    }

}

static void usb_venus_handle_reset(void)
{
    LOG_DBG("USB RESET event");

    //CSK_USBC->FADDR = 0; //TODO: is it right?
    //usb_venus_ctrl.address = 0;
    //usb_venus_ctrl.addressed = 0;


    LOG_DBG("%s, change status to USB_ARCS_STS_SETUP", __func__);
    usb_venus_ctrl.status = USB_ARCS_STS_SETUP;

    CSK_USBC->SOFT_RST |= (USB_ARCS_SOFT_RST_NRST | USB_ARCS_SOFT_RST_NRSTX);
    /* enable global EP interrupts */
    /* enable EP0 interrupts */
//    CSK_USBC->INTRTXE |= USB_ARCS_INTRTX_EP0;

    // resume USB core
    //CSK_USBC->POWER |= USB_ARCS_POWER_RESUME;

#if TUD_OPT_HIGH_SPEED
    /* Set device speed to High Speed */
    CSK_USBC->POWER |= USB_ARCS_POWER_HSENABLE;   //high speed enable
#else
    /* Set device speed to Full Speed */
    CSK_USBC->POWER &= ~USB_ARCS_POWER_HSENABLE;   //Full speed enable
#endif

    //BSD: DON'T connect on the initiative, tud_connect (calling dcd_connect) SHOULD be called explicitly...
//  /* Enable soft connect */
//  CSK_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;

    /* Enable usb module interrupt */
    CSK_USBC->INTRUSBE = 0x00;
    //CSK_USBC->INTRUSBE |= USB_ARCS_INTRUSBE_RESET;
    CSK_USBC->INTRUSBE = usb_venus_ctrl.intr_usbe;

    LOG_DBG("%s, enable EP0 interrupts", __func__);
    /* enable EP0 interrupts */
    control_epx_interrupt(DIR_IDX_IN, 0, true);
    control_epx_interrupt(DIR_IDX_OUT, 0, true);

    // reset internal state of the controller driver
    usbd_reset_internal_state();

}






/**
  \fn          void usb_dma_clear_channel_active_flag (uint8_t ch)
  \brief       Protected clear of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
*/
_FAST_FUNC_RO static void usb_dma_clear_channel_active_flag (uint8_t ch)
{
    // disable only USB interrupt for this DMA is used for USB only
    uint8_t irq_en = IRQ_enabled(IRQ_USBC_VECTOR);
    if (irq_en) disable_IRQ(IRQ_USBC_VECTOR);
    usb_venus_ctrl.channel_active &= ~(1U << ch);
    if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
}


void dcd_int_handler(uint8_t rhport) {
    uint32_t txsr, rxsr, intsr, dmaintsr, dmaie=0;

    ARG_UNUSED(rhport);


    //endpoint & common interrupt status -> read clear
    intsr = CSK_USBC->INTRUSB & CSK_USBC->INTRUSBE;
    txsr = CSK_USBC->INTRTX & CSK_USBC->INTRTXE;
    rxsr = CSK_USBC->INTRRX & CSK_USBC->INTRRXE;
    // for(uint32_t i=0; i<6; i++){
    //     dmaie |= (((CSK_USBC->USB_DMA[i].CNTL & USB_ARCS_DMA_CNTL_DMAIE) >> USB_ARCS_DMA_CNTL_DMAIE_POS)<<i);
    // }
    // dmaintsr = CSK_USBC->DMA_INTR & dmaie;

    // LOG_DBG("USB venus interrupt handler entered");
    LOG_DBG("USB INTRUSB= 0x%x, INTRTX = 0x%x, INTRRX = 0x%x, DMA_INTR = 0x%x", intsr, txsr, rxsr, dmaintsr);

    if (intsr & USB_ARCS_INTRUSB_RESET) {
        /* Reset detected */
        LOG_DBG("usb reset interrupt");
    #if TUD_OPT_HIGH_SPEED
        dcd_event_bus_reset(0, TUSB_SPEED_HIGH, true);
    #else
        dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
    #endif
        usb_venus_handle_reset();
    }

    if (intsr & USB_ARCS_INTRUSB_SUSPEND) {
        LOG_DBG("usb suspend interrupt");
        dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
    }

    if (intsr & USB_ARCS_INTRUSB_RESUME) {
        //NOTE: no RESUME interrupt if RESUME signal is triggered by CPU
        LOG_DBG("usb resume interrupt");
        dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
    }

    if (intsr & USB_ARCS_INTRUSB_SOF) {
        LOG_DBG("usb sof interrupt");
        dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
    }


    /* EP1-5 tx endpoint interrupt for IN endpoint */
    if (txsr & USB_ARCS_INTRTX_EP_MASK) {
        usb_venus_int_iep_handler(txsr);
    }

    /* EP1-5 rx endpoint interrupt for OUT endpoint */
    if (rxsr & USB_ARCS_INTRRX_EP_MASK) {
        usb_venus_int_oep_handler(rxsr);
    }

    if(dmaintsr & USB_DMA_INTR_EP_ALL_MASK){
        // usb_venus_dma_isr(dmaintsr);
    }

    /* EP0 tx&rx endpoint interrupt */
    if (txsr & USB_ARCS_INTRTX_EP0_MASK) {
        LOG_DBG("usb ep 0 interrupt, txsr=0x%x", txsr);
        usb_venus_ep0_isr();
    }


}





#endif
