/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */


#include "tusb_option.h"
#include "device/dcd.h"

#include "controller/usb_controller.h"
//#include "SEGGER_RTT.h"
#include "log_print.h"

#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "Driver_Common.h"
#include "cache.h"

//----------------------------------------------------------------------------------
// configure SOF_CNT feature, that is, user can specify
// an interrupt is triggered on receiving how many SOF packets...
#ifndef CONFIG_SOF_CNT
#define CONFIG_SOF_CNT  0 // 1
#define SOF_CNT_OVFLOW_THRESHOLD    32 // 8 // *125um
#endif

// configure Debug EP (IN EP8 & OUT EP8 are dedicated to debug only!!)
#ifndef CONFIG_DBG_EP
#define CONFIG_DBG_EP   0 // 1
#endif

// enable Double Packet Buffering for EP?
// NOTE: CANNOT define CONFIG_DPB_IN as 1 for TX DPB feature CANNOT work as yet!!
#ifndef CONFIG_DPB_IN
#define CONFIG_DPB_IN   0 //1 // for IN EP
#endif

#ifndef CONFIG_DPB_OUT
#define CONFIG_DPB_OUT  1 //0 // for OUT EP
#endif

// DPB @ bit[4] of RXFIFOSZ & TXFIFOSZ register

// NOTE: for ARCS, USB DMA works ONLY in burst mode 0 if buffer locates on PSRAM
// and NOT work in burst mode 1/2/3 on PSRAM!
//
#define USB_DMA_BURST_MODE  USB_ARCS_DMA_CNTL_DMA_BRSTM_0 // ONLY BRSTM_0 WORK for PSRAM
//#define USB_DMA_BURST_MODE  USB_ARCS_DMA_CNTL_DMA_BRSTM_3 // _1, _2, _3 DON'T WORK for PSRAM

//----------------------------------------------------------------------------------
#define DEBUG_LOG   0 // 1
#if DEBUG_LOG
#define LOG_DBG(fmt, ...)   CLOGD(fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)   ((void)0)  //SEGGER_RTT_printf(0, fmt"\n", ##__VA_ARGS__)
#endif // DEBUG_LOG

#define DEBUG_GPIO_PIN  0 // 0
#define PIN_DBG         21 //FIXME:

#if DEBUG_GPIO_PIN
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
static void *gpio_A = NULL;
//static void *gpio_B = NULL;
#define STRCAT(a, b)   a##b             ///< concat without expand
#define XSTRCAT(a, b)  STRCAT(a, b)     ///< expand then concat
#define CSK_GPIO_PIN_DBG    XSTRCAT(CSK_GPIO_PIN, PIN_DBG)

// DBG_PIN_WR
#define DBG_PIN_WR(N)       GPIO_PinWrite(gpio_A, CSK_GPIO_PIN_DBG, N)

// DBG_PIN_INIT
static void DBG_PIN_INIT() {
    gpio_A = GPIOA();
    assert(gpio_A != NULL);
    GPIO_Initialize(gpio_A, NULL, NULL);
    //gpio_B = GPIOB();
    //assert(gpio_B != NULL);
    //GPIO_Initialize(gpio_B, NULL, NULL);
    //
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PIN_DBG, 0); //PIN as GPIO (func=0)
    GPIO_SetDir(gpio_A, CSK_GPIO_PIN_DBG, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpio_A, CSK_GPIO_PIN_DBG, 0); // LOW initially
    //
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, PIN_DBG, 0); //PIN as GPIO (func=0)
    //GPIO_SetDir(gpio_B, CSK_GPIO_PIN_DBG, CSK_GPIO_DIR_OUTPUT);
    //GPIO_PinWrite(gpio_B, CSK_GPIO_PIN_DBG, 0); // LOW initially
}

#else // !DEBUG_GPIO_PIN

// DBG_PIN_WR
#define DBG_PIN_WR(N)       ((void)0)

// DBG_PIN_INIT
static void DBG_PIN_INIT() { }

#endif // DEBUG_GPIO_PIN

//----------------------------------------------------------------------------------
#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

#define TX_IS_IDLE()  \
    (!(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_TXPKTRDY) && \
     !(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_FIFONOTEMPTY))


//CMN_SYS_RegDef *g_CmnSys = IP_CMN_SYS;
static struct usb_arcs_ctrl_prv usb_arcs_ctrl;

static void usb_arcs_isr_handler(const void *unused);

static void usbd_reset_internal_state(void);

static void usb_dma_clear_channel_active_flag (uint8_t ch);

//--------------------------------------------------------------------+
// SOF_CNT Feature
//--------------------------------------------------------------------+

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
static inline uint32_t get_sof_cnt()
{ return IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_CNT; }

// clear current count of SOF packet, and overflow flag
static inline void clr_sof_frame_cnt()
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
#if DEBUG_GPIO_PIN
    static uint32_t sof_cnt_toggle = 0;
    sof_cnt_toggle ^= 1; // XOR 1
    DBG_PIN_WR(sof_cnt_toggle); // Switch High/Low
#endif

    uint32_t sof_cnt = get_sof_cnt();
    clr_sof_frame_cnt();
//    LOG_DBG("ISR: sof_cnt = %d\r\n", sof_cnt);

    //TODO: ADD handler here...
}

#endif //CONFIG_SOF_CNT


//--------------------------------------------------------------------+
// DBG_EP Feature (IN & OUT BULK EP8)
//--------------------------------------------------------------------+
#if CONFIG_DBG_EP

#define DBG_EP_MPS  64
#define DBG_EP_OUT_DMA_CH   6
#define DBG_EP_IN_DMA_CH    7

static void config_dbg_ep()
{
    EDPxReg_SEL(USB_EP_NO_DBG);

    // BULK OUT EP (receive DBG descriptor)
    CSK_USBC->RXMAXP = DBG_EP_MPS; //64 bytes
    CSK_USBC->RXFIFOSZ = USB_ARCS_FIFOSZ_SZ_64; //0x3, 64 bytes
    CSK_USBC->RXFIFOADD = (USB_EP_FIFO_TOTAL_SIZE - DBG_EP_MPS) >> 3; //0x1F8, last 64 bytes of 4KB
    CSK_USBC->RXCSRL = USB_ARCS_RXCSRL_CLRDATATOG; //0x80, Set ClrDataTog
    //0xB8, Set DMAReqEnab, DMAReqMode1, DisNyet, AutoClear
    CSK_USBC->RXCSRH = USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1 |
            USB_ARCS_RXCSRH_DISNYET | USB_ARCS_RXCSRH_AUTOCLEAR;

    CSK_USBC->USB_DMA[DBG_EP_OUT_DMA_CH].ADDR = USB_DM_BASE;
    CSK_USBC->USB_DMA[DBG_EP_OUT_DMA_CH].COUNT = 0xFFFFFFFF; // max. value unsigned 32bits? FIXME:
    //0x685, Set Burst Mode = 2'b11 (INCR16), EP assigned = 8, set DMA DIR = 0 (RX) DMAMODE mode1
    CSK_USBC->USB_DMA[DBG_EP_OUT_DMA_CH].CNTL = USB_DMA_BURST_MODE | USB_ARCS_DMA_CNTL_DMAEP(USB_EP_NO_DBG) |
            USB_ARCS_DMA_CNTL_DMA_DIR(DIR_IDX_OUT) | USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAMODE_1;

    // BULK IN EP (send Operation result)
    CSK_USBC->TXMAXP = DBG_EP_MPS; //64 bytes
    CSK_USBC->TXFIFOSZ = USB_ARCS_FIFOSZ_SZ_64; //0x3, 64 bytes
    CSK_USBC->TXFIFOADD = (USB_EP_FIFO_TOTAL_SIZE - DBG_EP_MPS * 2) >> 3; //0x1F0, last 128 ~ 64 bytes of 4KB
    CSK_USBC->TXCSRL = USB_ARCS_TXCSRL_CLRDATATOG | USB_ARCS_TXCSRL_SENDSTALL; //0x50, set ClrDataTog & SendStall(why to send STALL, FIXME:)
    //0x94, Set AutoSet, DMAReqEnab, DMAReqMode, Clear Mode = 0
    CSK_USBC->TXCSRH = USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE;

    CSK_USBC->USB_DMA[DBG_EP_IN_DMA_CH].ADDR = USB_DM_BASE;
    CSK_USBC->USB_DMA[DBG_EP_IN_DMA_CH].COUNT = 0xFFFFFFFF; // max. value unsigned 32bits? FIXME:
    //0x687, Set Burst Mode = 2'b11 (INCR16), EP assigned = 8, set DMA DIR = 1 (TX) DMAMODE mode1
    CSK_USBC->USB_DMA[DBG_EP_IN_DMA_CH].CNTL = USB_DMA_BURST_MODE | USB_ARCS_DMA_CNTL_DMAEP(USB_EP_NO_DBG) |
            USB_ARCS_DMA_CNTL_DMA_DIR(DIR_IDX_IN) | USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAMODE_1;
}

#endif // CONFIG_DBG_EP


//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

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


// Wake up host
void dcd_remote_wakeup(uint8_t rhport)
{
    // TODO: Add Remote_Wakeup function...
    // please refer to P72, Chap 3.11.2. LPM_CNTRL @musbmhdrc_pspg.pdf
}

// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
//void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request) TU_ATTR_WEAK;


// Initialize controller to device mode
//static int usb_arcs_init(void)
void dcd_init       (uint8_t rhport)
{
    //g_CmnSys = IP_CMN_SYS;

    /* Clear private data */
    (void)memset(&usb_arcs_ctrl, 0, sizeof(usb_arcs_ctrl));

    /* Initial usb_arcs_ctrl */
    usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;

    /* Endpoint0 is fifo size is fixed to 16 bytes */
    usb_arcs_ctrl.ep_info[0][0].fixed.mps = USB_MAX_CTRL_MPS;
    usb_arcs_ctrl.ep_info[0][0].fixed.fifo_size = USB_CTRL_FIFO_SIZE;
    usb_arcs_ctrl.ep_info[1][0].fixed.mps = USB_MAX_CTRL_MPS;
    usb_arcs_ctrl.ep_info[1][0].fixed.fifo_size = USB_CTRL_FIFO_SIZE;

    usb_arcs_ctrl.ep_info[0][0].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
    usb_arcs_ctrl.ep_info[1][0].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;

    /* Set RX/TX FIFO space base address */
    //BSD: MUST reserve USB_CTRL_FIFO_SIZE bytes (Starting from 0) for EP0 FIFO
    usb_arcs_ctrl.fifo_alloc_addr = (USB_EP_FIFO_BASE + USB_CTRL_FIFO_SIZE) >> 3;

    /* USB_ARCS_IN_EP_NUM == USB_ARCS_OUT_EP_NUM */
    uint32_t i;
    for(i = 1; i < USB_ARCS_IN_EP_NUM; i++){
        usb_arcs_ctrl.ep_info[0][i].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        usb_arcs_ctrl.ep_info[1][i].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        // enable EP DMA by default except EP0
        usb_arcs_ctrl.ep_info[0][i].dma_ena = 1;
        usb_arcs_ctrl.ep_info[1][i].dma_ena = 1;
    }

    // Reset USB controller first of all
    //CSK_USBC->SOFT_RST |= (USB_ARCS_SOFT_RST_NRST | USB_ARCS_SOFT_RST_NRSTX);

    // enable the SUSPENDM output (CPU can trigger RESUME signaling)
    //CSK_USBC->POWER |= USB_ARCS_POWER_EN_SUSPENDM;

#if TUD_OPT_HIGH_SPEED
    /* Set device speed to High Speed */
    CSK_USBC->POWER |= USB_ARCS_POWER_HSENABLE;   //high speed enable
#else
    /* Set device speed to Full Speed */
    CSK_USBC->POWER &= ~USB_ARCS_POWER_HSENABLE;   //Full speed enable
#endif

    //BSD: dcd_int_enable SHOULD be called in the end, so move it downward ...
//    // Enable soft connect
//    CSK_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;

    /* Register USB ISR */
    register_ISR(IRQ_USBC_VECTOR, (ISR)usb_arcs_isr_handler, NULL);

    // Initialize usb interrupt enable
    //TODO: enable SOF interrupt if necessary...
    usb_arcs_ctrl.intr_usbe = USB_ARCS_INTRUSBE_RESET | USB_ARCS_INTRUSBE_SUSPEND |
                               USB_ARCS_INTRUSBE_RESUME | USB_ARCS_INTRUSBE_DISCON; //USB_ARCS_INTRUSBE_CONN |
    usb_arcs_ctrl.intr_txe = USB_ARCS_INTRTX_EP0;
    usb_arcs_ctrl.intr_rxe = 0;

#if CONFIG_SOF_CNT
    //IP_CMN_SYS->REG_USB_SOF_CNT1.bit.SOF_FRAME_CNT_CLR = 1; // clear SOF_CNT (cnt & overflow fields)
    //IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_CNT = 8; //default, 8 * 125us (USB 2.0) = 1ms
    //IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_OVERFLOW_INT_ENABLE = 1;
    //IP_CMN_SYS->REG_USB_SOF_CNT.bit.SOF_FRAME_CNT_ENABLE = 1;
    clr_sof_frame_cnt();
    set_sof_cnt_ovf_thr(SOF_CNT_OVFLOW_THRESHOLD);
    enable_sof_cnt(true);
    register_ISR(IRQ_SOF_CNT_VECTOR, (ISR)usb_sof_cnt_isr_handler, NULL);
#endif // CONFIG_SOF_CNT

#if CONFIG_DBG_EP
    config_dbg_ep();
#endif

    // Debug pin initialize
    DBG_PIN_INIT();

    //BSD: dcd_int_enable will be called by USBD, so remove it here ...
//    // Enable usb module interrupt
//    dcd_int_enable(0);

    //BSD: DON'T connect on the initiative, tud_connect (calling dcd_connect) SHOULD be called explicitly...
//    // Enable soft connect
//    CSK_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;

    LOG_DBG("%s has been called!", __func__);
}


