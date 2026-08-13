
#include <stdint.h>
#include "usbd_core.h"
#include "usb_dc_lisa.h"

#define USB_DMA_BURST_MODE  USB_DMA_CNTL_DMA_BRSTM_0 // ONLY BRSTM_0 WORK for PSRAM
//#define USB_DMA_BURST_MODE  USB_DMA_CNTL_DMA_BRSTM_3 // _1, _2, _3 DON'T WORK for PSRAM

/**
 * CSR0L/CSR0H: Control and Status Register(Endpoint 0) Low/High
 *
 * [L0:RxPktRdy]
 * - This bit is set when a data packet has been received. 
 * - An interrupt is generated when this bit is set. 
 * - CPU clears this bit by setting the ServicedRxPktRdy bit
 * 
 * [L1:TxPktRdy] self-clearing
 * - CPU sets this bit after loading a data packet into the FIFO. 
 * - It is cleared automatically when a data packet has been transmitted.
 * - An interrupt is also generated at this point (if enabled)
 *
 * [L2:SentStall]
 * - This bit is set when a STALL handshake is transmitted. 
 * - CPU should clear this bit.
 * 
 * [L3:DataEnd] self-clearing
 * - CPU sets this bit when:
 *    + 1. setting TxPktRdy for the last data packet.
 *    + 2. clearing RxPktRdy after unloading the last data packet.
 *    + 3. setting TxPktRdy for a ZLP.
 * - It is cleared automatically
 * 
 * [L4:SetupEnd]
 * - This bit will be set when a control transaction ends before the DataEnd bit has been set.
 * - An interrupt will be generated and the FIFO flushed at this time. 
 * - The bit is cleared by the CPU writing a 1 to the ServicedSetupEnd bit.
 *
 * [L5:SendStall] self-clearing
 * - CPU writes a 1 to this bit to terminate the current transaction. 
 * - The STALL handshake will be transmitted and then this bit will be cleared automatically
 *
 * [L6:ServicedRxPktRdy] self-clearing
 * - CPU writes a 1 to this bit to clear the RxPktRdy bit. 
 * - It is cleared automatically.
 *
 * [L7:ServicedSetupEnd] self-clearing
 * - CPU writes a 1 to this bit to clear the SetupEnd bit. 
 * - It is cleared automatically
 *
 * [H0:FlushFIFO] self-clearing
 * - CPU writes a 1 to this bit to flush the next packet to be transmitted/read from the Endpoint 0 FIFO. 
 * - The FIFO pointer is reset and the TxPktRdy/RxPktRdy bit (below) is cleared. 
 * - FlushFIFO should only be used when TxPktRdy/RxPktRdy is set. At other times, it may cause data to be corrupted
 * 
 * [Hx:Reserved]
 */

static struct usb_ctrl_priv prvctx = { 0 };

static void usbd_dump_packet(const char *title, uint8_t epaddr, const void *const data, uint32_t const size)
{
#if 1
    #define FMT8(F, V) F F F F F F F F, V[-8], V[-7], V[-6], V[-5], V[-4], V[-3], V[-2], V[-1]    
    #define FMT7(F, V) F F F F F F F         , V[-7], V[-6], V[-5], V[-4], V[-3], V[-2], V[-1]    
    #define FMT6(F, V) F F F F F F                  , V[-6], V[-5], V[-4], V[-3], V[-2], V[-1]
    #define FMT5(F, V) F F F F F                           , V[-5], V[-4], V[-3], V[-2], V[-1]
    #define FMT4(F, V) F F F F                                    , V[-4], V[-3], V[-2], V[-1]
    #define FMT3(F, V) F F F                                             , V[-3], V[-2], V[-1]    
    #define FMT2(F, V) F F                                                      , V[-2], V[-1]
    #define FMT1(F, V) F                                                               , V[-1]

    uint8_t xlen = MIN(size, 16); uint8_t const *xptr = data;
    USBD_PRINT("| %s/ep%02x/%p+%ld: \e[4m", title, epaddr, data, size);
    for (uint8_t i = 0; i < xlen; i++) USBD_PRINT("%02x ", xptr[i]);
    xlen = size - xlen, xptr += size;
    if (xlen > 8) { xlen = 8; USBD_PRINT(".. "); }
    switch (xlen) {
    case 1: USBD_PRINT(FMT1("%02X ", xptr)); break;
    case 2: USBD_PRINT(FMT2("%02X ", xptr)); break;
    case 3: USBD_PRINT(FMT3("%02X ", xptr)); break;
    case 4: USBD_PRINT(FMT4("%02X ", xptr)); break;
    case 5: USBD_PRINT(FMT5("%02X ", xptr)); break;
    case 6: USBD_PRINT(FMT6("%02X ", xptr)); break;
    case 7: USBD_PRINT(FMT7("%02X ", xptr)); break;
    case 8: USBD_PRINT(FMT8("%02X ", xptr)); break;
    }
    USBD_PRINT("\e[0m\r\n");
#endif
}
static void usbd_edpt_notify(uint8_t busid, uint8_t epdir, uint8_t epidx)
{
    struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
    if (epp->reqsize) {
        const uint32_t szdone = epp->szdone;
        const uint8_t epaddr = EP_GET_ADDR(epidx, epdir);
        if (epdir == EPoIDX && (epp->szdone == epp->reqsize || epp->termin)) {
            usbd_dump_packet("rx", epaddr, (uint8_t *)epp->reqaddr, szdone);
            epp->reqaddr = epp->reqsize = epp->szdone = epp->szxfer = epp->termin = 0;
            usbd_event_ep_out_complete_handler(busid, epaddr, szdone);
        }
        if (epdir == EPiIDX && epp->szdone == epp->reqsize) {
            usbd_dump_packet("tx", epaddr, (uint8_t *)epp->reqaddr, szdone);
            epp->reqaddr = epp->reqsize = epp->szdone = epp->szxfer = 0;
            usbd_event_ep_in_complete_handler(busid, epaddr, szdone);
        }
    }
}

/**
 * @attention usb controller support 8/16/32-bits fifo access, but it requires keeping the same access 
 * size during one whole packet transmitting, unless the size of the last packet is less than the access 
 * size, then polling the rest data with other bit access mode is allowed.
 */