// Receive Set Address request, mcu port must also include status IN response
// BSD: dcd_set_address is called in SETUP phase, but new USB address is configured into HW only in STATUS phase!!
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
//int usb_dc_set_address(const uint8_t addr)
{
  //TODO: select either this solution or the below one...
    ARG_UNUSED(rhport);

    if (dev_addr > USB_ARCS_FADDR_ADDR_MASK || dev_addr == 0) {
        CLOGW("%s: invalid USB dev_addr (%d)!", __func__, dev_addr);
        return;
    }

    usb_arcs_ctrl.address = dev_addr; // save the new address
    usb_arcs_ctrl.addressed = 0; // restore to un-addressed state once called

    // The only case to force entering STATUS stage!!
    usb_arcs_ctrl.status = USB_ARCS_STS_STATUS;

/*
    ARG_UNUSED(rhport);

    if (dev_addr > USB_ARCS_FADDR_ADDR_MASK || dev_addr == 0) {
        CLOGW("%s: invalid USB dev_addr (%d)!", __func__, dev_addr);
        return;
    }

    // wait until ZLP is sent out
//    while(CSK_USBC->CSR0L & USB_ARCS_CSR0L_TXPKTRDY); // NOT WORK!!
//    volatile uint32_t count = 100; //1000; // 100 WORK, 1000 NOT WORK!!
//    while(count-- > 0);

    // call memory barrier + flush instruction pipeline for short-delay delay after sending ZLP
    __DMB(); __ISB(); // WORK!!
    CSK_USBC->FADDR = dev_addr & USB_ARCS_FADDR_ADDR_MASK;
    usb_arcs_ctrl.address = dev_addr; // save the new address
    usb_arcs_ctrl.addressed = 1; // addressed

    LOG_DBG("%s: Addr = 0x%x", __func__, dev_addr);
*/

}


// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
    ARG_UNUSED(rhport);

    /* Enable soft connect */
    CSK_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
    ARG_UNUSED(rhport);

    // Enable soft disconnect
    CSK_USBC->POWER &= ~USB_ARCS_POWER_SOFTCONN;
}


//BSD: Notify that VBus level is changed
void dcd_notify_vbus_level_changed(uint8_t rhport, uint8_t vbus_level)
{
    ARG_UNUSED(rhport);

    if (vbus_level == 0) { // LOW
        // reset internal state of the controller driver
        usbd_reset_internal_state();
        CLOGI("usb disconnected (VBUS low)!");
        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
    } else { // HIGH
        CLOGI("usb connected (VBUS high)!");
        //dcd_event_bus_signal(0, DCD_EVENT_PLUGGED, true); //NO PLUGGED event!
    }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

static bool usbd_is_enabled(uint8_t ep_addr)
{
    uint8_t ep_idx, dir_idx;

    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return false;

    return (usb_arcs_ctrl.ep_info[dir_idx][ep_idx].ep_ena != 0);
}


//static int usb_arcs_ep_set(uint8_t ep, uint32_t ep_mps, enum usb_dc_ep_transfer_type ep_type)
static int usbd_ep_set(uint8_t ep_idx, uint8_t dir_idx, uint16_t ep_mps, tusb_xfer_type_t ep_type)
{
    LOG_DBG("%s ep %d (dir=%d), mps %d, type %d", __func__, ep_idx, dir_idx, ep_mps, ep_type);

#if CONFIG_DBG_EP
    if (ep_idx == USB_EP_NO_DBG) { // Debug EP
        LOG_DBG("%s: DBG EP %d is set, do nothing!\r\n", __func__, ep_idx);
        return CSK_DRIVER_OK;
    }
#endif

    if (ep_idx == 0) { // EP0, ignore dir_idx
        if (ep_type != TUSB_XFER_CONTROL || ep_mps > USB_CTRL_FIFO_SIZE)
            return CSK_DRIVER_ERROR_PARAMETER;
        usb_arcs_ctrl.ep_info[0][0].fixed.mps = usb_arcs_ctrl.ep_info[1][0].fixed.mps = ep_mps;
        return CSK_DRIVER_OK;
    }

    if ((dir_idx && ep_idx >= USB_ARCS_OUT_EP_NUM) || (!dir_idx && ep_idx >= USB_ARCS_IN_EP_NUM))
        return CSK_DRIVER_ERROR_PARAMETER;

    if (ep_mps > USB_ARCS_MAXP_MASK || ep_type > TUSB_XFER_INTERRUPT)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint16_t fifo_size, val;
    struct usb_ep_ctrl_prv *epp;

    epp = &usb_arcs_ctrl.ep_info[dir_idx][ep_idx];
    fifo_size = epp->fixed.fifo_size;
    if (fifo_size > 0 && ep_mps > fifo_size )
        return CSK_DRIVER_ERROR_PARAMETER;

    // Set max packet size
    EDPxReg_SEL(ep_idx);
    if (dir_idx == DIR_IDX_OUT)
        CSK_USBC->RXMAXP = ep_mps;
    else
        CSK_USBC->TXMAXP = ep_mps;

    EDPxReg_SEL(ep_idx);

    /* Set endpoint type and FIFO size for EP1-15 */

    if (ep_type == TUSB_XFER_ISOCHRONOUS) { // ISO
        // Set DMA Request Mode & DMA Mode to 0 accroding to P110, MUSB data sheet
        // if the endpoint is configured for Isochronous transfers,
        // DMA Request Mode 0 should always be selected where DMA is used.
        epp->dma_reqmode = 0;
        epp->dma_mode = 1; //0

        // Set ISO type bit
        if (dir_idx == DIR_IDX_OUT) {
            //epp->dma_reqmode = 1;  // 1 for RX
            CSK_USBC->RXCSRH |= USB_ARCS_RXCSRH_ISO;
        } else {
            //epp->dma_reqmode = 0;  // 0 for TX
            CSK_USBC->TXCSRH |= USB_ARCS_TXCSRH_ISO;
        }
    } else { // BULK or INT
        //BSD2013.1.22 HID button experiment (INT IN EP):
        // dma_mode = 1, dma_reqmode = 0 or 1, it's OK;
        // dma_mode = 0, dma_reqmode = 0, it's OK;
        // dma_mode = 0, dma_reqmode = 1, it doesn't work!
        // Many reports are uploaded in one interrupt transfer!!
        // A lot of DMA interrupts without any IN EP interrupt!

        // Set DMA Request Mode & DMA Mode to 0
        // dma_reqmode decides 1) When to raise DMA request for RX 2) Whether to trigger EP interrupt
//        epp->dma_reqmode = 1; //0; // RX = 1, TX = 0? It seems no difference for INT when dma_mode=1
        //dma_mode = 1: one EP interrupt (optional) for one xfer (1 or more packets)
        //dma_mode = 0: one EP interrupt (optional) for one packet
        epp->dma_mode = 1; //0;

        if (dir_idx == DIR_IDX_OUT) {
            epp->dma_reqmode = 1; // 1 for RX, notify complete event in DMA ISR
            /* Clear ISO type bit */
            CSK_USBC->RXCSRH &= (~USB_ARCS_RXCSRH_ISO_MASK);
            /* force the endpoint data toggle */
            if (ep_type == TUSB_XFER_BULK)
                CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_CLRDATATOG;
        } else {
        #if CONFIG_DPB_IN
            epp->dma_reqmode = 0; // 0 for TX, notify complete event in IN EP ISR
        #else
            epp->dma_reqmode = 1; // 1 for TX, notify complete event in DMA ISR
        #endif
            /* Clear ISO type bit */
            CSK_USBC->TXCSRH &= (~USB_ARCS_TXCSRH_ISO_MASK);
            /* force the endpoint data toggle */
            if (ep_type == TUSB_XFER_BULK)
                CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_CLRDATATOG;
                //CSK_USBC->TXCSRH |= USB_ARCS_TXCSRH_FRCDATATOG;
        }
    }

    //val = GET_FIFOSZ_CFG(ep_mps); // mps => FIFOSZ
    if (dir_idx == DIR_IDX_OUT) {
#if CONFIG_DPB_OUT
        val = GET_FIFOSZ_CFG(ep_mps << 1) | USB_ARCS_FIFOSZ_DPB;
#else
        val = GET_FIFOSZ_CFG(ep_mps); // mps => FIFOSZ
#endif
        // set endpoint fifo size and fifo address
        //CSK_USBC->RXFIFOSZ &= ~(USB_ARCS_FIFOSZ_DPB_MASK | USB_ARCS_FIFOSZ_SZ_MASK);
        //CSK_USBC->RXFIFOSZ |= val; // GET_FIFOSZ_CFG(ep_mps);
        CSK_USBC->RXFIFOSZ = val;

        usb_arcs_ctrl.ep_info[0][ep_idx].fixed.mps = ep_mps;
        if (fifo_size == 0) {
            //BSD: the actual fifo size can be smaller (no less than ep_mps) to save FIFO space,
            // and make sure that it is an integral multiple of 8!!
            //fifo_size = FIFOSZ_CFG_TO_BYTES(val);
#if CONFIG_DPB_OUT
            fifo_size = ((ep_mps << 1) + 7) & ~0x7;
#else
            fifo_size = (ep_mps + 7) & ~0x7;
#endif
            usb_arcs_ctrl.ep_info[0][ep_idx].fixed.fifo_size = fifo_size; // ep_mps;

            CLOGI("EP%x (dir=%d) RX FIFO address is 0x%x, size is %d", ep_idx, dir_idx,
                    usb_arcs_ctrl.fifo_alloc_addr << 3, fifo_size);

            CSK_USBC->RXFIFOADD = usb_arcs_ctrl.fifo_alloc_addr;
            usb_arcs_ctrl.fifo_alloc_addr += fifo_size >> 3; // ep_mps >> 3;  // 8bytes aligned address
        }

        if(usb_arcs_ctrl.ep_info[0][ep_idx].dma_ena) {
            CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_DMAREQMODE_1);
            //CSK_USBC->RXCSRH |= (USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
            CSK_USBC->RXCSRH |= (USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_RXCSRH_DMAREQMODE_POS));
        }else{
            /* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
            CSK_USBC->RXCSRH &= ~USB_ARCS_RXCSRH_DMAREQENAB_MASK;
        }
    } else {
#if CONFIG_DPB_IN
        val = GET_FIFOSZ_CFG(ep_mps << 1) | USB_ARCS_FIFOSZ_DPB;
#else
        val = GET_FIFOSZ_CFG(ep_mps); // mps => FIFOSZ
#endif
        // set endpoint fifo size and fifo address
        //CSK_USBC->TXFIFOSZ &= ~(USB_ARCS_FIFOSZ_DPB_MASK | USB_ARCS_FIFOSZ_SZ_MASK);
        //CSK_USBC->TXFIFOSZ |= val; // GET_FIFOSZ_CFG(ep_mps);
        CSK_USBC->TXFIFOSZ = val;

        if(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_FIFONOTEMPTY) {
            //The CPU write 1 to this bit to flush the latest packet from endpoint TX FIFO
            CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO;
        }

        usb_arcs_ctrl.ep_info[1][ep_idx].fixed.mps = ep_mps;
        if (fifo_size == 0) {
            //BSD: the actual fifo size can be smaller (no less than ep_mps) to save FIFO space,
            // and make sure that it is an integral multiple of 8!!
            //fifo_size = FIFOSZ_CFG_TO_BYTES(val);
#if CONFIG_DPB_IN
            fifo_size = ((ep_mps << 1) + 7) & ~0x7;
#else
            fifo_size = (ep_mps + 7) & ~0x7;
#endif
            usb_arcs_ctrl.ep_info[1][ep_idx].fixed.fifo_size = fifo_size; // ep_mps;

            CLOGI("EP%x (dir=%d) TX FIFO address is 0x%x, size is %d", ep_idx, dir_idx,
                    usb_arcs_ctrl.fifo_alloc_addr << 3, fifo_size);

            CSK_USBC->TXFIFOADD = usb_arcs_ctrl.fifo_alloc_addr;
            usb_arcs_ctrl.fifo_alloc_addr += fifo_size >> 3; // ep_mps >> 3;  // 8bytes aligned address
        }

        if(usb_arcs_ctrl.ep_info[1][ep_idx].dma_ena) {
            CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_DMAREQMODE_1);
            //BSD NOTE: USB_ARCS_TXCSRH_DMAREQMODE_0 had better be used if we need IN EP interrupt (usb_arcs_int_iep_handler) together with DMA interrupt!!
            CSK_USBC->TXCSRH |= (USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_TXCSRH_DMAREQMODE_POS)); //USB_ARCS_TXCSRH_DMAREQMODE_1
        }else{
            /* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
            CSK_USBC->TXCSRH &= ~USB_ARCS_TXCSRH_DMAREQENAB_MASK;
        }
    }

    return CSK_DRIVER_OK;
}


// Configure endpoint's registers according to descriptor
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
    uint8_t ep_idx, dir_idx;
    uint16_t val;

    ARG_UNUSED(rhport);
    if (p_endpoint_desc == NULL)
        return false;

    ep_idx = USB_EP_GET_IDX(p_endpoint_desc->bEndpointAddress);
    dir_idx = USB_EP_GET_DIR_IDX(p_endpoint_desc->bEndpointAddress);

#if CONFIG_DBG_EP
    if (ep_idx == USB_EP_NO_DBG) { // Debug EP
        LOG_DBG("%s: DBG EP (0x%x) is opened, do nothing!", __func__, p_endpoint_desc->bEndpointAddress);
        return true;
    }
#endif

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return false;

    EDPxReg_SEL(ep_idx);

    // enable EP interrupts etc.
    if (dir_idx == DIR_IDX_IN || ep_idx == 0) { //TX (IN) EP or EP0
        if (CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_TXPKTRDY) { // TX packet ready
            CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO; // flush TX FIFO
        }
        val = USB_ARCS_DAINT_IN_EP_INT(ep_idx);
        usb_arcs_ctrl.intr_txe |= val;
        CSK_USBC->INTRTXE |= val;
    } else { //RX (OUT) EP
        if (CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) { // RX packet ready
            CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO; // flush RX FIFO
            //CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
        }
        val = USB_ARCS_DAINT_OUT_EP_INT(ep_idx);
        usb_arcs_ctrl.intr_rxe |= val;
        CSK_USBC->INTRRXE |= val;
    }

    EDPxReg_SEL(0);

    memcpy(&usb_arcs_ctrl.ep_info[dir_idx][ep_idx].fixed.attr_bits,
           &p_endpoint_desc->bmAttributes, 1);

    usbd_ep_set(ep_idx, dir_idx,
            p_endpoint_desc->wMaxPacketSize.size,
            (tusb_xfer_type_t)p_endpoint_desc->bmAttributes.xfer);

    usb_arcs_ctrl.ep_info[dir_idx][ep_idx].ep_ena = 1U;
    if (ep_idx == 0)
        usb_arcs_ctrl.ep_info[1][ep_idx].ep_ena = 1U;

    return true;
}


//BSD: Cancel the ongoing transfer on some endpoint (internal)
static void dcd_edpt_cancel_xfer_internal (uint8_t rhport, uint8_t ep_idx, uint8_t dir_idx)
{
    uint16_t val;
    struct usb_ep_ctrl_prv *epp;

    // abort DMA operation if any...
    epp = &usb_arcs_ctrl.ep_info[dir_idx][ep_idx];
    val = epp->dma_ch;

    if (val != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED) {
        if (dir_idx == 0) {
            CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
        } else {
            //CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_1);
            CSK_USBC->TXCSRH &= ~USB_ARCS_TXCSRH_DMAREQENAB;
            CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQMODE_1);
        }
        CSK_USBC->USB_DMA[val].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
        CSK_USBC->USB_DMA[val].COUNT = 0;
        usb_dma_clear_channel_active_flag(val);
        epp->dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        LOG_DBG("%s: abort dma for EP 0x%x(dir = %d)\n", __func__, ep_idx, dir_idx);
    }

    // reset transfer state
    epp->req_len = epp->xfer_len = 0;
}


// Close an endpoint.
// Since it is weak, caller must TU_ASSERT this function's existence before calling it.
void dcd_edpt_close (uint8_t rhport, uint8_t ep_addr)
//int usb_dc_ep_disable(const uint8_t ep)
{
    uint8_t ep_idx, dir_idx;
    uint16_t val;
    struct usb_ep_ctrl_prv *epp;

    ARG_UNUSED(rhport);
    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

#if CONFIG_DBG_EP
    if (ep_idx == USB_EP_NO_DBG) { // Debug EP
        LOG_DBG("%s: DBG EP (0x%x) NEED NOT be closed, do nothing!\r\n", __func__, ep_addr);
        return;
    }
#endif

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return;

    EDPxReg_SEL(ep_idx);
    dcd_edpt_cancel_xfer_internal(rhport, ep_idx, dir_idx);
/*
    epp = &usb_arcs_ctrl.ep_info[dir_idx][ep_idx];

    // abort DMA operation if any...
    val = epp->dma_ch;

    if (val != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED) {
        if (dir_idx == 0) {
            CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
        } else {
            //CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_1);
            CSK_USBC->TXCSRH &= ~USB_ARCS_TXCSRH_DMAREQENAB;
            CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQMODE_1);
        }
        CSK_USBC->USB_DMA[val].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
        CSK_USBC->USB_DMA[val].COUNT = 0;
        usb_dma_clear_channel_active_flag(val);
        epp->dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        LOG_DBG("%s: abort dma for EP 0x%x\n", __func__, ep_addr);
    }

    // reset transfer state
    //epp->req_len = epp->xfer_len = 0;
*/

    //usbd_ep_flush(ep_addr); // it seems useless...

    /* Disable EP interrupts */
    if (dir_idx == DIR_IDX_IN || ep_idx == 0) { //TX (IN) EP or EP0
        val = USB_ARCS_DAINT_IN_EP_INT(ep_idx);
        usb_arcs_ctrl.intr_txe &= ~val;
        CSK_USBC->INTRTXE &= ~val;

    } else { //RX (OUT) EP
        val = USB_ARCS_DAINT_OUT_EP_INT(ep_idx);
        usb_arcs_ctrl.intr_rxe &= ~val;
        CSK_USBC->INTRRXE &= ~val;
    }

    /* release fifo */
    //TODO: CANNOT release the FIFO space of one single EP only!!

    /* De-activate, disable and set NAK for EP */
    usb_arcs_ctrl.ep_info[dir_idx][ep_idx].ep_ena = 0;
}

//BSD: Cancel the ongoing transfer on some endpoint (API)
void dcd_edpt_cancel_xfer (uint8_t rhport, uint8_t ep_addr)
{
    uint8_t ep_idx, dir_idx;

    ARG_UNUSED(rhport);
    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

#if CONFIG_DBG_EP
    if (ep_idx == USB_EP_NO_DBG) { // Debug EP
        LOG_DBG("%s: DBG EP (0x%x) Xfer CANNOT be cancelled, do nothing!\r\n", __func__, ep_addr);
        return;
    }
#endif

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return;

    EDPxReg_SEL(ep_idx);
    dcd_edpt_cancel_xfer_internal(rhport, ep_idx, dir_idx);
}


// Stall endpoint
//int usb_dc_ep_set_stall(const uint8_t ep)
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
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
            // When STALL handshake is transmitted, FIFO is flushed and the TxPktRdy bit is cleared
            CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_SENDSTALL;
        }
    }
    CLOGW("SW SENDSTALL on EP(0x%02x)!\n", ep_addr);
}


// clear stall, data toggle is also reset to DATA0
//int usb_dc_ep_clear_stall(const uint8_t ep)
void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
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
    //NOTE: it's SEN'D'STALL, NOT SEN'T'STALL!!
    if (dir_idx == DIR_IDX_OUT)
        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_SENDSTALL; // D NOT T!!
    else
        CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_SENDSTALL; // D NOT T!!

    CLOGW("CLEAR SENDSTALL on EP(0x%02x)!\n", ep_addr);
}


int32_t usbd_ep_is_stalled(uint8_t ep_addr, uint8_t *stalled)
{
    uint8_t ep_idx, dir_idx;

    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (!usbd_ep_is_valid(ep_idx, dir_idx) || !stalled) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *stalled = 0;

    EDPxReg_SEL(ep_idx);
    if(ep_idx == 0) {
        //if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_SENTSTALL)
        if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_SENDSTALL) // D NOT T!!
            *stalled = 1U;
    } else {
        if (dir_idx == DIR_IDX_OUT) {
            //if (CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_SENTSTALL)
            if (CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_SENDSTALL) // D NOT T!!
                *stalled = 1U;
        } else {
            //if(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_SENTSTALL)
            if(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_SENDSTALL) // D NOT T!!
                *stalled = 1U;
        }
    }

    return CSK_DRIVER_OK;
}


// flush RX or TX FIFO of endpoint
int usbd_ep_flush(const uint8_t ep_addr)
{
    uint8_t ep_idx, dir_idx;

    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);


    if (!usbd_ep_is_valid(ep_idx, dir_idx)) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Each endpoint has dedicated Tx FIFO */
    EDPxReg_SEL(ep_idx);
    if(ep_idx > 0) {
        if (dir_idx == DIR_IDX_OUT) {
            if (CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) // RX packet ready
                CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO;
        } else {
            if (CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_TXPKTRDY) // TX packet ready
                CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO;
        }
    }
    else{
        /* Endpoint0 fifo flush */
        if (CSK_USBC->CSR0L & (USB_ARCS_CSR0L_RXPKTRDY | USB_ARCS_CSR0L_TXPKTRDY)) // RX or TX packet ready
            CSK_USBC->CSR0H |= USB_ARCS_CSR0H_FLUSHFIFO;
    }
    EDPxReg_SEL(0);

    return CSK_DRIVER_OK;
}