static int usbd_edpt_dma_xfer(uint8_t busid, uint8_t epdir, uint8_t epidx, const uint32_t addr, const uint32_t size)
{
    //USBD_ASSERT(epidx > EP0IDX && epidx != USB_EP_NO_DBG, "ep0/dbg cannot use dma");
    USBD_ASSERT(epidx > EP0IDX, "ep0 cannot use dma");
    struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
    USBD_ASSERT(epp->dma_ch == DMA_NOT_ASSIGNED, "ep%d dma ch=%d invalid", epidx, epp->dma_ch);
#if 0
    for (uint8_t i = 0; i < USB_DMA_CH_COUNT_AVAIL; i++) if (!(prvctx.dma_mask & USBD_BIT(i))) { epp->dma_ch = i; break; }
#else
    epp->dma_ch = epidx;
#endif
    USBD_ASSERT(epp->dma_ch != DMA_NOT_ASSIGNED, "ep%d dma no valid channel", epidx);
    prvctx.dma_mask |= USBD_BIT(epp->dma_ch);
    epp->szxfer = size;
    if (epdir == EPoIDX) {
/*
        IP_USBC->RXCSRH &= ~(USB_RXCSRH_AUTOCLEAR_MASK | USB_RXCSRH_DMAREQENAB_MASK | USB_RXCSRH_DMAREQMODE_MASK);
        IP_USBC->RXCSRH |= (0
            | USB_RXCSRH_AUTOCLEAR     // set 1 to clear AutoClear bit, auto cleared by hardware
            | USB_RXCSRH_DMAREQENAB    // set 1 to enable dma request enable
            | (DMA_REQ_MODE << USB_RXCSRH_DMAREQMODE_POS) // set 1 to select dma request mode 1, clear for mode 0
        );
        IP_USBC->RXCSRL &= ~USB_RXCSRL_OVERRUN;
*/
        uint32_t reg_val = IP_USBC->RXCSRH;
        reg_val &= ~(USB_RXCSRH_DMAREQMODE_1);
        reg_val |= (USB_RXCSRH_AUTOCLEAR    // set 1 to clear AutoClear bit, auto cleared by hardware
                | USB_RXCSRH_DMAREQENAB     // set 1 to enable dma request enable
                | (DMA_REQ_MODE << USB_RXCSRH_DMAREQMODE_POS) // set 1 to select dma request mode 1, clear for mode 0
                );
        IP_USBC->RXCSRH = reg_val;

    } else {
/*
        IP_USBC->TXCSRH &= ~(USB_TXCSRH_AUTOSET_MASK | USB_TXCSRH_DMAREQENAB_MASK | USB_TXCSRH_DMAREQMODE_MASK);
        IP_USBC->TXCSRH |= (0
            | USB_TXCSRH_AUTOSET       // set 1 to clear SetupEnd bit, auto cleared by hardware
            | USB_TXCSRH_DMAREQENAB    // set 1 to enable dma request enable
            | (DMA_REQ_MODE << USB_TXCSRH_DMAREQMODE_POS) // set 1 to select dma request mode 1, clear for mode 0
        );
        IP_USBC->TXCSRL &= ~USB_TXCSRL_UNDERRUN;
*/
        //NOTE: if EP TX is NOT in idle state, it may have side effect to change TXCSRH,
        //e.g. clear DMAREQENAB to cause IN EP interrupt, cause ZLP?
        uint32_t reg_val = IP_USBC->TXCSRH;
        reg_val &= ~(USB_TXCSRH_DMAREQMODE_1);
        reg_val |= (USB_TXCSRH_AUTOSET      // set 1 to clear SetupEnd bit, auto cleared by hardware
                | USB_TXCSRH_DMAREQENAB     // set 1 to enable dma request enable
                | (DMA_REQ_MODE << USB_TXCSRH_DMAREQMODE_POS) // set 1 to select dma request mode 1, clear for mode 0
                );
        IP_USBC->TXCSRH = reg_val;

    }
    CSK_USB_DMA_RegDef volatile *const dmareg = &IP_USBC->USB_DMA[epp->dma_ch];
    dmareg->ADDR = addr;      // address must be aligned with 4 bytes according to MUSB datasheet
    dmareg->COUNT = size;     // length no align requirement
    dmareg->CNTL = (epdir << USB_DMA_CNTL_DMA_DIR_POS) | (epidx << USB_DMA_CNTL_DMAEP_POS)
        | USB_DMA_CNTL_DMA_ENAB        // set 1 to start dma transfer
        | USB_DMA_CNTL_DMAIE           // set 1 to enable dma interrupt
        | USB_DMA_BURST_MODE        // set dma burst mode
        | USB_DMA_CNTL_DMAMODE_1;      // set 1 to select dma transfer mode 1, clear for mode 0

    USBD_LOGW("dma mode1: ep%d%d addr=%#lx size=%ld dmach=%d rxrdy=%d"
        , epdir, epidx, addr, size, epp->dma_ch, IP_USBC->RXCSRL & USB_RXCSRL_RXPKTRDY);
    return 0;
}
static int usbd_edpt_pio_xfer(uint8_t busid, uint8_t epdir, uint8_t epidx, const uint32_t addr, const uint32_t size)
{
    struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
    epp->szxfer = size;
    if (addr == 0 || size == 0) return 0;
    volatile uint8_t *const fifo08 = (uint8_t *)&IP_USBC->FIFOX[epidx];
    uint32_t xcnt = addr & 3;
    uint8_t *pd08 = NULL;
    if (!xcnt) { // 32-bits aligned
        volatile uint32_t *const fifo32 = (uint32_t *)fifo08;
        uint32_t *pd32 = (uint32_t *)addr;
        xcnt = size >> 2;
        if (epdir == EPoIDX) while (xcnt--) *pd32++ = *fifo32;
        else                 while (xcnt--) *fifo32 = *pd32++;
        pd08 = (uint8_t *)pd32, xcnt = size & 3;
    } else if (xcnt == 2) { // 16-bits aligned
        volatile uint16_t *const fifo16 = (uint16_t *)fifo08;
        uint16_t *pd16 = (uint16_t *)addr;
        xcnt = size >> 1;
        if (epdir == EPoIDX) while (xcnt--) *pd16++ = *fifo16;
        else                 while (xcnt--) *fifo16 = *pd16++;
        pd08 = (uint8_t *)pd16, xcnt = size & 1;
    } else {
        pd08 = (uint8_t *)addr, xcnt = size;
    }
    if (pd08 && xcnt) {
        if (epdir == EPoIDX) while (xcnt--) *pd08++ = *fifo08;
        else                 while (xcnt--) *fifo08 = *pd08++;
    }
    epp->szdone += size;
    return size;
}
static int usbd_ep0o_pio_read_request(uint8_t busid)
{
    struct usb_ep_info *const epp = &prvctx.ep_info[EPoIDX][EP0IDX];
    USBD_ASSERT(epp->szdone <= epp->reqsize, "ep0 buff overflow!");
    // read ep0(it should be in data-out/status stage)
    if (prvctx.ep0stat == USBD_EP0_STATE_IN_DATA) USBD_LOGE("ep0: read in data-in-stage!");
    else if (!(IP_USBC->CSR0L & USB_CSR0L_RXPKTRDY)) USBD_LOGW("ep0: rx none(RxPktRdy=0)!");
    else {  // Count0/RxCount is valid only when RxPktRdy bit is set
        uint16_t rxrdy = IP_USBC->COUNT0 & USB_COUNT0_MASK;
        uint32_t rxreq = epp->reqsize - epp->szdone;
        USBD_ASSERT(rxreq || !rxrdy, "ep0 rxreq=%ld but rxrdy(%d)>0!", rxreq, rxrdy);
        if (rxrdy) {
            if (rxreq > rxrdy) rxreq = rxrdy;
            usbd_edpt_pio_xfer(busid, EPoIDX, EP0IDX, epp->reqaddr + epp->szdone, rxreq);
        }
        // notify host of DATA received if the packet is read out, otherwise notify NAK in later incoming packet..
        if (rxreq == rxrdy) {
            if (rxrdy < epp->ep_mps || epp->szxfer == epp->reqsize) {
                IP_USBC->CSR0L |= USB_CSR0L_SERVICEDRXPKTRDY | USB_CSR0L_DATAEND; // clear RxPktRdy and mark data-end
                prvctx.ep0stat = USBD_EP0_STATE_SETUP;
            } else IP_USBC->CSR0L |= USB_CSR0L_SERVICEDRXPKTRDY; // clear RxPktRdy only
        }
    }
    usbd_edpt_notify(busid, EPoIDX, EP0IDX);
    return 0;
}
static int usbd_ep0i_pio_write_request(uint8_t busid, bool zlp_if_nul)
{
    struct usb_ep_info *const epp = &prvctx.ep_info[EPiIDX][EP0IDX];
    // write ep0(it should be in data-in/status stage)
    if (prvctx.ep0stat == USBD_EP0_STATE_OUT_DATA) USBD_LOGE("ep0 write in data-out-stage!");
    else if (IP_USBC->CSR0L & USB_CSR0L_TXPKTRDY); //USBD_LOGE("ep0: tx busy(TxPktRdy=1)!");
    else { // continue writing only if TxPktRdy bit is cleared
        uint32_t txreq = epp->reqsize - epp->szdone;
        USBD_ASSERT(txreq || zlp_if_nul, "ep0 txreq=0 but not zlp!");
        if (txreq > epp->ep_mps) txreq = epp->ep_mps;
        usbd_edpt_pio_xfer(busid, EPiIDX, EP0IDX, epp->reqaddr + epp->szdone, txreq);
        if (txreq < epp->ep_mps || epp->szdone == epp->reqsize/*epp->szxfer == epp->reqsize*/) {
            IP_USBC->CSR0L |= USB_CSR0L_TXPKTRDY | USB_CSR0L_DATAEND; // clear TxPktRdy and mark data-end
            prvctx.ep0stat = USBD_EP0_STATE_SETUP;
        } else IP_USBC->CSR0L |= USB_CSR0L_TXPKTRDY; // clear TxPktRdy
    }
    return 0;
}
static int usbd_epxo_pio_read_request(uint8_t busid, uint8_t epidx)
{
    USBD_ASSERT(epidx > EP0IDX, "ep0 read is not allowed!");
    struct usb_ep_info *const epp = &prvctx.ep_info[EPoIDX][epidx];
    USBD_ASSERT(epp->szdone <= epp->reqsize, "ep%d done(%ld)>req(%ld)!", epidx, epp->szdone, epp->reqsize);
    if (!(IP_USBC->RXCSRL & USB_RXCSRL_RXPKTRDY)) ; //USBD_LOGW("ep%d: rx none(RxPktRdy=0)!", epidx);
    else { // Count0/RxCount is valid only when RxPktRdy bit is set
        uint32_t rxreq = epp->reqsize - epp->szdone;
        uint16_t rxrdy = IP_USBC->RXCOUNT & USB_RXCOUNT_MASK;
        USBD_ASSERT(rxreq || !rxrdy, "ep%d: rxreq=%ld but rxrdy(%d)>0!", epidx, rxreq, rxrdy);
        IP_USBC->RXCSRH &= ~(USB_RXCSRH_AUTOCLEAR_MASK | USB_RXCSRH_DMAREQENAB_MASK | USB_RXCSRH_DMAREQMODE_MASK);
        if (epp->dma_ch != DMA_NOT_ASSIGNED) {
            USBD_LOGW("ep%d%d dma disable", EPoIDX, epidx);
            IP_USBC->USB_DMA[epp->dma_ch].CNTL &= ~(USB_DMA_CNTL_DMA_ENAB_MASK | USB_DMA_CNTL_DMAIE_MASK | USB_DMA_CNTL_DMAERR_MASK);
            prvctx.dma_mask &= ~USBD_BIT(epp->dma_ch);
            epp->dma_ch = DMA_NOT_ASSIGNED;
        }
        if (rxrdy) {
            if (rxreq > rxrdy) rxreq = rxrdy;
            usbd_edpt_pio_xfer(busid, EPoIDX, epidx, epp->reqaddr + epp->szdone, rxreq);
        }
        // notify host of DATA received if the packet is read out, otherwise notify NAK in later incoming packet..
        if (rxreq == rxrdy) {
            // USBD_LOGE("clear ep%d%d rxpktrdy", EPoIDX, epidx);
            IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY; // clear RxPktRdy
        }
    }
    usbd_edpt_notify(busid, EPoIDX, epidx);
    return 0;
}
static int usbd_epxi_pio_write_request(uint8_t busid, uint8_t epidx, bool zlp_if_nul)
{
    USBD_ASSERT(epidx > EP0IDX, "ep0 not allowed!");
    struct usb_ep_info *const epp = &prvctx.ep_info[EPiIDX][epidx];
    if (IP_USBC->TXCSRL & USB_TXCSRL_TXPKTRDY) USBD_LOGW("ep%d: tx busy(TxPktRdy=1)!", epidx);
    else { // continue writing only if TxPktRdy bit is cleared
        uint32_t txreq = epp->reqsize - epp->szdone;
        USBD_ASSERT(txreq || zlp_if_nul, "ep%d txreq=0 but not zlp!", epidx);
        IP_USBC->TXCSRH &= ~(USB_TXCSRH_AUTOSET_MASK | USB_TXCSRH_DMAREQENAB_MASK | USB_TXCSRH_DMAREQMODE_MASK);
        if (epp->dma_ch != DMA_NOT_ASSIGNED) {
            USBD_LOGW("ep%d%d dma disable", EPiIDX, epidx);
            IP_USBC->USB_DMA[epp->dma_ch].CNTL &= ~(USB_DMA_CNTL_DMA_ENAB_MASK | USB_DMA_CNTL_DMAIE_MASK | USB_DMA_CNTL_DMAERR_MASK);
            prvctx.dma_mask &= ~USBD_BIT(epp->dma_ch);
            epp->dma_ch = DMA_NOT_ASSIGNED;
        }
        if (txreq > epp->ep_mps) txreq = epp->ep_mps;
        usbd_edpt_pio_xfer(busid, EPiIDX, epidx, epp->reqaddr + epp->szdone, txreq);
        IP_USBC->TXCSRL |= USB_TXCSRL_TXPKTRDY; // clear TxPktRdy only
    }
    return 0;
}
static int usbd_edpt_xfer_request(uint8_t busid, uint8_t epaddr, const uint32_t addr, const uint32_t size)
{
    uint8_t const epidx = USB_EP_GET_IDX(epaddr), epdir = EP_GET_IDIR(epaddr);
    struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
    epp->reqaddr = addr, epp->reqsize = size, epp->szdone = epp->szxfer = epp->termin = 0;
    // USBD_LOGD("ep=%#02x size=%d dmach=%d", epaddr, size, epp->dma_ch);
    EPSEL(epidx);
    if (epidx == EP0IDX) {
        if (epdir == EPoIDX) usbd_ep0o_pio_read_request(busid);
        else usbd_ep0i_pio_write_request(busid, !size);
    } else if (size == 0 || !epp->dma_en) { // pio transfer
        if (epdir == EPiIDX) usbd_epxi_pio_write_request(busid, epidx, !size);
        else if (IP_USBC->RXCSRL & USB_RXCSRL_RXPKTRDY) // if not RxPkgRdy, defer above operations into next ISR of RxPkgRdy...
            usbd_epxo_pio_read_request(busid, epidx);
    } else { // dma enabled
    #if 0
        if (!(addr & 3)) // transfer aligned data
            usbd_edpt_dma_xfer(busid, epdir, epidx, addr, size);
        else if (epdir == EPiIDX) { // write non-aligned data
            uint32_t txreq = 4 - (addr & 3);
            if (size - txreq < 4)
                usbd_epxi_pio_write_request(busid, epidx, false);
            else {
                usbd_edpt_pio_xfer(busid, epdir, epidx, addr, txreq);
                usbd_edpt_dma_xfer(busid, epdir, epidx, addr + txreq, size - txreq);
            }
        } else if (epdir == EPoIDX) { // read non-aligned data
            if (IP_USBC->RXCSRL & USB_RXCSRL_RXPKTRDY) { // if not RxPkgRdy, defer above operations into next ISR of RxPkgRdy...
                uint32_t rxreq = 4 - (addr & 3), rxrdy = IP_USBC->RXCOUNT;
                if (size - rxreq < 4 && size <= rxrdy) usbd_epxo_pio_read_request(busid, epidx);
                else {
                    usbd_edpt_pio_xfer(busid, epdir, epidx, addr, rxreq);
                    if (size <= rxreq) usbd_edpt_notify(busid, epdir, epidx);
                    else usbd_edpt_dma_xfer(busid, epdir, epidx, addr + rxreq, size - rxreq);
                }
            } 
        }
    #else
        USBD_ASSERT((addr & 3) == 0, "ep%d%d addr=%#lx", epdir, epidx, addr);
        usbd_edpt_dma_xfer(busid, epdir, epidx, addr, size);
    #endif
    }
    EPSEL(EP0IDX); // Change index to EP0, otherwise EP0 control transfer will not respond
    return 0;
}