//--------------------------------------------------------------------+
// DMA & PIO endpoint read / write
//--------------------------------------------------------------------+

/**
  \fn          int32_t usb_dma_set_channel_active_flag (uint8_t ch)
  \brief       Protected set of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
_FAST_FUNC_RO static int32_t usb_dma_set_channel_active_flag (uint8_t ch)
{
    uint8_t irq_en;

    // disable only USB interrupt for this DMA is used for USB only
    irq_en = IRQ_enabled(IRQ_USBC_VECTOR);
    if (irq_en) disable_IRQ(IRQ_USBC_VECTOR);
    if (usb_arcs_ctrl.channel_active & (1U << ch)) {
        if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
        return -1;
    }
    usb_arcs_ctrl.channel_active |= (1U << ch);
    if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
    return 0;
}


/**
  \fn          void usb_dma_clear_channel_active_flag (uint8_t ch)
  \brief       Protected clear of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
*/
_FAST_FUNC_RO static void usb_dma_clear_channel_active_flag (uint8_t ch)
{
    // disable only USB interrupt for this DMA is used for USB only
    uint32_t irq_en = IRQ_enabled(IRQ_USBC_VECTOR);
    if (irq_en) disable_IRQ(IRQ_USBC_VECTOR);
    usb_arcs_ctrl.channel_active &= ~(1U << ch);
    if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
}


//dynamically allocate available free DMA channel
//return 0: function succeeded
//return -1: function failed
_FAST_FUNC_RO static int32_t usb_dma_get_free_channel(uint8_t *pch)
{

    uint8_t i, irq_en, found = 0;

    if (pch == NULL)
        return -1;

    // disable only USB interrupt for this DMA is used for USB only
    irq_en = IRQ_enabled(IRQ_USBC_VECTOR);
    if (irq_en) disable_IRQ(IRQ_USBC_VECTOR);
    for (i=0; i<USB_DMA_CH_COUNT_AVAIL; i++) {
        if ((usb_arcs_ctrl.channel_active & (1U << i)) == 0) {
            *pch = i;
            found = 1;
            break;
        }
    }
    if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
    return (found ? 0 : -1);
}


_FAST_FUNC_RO static int32_t usb_arcs_dma_setting(uint8_t ep_addr, uint32_t address, uint32_t length)
{
    uint8_t ep_idx, dir_idx;
    uint8_t dma_channel, val;
    struct usb_ep_ctrl_prv *epp;

    ep_idx = USB_EP_GET_IDX(ep_addr);
    dir_idx = USB_EP_GET_DIR_IDX(ep_addr);
    epp = &usb_arcs_ctrl.ep_info[dir_idx][ep_idx];

    if (ep_idx == 0 || ep_idx == USB_EP_NO_DBG) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    //check whether dma channel is assigned before
    if (epp->dma_ch != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED)
        return 0;

    int ret;
    dma_channel = 0xFF;
    ret = usb_dma_get_free_channel(&dma_channel);
    if(ret != 0){
        LOG_DBG("DMA channel resource is not available");
    }

    epp->dma_ch = dma_channel;

    usb_dma_set_channel_active_flag(dma_channel);

    EDPxReg_SEL(ep_idx);

/*
    // The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 1.
#if DMA_MULTIPLE // DMA multiple packet
    if(dir_idx == USB_ARCS_DMA_DIR_RX_ENDPOINT){
        CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
        CSK_USBC->RXCSRH |= (USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
    }else{
        CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_1);
        //BSD NOTE: USB_ARCS_TXCSRH_DMAREQMODE_0 MUST be used if we need IN EP interrupt (usb_arcs_int_iep_handler) together with DMA interrupt!!
        CSK_USBC->TXCSRH |= (USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_0); //USB_ARCS_TXCSRH_DMAREQMODE_1
        CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_UNDERRUN; //BSD:
    }
#else // DMA single packet
    if(dir_idx == USB_ARCS_DMA_DIR_RX_ENDPOINT){
        CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR_MASK | USB_ARCS_RXCSRH_DMAREQENAB_MASK | USB_ARCS_RXCSRH_DMAREQMODE_MASK);
    }else{
        CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET_MASK | USB_ARCS_TXCSRH_DMAREQENAB_MASK | USB_ARCS_TXCSRH_DMAREQMODE_MASK);
    }
#endif
*/

    if(dir_idx == USB_ARCS_DMA_DIR_RX_ENDPOINT){
/*
        CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
        CSK_USBC->RXCSRH |= (USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_RXCSRH_DMAREQMODE_POS));
        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_OVERRUN;
*/
        uint32_t reg_val = CSK_USBC->RXCSRH;
        reg_val &= ~(USB_ARCS_RXCSRH_DMAREQMODE_1);
        reg_val |= (USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_RXCSRH_DMAREQMODE_POS));
        CSK_USBC->RXCSRH = reg_val;

    }else{
#if CONFIG_DPB_IN
        if (TX_IS_IDLE()) { // TxPktRdy & NOTEMPTY NOT SET, indicates idle
            if (length > epp->fixed.mps) // >= MPS
                CSK_USBC->TXFIFOSZ |= USB_ARCS_FIFOSZ_DPB;
                //CSK_USBC->TXFIFOSZ = GET_FIFOSZ_CFG(epp->fixed.mps << 1) | USB_ARCS_FIFOSZ_DPB;
            else // < MPS
                CSK_USBC->TXFIFOSZ &= ~USB_ARCS_FIFOSZ_DPB;
                //CSK_USBC->TXFIFOSZ = GET_FIFOSZ_CFG(epp->fixed.mps);
        }
#endif
/*
        CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_1);
        //BSD NOTE: USB_ARCS_TXCSRH_DMAREQMODE_0 had better be used if we need IN EP interrupt (usb_arcs_int_iep_handler) together with DMA interrupt!!
        //CSK_USBC->TXCSRH |= ((length >= epp->fixed.mps ? USB_ARCS_TXCSRH_AUTOSET : 0) | USB_ARCS_TXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_TXCSRH_DMAREQMODE_POS));
        CSK_USBC->TXCSRH |= (USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_TXCSRH_DMAREQMODE_POS));
        CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_UNDERRUN;
*/
        //NOTE: if EP TX is NOT in idle state, it may have side effect to change TXCSRH, e.g. clear DMAREQENAB to cause IN EP interrupt, cause ZLP?
        uint32_t reg_val = CSK_USBC->TXCSRH;
        reg_val &= ~(USB_ARCS_TXCSRH_DMAREQMODE_1);
        reg_val |= (USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | (epp->dma_reqmode << USB_ARCS_TXCSRH_DMAREQMODE_POS));
        CSK_USBC->TXCSRH = reg_val;
    }

    //BSD NOTE: USB DMAC can access M5(AP CODE RAM) only, and is it cacheable?
    if (range_is_cacheable(address, length)) {
        if (dir_idx == DIR_IDX_OUT) // host -> device
            cache_dma_fast_inv_stage1(address, address + length);
        else // device -> host
            //dcache_clean_range(address, address + length);
            HAL_FlushDCache_by_Addr((uint32_t *)address, length);
    }

    //NOTE: address must be aligned with 4 bytes according to MUSB datasheet
    //usb_arcs_ctrl.ep_info[dir_idx][ep_idx].last_len = length; // to be transferred
    CSK_USBC->USB_DMA[dma_channel].ADDR = address;
    CSK_USBC->USB_DMA[dma_channel].COUNT = length;
    CSK_USBC->USB_DMA[dma_channel].CNTL = USB_ARCS_DMA_CNTL_DMA_ENAB | (dir_idx << USB_ARCS_DMA_CNTL_DMA_DIR_POS) | (epp->dma_mode << USB_ARCS_DMA_CNTL_DMAMODE_POS) |
                                     USB_ARCS_DMA_CNTL_DMAIE | USB_DMA_BURST_MODE | (ep_idx << USB_ARCS_DMA_CNTL_DMAEP_POS);

    return 0;
}


/*
static int32_t usbd_ep_set_dma_callback(const uint8_t ep_addr, const usbd_ep_dma_cb cb)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);
    uint8_t dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return CSK_DRIVER_ERROR_PARAMETER;

    usb_arcs_ctrl.ep_info[dir_idx][ep_idx].dma_cb = cb;
    return CSK_DRIVER_OK;
}


static int32_t usbd_ep_enable_dma(const uint8_t ep_addr)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);
    uint8_t dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

    if (ep_idx == 0)
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return CSK_DRIVER_ERROR_PARAMETER;

    usb_arcs_ctrl.ep_info[dir_idx][ep_idx].dma_ena = 1;
    return CSK_DRIVER_OK;
}


static int32_t usbd_ep_disable_dma(const uint8_t ep_addr)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);
    uint8_t dir_idx = USB_EP_GET_DIR_IDX(ep_addr);
    uint8_t ch;

    if (ep_idx == 0)
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    if (!usbd_ep_is_valid(ep_idx, dir_idx))
        return CSK_DRIVER_ERROR_PARAMETER;

    // just return if NOT enabled yet
    if (!usb_arcs_ctrl.ep_info[dir_idx][ep_idx].dma_ena)
        return CSK_DRIVER_OK;

    // get current ongoing DMA channel
    ch = usb_arcs_ctrl.ep_info[dir_idx][ep_idx].dma_ch;
    if (ch < USB_DMA_CH_COUNT_AVAIL) {
        // stop HW DMA operation first?
        CSK_USBC->USB_DMA[ch].ADDR = 0;
        CSK_USBC->USB_DMA[ch].COUNT = 0;
        CSK_USBC->USB_DMA[ch].CNTL = 0;

        // clear DMA operation status
        usb_arcs_ctrl.ep_info[dir_idx][ep_idx].req_addr = 0;
        usb_arcs_ctrl.ep_info[dir_idx][ep_idx].req_len = 0;
        usb_arcs_ctrl.ep_info[dir_idx][ep_idx].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;

        // release DMA channel
        usb_dma_clear_channel_active_flag(ch);
    }

    // set disabled
    usb_arcs_ctrl.ep_info[dir_idx][ep_idx].dma_ena = 0;
    return CSK_DRIVER_OK;
}
*/


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

static inline void usbd_ep_pio_write(uint8_t ep_idx, uint8_t *buffer, uint32_t len)
{
    uint32_t i;
    uint8_t *pFifo_addr;
    //uint8_t const ep_idx = USB_EP_GET_IDX(ep_addr);
    //uint8_t const dir   = USB_EP_GET_DIR(ep_addr);
    //assert(dir == USB_EP_DIR_IN);

    pFifo_addr = (uint8_t*)(&CSK_USBC->FIFOX[ep_idx]);
    for (i = 0U; i < len; i++) {
        *pFifo_addr = buffer[i];
    }
}


// Read Endpoint 0 (it SHOULD be in DATA OUT or STATUS stage)
// return whether DATAEND bit is set or not, 1 = DATAEND is set
static bool usbd_ep0_read()
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t bytes_to_read;
    uint16_t pkt_len = 0;
    bool bret = false;

    if (usb_arcs_ctrl.status == USB_ARCS_STS_IN) {
        CLOGW("%s: Error, read EP0 in DATA IN stage!!", __func__);
    }

    epp = &usb_arcs_ctrl.ep_info[0][0];
    bytes_to_read = epp->req_len - epp->xfer_len;

    EDPxReg_SEL(0);

    // continue reading only if XXX_RXPKTRDY bit is set
    //NOTE: COUNT0 / RXCOUNT is VALID only if XXX_RXPKTRDY bit is set
    if( !(CSK_USBC->CSR0L & USB_ARCS_CSR0L_RXPKTRDY) )
        return false;
    pkt_len = CSK_USBC->COUNT0 & USB_ARCS_COUNT0_MASK;

//    if (pkt_len == 0) {
//        LOG_DBG("%s: ZLP is received in stage 0x%x!!", __func__, usb_arcs_ctrl.status);
//    }

    // NO buffer to hold data (usually dcd_edpt_xfer has NOT been called)
    if (bytes_to_read == 0 && pkt_len > 0)
        return false;

    if (pkt_len > 0) {
        if (bytes_to_read > pkt_len)
            bytes_to_read = pkt_len;

        //TODO: should remaining DATA in RX FIFO should be flushed if bytes_to_read < pkt_len?
        if (bytes_to_read > 0) {
            epp->last_len = bytes_to_read;
            usbd_ep_pio_read(0, (uint8_t *)(epp->req_addr + epp->xfer_len), bytes_to_read);
            epp->xfer_len += bytes_to_read;
            usb_arcs_ctrl.ep0_xfer_len += bytes_to_read;
        }
    }

    // notify host of DATA received if the packet is read out,
    // or else NAK will be responded to host for later incoming packet..
    if (bytes_to_read == pkt_len) {
        if (pkt_len < epp->fixed.mps ||
            //epp->xfer_len == usb_arcs_ctrl.ep0_data_len) {
            usb_arcs_ctrl.ep0_xfer_len == usb_arcs_ctrl.ep0_data_len) {
            //last packet, set DATAEND flag
            CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY | USB_ARCS_CSR0L_DATAEND;

            //uint32_t changed = USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
            //if (usb_arcs_ctrl.status == USB_ARCS_STS_OUT)
            //    changed |= USB_ARCS_CSR0L_DATAEND;
            //CSK_USBC->CSR0L |= changed;

            //TODO: SW driver bypasses STATUS stage and restore to next SETUP stage
            //usb_arcs_ctrl.status = USB_ARCS_STS_STATUS;
            usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
            bret = true;

        } else {
            CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
        }
    }

    return bret;
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

    epp = &usb_arcs_ctrl.ep_info[0][ep_idx];
    bytes_to_read = epp->req_len - epp->xfer_len;

    EDPxReg_SEL(ep_idx);

    // continue reading only if XXX_RXPKTRDY bit is set
    //NOTE: COUNT0 / RXCOUNT is VALID only if XXX_RXPKTRDY bit is set
    if( !(CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) )
        return -1;
    pkt_len = CSK_USBC->RXCOUNT & USB_ARCS_RXCOUNT_MASK;

//    if (pkt_len == 0) {
//        LOG_DBG("%s: ZLP is received in stage 0x%x!!", __func__, usb_arcs_ctrl.status);
//    }

    // NO buffer to hold data (usually dcd_edpt_xfer has NOT been called)
    if (bytes_to_read == 0 && pkt_len > 0)
        return -1;

    // make sure to clear DMA operation
    CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
    if (epp->dma_ch != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED) {
        CSK_USBC->USB_DMA[epp->dma_ch].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
        usb_dma_clear_channel_active_flag(epp->dma_ch);
        epp->dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
    }

    if (pkt_len > 0) {
        if (bytes_to_read > pkt_len)
            bytes_to_read = pkt_len;

        //TODO: should remaining DATA in RX FIFO should be flushed if bytes_to_read < pkt_len?
        if (bytes_to_read > 0) {
            epp->last_len = bytes_to_read;
            epp->last_io = 2; // PIO
            usbd_ep_pio_read(ep_idx, (uint8_t *)(epp->req_addr + epp->xfer_len), bytes_to_read);
            epp->xfer_len += bytes_to_read;
        }
    }

    // notify host of DATA received if the packet is read out,
    // or else NAK will be responded to host for later incoming packet..
    if (bytes_to_read == pkt_len) {
        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
    }

    return bytes_to_read;
}


// Write Endpoint 0 (it SHOULD be in DATA IN or STATUS stage)
// return whether DATAEND bit is set or not, 1 = DATAEND is set
static bool usbd_ep0_write(bool send_zlp_if_none)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t bytes_to_write, xferred_bytes;
    bool bret = false;

    epp = &usb_arcs_ctrl.ep_info[1][0];
    bytes_to_write = epp->req_len - epp->xfer_len;

    if (usb_arcs_ctrl.status == USB_ARCS_STS_OUT) {
        CLOGW("%s: Error, write EP0 in DATA OUT stage!!", __func__);
    }

    EDPxReg_SEL(0);

    // continue writing only if XXX_TXPKTRDY bit is cleared
    //TODO: check if TX FIFO is empty or available?
    if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_TXPKTRDY)
       return false;

    if (bytes_to_write == 0) { // nothing to be written
        if (!send_zlp_if_none)
            return false;
    }

    // fill TX FIFO with prepared data
    if (bytes_to_write > epp->fixed.mps)
        bytes_to_write = epp->fixed.mps;
    if (bytes_to_write > 0) {
        usbd_ep_pio_write(0, (uint8_t *)(epp->req_addr + epp->xfer_len), bytes_to_write);
        epp->xfer_len += bytes_to_write;
        usb_arcs_ctrl.ep0_xfer_len += bytes_to_write;
    }
    epp->last_len = bytes_to_write; // bytes_to_write may be 0...

    // set TXPKTRDY (and DATAEND if necessary) flag
    if (bytes_to_write < epp->fixed.mps ||
        //epp->xfer_len == usb_arcs_ctrl.ep0_data_len) {
        usb_arcs_ctrl.ep0_xfer_len == usb_arcs_ctrl.ep0_data_len) {
        //last packet, set DATAEND flag
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_TXPKTRDY | USB_ARCS_CSR0L_DATAEND;

        //uint32_t changed = USB_ARCS_CSR0L_TXPKTRDY;
        //if (usb_arcs_ctrl.status == USB_ARCS_STS_IN)
        //    changed |= USB_ARCS_CSR0L_DATAEND;
        //CSK_USBC->CSR0L |= changed;

        //TODO: SW driver bypasses STATUS stage and restore to next SETUP stage
        //usb_arcs_ctrl.status = USB_ARCS_STS_STATUS;
        usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
        bret = true;
        //LOG_DBG("set TXRDY & DATAEND");
    } else {
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_TXPKTRDY;
        //LOG_DBG("set TXRDY only");
    }

    //LOG_DBG("Write EP0 %d bytes!", bytes_to_write);
    return bret;
}

// Write Endpoint (NOT EP0)
// return written bytes, and -1 if error.
static int32_t usbd_epx_write(uint8_t ep_idx, bool send_zlp_if_none)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t bytes_to_write, xferred_bytes;

    if (ep_idx == 0 || ep_idx >= USB_ARCS_IN_EP_NUM)
        return -1;

    epp = &usb_arcs_ctrl.ep_info[1][ep_idx];
    bytes_to_write = epp->req_len - epp->xfer_len;

    EDPxReg_SEL(ep_idx);

    // continue writing only if XXX_TXPKTRDY bit is cleared
    if(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_TXPKTRDY)
        return -1;

//    //FIXME: CAN it go here??
//    if(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_FIFONOTEMPTY) {
//        CLOGW("%s: TX FIFO NOT empty, but TXPKTRDY flag NOT set!\n", __func__);
//        CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;
//        return -1;
//    }

    if (bytes_to_write == 0) { // nothing to be written
        if (!send_zlp_if_none)
            return -1;
    }

    // make sure to clear DMA operation
    CSK_USBC->TXCSRH &= ~(USB_ARCS_TXCSRH_AUTOSET | USB_ARCS_TXCSRH_DMAREQENAB | USB_ARCS_TXCSRH_DMAREQMODE_1);
    if (epp->dma_ch != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED) {
        CSK_USBC->USB_DMA[epp->dma_ch].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
        usb_dma_clear_channel_active_flag(epp->dma_ch);
        epp->dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
    }

    // fill TX FIFO with prepared data
    if (bytes_to_write > epp->fixed.mps)
        bytes_to_write = epp->fixed.mps;
    if (bytes_to_write > 0) {
        usbd_ep_pio_write(ep_idx, (uint8_t *)(epp->req_addr + epp->xfer_len), bytes_to_write);
        epp->xfer_len += bytes_to_write;
    }
    epp->last_len = bytes_to_write; // bytes_to_write may be 0...
    epp->last_io = 2; // PIO
    CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;

    //LOG_DBG("Write EP%d %d bytes!", ep_idx, bytes_to_write);
    return bytes_to_write;
}

static void update_ep_rx_status(uint8_t ep_idx)
{
    struct usb_ep_ctrl_prv *epp;
    epp = &usb_arcs_ctrl.ep_info[0][ep_idx];
    if (epp->req_len > 0) {
        if (epp->xfer_len == epp->req_len || epp->term_early) {
//            // last packet has been transfered, change to STS_STATUS or STS_SETUP...
//            if (ep_idx == 0 && epp->last_len < epp->fixed.mps) {
//                //TODO: SW driver bypasses STATUS stage and restore to next SETUP stage
//                //usb_arcs_ctrl.status = USB_ARCS_STS_STATUS;
//                usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
//            }

            // cleanup last read operation?
            uint32_t xferred_bytes = epp->xfer_len;
            epp->req_addr = 0;
            epp->req_len = epp->xfer_len = epp->last_len = 0;
            epp->term_early = 0;
            epp->last_io = 0; // NO IO

            // notify upper USBD driver
            dcd_event_xfer_complete(0, USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_OUT),
                                    xferred_bytes, XFER_RESULT_SUCCESS, true);
        }
    }
}

static void update_ep_tx_status(uint8_t ep_idx)
{
    struct usb_ep_ctrl_prv *epp;
    epp = &usb_arcs_ctrl.ep_info[1][ep_idx];
    if (epp->req_len > 0) {
//        // last packet has been transfered, change to STS_STATUS or STS_SETUP...
//        if (ep_idx == 0 && epp->last_len < epp->fixed.mps) {
//            //TODO: SW driver bypasses STATUS stage and restore to next SETUP stage
//            //usb_arcs_ctrl.status = USB_ARCS_STS_STATUS;
//            usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
//        }
        if (epp->xfer_len == epp->req_len) {
            // cleanup last read operation?
            uint32_t xferred_bytes = epp->xfer_len;
            epp->req_addr = 0;
            epp->req_len = epp->xfer_len = epp->last_len = 0;
            epp->last_io = 0; // NO IO

            // notify upper USBD driver
            dcd_event_xfer_complete(0, USB_EP_GET_ADDR(0, USB_EP_DIR_IN),
                                    xferred_bytes, XFER_RESULT_SUCCESS, true);
        }
    }
}


//FIXME: HOW TO handle R/W ZLP for dcd_edpt_xfer?? STATUS stage INT overlap next SETUP stage INT?
// Submit a transfer, and dcd_event_xfer_complete() is invoked to notify the stack when complete interrupt
//bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint32_t total_bytes)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t addr = (uint32_t)buffer;
    uint8_t const ep_idx = USB_EP_GET_IDX(ep_addr);
    uint8_t const dir_idx = USB_EP_GET_DIR_IDX(ep_addr);

#if CONFIG_DBG_EP
    if (ep_idx == USB_EP_NO_DBG) { // Debug EP
        LOG_DBG("%s: DBG EP (0x%x) is used internally, do nothing!\r\n", __func__, ep_addr);
        return true;
    }