// irq handlers
static void usbd_handle_ep0_xfer_complete(uint8_t busid)
{
    struct usb_ep_info *const epp = &prvctx.ep_info[0][EP0IDX];
    EPSEL(EP0IDX);
    if (IP_USBC->CSR0L & USB_CSR0L_SETUPEND) {
        IP_USBC->CSR0L |= USB_CSR0L_SERVICEDSETUPEND;  // clear SetupEnd
        USBD_LOGE("switch ep0 to setup-stage as ep0 setupend");
        prvctx.ep0stat = USBD_EP0_STATE_SETUP;
        return; //TODO: is it OK?
    }
    if (IP_USBC->CSR0L & USB_CSR0L_SENTSTALL) {
        IP_USBC->CSR0L &= ~USB_CSR0L_SENTSTALL;  // clear SentStall
        USBD_LOGE("switch ep0 to setup-stage as stall");
        prvctx.ep0stat = USBD_EP0_STATE_SETUP;
        return; //TODO: is it OK?
    }
    switch (prvctx.ep0stat) {
    case USBD_EP0_STATE_SETUP:
        // USBD_LOGD("USB ep0 setup-stage");
        if (!(IP_USBC->CSR0L & USB_CSR0L_RXPKTRDY)) ;//USBD_LOGE("ep0: rx none(RxPktRdy=0)!");
        else {
            uint16_t szpkt = IP_USBC->COUNT0 & USB_COUNT0_MASK;
            if (szpkt == sizeof(struct usb_setup_packet)) {
                // struct usb_setup_packet __attribute__((aligned(32))) setup = { 0 };
                struct usb_setup_packet *const setup = &prvctx.setup;
                if (sizeof(*setup) == usbd_edpt_pio_xfer(busid, EPoIDX, EP0IDX, (uint32_t)setup, sizeof(*setup))) {
                    epp->reqsize = setup->wLength;
                    epp->szxfer = 0; // initialize to 0 once new request arrives
                    if (epp->reqsize) {
                        // bmRequestType[7] stands for direction, O=0 I=1
                        prvctx.ep0stat = (setup->bmRequestType & USBD_BIT(7)) ? USBD_EP0_STATE_IN_DATA : USBD_EP0_STATE_OUT_DATA;
                        IP_USBC->CSR0L |= USB_CSR0L_SERVICEDRXPKTRDY;  // clear RxPktRdy
                    } else {
                        // DATAEND must be set here. Because status stage don't check txpktrdy signal,
                        // if software don't set DATAEND before status stage end, SETUPEND flag will be set.
                        IP_USBC->CSR0L |= (USB_CSR0L_SERVICEDRXPKTRDY | USB_CSR0L_DATAEND);  // clear RxPktRdy and mark data-end
                        USBD_LOGI("ep0: send zlp");
                        usbd_event_ep_in_complete_handler(busid, EP_GET_ADDR(EP0IDX, EPiIDX), 0);
                    }
                    // USBD_LOGW("ep0 setup-stage, wLength is %d", setup.wLength);
                    usbd_event_ep0_setup_complete_handler(busid, (uint8_t *)setup);
                }
            } else {
                IP_USBC->CSR0L |= USB_CSR0L_SERVICEDRXPKTRDY;
                if (szpkt)  USBD_LOGE("ep0 illegal size=%d in setup-stage!", szpkt);
                else        USBD_LOGE("ep0 zlp received in setup/status-stage!");    
            }
        }
        break;
    case USBD_EP0_STATE_OUT_DATA:
        // USBD_LOGD("usb ep0 dat-out-stage");
        if (IP_USBC->CSR0L & USB_CSR0L_RXPKTRDY) usbd_ep0o_pio_read_request(busid);
        else USBD_LOGE("ep0: rx none(RxPktRdy=0)!");
        break;
    case USBD_EP0_STATE_IN_DATA:
        // USBD_LOGD("usb ep0 dat-in-stage");
        if (!(IP_USBC->CSR0L & USB_CSR0L_TXPKTRDY)) {
            usbd_edpt_notify(busid, EPiIDX, EP0IDX);
            usbd_ep0i_pio_write_request(busid, false);
        } else USBD_LOGE("ep0: tx busy(TxPktRdy=1)!");
        break;
    case USBD_EP0_STATE_IN_STATUS:
        // USBD_LOGD("usb ep0 status-stage");
        if (!(prvctx.dev_addr & 0x80) && (prvctx.dev_addr & USB_FADDR_ADDR_MASK)) {
            IP_USBC->FADDR = prvctx.dev_addr & USB_FADDR_ADDR_MASK;
            __DSB();
            prvctx.dev_addr |= 0x80; // set bit7 to indicate address allocated
        }
        prvctx.ep0stat = USBD_EP0_STATE_SETUP;
        break;
    case USBD_EP0_STATE_OUT_STATUS:
    default:
        break;
    }
}
static void usbd_handle_epi_send_complete(uint8_t busid, uint16_t txis)
{    
    for (uint8_t epidx = 1; epidx < USB_IN_EP_NUM; epidx++) {
        if (!(txis & (USB_INTRTX_EP_POS << epidx))) continue;
        EPSEL(epidx);

        struct usb_ep_info *const epp = &prvctx.ep_info[EPiIDX][epidx];
        USBD_LOGD("ep%d%d/tx: addr=%#lx szreq=%ld szdone=%ld", EPiIDX, epidx, epp->reqaddr, epp->reqsize, epp->szdone);

        // check all ep interrupts, including TxPktRdy/UnderRun/SentStall etc.
        uint32_t txcsr = IP_USBC->TXCSRL;
        if (txcsr & USB_TXCSRL_SENTSTALL) {
            IP_USBC->TXCSRL &= ~USB_TXCSRL_SENTSTALL;
            USBD_LOGW("epi: sentstall!");
        }
        if (txcsr & USB_TXCSRL_UNDERRUN) {
            // BSD: UnderRun is set if an in-token is received when TxPktRdy is not set
            // Here we just set TxPktRdy, and it will send an ZLP packet, otherwise it will be 
            // trapped into the UnderRun interrupt repeatedly...
            if (!(txcsr & USB_TXCSRL_TXPKTRDY)) IP_USBC->TXCSRL |= USB_TXCSRL_TXPKTRDY;
            IP_USBC->TXCSRL &= ~USB_TXCSRL_UNDERRUN;
            USBD_LOGW("epi: underrun!");
        }

        // BSD NOTE: Here dcd_event_xfer_complete is called for TX (IN) and DMA Request Mode 0 (with IN EP interrupt)
        // at this point, the whole epi/tx process(ram => in-fifo => usb) is finally done!
        if (!(txcsr & USB_TXCSRL_TXPKTRDY)) { // TX done
            if (epp->dma_ch == DMA_NOT_ASSIGNED || DMA_REQ_MODE == 0) {
                epp->szdone += epp->szxfer;
                usbd_edpt_notify(busid, EPiIDX, epidx);
            }
        }
    }
    EPSEL(EP0IDX); // change index to EP0, otherwise EP0 control transfer will not respond when index is not zero
    /* Clear interrupt. */
}
static void usbd_handle_epo_recv_complete(uint8_t busid, uint16_t rxis)
{
    for (uint8_t epidx = 1; epidx < USB_OUT_EP_NUM; epidx++) {
        if (!(rxis & (USB_INTRRX_EP_POS << epidx))) continue;
        EPSEL(epidx);

        // check all ep interrupts, including RxPktRdy/OverRun/SentStall etc. 
        uint8_t rxcsr = IP_USBC->RXCSRL; // interrupt status
        if (rxcsr & USB_RXCSRL_SENTSTALL) {
            IP_USBC->RXCSRL &= ~USB_RXCSRL_SENTSTALL;
            USBD_LOGE("ep%d%d sentstall!", EPoIDX, epidx);
        }
        if (rxcsr & USB_RXCSRL_OVERRUN) {
            // OverRun is set if an out packet cannot be loaded into the RxFIFO.
            // Note: it is only valid for iso ep, it returns 0 always for bulk ep.
            // FIXME: we just flush RxFIFO(?), or else it could trap into the OverRun repeatedly
            if (rxcsr & USB_RXCSRL_RXPKTRDY) IP_USBC->RXCSRL |= USB_RXCSRL_FLUSHFIFO;
            IP_USBC->RXCSRL &= ~USB_RXCSRL_OVERRUN;
            USBD_LOGE("ep%d%d overrun!", EPoIDX, epidx);
        }
        if (!(rxcsr & USB_RXCSRL_RXPKTRDY)) continue;

        uint16_t rxcnt = IP_USBC->RXCOUNT;
        struct usb_ep_info *const epp = &prvctx.ep_info[EPoIDX][epidx];
        CSK_USB_DMA_RegDef volatile *const dmareg = &IP_USBC->USB_DMA[epp->dma_ch];
        USBD_LOGI("ep0%d/rx: addr=%#lx size=%ld done=%ld rxcnt=%d", epidx, epp->reqaddr, epp->reqsize, epp->szdone, rxcnt);
        if (rxcnt == 0) {
            // TODO: wait for OUT data by DMA, but ZLP arrives...
            // NO user RX, or RX is pending, but NONE has been received(it always indicates END of last transfer)
            if (epp->reqsize == 0) {
                // USBD_LOGE("clear ep%d%d rxpktrdy", EPoIDX, epidx);
                IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY;
                usbd_edpt_notify(busid, EPoIDX, epidx); // danny add
                continue;
            }
            uint32_t szdone = epp->szdone;
            if (epp->dma_ch != DMA_NOT_ASSIGNED && (dmareg->CNTL & USB_DMA_CNTL_DMA_ENAB))
                szdone += epp->szxfer - dmareg->COUNT;
            if (szdone == 0) {
                // USBD_LOGE("clear ep%d%d rxpktrdy", EPoIDX, epidx);
                IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY;
                continue;
            }
        }
        if (epp->reqaddr == 0 || epp->reqsize == 0) {
            USBD_LOGW("ep%d%d no user rx, keep data in fifo", EPoIDX, epidx);
            //USBD_LOGW("ep%d%d no user rx, flush fifo and drop data", EPoIDX, epidx);
            //IP_USBC->RXCSRL |= USB_RXCSRL_FLUSHFIFO;
        } else if (epp->dma_ch == DMA_NOT_ASSIGNED) {
            USBD_LOGW("ep%d%d no dma setup, do pio read", EPoIDX, epidx);
            usbd_epxo_pio_read_request(busid, epidx);
        } else { // dma enabled
            // start address is aligned with 4, or szdone>0, indicating that DMA has already started and this is last short package
            if (!((epp->reqaddr + epp->szdone) & 3)) {
                // fetch all data in RX FIFO, and remain count may be greater than RXCOUNT!
                uint32_t remain = dmareg->COUNT;
                // if (rxcnt > 0) epp->szxfer = remain;
                if (rxcnt > 0 && rxcnt < remain) {
                    // when several mps packets have been transferred and there are extra odd bytes 
                    usbd_edpt_pio_xfer(busid, EPoIDX, epidx, epp->reqaddr + epp->szdone, rxcnt);
                    epp->szdone += rxcnt;
                    epp->szxfer = epp->reqsize - epp->szdone;
                    dmareg->COUNT= epp->reqsize - epp->szdone;
                    dmareg->ADDR = epp->reqaddr + epp->szdone;
                    USBD_LOGE("ep0%d: szdone=%ld szreq=%ld szxfer=%ld, wait dma", epidx, epp->szdone, epp->reqsize, epp->szxfer);
                    IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY;
                    // epp->szdone = epp->reqsize - remain;
                    // epp->termin = 1;
                    continue;
                }
                if (rxcnt < 8) { //TODO: 8 => ?
                    USBD_LOGI("disable dma, do pio read");
                    IP_USBC->RXCSRH &= ~(USB_RXCSRH_AUTOCLEAR | USB_RXCSRH_DMAREQENAB | USB_RXCSRH_DMAREQMODE_1);
                    dmareg->CNTL &= ~(USB_DMA_CNTL_DMA_ENAB | USB_DMA_CNTL_DMAIE | USB_DMA_CNTL_DMAERR);
                    dmareg->COUNT = 0;
                    prvctx.dma_mask &= ~USBD_BIT(epp->dma_ch);
                    epp->dma_ch = DMA_NOT_ASSIGNED;
                    usbd_edpt_pio_xfer(busid, EPoIDX, epidx, epp->reqaddr + epp->szdone, rxcnt);
                    if (rxcnt == 0 && epp->szxfer > remain) epp->szdone += epp->szxfer - remain;
                    // USBD_LOGE("clear ep%d%d rxpktrdy", EPoIDX, epidx);
                    IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY;
                    usbd_edpt_notify(busid, EPoIDX, epidx);
                } else { // continue using DMA to read short packet
                    if (rxcnt < remain) dmareg->COUNT = rxcnt;
                    dmareg->CNTL = USB_DMA_CNTL_DMA_DIR_RXEP | (epidx << USB_DMA_CNTL_DMAEP_POS)
                        | USB_DMA_CNTL_DMA_ENAB        // enable dma
                        | USB_DMA_CNTL_DMAIE           // enable interrupt
                        | USB_DMA_CNTL_DMAMODE_0       // mode 0
                        | USB_DMA_BURST_MODE;    // burst mode
                    USBD_LOGI("dma mode0: ep%d%d addr=%#lx size=%ld dmach=%d", 0
                        , epidx, epp->reqaddr, epp->reqsize, epp->dma_ch);
                }
            } else {
                uint16_t rxreq = 4 - (epp->reqaddr & 3), szpkt = IP_USBC->RXCOUNT & USB_RXCOUNT_MASK;
                if (epp->reqsize - rxreq < 4 && epp->reqsize <= szpkt)
                    usbd_epxo_pio_read_request(busid, epidx);
                else {
                    usbd_edpt_pio_xfer(busid, EPoIDX, epidx, epp->reqaddr, rxreq);
                    if (epp->reqsize > rxreq)
                        usbd_edpt_dma_xfer(busid, EPoIDX, epidx, epp->reqaddr + rxreq, epp->reqsize - rxreq);
                    else {
                        // USBD_LOGE("clear ep%d%d rxpktrdy", EPoIDX, epidx);
                        IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY;
                        usbd_edpt_notify(busid, EPoIDX, epidx);
                    }
                }
            }
        }
    }
    EPSEL(EP0IDX); // change index to EP0, otherwise EP0 control transfer will not respond when index is not zero
    /* Clear interrupt. */
}
static void usbd_handle_dma_xfer_complete(uint8_t busid, uint32_t dmas)
{
    for (uint8_t dmach = 0; dmach < USB_DMA_CH_COUNT_AVAIL; dmach++) {
        CSK_USB_DMA_RegDef volatile *const dmareg = &IP_USBC->USB_DMA[dmach];
        if (!((dmareg->CNTL & USB_DMA_CNTL_DMAIE) && (dmas & USBD_BIT(dmach)))) continue;

        USBD_LOGI("disable dma ch=%d", dmach);
        dmareg->CNTL &= ~USB_DMA_CNTL_DMAIE; // danny add

        uint8_t epidx = (dmareg->CNTL & USB_DMA_CNTL_DMAEP_MASK) >> USB_DMA_CNTL_DMAEP_POS
            , epdir = (dmareg->CNTL & USB_DMA_CNTL_DMA_DIR_MASK) >> USB_DMA_CNTL_DMA_DIR_POS;
        struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
        prvctx.dma_mask &= ~USBD_BIT(dmach);
        epp->dma_ch = DMA_NOT_ASSIGNED;
        USBD_LOGD("dma-ep%d%d-done: addr=%#lx size=%ld xfer=%ld", epdir, epidx, epp->reqaddr, epp->reqsize, epp->szxfer);

        EPSEL(epidx);
        if (epdir == EPoIDX) {
            if ((IP_USBC->RXCSRL & USB_RXCSRL_RXPKTRDY) && (!(IP_USBC->RXCSRH & USB_RXCSRH_AUTOCLEAR) || (0 < epp->szxfer && epp->szxfer < epp->ep_mps))) {
                // USBD_LOGE("clear ep%d%d rxpktrdy", EPoIDX, epidx);
                IP_USBC->RXCSRL &= ~USB_RXCSRL_RXPKTRDY;
            }
        } else if (epdir == EPiIDX) {
            if (IP_USBC->TXCSRL & USB_TXCSRL_UNDERRUN) IP_USBC->TXCSRL &= ~USB_TXCSRL_UNDERRUN;
            if (!(IP_USBC->TXCSRL & USB_TXCSRL_TXPKTRDY) && (!(IP_USBC->TXCSRH & USB_TXCSRH_AUTOSET) || (0 < epp->szxfer && epp->szxfer < epp->ep_mps)))
                IP_USBC->TXCSRL |= USB_TXCSRL_TXPKTRDY;
        }
        // here is called for rx/out or tx/in and dma req mode1(no ep-in interrupt)
        // for epo/rx process, the whole rx stage(bus=>out-fifo=>ram) has completed at this point.
        // for epi/tx process, only the first part of tx stage(ram=>in-fifo=>bus) has done
        if (epdir == EPoIDX || DMA_REQ_MODE == 1) {
            epp->szdone += epp->szxfer;
            usbd_edpt_notify(busid, epdir, epidx);
        }
    }
    EPSEL(EP0IDX); // change index to EP0, otherwise EP0 control transfer will not respond when index is not zero
}
static void usbd_handle_reset(uint8_t busid)
{
    USBD_LOGI("reset");
    prvctx.ep0stat = USBD_EP0_STATE_SETUP;
    IP_USBC->SOFT_RST |= USB_SOFT_RST_NRST | USB_SOFT_RST_NRSTX;
    IP_USBC->POWER |= USB_POWER_HSENABLE;
    IP_USBC->INTRUSBE = prvctx.intr.usbe;
    IP_USBC->INTRTXE = prvctx.intr.txe;
    IP_USBC->INTRRXE = prvctx.intr.rxe;

    // reset internal state of the controller driver, if NOT addressed yet, already in initial state, do nothing...
    if (!prvctx.dev_addr) return;

    struct usb_ep_info *const epp = &prvctx.ep_info[0][EP0IDX];
    // stop DMA operation if any
    for (uint8_t i = 0; i < USB_DMA_CH_COUNT_AVAIL; i++)
        if (prvctx.dma_mask & USBD_BIT(i)) {
            CSK_USB_DMA_RegDef volatile *const dmareg = &IP_USBC->USB_DMA[i];
            dmareg->CNTL &= ~(USB_DMA_CNTL_DMA_ENAB | USB_DMA_CNTL_DMAIE | USB_DMA_CNTL_DMAERR);
            dmareg->COUNT = 0;
            dmareg->ADDR = 0;
        }
    prvctx.dma_mask = 0;
    for (uint8_t i = 1; i < USB_IN_EP_NUM; i++) {
        EPSEL(i);
        IP_USBC->RXCSRL |= USB_RXCSRL_FLUSHFIFO;
        IP_USBC->TXCSRL |= USB_TXCSRL_FLUSHFIFO;
    }
    EPSEL(EP0IDX);

    // clear private data of all EPs except EP0
    memset(&prvctx.ep_info[EPoIDX][EP0IDX + 1], 0, sizeof(struct usb_ep_info) * USB_EP_NO_MAX_AVAIL);
    memset(&prvctx.ep_info[EPiIDX][EP0IDX + 1], 0, sizeof(struct usb_ep_info) * USB_EP_NO_MAX_AVAIL);
    for (uint8_t i = 1; i < USB_DMA_CH_COUNT_AVAIL; i++) {
        prvctx.ep_info[EPoIDX][i].dma_ch = prvctx.ep_info[EPiIDX][i].dma_ch = DMA_NOT_ASSIGNED;
        prvctx.ep_info[EPoIDX][i].dma_en = prvctx.ep_info[EPiIDX][i].dma_en = DMA_XFER_EN;
    }
    prvctx.fifo_htop = USB_CTRL_FIFO_SIZE; // reserved USB_CTRL_FIFO_SIZE bytes for ep0
    prvctx.dev_addr = epp->reqsize = 0;
    usbd_event_reset_handler(busid);
}
static void usbd_irq_handler(void)
{
    #define BUSID 0
    //USBD_PRINT("\r\n--------------------------------\r\n");
    // IntrUSB/IntrTX/IntrRX are read-clear registers, 
    // we should read out the values to variables at first, and then which will be auto-cleared.
    uint8_t ints = IP_USBC->INTRUSB;///* read-clear */ & IP_USBC->INTRUSBE;
    uint16_t txis = IP_USBC->INTRTX;///* read-clear */ & IP_USBC->INTRTXE;
    uint16_t rxis = IP_USBC->INTRRX;///* read-clear */ & IP_USBC->INTRRXE;
    uint32_t dmas = IP_USBC->DMA_INTR;// & prvctx.dma_mask;
    uint16_t frame = IP_USBC->FRAME;
    USBD_ASSERT((dmas & prvctx.dma_mask) == dmas, "dmas=%lx dmae=%x", dmas, prvctx.dma_mask);

#if 0
    uint8_t dame = 0;
    for (int i = 0; i < USB_EP_NO_MAX_AVAIL; i++)
        if (IP_USBC->USB_DMA[i].CNTL & USB_DMA_CNTL_DMAIE) dame |= USBD_BIT(i);
    uint32_t dmas = IP_USBC->DMA_INTR & dame;
    USBD_ASSERT(dame == prvctx.dma_mask, "%x != %x", dame, prvctx.dma_mask);
#endif

    //USBD_TRACE("{%d: IS(C=%x T=%x R=%x D=%lx) IE(C=%x T=%x R=%x D=%x)}"
    //    , frame, ints, txis, rxis, dmas, IP_USBC->INTRUSBE, IP_USBC->INTRTXE, IP_USBC->INTRRXE, prvctx.dma_mask);
    
    ints &= IP_USBC->INTRUSBE;
    txis &= IP_USBC->INTRTXE;
    rxis &= IP_USBC->INTRRXE;

    if (ints & USB_INTRUSB_RESUME ) usbd_event_resume_handler(BUSID);
    if (ints & USB_INTRUSB_SUSPEND) usbd_event_suspend_handler(BUSID);
    if (ints & USB_INTRUSB_CONN   ) usbd_event_connect_handler(BUSID);
    if (ints & USB_INTRUSB_DISCON ) usbd_event_disconnect_handler(BUSID);
    if (ints & USB_INTRUSB_SOF    ) usbd_event_sof_handler(BUSID);
    if (ints & USB_INTRUSB_RESET  ) usbd_handle_reset(BUSID);
    if (dmas & USB_DMA_INTR_EP_ALL_MASK) usbd_handle_dma_xfer_complete(BUSID, dmas);
    if (txis & USB_INTRTX_EP0_MASK) usbd_handle_ep0_xfer_complete(BUSID);
    if (txis & USB_INTRTX_EP_MASK ) usbd_handle_epi_send_complete(BUSID, txis);
    if (rxis & USB_INTRRX_EP_MASK ) usbd_handle_epo_recv_complete(BUSID, rxis);

    //USBD_PRINT("{%d: IE(C=%x T=%x R=%x D=%x)}\r\n", frame, IP_USBC->INTRUSBE, IP_USBC->INTRTXE, IP_USBC->INTRRXE, prvctx.dma_mask);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief init device controller registers.
 * @return On success will return 0, and others indicate fail.
 */
int usb_dc_init(uint8_t busid)
{
    USBD_LOGI("ep0 fifo: offs=0 size=%d", USB_CTRL_FIFO_SIZE);
    memset(&prvctx, 0, sizeof(prvctx));   
    prvctx.fifo_htop = 0;
    prvctx.ep_info[EPoIDX][EP0IDX].ep_mps = prvctx.ep_info[EPiIDX][EP0IDX].ep_mps = USB_MAX_CTRL_MPS;
    prvctx.ep_info[EPoIDX][EP0IDX].dma_ch = prvctx.ep_info[EPiIDX][EP0IDX].dma_ch = DMA_NOT_ASSIGNED;
    prvctx.ep_info[EPoIDX][EP0IDX].fifo.addr = prvctx.ep_info[EPiIDX][EP0IDX].fifo.addr = prvctx.fifo_htop;
    prvctx.ep_info[EPoIDX][EP0IDX].fifo.size = prvctx.ep_info[EPiIDX][EP0IDX].fifo.size = USB_CTRL_FIFO_SIZE;
    prvctx.fifo_htop += USB_CTRL_FIFO_SIZE;
    prvctx.intr.usbe = (0 
        | USB_INTRUSBE_RESET       // set 1 to enable reset interrupt
        | USB_INTRUSBE_SUSPEND     // set 1 to enable suspend interrupt
        | USB_INTRUSBE_RESUME      // set 1 to enable resume interrupt
        | USB_INTRUSBE_CONN        // set 1 to enable connect interrupt
        | USB_INTRUSBE_DISCON      // set 1 to enable disconnect interrupt
    );
    prvctx.intr.txe = USB_INTRTX_EP0;// set bits to enable endpoint channel transmit interrupt
    prvctx.intr.rxe = 0;                  // set bits to disable endpoint channel receive interrupt
    prvctx.ep0stat = USBD_EP0_STATE_SETUP;
    for (uint8_t i = 1; i < USB_DMA_CH_COUNT_AVAIL; i++) {
        prvctx.ep_info[EPoIDX][i].dma_ch = prvctx.ep_info[EPiIDX][i].dma_ch = DMA_NOT_ASSIGNED;
        prvctx.ep_info[EPoIDX][i].dma_en = prvctx.ep_info[EPiIDX][i].dma_en = DMA_XFER_EN;
    }
    register_ISR(IRQ_USBC_VECTOR, (ISR)usbd_irq_handler, NULL);
    enable_IRQ(IRQ_USBC_VECTOR);

    //FIXME: following lines had better be moved into usb_glue_xxx.c which is defined for specific chip respectively!
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 1; //16bit mode
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 1;

    //enable usb clock
    //__HAL_CRM_USB_CLK_ENABLE();
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 1; // for ARCS

    IP_USBC->POWER |= USB_POWER_HSENABLE | USB_POWER_SOFTCONN;
    usbd_event_connect_handler(busid);
    return 0;
}

/**
 * @brief deinit device controller registers.
 * @return On success will return 0, and others indicate fail.
 */
int usb_dc_deinit(uint8_t busid)
{
    USBD_LOGI("deinit");
    disable_IRQ(IRQ_USBC_VECTOR);
    register_ISR(IRQ_USBC_VECTOR, NULL, NULL);
    IP_USBC->POWER &= ~USB_POWER_SOFTCONN;
    usbd_event_disconnect_handler(busid);
    return 0;
}

/**
 * @brief Set USB device address
 * @param[in] devaddr   Device address
 * @return On success will return 0, and others indicate fail.
 */
int usbd_set_address(uint8_t busid, const uint8_t devaddr)
{
    //USBD_LOGI("devaddr=0x%02x", devaddr);
    prvctx.dev_addr = devaddr & 0x7f;           // save the new address, and restore to un-addressed state once called
    prvctx.ep0stat = USBD_EP0_STATE_IN_STATUS;  // The only case to force entering STATUS stage!!
    return 0;
}

/**
 * @brief Set remote wakeup feature
 * @return On success will return 0, and others indicate fail.
 */
int usbd_set_remote_wakeup(uint8_t busid)
{
    USBD_LOGI("wakeup");
    return 0;
}

/**
 * @brief Get USB device speed
 * @param[in] busid     bus index
 * @return port speed, USB_SPEED_LOW or USB_SPEED_FULL or USB_SPEED_HIGH
 */
uint8_t usbd_get_port_speed(uint8_t busid)
{
    USBD_LOGI("speed");
    if (IP_USBC->POWER & USB_POWER_HSENABLE)
        return USB_SPEED_HIGH;
    return USB_SPEED_FULL;
}

/**
 * @brief configure and enable endpoint.
 * @param [in]  epcfg   Endpoint config.
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *epcfg)
{
    const uint16_t epmps = epcfg->wMaxPacketSize;
    const uint8_t epattr = epcfg->bmAttributes, epaddr = epcfg->bEndpointAddress;
    // USBD_LOGI("ep=0x%02x mps=%d mod=%d", epaddr, epmps, epattr);
    uint8_t const epidx = USB_EP_GET_IDX(epaddr), epdir = EP_GET_IDIR(epaddr), oldep = EPSEL(epidx);
    if (epdir == EPiIDX || epidx == EP0IDX) {
        if (IP_USBC->TXCSRL & USB_TXCSRL_TXPKTRDY) IP_USBC->TXCSRL |= USB_TXCSRL_FLUSHFIFO;
        uint16_t val = USB_DAINT_IN_EP_INT(epidx);
        prvctx.intr.txe |= val;
        IP_USBC->INTRTXE |= val;
    } else {
        if (IP_USBC->RXCSRL & USB_RXCSRL_RXPKTRDY) IP_USBC->RXCSRL |= USB_RXCSRL_FLUSHFIFO;
        uint16_t val = USB_DAINT_OUT_EP_INT(epidx);
        prvctx.intr.rxe |= val;
        IP_USBC->INTRRXE |= val;
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // usbd_epx_conf(epdir, epidx, epmps, epattr);
    struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
    if (!epp->fifo.addr) {
        epp->fifo.addr = prvctx.fifo_htop;
        epp->fifo.size = (epmps + 7) & ~7;
        prvctx.fifo_htop += epp->fifo.size;
        USBD_ASSERT(prvctx.fifo_htop <= EP_FIFO_TOTAL, "usb fifo overflow");
        USBD_LOGI("ep%d%d fifo: addr=%d size=%d hwused=%ld", epdir, epidx, epp->fifo.addr, epp->fifo.size, prvctx.fifo_htop);
    }
    USBD_ASSERT(epmps <= epp->fifo.size, "ep%d%d fifo size=%d mps=%d", epdir, epidx, epp->fifo.size, epmps);
    epp->ep_mps = epmps;
    if (epdir == EPoIDX) {
        if (epattr == USB_ENDPOINT_TYPE_ISOCHRONOUS) IP_USBC->RXCSRH |= USB_RXCSRH_ISO;
        else IP_USBC->RXCSRH &= ~USB_RXCSRH_ISO_MASK;  // BULK/INT
        if (epattr == USB_ENDPOINT_TYPE_BULK) IP_USBC->RXCSRL |= USB_RXCSRL_CLRDATATOG;
        IP_USBC->RXMAXP = epmps;
        IP_USBC->RXFIFOADD = epp->fifo.addr >> 3;
        IP_USBC->RXFIFOSZ = (IP_USBC->RXFIFOSZ & ~(USB_FIFOSZ_DPB_MASK | USB_FIFOSZ_SZ_MASK)) | GET_FIFOSZ_CFG(epmps);
        if (!epp->dma_en) IP_USBC->RXCSRH &= ~USB_RXCSRH_DMAREQENAB_MASK;
        else {
            IP_USBC->RXCSRH &= ~(USB_RXCSRH_AUTOCLEAR | USB_RXCSRH_DMAREQENAB | USB_RXCSRH_DMAREQMODE_1);
            IP_USBC->RXCSRH |= (0
                | USB_RXCSRH_AUTOCLEAR             // set 1 to clear AutoClear bit, auto cleared by hardware
                | USB_RXCSRH_DMAREQENAB            // set 1 to enable dma request enable
                | (DMA_REQ_MODE << USB_RXCSRH_DMAREQMODE_POS) // dma request mode(0 or 1)
            );
        }
    } else {
        if (epattr == USB_ENDPOINT_TYPE_ISOCHRONOUS) IP_USBC->TXCSRH |= USB_TXCSRH_ISO;
        else IP_USBC->TXCSRH &= ~USB_TXCSRH_ISO_MASK;  // BULK/INT
        if (epattr == USB_ENDPOINT_TYPE_BULK) IP_USBC->TXCSRH |= USB_TXCSRH_FRCDATATOG;
        IP_USBC->TXMAXP = epmps;
        IP_USBC->TXFIFOADD = epp->fifo.addr >> 3;
        IP_USBC->TXFIFOSZ = (IP_USBC->TXFIFOSZ & ~(USB_FIFOSZ_DPB_MASK | USB_FIFOSZ_SZ_MASK)) | GET_FIFOSZ_CFG(epmps);
        if (IP_USBC->TXCSRL & USB_TXCSRL_FIFONOTEMPTY) IP_USBC->TXCSRL |= USB_TXCSRL_FLUSHFIFO;
        if (!epp->dma_en) IP_USBC->TXCSRH &= ~USB_TXCSRH_DMAREQENAB_MASK;
        else {
            IP_USBC->TXCSRH &= ~(USB_TXCSRH_AUTOSET | USB_TXCSRH_DMAREQENAB | USB_TXCSRH_DMAREQMODE_1);
            IP_USBC->TXCSRH |= (0
                | USB_TXCSRH_AUTOSET               // set 1 to clear SetupEnd bit, auto cleared by hardware
                | USB_TXCSRH_DMAREQENAB            // set 1 to enable dma request enable
                | (DMA_REQ_MODE << USB_TXCSRH_DMAREQMODE_POS) // dma request mode(0 or 1)
            );
        }
    }
#ifdef DOUBLE_PACKET_ENABLE
    IP_USBC->TXFIFOSZ |= USB_FIFOSZ_DPB;
#endif

    ////////////////////////////////////////////////////////////////////////////////////////////////
    prvctx.ep_info[epdir][epidx].ep_en = 1;
    if (epidx == EP0IDX) prvctx.ep_info[EPiIDX][epidx].ep_en = 1;
    EPSEL(oldep);
    return 0;
}

/**
 * @brief Disable the selected endpoint
 * @param[in] epaddr    Endpoint address
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_close(uint8_t busid, const uint8_t epaddr)
{
    USBD_LOGI("ep=0x%02x", epaddr);
    uint8_t const epidx = USB_EP_GET_IDX(epaddr), epdir = EP_GET_IDIR(epaddr), oldep = EPSEL(epidx);
    struct usb_ep_info *const epp = &prvctx.ep_info[epdir][epidx];
    if (epp->dma_ch != DMA_NOT_ASSIGNED) {
        if      (epdir == EPoIDX) IP_USBC->RXCSRH &= ~(USB_RXCSRH_AUTOCLEAR | USB_RXCSRH_DMAREQENAB | USB_RXCSRH_DMAREQMODE_1);
        else if (epdir == EPiIDX) IP_USBC->TXCSRH &= ~(USB_TXCSRH_AUTOSET   | USB_TXCSRH_DMAREQENAB | USB_TXCSRH_DMAREQMODE_1);
        CSK_USB_DMA_RegDef volatile *const dmareg = &IP_USBC->USB_DMA[epp->dma_ch];
        dmareg->CNTL &= ~(USB_DMA_CNTL_DMA_ENAB | USB_DMA_CNTL_DMAIE | USB_DMA_CNTL_DMAERR);
        dmareg->COUNT = 0;
        prvctx.dma_mask &= ~USBD_BIT(epp->dma_ch);
        epp->dma_ch = DMA_NOT_ASSIGNED;
    }
    epp->reqsize = epp->szdone = 0;

    // Disable EP interrupts
    if (epdir == EPiIDX || epidx == EP0IDX) {
        uint16_t val = USB_DAINT_IN_EP_INT(epidx);
        prvctx.intr.txe &= ~val;
        IP_USBC->INTRTXE &= ~val;
    } else {
        uint16_t val = USB_DAINT_OUT_EP_INT(epidx);
        prvctx.intr.rxe &= ~val;
        IP_USBC->INTRRXE &= ~val;
    }

    // De-activate, disable and set NAK for EP
    epp->ep_en = 0;
    EPSEL(oldep);
    return 0;
}

/**
 * @brief Set stall condition for the selected endpoint
 * @param[in] epaddr    Endpoint address
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_set_stall(uint8_t busid, const uint8_t epaddr)
{
    USBD_LOGW("ep=0x%02x", epaddr);
    uint8_t const epidx = USB_EP_GET_IDX(epaddr), epdir = EP_GET_IDIR(epaddr), oldep = EPSEL(epidx);
    if (epidx == EP0IDX)      IP_USBC->CSR0L  |= USB_CSR0L_SENDSTALL;
    else if (epdir == EPoIDX) IP_USBC->RXCSRL |= USB_RXCSRL_SENDSTALL;
    else if (epdir == EPiIDX) IP_USBC->TXCSRL |= USB_TXCSRL_SENDSTALL;
    EPSEL(oldep);
    return 0;
}

/**
 * @brief Clear stall condition for the selected endpoint
 * @param[in] epaddr    Endpoint address corresponding to the one listed in the device configuration table
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_clear_stall(uint8_t busid, const uint8_t epaddr)
{
    USBD_LOGW("ep=0x%02x", epaddr);
    uint8_t const epidx = USB_EP_GET_IDX(epaddr), epdir = EP_GET_IDIR(epaddr), oldep = EPSEL(epidx);
    if (epidx == EP0IDX) return -1; /* Not possible to clear stall for EP0 */
    if      (epdir == EPoIDX) IP_USBC->RXCSRL &= ~USB_RXCSRL_SENTSTALL_MASK;
    else if (epdir == EPiIDX) IP_USBC->TXCSRL &= ~USB_TXCSRL_SENTSTALL_MASK;
    EPSEL(oldep);
    return 0;
}