#endif

    ARG_UNUSED(rhport);
    epp = &usb_arcs_ctrl.ep_info[dir_idx][ep_idx];
    epp->req_addr = addr;
    epp->req_len = total_bytes;
    epp->xfer_len = 0;
    epp->last_len = 0;
    epp->last_io = 0; // NO IO
    epp->term_early = 0;

    if (ep_idx == 0) { // EP0 Control Read / Write
        if (dir_idx == DIR_IDX_OUT) { // && usb_arcs_ctrl.status == USB_ARCS_STS_OUT
             usbd_ep0_read();
             update_ep_rx_status(0);
        } else if (dir_idx == DIR_IDX_IN) { // && usb_arcs_ctrl.status == USB_ARCS_STS_IN
            // send ZLP when no more data? so, device can enter into IDLE (SETUP) stage...
            // update TX status in advance if last packet of EP0 data
//            usbd_ep0_write(total_bytes==0);
//            update_ep_tx_status(0);
            if (usbd_ep0_write(total_bytes==0))
                update_ep_tx_status(0);
        }

        goto MY_EXIT;
    }

    if (epp->dma_ena && total_bytes) { // EP DMA Read / Write (other than EP0)
        if ((addr & 0x3) == 0) { // address is aligned with 4
            epp->last_len = total_bytes; // to be transferred
            epp->last_io = 1; // DMA
            usb_arcs_dma_setting(ep_addr, addr, total_bytes);
            //LOG_DBG("USB dma multiple packet setting");
            goto MY_EXIT;
        }

        //FIXME: SHOULD check the following handling code!! bauldeng2023.1.13.

        uint16_t count, pkt_len;

         // write, address is NOT aligned with 4
        if (dir_idx == DIR_IDX_IN) {
            count = 4 - (addr & 0x3);
            if (total_bytes - count < 4) { // just PIO write if less than 4
                usbd_epx_write(ep_idx, false);
                goto MY_EXIT;
            }
            //step 1: PIO write the odd bytes
            epp->last_len = count;
            epp->last_io = 2; // PIO
            usbd_ep_pio_write(ep_idx, (uint8_t *)addr, count);
            epp->xfer_len += count;
            //step 2: DMA write the following bytes
            if (total_bytes > count) {
                epp->last_len = total_bytes - count;
                epp->last_io = 1; // DMA
                usb_arcs_dma_setting(ep_addr, addr+count, epp->last_len);
            }
            goto MY_EXIT;
        }

        // read, address is NOT aligned with 4, and packet has already arrived...
        // if NOT RxPkgRdy, defer following operations into ISR of RxPkgRdy...
        EDPxReg_SEL(ep_idx);
        if(CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) { // dir_idx == DIR_IDX_OUT
            count = 4 - (addr & 0x3);
            pkt_len = CSK_USBC->RXCOUNT & USB_ARCS_RXCOUNT_MASK;
            if (total_bytes - count < 4  && total_bytes <= pkt_len) { // just PIO read if less than 4
                usbd_epx_read(ep_idx);
                update_ep_rx_status(ep_idx);
                goto MY_EXIT;
            }
            //step 1: PIO read the odd bytes
            assert(count <= pkt_len);
            epp->last_len = count;
            epp->last_io = 2; // PIO
            usbd_ep_pio_read(ep_idx, (uint8_t *)addr, count);
            epp->xfer_len += count;
            //step 2: DMA read the following bytes
            if (total_bytes > count) {
                epp->last_len = total_bytes - count;
                epp->last_io = 1; // DMA
                usb_arcs_dma_setting(ep_addr, addr+count, epp->last_len);
            } else {
                update_ep_rx_status(ep_idx);
            }
        }
        goto MY_EXIT;
    } // DMA enabled and total_bytes > 0

    // EP PIO Read / Write (other than EP0)
    if (dir_idx == DIR_IDX_OUT) {
        EDPxReg_SEL(ep_idx);
        if(CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) {
            usbd_epx_read(ep_idx);
            update_ep_rx_status(ep_idx);
        }
    } else if (dir_idx == DIR_IDX_IN) {
        // send ZLP when no more data? so, device can enter into IDLE (SETUP) stage...
        usbd_epx_write(ep_idx, (total_bytes==0));
    }

MY_EXIT:
    /* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
    EDPxReg_SEL(0);

    return true;
}

//--------------------------------------------------------------------+
// USB interrupt handling
//--------------------------------------------------------------------+

/* Handle interrupts on a control endpoint */
static void usb_arcs_ep0_isr(void)
{
    uint8_t ep_addr, ep_idx = 0;
    uint16_t pkt_len;
    struct usb_ep_ctrl_prv *epp;

    //select endpoint0
    EDPxReg_SEL(ep_idx);

    //endpoint0 setupend interrupt
    if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_SETUPEND){
        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDSETUPEND;
        usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
        LOG_DBG("Endpoint0 setupend interrupt generated");
        return; //TODO: is it OK?
    }

    if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_SENTSTALL){
        CSK_USBC->CSR0L &= ~USB_ARCS_CSR0L_SENTSTALL;
        //BSD: NO SentStall interrupt according to MUSB datasheet!!
        CLOGW("SENTSTALL on EP0!");
        //LOG_DBG("Endpoint0 sentstall interrupt generated");
        usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
        return; //TODO: is it OK?
    }

    switch (usb_arcs_ctrl.status) {
    case USB_ARCS_STS_SETUP:
        /* Call the registered callback if any */
//      LOG_DBG("usb ep0 SETUP phase (idle state)"); //BSD: open it?
        //rx packet ready
        if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_RXPKTRDY) {
            EDPxReg_SEL(0);
            pkt_len = CSK_USBC->COUNT0 & USB_ARCS_COUNT0_MASK;
//            LOG_DBG("usb ep0: usb_arcs_ctrl status %u, size %u",
//                    usb_arcs_ctrl.status, pkt_len);
            if(pkt_len >= sizeof(tusb_control_request_t)) {
                uint32_t data[2];
                data[0] = CSK_USBC->FIFOX[0];
                data[1] = CSK_USBC->FIFOX[0];

                tusb_control_request_t *setup_tmp;
                setup_tmp = (tusb_control_request_t *)data;
                usb_arcs_ctrl.ep0_data_len = setup_tmp->wLength;
                usb_arcs_ctrl.ep0_xfer_len = 0; // initialize to 0 once new request arrives
                if (setup_tmp->wLength == 0) {
                    //CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
                    // DATAEND must be set here. Because status stage don't check txpktrdy signal,
                    // if software don't set DATAEND before status stage end, SETUPEND flag will be set.
                    CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY | USB_ARCS_CSR0L_DATAEND;

                    //TODO: SW driver bypasses STATUS stage and re-enters into next SETUP stage...
                    //usb_arcs_ctrl.status = USB_ARCS_STS_STATUS;

                } else { // if (setup_tmp->wLength > 0)
                    //data phase expected
                    if (setup_tmp->bmRequestType_bit.direction == REQTYPE_DIR_TO_DEVICE) {
                        usb_arcs_ctrl.status = USB_ARCS_STS_OUT;
                        // clear RXPKTRDY only if no data left in FIFO
                        if ((CSK_USBC->COUNT0 & USB_ARCS_COUNT0_MASK) == 0)
                            CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
                    } else {
                        usb_arcs_ctrl.status = USB_ARCS_STS_IN;
                        CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
                    }
                }

                dcd_event_setup_received(0, (uint8_t const *)data, true);

            } else { // if(pkt_len == 0 || pkt_len < sizeof(tusb_control_request_t))
                CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;
                if (pkt_len == 0) {
                    LOG_DBG("ZLP received in SETUP / STATUS stage!");
                } else {
                    LOG_DBG("Illegal EP0 packet size (%d bytes) in SETUP stage!", pkt_len);
                }
            }
        } else { // no USB_ARCS_CSR0L_RXPKTRDY flag set
//            LOG_DBG("Unknown interrupt (maybe ZLP xmitted?) in SETUP / STATUS stage!"); //BSD: open it?
        }
        break;

    case USB_ARCS_STS_OUT:
        if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_RXPKTRDY) {
            usbd_ep0_read();
            update_ep_rx_status(0);
            //LOG_DBG("usb ep0 DATA OUT stage (RX packet arrived)");
        }
        break;

    case USB_ARCS_STS_IN:
        // TXPKTRDY = 1, no interrupt of TX send out
        if (!(CSK_USBC->CSR0L & USB_ARCS_CSR0L_TXPKTRDY)) {
            // don't send ZLP when no more data
//            usbd_ep0_write(false);
//            update_ep_tx_status(0);
            update_ep_tx_status(0);
            usbd_ep0_write(false);
            //LOG_DBG("usb ep0 DATA IN stage (TX packet sent out)");
        }
        break;

    case USB_ARCS_STS_STATUS:
        LOG_DBG("usb ep0 STATUS stage");
        if (!usb_arcs_ctrl.addressed && usb_arcs_ctrl.address) {
            CSK_USBC->FADDR = (usb_arcs_ctrl.address) & USB_ARCS_FADDR_ADDR_MASK;
            __DSB(); //TODO: make sure memory access is done before next instruction execution
            usb_arcs_ctrl.addressed = 1; // addressed
            LOG_DBG("Config. Addr = 0x%x", usb_arcs_ctrl.address);
        }
        // Clear RxPktRdy bit if ZLP is received in STATUS stage
        //if(CSK_USBC->CSR0L & USB_ARCS_CSR0L_RXPKTRDY)
        //    CSK_USBC->CSR0L |= USB_ARCS_CSR0L_SERVICEDRXPKTRDY;

        usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
        break;

    default:
        break;
    }
}


//transfer last short packet of DMA operation
//static inline void usb_arcs_oep_remainpkt_handler(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
static inline void usb_arcs_oep_remainpkt_handler(uint8_t ep_idx, uint8_t dir_idx)
{
    uint8_t dma_channel; //ep_idx, dir_idx,
    uint32_t remained, rxcnt;
    struct usb_ep_ctrl_prv *epp;

//    ep_idx = USB_EP_GET_IDX(ep);
//    dir_idx = USB_EP_GET_DIR_IDX(ep);

    EDPxReg_SEL(ep_idx);
    //CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);

    epp = &usb_arcs_ctrl.ep_info[0][ep_idx];
    dma_channel = epp->dma_ch;

    //BSD: fetch all data in RX FIFO, and remain count may be greater than RXCOUNT!
    remained = CSK_USBC->USB_DMA[dma_channel].COUNT;
    rxcnt = CSK_USBC->RXCOUNT;
    //LOG_DBG("oep_remainpkt_handler: dma remain: %d bytes, RX: %d bytes", remained, rxcnt);

    // require data of specified length, but only less data has arrived,
    // change req_len to
    if (rxcnt > 0)
        epp->last_len = rxcnt;
    if (remained > rxcnt) {
        // when several MPS packets have been transferred and there are extra odd bytes
        // those transferred packets should be included here...
        epp->xfer_len = epp->req_len - remained;
        epp->term_early = 1;
    }

    // read RX FIFO directly & abandon DMA operation if too short packet
    if (rxcnt < 8) { //TODO: 8 => ?
        CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1);
        CSK_USBC->USB_DMA[dma_channel].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
        CSK_USBC->USB_DMA[dma_channel].COUNT = 0;
        usb_dma_clear_channel_active_flag(dma_channel);
        epp->dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;

        if (rxcnt > 0) {
            epp->last_io = 2; // PIO
            usbd_ep_pio_read(ep_idx, (uint8_t *)(epp->req_addr + epp->xfer_len), rxcnt);
            epp->xfer_len += rxcnt;
        } else if (rxcnt == 0 && epp->last_len > remained) {
            epp->xfer_len += epp->last_len - remained;
        }

        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
        update_ep_rx_status(ep_idx);

    } else { // continue using DMA to read short packet
        // The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0.
        // DMA single packet register setting
        //CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1); // work (bus reset, slow)
        //CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQMODE_1); // | USB_ARCS_RXCSRH_DMAREQENAB, NOT work (bus reset, slow)
        //CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR | USB_ARCS_RXCSRH_DMAREQENAB); //  | USB_ARCS_RXCSRH_DMAREQMODE_1, work (bus reset, slow)
        //CSK_USBC->RXCSRH &= ~(USB_ARCS_RXCSRH_AUTOCLEAR); // | USB_ARCS_RXCSRH_DMAREQENAB | USB_ARCS_RXCSRH_DMAREQMODE_1, work

        epp->last_io = 1; // DMA
        if (remained > rxcnt)
            CSK_USBC->USB_DMA[dma_channel].COUNT = rxcnt;
        CSK_USBC->USB_DMA[dma_channel].CNTL = USB_ARCS_DMA_CNTL_DMA_ENAB | (dir_idx << USB_ARCS_DMA_CNTL_DMA_DIR_POS) | USB_ARCS_DMA_CNTL_DMAMODE_0 |
                                         USB_ARCS_DMA_CNTL_DMAIE | USB_DMA_BURST_MODE | (ep_idx << USB_ARCS_DMA_CNTL_DMAEP_POS);
    }

}

/*
static inline bool usb_ep_dma_is_started(uint8_t dma_ch)
{
    //NOTE:  EDPxReg_SEL(ep_idx) SHOULD be called before
    assert(dma_ch != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED);
    //if (dma_ch == USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED)
    //    return false;
    return ((CSK_USBC->USB_DMA[dma_ch].CNTL & USB_ARCS_DMA_CNTL_DMA_ENAB) != 0 &&
            CSK_USBC->USB_DMA[dma_ch].COUNT > 0);
}
*/

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

static inline void usb_arcs_int_oep_handler(uint32_t intsr)
{
    struct usb_ep_ctrl_prv *epp;
    uint32_t ep_int_status;
    uint16_t rxcnt;
    uint8_t ep_idx;

    //LOG_DBG("usb_arcs_int_oep_handler");
    for (ep_idx = 1U; ep_idx < USB_ARCS_OUT_EP_NUM; ep_idx++) {
        if (intsr & (USB_ARCS_INTRRX_EP_POS << ep_idx)) {
            /* Read OUT RX EP interrupt status */
            EDPxReg_SEL(ep_idx);
            ep_int_status = CSK_USBC->RXCSRL;
            rxcnt = CSK_USBC->RXCOUNT;
            //LOG_DBG("USB OUT EP%u interrupt status: 0x%x data_length: 0x%x", ep_idx, ep_int_status, rxcnt);

            // check all EP interrupts, including RxPktRdy, OverRun, SentStall etc.
            // if it's RxPktRdy, and RXCOUNT may be or may be NOT max packet size,
            // and call usb_arcs_oep_remainpkt_handler if NOT max packet size
            //     or even if max packet size but usb_arcs_dma_setting has not called yet!

            if (ep_int_status & USB_ARCS_RXCSRL_SENTSTALL) { // SentStall
                CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_SENTSTALL;
                LOG_DBG("SentStall on OUT EP%d, clear it", ep_idx);
            }

            if (ep_int_status & USB_ARCS_RXCSRL_OVERRUN) { // Overrun
                // OVERRUN is set if an OUT packet cannot be loaded into the Rx FIFO.
                // Note: it is only valid for ISO EP. For Bulk EP, it always returns zero.
                // FIXME:Here we just set Flush RX FIFO (?),
                // or else it could trap into the EP's OVERRUN interrupt repeatedly...
                if (ep_int_status & USB_ARCS_RXCSRL_RXPKTRDY)
                    CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO;

                CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_OVERRUN;
                CLOGW("Overrun on OUT EP%d! clear it", ep_idx);
            }

            if (!(ep_int_status & USB_ARCS_RXCSRL_RXPKTRDY))
                continue;

            epp = &usb_arcs_ctrl.ep_info[0][ep_idx];
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
                LOG_DBG("%s: NO dcd_edpt_xfer (RX) is called!, keep data in RX FIFO!", __func__);
                //LOG_DBG("%s: NO dcd_edpt_xfer (RX) is called!, just flush RX FIFO!", __func__);
                //CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO;

            //} else if(epp->dma_ena) {
            } else if(epp->dma_ch != USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED) {

//              if (usb_ep_dma_is_started(epp->dma_ch)) { // DMA has already started
//                  EDPxReg_SEL(0);
//                  continue; // check next EP
//              }

                //FIXME: SHOULD check the following handling code!! bauldeng2023.1.13.

                // start address is aligned with 4, OR start address is NOT aligned with 4 and xfer_len is greater than 0,
                //  indicating that DMA has already started and this is last short package
                //if (!(epp->req_addr & 0x3) || epp->xfer_len > 0) {
                if (!((epp->req_addr + epp->xfer_len) & 0x3)) {
                    //usb_arcs_oep_remainpkt_handler(USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_OUT), USB_DC_EP_DATA_OUT);
                    usb_arcs_oep_remainpkt_handler(ep_idx, USB_EP_DIR_OUT);

                } else { // NOT aligned with 4, indicating that DMA has not been started
                    uint16_t count, pkt_len;
                    count = 4 - (epp->req_addr & 0x3);
                    pkt_len = CSK_USBC->RXCOUNT & USB_ARCS_RXCOUNT_MASK;
                    if (epp->req_len - count < 4  && epp->req_len <= pkt_len) { // just PIO read if less than 4
                        usbd_epx_read(ep_idx);
                        update_ep_rx_status(ep_idx);
                    } else { // PIO read the odd, and then DMA read the following
                        //step 1: PIO read the odd bytes
                        assert(count <= pkt_len);
                        epp->last_len = count;
                        epp->last_io = 2; // PIO
                        usbd_ep_pio_read(ep_idx, (uint8_t *)epp->req_addr, count);
                        epp->xfer_len += count;
                        //step 2: DMA read the following bytes
                        if (epp->req_len > count) {
                            epp->last_len = epp->req_len - count;
                            epp->last_io = 1; // DMA
                            usb_arcs_dma_setting(USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_OUT), epp->req_addr+count, epp->last_len);
                        } else {
                            CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
                            update_ep_rx_status(ep_idx);
                        }
                    }
                }
            } else {
                //TODO: is it enough?
                usbd_epx_read(ep_idx);
                update_ep_rx_status(ep_idx);
            }

            /* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
            EDPxReg_SEL(0);
        }
    }
    /* Clear interrupt. */
}