/**
 * @brief Check if the selected endpoint is stalled
 * @param[in]  epaddr   Endpoint address
 * @param[out] stalled  Endpoint stall status
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_is_stalled(uint8_t busid, const uint8_t epaddr, uint8_t *stalled)
{
    if (!stalled) return -1;
    USBD_LOGW("ep=0x%02x", epaddr);
    uint8_t const epidx = USB_EP_GET_IDX(epaddr), epdir = EP_GET_IDIR(epaddr), oldep = EPSEL(epidx);
    *stalled = (epidx == EP0IDX && (IP_USBC->CSR0L & USB_CSR0L_SENTSTALL_MASK))
        || (epdir == EPoIDX && (IP_USBC->RXCSRL & USB_RXCSRL_SENTSTALL_MASK))
        || (epdir == EPiIDX && (IP_USBC->TXCSRL & USB_TXCSRL_SENTSTALL_MASK));
    EPSEL(oldep);
    return 0;
}

/**
 * @brief Setup in ep transfer setting and start transfer.
 *  This function is asynchronous.
 *  This function is called to write data to the specified endpoint.
 *  The supplied usbd_endpoint_callback function will be called when data is transmitted out.
 * @param[in]  epaddr   Endpoint address corresponding to the one listed in the device configuration table
 * @param[in]  data     Pointer to data to write
 * @param[in]  size     Length of the data requested to write. This maybe zero for a zero length status packet.
 * @return 0 on success, negative errno code on fail.
 */
int usbd_ep_start_write(uint8_t busid, const uint8_t epaddr, const uint8_t *data, uint32_t size)
{
    // USBD_LOGI("ep=0x%02x size=%ld", epaddr, size);
    return usbd_edpt_xfer_request(busid, epaddr, (uint32_t)data, size);
}

/**
 * @brief Setup out ep transfer setting and start transfer.
 *  This function is asynchronous.
 *  This function is called to read data to the specified endpoint. 
 *  The supplied usbd_endpoint_callback function will be called when data is received in.
 * @param[in]  epaddr   Endpoint address corresponding to the one listed in the device configuration table
 * @param[in]  data     Pointer to data to read
 * @param[in]  size     Max length of the data requested to read.
 * @return 0 on success, negative errno code on fail.
 */
int usbd_ep_start_read(uint8_t busid, const uint8_t epaddr, uint8_t *data, uint32_t size)
{
    // USBD_LOGI("ep=0x%02x size=%ld", epaddr, size);
    return usbd_edpt_xfer_request(busid, epaddr, (uint32_t)data, size);
}