static inline void usb_arcs_int_iep_handler(uint32_t intsr)
{
    uint32_t ep_int_status;
    uint8_t ep_idx;
    struct usb_ep_ctrl_prv *epp;
    uint32_t xferred_bytes;

    //LOG_DBG("usb_arcs_int_iep_handler");
    for (ep_idx = 1U; ep_idx < USB_ARCS_IN_EP_NUM; ep_idx++) {
        if (intsr & (USB_ARCS_INTRTX_EP_POS << ep_idx)) {
            /* Read IN TX EP interrupt status */
            EDPxReg_SEL(ep_idx);
            ep_int_status = CSK_USBC->TXCSRL;

            //LOG_DBG("USB IN EP%u interrupt status: 0x%x", ep_idx, ep_int_status);

            // check all EP interrupts, including TxPktRdy, UnderRun, SentStall etc.
            if (ep_int_status & USB_ARCS_TXCSRL_SENTSTALL) { // SentStall
                CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_SENTSTALL;
                CLOGW("SentStall on IN EP%d, clear it", ep_idx);
            }

            if (ep_int_status & USB_ARCS_TXCSRL_UNDERRUN) { // UnderRun
                //BSD: UnderRun is set if an IN token is received when TxPktRdy is not set
                // Here we just set TxPktRdy, and it could send an ZLP packet,
                // or else it will be trapped into the EP's UNDERRUN interrupt repeatedly...
                //
                //BSD: It may cause ERROR to set TxPktRdy to send ZLP, and host may request to RESET USB controller,
                //  e.g. in MSC transfer... The correct way is to do nothing, and let USBC respond with NAK...
                //
                //if (!(ep_int_status & USB_ARCS_TXCSRL_TXPKTRDY))
                //    CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;

                CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_UNDERRUN;
                LOG_DBG("Underrun on IN EP%d, clear it");
            }

            // write endpoint if necessary
            //usbd_epx_write(ep_idx, false); //TODO: is it enough?

            //BSD NOTE: Here dcd_event_xfer_complete is called for
            // TX (IN) and DMA Request Mode 0 (with IN EP interrupt)
            // At this point the whole TX process (RAM => IN EP FIFO => USB bus) is finally done!
            epp = &usb_arcs_ctrl.ep_info[1][ep_idx];
#if CONFIG_DPB_IN
            // TxPktRdy & NOTEMPTY NOT SET, indicates TX done (idle)
            if (TX_IS_IDLE() && epp->last_len > 0) { // TxPktRdy & NOTEMPTY NOT SET, indicates idle
#else
            if (TX_IS_IDLE() && epp->last_len > 0) { // TxPktRdy & NOTEMPTY NOT SET, indicates idle
            //if (!(ep_int_status & USB_ARCS_TXCSRL_TXPKTRDY) && epp->last_len > 0) { // TX done
#endif
                //if (epp->dma_ch == USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED || epp->dma_reqmode == 0) { // PIO or DMA with Request Mode 0
                if (epp->last_io == 2 || (epp->last_io == 1 && epp->dma_reqmode == 0)) { // PIO or DMA with Request Mode 0
                epp = &usb_arcs_ctrl.ep_info[1][ep_idx];
                epp->xfer_len += epp->last_len;
                xferred_bytes = epp->xfer_len;
                //LOG_DBG("%s: req_len = %d, xfer_len = %d\r\n", __func__, epp->req_len, epp->xfer_len);

                // notify upper USBD driver
                if (epp->req_len == epp->xfer_len) {
                    //CLOGI("IEP, Ntf");
                    epp->req_addr = 0;
                    epp->req_len = epp->xfer_len = epp->last_len = 0;
                    epp->last_io = 0; // NO IO
                    dcd_event_xfer_complete(0, USB_EP_GET_ADDR2(ep_idx, 1),
                                        xferred_bytes, XFER_RESULT_SUCCESS, true);
                }
                }
            }

            /* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
            EDPxReg_SEL(0);
        }
    }

    /* Clear interrupt. */
}


static void usb_arcs_dma_isr(uint32_t dmaintsr)
{
    uint8_t ch, ep_idx, dir_idx, flag;
    struct usb_ep_ctrl_prv *epp;
    uint32_t xferred_bytes = 0;

    //LOG_DBG("usb_arcs_dma_isr");
    for(ch = 0; ch < USB_DMA_CH_COUNT_AVAIL; ch++) {
        if( (CSK_USBC->USB_DMA[ch].CNTL & USB_ARCS_DMA_CNTL_DMAIE) && (dmaintsr & (0x01 << ch)) ) {
            //interrupt enabled and interrupt flag is set

            ep_idx = (CSK_USBC->USB_DMA[ch].CNTL & USB_ARCS_DMA_CNTL_DMAEP_MASK) >> USB_ARCS_DMA_CNTL_DMAEP_POS;
            dir_idx = (CSK_USBC->USB_DMA[ch].CNTL & USB_ARCS_DMA_CNTL_DMA_DIR_MASK) >> USB_ARCS_DMA_CNTL_DMA_DIR_POS;
            epp = &usb_arcs_ctrl.ep_info[dir_idx][ep_idx];
            //LOG_DBG("%s: ep_idx = %d, dir_idx = %d\r\n", __func__, ep_idx, dir_idx);

            EDPxReg_SEL(ep_idx);

            // clear DMA enable & interrupt enable bits of DMA channel
            CSK_USBC->USB_DMA[ch].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAEP_MASK |
                                        USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR); //BSD:

            usb_dma_clear_channel_active_flag(ch);
            epp->dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;

            //EDPxReg_SEL(ep_idx);
            if(dir_idx == DIR_IDX_OUT) { // OUT, read
                if (CSK_USBC->RXCSRL & USB_ARCS_RXCSRL_RXPKTRDY) {
                    uint32_t len = epp->last_len % epp->fixed.mps;
                    if (!(CSK_USBC->RXCSRH & USB_ARCS_RXCSRH_AUTOCLEAR) ||
                        //(epp->last_len > 0 && epp->last_len < epp->fixed.mps))
                        (len > 0 && len < epp->fixed.mps))
                        CSK_USBC->RXCSRL &= ~USB_ARCS_RXCSRL_RXPKTRDY;
                }

                //BSD NOTE: USB DMAC can access M5(AP CODE RAM) only, and is it cacheable?
                if (range_is_cacheable(epp->req_addr, epp->req_len))
                    cache_dma_fast_inv_stage2(epp->req_addr, epp->req_addr + epp->req_len);

//                if(epp->dma_cb)
//                    epp->dma_cb(USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_OUT), USB_DC_EP_DATA_OUT);

            } else { // IN, write
                //uint8_t txcsrl = CSK_USBC->TXCSRL;
                if (CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_UNDERRUN)
                    CSK_USBC->TXCSRL &= ~USB_ARCS_TXCSRL_UNDERRUN;

                //handle "last short packet" if AUTOSET is not set, SHOULD set TXPKTRDY manually!
                if (!(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_TXPKTRDY)) {
//                    if (!(CSK_USBC->TXCSRH & USB_ARCS_TXCSRH_AUTOSET) ||
//                        (epp->last_len > 0 && epp->last_len < epp->fixed.mps))
//                        CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;

                    uint32_t len = epp->last_len % epp->fixed.mps;
                    bool auto_set = CSK_USBC->TXCSRH & USB_ARCS_TXCSRH_AUTOSET;
                    if (!auto_set || (len > 0 && len < epp->fixed.mps)) {
                        CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;
                        //CLOGI("short len = %d\n", len);
                        //if (!(CSK_USBC->TXCSRL & USB_ARCS_TXCSRL_FIFONOTEMPTY)) {
                        //    LOG_DBG("ERR, ZLP!\n");
                        //}
                    }
                }

//                flag = USB_ARCS_TXCSRL_FIFONOTEMPTY | USB_ARCS_TXCSRL_TXPKTRDY;
//                if ((txcsrl & flag) == USB_ARCS_TXCSRL_FIFONOTEMPTY)
//                  CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_TXPKTRDY;

//                if(epp->dma_cb)
//                    epp->dma_cb(USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_IN), USB_DC_EP_DATA_IN);

            }

            /* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
            EDPxReg_SEL(0);

            //BSD NOTE: Here dcd_event_xfer_complete is called for RX (OUT) or
            // TX (IN) and DMA Request Mode 1 (NO IN EP interrupt)
            // for the whole RX process (USB bus => OUT EP FIFO => RAM) has been completed at this point,
            // while only first part of the whole TX process (RAM => IN EP FIFO => USB bus) is done...
            if((dir_idx == DIR_IDX_OUT) || (dir_idx == DIR_IDX_IN && epp->dma_reqmode == 1)) {
                epp->xfer_len += epp->last_len;
                xferred_bytes = epp->xfer_len;
                //LOG_DBG("%s: req_len = %d, xfer_len = %d\r\n", __func__, epp->req_len, epp->xfer_len);

                // notify upper USBD driver
                if (epp->req_len == epp->xfer_len || epp->term_early) {
                    epp->req_addr = 0;
                    epp->req_len = epp->xfer_len = epp->last_len = 0;
                    epp->term_early = 0;
                    epp->last_io = 0; // NO IO
                    dcd_event_xfer_complete(0, USB_EP_GET_ADDR2(ep_idx, dir_idx),
                                        xferred_bytes, XFER_RESULT_SUCCESS, true);
                }
            }
        } // end if
    } // end for
}


// reset internal state of the controller driver
static void usbd_reset_internal_state(void)
{
    struct usb_ep_ctrl_prv *epp_in, *epp_out;
    uint8_t i, flag, irq_en;

    // if NOT addressed yet, already in initial state, do nothing...
    if (usb_arcs_ctrl.addressed == 0 && usb_arcs_ctrl.address == 0) {
        LOG_DBG("usbd_reset_internal_state: no need to reset");
        return;
    }

    LOG_DBG("usbd_reset_internal_state");

    irq_en = IRQ_enabled(IRQ_USBC_VECTOR);
    if (irq_en) disable_IRQ(IRQ_USBC_VECTOR);

    // stop DMA operation if any
    flag = usb_arcs_ctrl.channel_active;
    for (i=0; i<USB_DMA_CH_COUNT_AVAIL; i++) {
        if ((flag & (1U << i)) != 0) {
            CSK_USBC->USB_DMA[i].CNTL &= ~(USB_ARCS_DMA_CNTL_DMA_ENAB | USB_ARCS_DMA_CNTL_DMAIE | USB_ARCS_DMA_CNTL_DMAERR);
            CSK_USBC->USB_DMA[i].COUNT = 0;
            CSK_USBC->USB_DMA[i].ADDR = 0;
        }
    }
    usb_arcs_ctrl.channel_active = 0;

    //NOTE: USB_ARCS_IN_EP_NUM == USB_ARCS_OUT_EP_NUM
    // flush RX FIFO
    for (i = 1U; i < USB_ARCS_IN_EP_NUM; i++) {
        EDPxReg_SEL(i);
        CSK_USBC->RXCSRL |= USB_ARCS_RXCSRL_FLUSHFIFO; //TODO: check USB_ARCS_RXCSRL_RXPKTRDY bit?
        CSK_USBC->TXCSRL |= USB_ARCS_TXCSRL_FLUSHFIFO; //TODO: check USB_ARCS_TXCSRL_TXPKTRDY bit?
    }
    EDPxReg_SEL(0);

    // clear private data of all EPs except EP0
    (void)memset(&usb_arcs_ctrl.ep_info[0][1], 0,
            sizeof(struct usb_ep_ctrl_prv) * (USB_ARCS_IN_EP_NUM -1));
    (void)memset(&usb_arcs_ctrl.ep_info[1][1], 0,
            sizeof(struct usb_ep_ctrl_prv) * (USB_ARCS_IN_EP_NUM -1));

    // initialize some private of all EPs except EP0
    for(i = 1; i < USB_ARCS_IN_EP_NUM; i++){
        usb_arcs_ctrl.ep_info[0][i].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        usb_arcs_ctrl.ep_info[1][i].dma_ch = USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED;
        // enable EP DMA by default except EP0
        usb_arcs_ctrl.ep_info[0][i].dma_ena = 1;
        usb_arcs_ctrl.ep_info[1][i].dma_ena = 1;
    }

    // clear global USB status
    //BSD: MUST reserve USB_CTRL_FIFO_SIZE bytes (Starting from 0) for EP0 FIFO
    usb_arcs_ctrl.fifo_alloc_addr = (USB_EP_FIFO_BASE + USB_CTRL_FIFO_SIZE) >> 3;
    usb_arcs_ctrl.addressed = 0; // restore to NOT addressed
    usb_arcs_ctrl.address = 0;
    //usb_arcs_ctrl.configured = 0; // restore to NOT configured

    usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;
    usb_arcs_ctrl.ep0_data_len = 0;

    if (irq_en) enable_IRQ(IRQ_USBC_VECTOR);
}


static void usb_arcs_handle_reset(void)
{
    CLOGI("USB RESET event");

    //CSK_USBC->FADDR = 0; //TODO: is it right?
    //usb_arcs_ctrl.address = 0;
    //usb_arcs_ctrl.addressed = 0;

    usb_arcs_ctrl.status = USB_ARCS_STS_SETUP;

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

#if CONFIG_DBG_EP
    config_dbg_ep();
#endif

    /* Enable usb module interrupt */
    CSK_USBC->INTRUSBE = 0x00;
    //CSK_USBC->INTRUSBE |= USB_ARCS_INTRUSBE_RESET;
    CSK_USBC->INTRUSBE = usb_arcs_ctrl.intr_usbe;

    /* enable EP0 interrupts */
    CSK_USBC->INTRTXE = usb_arcs_ctrl.intr_txe;
    CSK_USBC->INTRRXE = usb_arcs_ctrl.intr_rxe;

    // reset internal state of the controller driver
    usbd_reset_internal_state();

    //FIXME: SHOULD clear RESUME bit about 10 ms after RESUME bit is set...
    //volatile uint32_t i = 10000;
    //while (i-- > 0);
    //CSK_USBC->POWER &= ~USB_ARCS_POWER_RESUME;

}


//#include "IOMuxManager.h"
//#include "Driver_GPIO.h"

// Interrupt Handler
//void dcd_int_handler(uint8_t rhport)
#define dcd_int_handler     usb_arcs_isr_handler
static void usb_arcs_isr_handler(const void *unused)
{
    uint32_t txsr, rxsr, intsr, dmaintsr, dmaie=0;

    //ARG_UNUSED(unused);
    //GPIO_PinWrite(GPIOA(), CSK_GPIO_PIN3, 1);

    //endpoint & common interrupt status -> read clear
    intsr = CSK_USBC->INTRUSB & CSK_USBC->INTRUSBE;
    txsr = CSK_USBC->INTRTX & CSK_USBC->INTRTXE;
    rxsr = CSK_USBC->INTRRX & CSK_USBC->INTRRXE;
    for(uint32_t i=0; i<6; i++){
        dmaie |= (((CSK_USBC->USB_DMA[i].CNTL & USB_ARCS_DMA_CNTL_DMAIE) >> USB_ARCS_DMA_CNTL_DMAIE_POS)<<i);
    }
    dmaintsr = CSK_USBC->DMA_INTR & dmaie;

//  LOG_DBG("USB interrupt handler entered");
//  LOG_DBG("USB INTRUSB= 0x%x, INTRTX = 0x%x, INTRRX = 0x%x, DMA_INTR = 0x%x", intsr, txsr, rxsr, dmaintsr);

    if (intsr & USB_ARCS_INTRUSB_RESET) {
        /* Reset detected */
        //LOG_DBG("usb reset interrupt");
    #if TUD_OPT_HIGH_SPEED
        dcd_event_bus_reset(0, TUSB_SPEED_HIGH, true);
    #else
        dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
    #endif
        usb_arcs_handle_reset();
    }

/*
    if (intsr & USB_ARCS_INTRUSB_CONN) { // Only valid in Host mode
        LOG_DBG("usb connect interrupt");
        //dcd_event_bus_signal(0, DCD_EVENT_PLUGGED, true); //NO PLUGGED event!
    }

    if (intsr & USB_ARCS_INTRUSB_DISCON) { // Work when OTG is supported
        LOG_DBG("usb disconnect interrupt");
        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
    }
*/

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

    if(dmaintsr & USB_DMA_INTR_EP_ALL_MASK){
//      LOG_DBG("usb dma interrupt");
        usb_arcs_dma_isr(dmaintsr);
    }

    /* EP0 tx&rx endpoint interrupt */
    if (txsr & USB_ARCS_INTRTX_EP0_MASK) {
//      LOG_DBG("usb endpoint 0 interrupt");
        usb_arcs_ep0_isr();
    }

    /* EP1-5 tx endpoint interrupt for IN endpoint */
    if (txsr & USB_ARCS_INTRTX_EP_MASK) {
//      LOG_DBG("usb endpoint x IN interrupt");
        usb_arcs_int_iep_handler(txsr);
    }

    /* EP1-5 rx endpoint interrupt for OUT endpoint */
    if (rxsr & USB_ARCS_INTRRX_EP_MASK) {
//      LOG_DBG("usb endpoint x OUT interrupt");
        usb_arcs_int_oep_handler(rxsr);
    }

    //GPIO_PinWrite(GPIOA(), CSK_GPIO_PIN3, 0);
//  LOG_DBG("!!!!!!");
}


// Enable device interrupt
void dcd_int_enable (uint8_t rhport)
{
    ARG_UNUSED(rhport);

    CSK_USBC->INTRUSBE = usb_arcs_ctrl.intr_usbe;
    CSK_USBC->INTRTXE = usb_arcs_ctrl.intr_txe;
    CSK_USBC->INTRRXE = usb_arcs_ctrl.intr_rxe;

    // Enable global interrupt
    enable_IRQ(IRQ_USBC_VECTOR);

#if CONFIG_SOF_CNT
    enable_IRQ(IRQ_SOF_CNT_VECTOR);
#endif
}


// Disable device interrupt
void dcd_int_disable(uint8_t rhport)
{
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
