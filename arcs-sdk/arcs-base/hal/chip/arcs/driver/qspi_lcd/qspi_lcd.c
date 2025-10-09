/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */

#include "qspi_lcd.h"
#include "ClockManager.h" // for CRM_GetSrcFreq() etc.
#include "log_print.h"
#include <assert.h>

//----------------------------------------------------------------
// Whether output log information via UART port or RTT ICD
#define DEBUG_LOG   0 // 1

#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

//----------------------------------------------------------------


//----------------------------------------------------------------


//FIXME: move the line to DMA diver
#define DMA_MASTER_SEL_MAX	0 // DMAC has only 1 master port

//----------------------------------------------------------------


//----------------------------------------------------------------
#define CSK_QSPI_LCD_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

// driver version
static const
CSK_DRIVER_VERSION spi_driver_version = { CSK_QSPI_LCD_API_VERSION, CSK_QSPI_LCD_DRV_VERSION };

static void spi_irq_handler(SPI_DEV *spi);

//------------------------------------------------------------------------------------
// QSPI_LCD
static SPI_DEV qspi_lcd_dev;
_FAST_FUNC_RO static void qspi_lcd_irq_handler(void) { spi_irq_handler(&qspi_lcd_dev); }

_FAST_DATA_ZI static SPI_INFO qspi_lcd_info = { 0 };
_FAST_DATA_VI static SPI_DEV qspi_lcd_dev = {
        CSK_QSPI_LCD,
        CSK_SPI_TXFIFO_DEPTH,
        CSK_SPI_RXFIFO_DEPTH,
        IRQ_QSPI_LCD_VECTOR,
        0,
        qspi_lcd_irq_handler,

        .dma_tx = {
                DMA_HSID_SPI0_TX,
                DMA_CH_SPI_TX_DEF,
                1, 0,
                NULL,
                (uint32_t)(&qspi_lcd_dev)
        },

        &qspi_lcd_info
};

// export SPI API function: QSPI_LCD
void* QSPI_LCD() { return &qspi_lcd_dev; }


uint32_t QSPI_LCD_Buf(void)
{
    return CSK_QSPI_LCD_BUF;
}


static void ap_cfg_video_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<15);
}

static void ap_cfg_qspi1_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI1_CLK = 0x1;  // bit21
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<21);
}

static void ap_cfg_qspi1_clk_sel(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI1_CLK = 0x1;  // bit20  0:XTAL  1:syspll_peri_clk
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<20);
}

static void ap_cfg_qspi1_clk_div_m(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_M = 0x1;  // bit22~25
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0xF << 22);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0xF) << 22);
}

static void ap_cfg_qspi1_clk_div_n(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_N = 0x1;  // bit26~28
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0x7 << 26);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0x7) << 26);
}

static void ap_cfg_qspi1_clk_inv(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_LD = 0x1;  // bit29  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<29);
}

static void ap_cfg_dma_sel_qspi_out(void)
{
    IP_AP_CFG->REG_DMA_SEL.all |= (1<<0);      // bit0  0:rgb  1:qspi_out
    //IP_AP_CFG->REG_DMA_SEL.all |= (1<<2);      // bit2  0:rgb  1:qspi_out
}

static void ap_cfg_qspi1_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.QSPI1_RESET = 0x1;      // bit11
    IP_AP_CFG->REG_SW_RESET.all |= (1<<11);
}


static void QSPI_LCD_Reset(void)
{
    ap_cfg_video_clk_enable();
    ap_cfg_qspi1_clk_enable();

    ap_cfg_qspi1_clk_sel();
    ap_cfg_qspi1_clk_div_m(2);
    ap_cfg_qspi1_clk_div_n(1);
    ap_cfg_qspi1_clk_inv();

    ap_cfg_dma_sel_qspi_out();
    ap_cfg_qspi1_reset();
}


//------------------------------------------------------------------------------------

_FAST_FUNC_RO static SPI_DEV * safe_spi_dev(void *spi_dev)
{
    //TODO: safe check of SPI device parameter
	if (spi_dev == &qspi_lcd_dev) {
		if (qspi_lcd_dev.reg != CSK_QSPI_LCD || qspi_lcd_dev.irq_num != IRQ_QSPI_LCD_VECTOR) {
			//CLOGW("SPI0 device context has been tampered illegally!!\n");
			return NULL;
		}
	}else {
		return NULL;
	}

    return (SPI_DEV *)spi_dev;
}

//Get SPI device index, return 0, 1,..., and 0xFF if not existing.
uint8_t QSPI_LCD_Index(void *spi_dev)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return 0xFF;
    return (spi->spi_idx & 0xFF);
}

static inline uint16_t
spi_get_txfifo_size(SPI_DEV *spi)
{
    uint8_t size_shift = spi->reg->REG_CONFIG.bit.TXFIFOSIZE;
    LOGD("[%s:%d] size_shift=%d", __func__, __LINE__, size_shift);
    return (2 << size_shift);
}

static inline uint16_t
spi_get_rxfifo_size(SPI_DEV *spi)
{
    uint8_t size_shift = spi->reg->REG_CONFIG.bit.RXFIFOSIZE;
    return (2 << size_shift);
}

static inline void
spi_polling_spiactive(SPI_DEV *spi)
{
    while ((spi->reg->REG_STATUS.bit.SPIACTIVE) & 0x1)
        ;
}

static inline void
spi_set_tx_fifo_threshold(SPI_DEV *spi, uint8_t thres)
{

    spi->reg->REG_CTRL.bit.TXTHRES = thres;
}

static inline void
spi_set_rx_fifo_threshold(SPI_DEV *spi, uint8_t thres)
{
    spi->reg->REG_CTRL.bit.RXTHRES = thres;
}


// export SPI API function: SPI_GetVersion
CSK_DRIVER_VERSION
QSPI_LCD_GetVersion()
{
    return spi_driver_version;
}

static uint32_t SPI_CLK(SPI_DEV *spi)
{
    if (spi->reg == CSK_QSPI_LCD){
        return CRM_GetQspi1Freq();
    }
    else {
        LOGD("%s: invalid SPI device (reg = 0x%08x)!", __func__, spi->reg);
        return 0;
    }
}

static void spi_init_common(SPI_DEV *spi, QSPI_LCD_SignalEvent_t cb_event, uint32_t usr_param)
{
    // initialize SPI run-time resources
    spi->info->cb_event = cb_event;
    spi->info->usr_param = usr_param;

    spi->info->status.all = 0U;

    spi->info->xfer.rx_buf = 0U;
    spi->info->xfer.tx_buf = 0U;
    spi->info->xfer.rx_cnt = 0U;
    spi->info->xfer.tx_cnt = 0U;

    spi->info->data_bits = 8; //default 8 bit xfer on SPI cable

    //spi->info->txrx_mode = 0U;
    spi->info->txrx_mode = CSK_QSPI_LCD_TXIO_AUTO | CSK_QSPI_LCD_RXIO_AUTO;
    spi->info->flags = SPI_FLAG_INITIALIZED;
}


// export SPI API function: SPI_Initialize
int32_t
QSPI_LCD_Initialize(void *spi_dev, QSPI_LCD_SignalEvent_t cb_event, uint32_t usr_param)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (spi->info->flags & SPI_FLAG_INITIALIZED)
        return CSK_DRIVER_OK;

    QSPI_LCD_Reset();

    spi_init_common(spi, cb_event, usr_param);

    return CSK_DRIVER_OK;
}


// export SPI API function: SPI_Uninitialize
int32_t
QSPI_LCD_Uninitialize(void *spi_dev)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    //TODO: any other clean operations?

    // Power off SPI if Powered on
    if (spi->info->flags & SPI_FLAG_POWERED)
        QSPI_LCD_PowerControl(spi_dev, CSK_POWER_OFF);

    spi->info->flags = 0U; // SPI is uninitialized

    return CSK_DRIVER_OK;
}


// export SPI API function: SPI_PowerControl
int32_t
QSPI_LCD_PowerControl(void *spi_dev, CSK_POWER_STATE state)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint32_t val;
    switch (state) {
    case CSK_POWER_OFF:
        // disable SPI IRQ
        disable_IRQ(spi->irq_num);

        // reset SPI and TX/RX FIFOs
        spi->reg->REG_CTRL.bit.SPIRST = 1;
        while(spi->reg->REG_CTRL.bit.SPIRST);
        spi->reg->REG_CTRL.bit.TXFIFORST = 1;
        while(spi->reg->REG_CTRL.bit.TXFIFORST);
        spi->reg->REG_CTRL.bit.RXFIFORST = 1;
        while(spi->reg->REG_CTRL.bit.RXFIFORST);

        // disable SPI clock here
        if (spi->reg == CSK_QSPI_LCD) {
            __HAL_CRM_QSPI1_CLK_DISABLE();
        }else {
            LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        }

        spi->info->flags &= ~(SPI_FLAG_POWERED | SPI_FLAG_CONFIGURED);

        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if ((spi->info->flags & SPI_FLAG_INITIALIZED) == 0U)
            return CSK_DRIVER_ERROR;

        if ((spi->info->flags & SPI_FLAG_POWERED) != 0U)
            return CSK_DRIVER_OK;

        // enable SPI clock here
        if (spi->reg == CSK_QSPI_LCD) {
            __HAL_CRM_QSPI1_CLK_ENABLE();
        }else {
            LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        }

        //fast mode?
        spi->reg->REG_TRANSFMT.bit.FAST_MD = 0;

        // reset SPI and TX/RX FIFOs
        spi->reg->REG_CTRL.bit.SPIRST = 1;
        while(spi->reg->REG_CTRL.bit.SPIRST);
        spi->reg->REG_CTRL.bit.TXFIFORST = 1;
        while(spi->reg->REG_CTRL.bit.TXFIFORST);
        spi->reg->REG_CTRL.bit.RXFIFORST = 1;
        while(spi->reg->REG_CTRL.bit.RXFIFORST);

        spi->txfifo_depth = spi_get_txfifo_size(spi);
        spi->rxfifo_depth = spi_get_rxfifo_size(spi);

        LOGD("[%s:%d] spi->txfifo_depth=%d", __func__, __LINE__, spi->txfifo_depth);

        // set TX FIFO threshold and RX FIFO threshold to half of TX/RX FIFO depth
//        spi->reg->REG_CTRL.bit.TXTHRES = spi->txfifo_depth / 2;
//        spi->reg->REG_CTRL.bit.RXTHRES = spi->rxfifo_depth / 2;
        spi->reg->REG_CTRL.bit.TXTHRES = 1;
        spi->reg->REG_CTRL.bit.RXTHRES = 1;
        spi->info->tx_bsize_shift = ITEMS_TO_BSIZE(spi->txfifo_depth / 2); //DMA_BSIZE_4;
        spi->info->rx_bsize_shift = ITEMS_TO_BSIZE(spi->rxfifo_depth / 2); //DMA_BSIZE_4;

        // clear SPI run-time resources
        spi->info->status.all = 0U;
        spi->info->xfer.rx_buf = 0U;
        spi->info->xfer.tx_buf = 0U;
        spi->info->xfer.rx_cnt = 0U;
        spi->info->xfer.tx_cnt = 0U;

        //spi->info->txrx_mode = 0U;
        spi->info->flags |= SPI_FLAG_POWERED;   // SPI is powered
        register_ISR(spi->irq_num, spi->irq_handler, NULL); // register SPI's ISR
        enable_IRQ(spi->irq_num); // enable SPI IRQ

        break;

    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }
    return CSK_DRIVER_OK;
}


_FAST_FUNC_RO static uint32_t
spi_fill_tx_fifo(SPI_DEV *spi, uint32_t xn)
{
    uint32_t i, count, data, limit;
    i = SPI_TXFIFO_ENTRIES(spi);
    count = 0;

    limit = spi->txfifo_depth;

    //CLOG("[%s:%d] xn=%d i=%d limit=%d", __func__, __LINE__, xn, i, limit);
    //CLOG("[%s:%d] tx_cnt=%d req_tx_cnt=%d", __func__, __LINE__, spi->info->xfer.tx_cnt, spi->info->xfer.req_tx_cnt);

    while (i < limit && spi->info->xfer.tx_cnt < spi->info->xfer.req_tx_cnt) {
        data = 0;

        // handle the data frame format
        if (spi->info->data_bits <= 8) { // data bits = 1....8
            uint8_t *tx_buf8 = (uint8_t *)spi->info->xfer.tx_buf;
            data = tx_buf8[spi->info->xfer.tx_cnt++];
        } else if (spi->info->data_bits <= 16) { // data bits = 9....16
            uint16_t *tx_buf16 = (uint16_t *)spi->info->xfer.tx_buf;
            data = tx_buf16[spi->info->xfer.tx_cnt++];
        } else { // data bits = 17....32
            uint32_t *tx_buf32 = (uint32_t *)spi->info->xfer.tx_buf;
            data = tx_buf32[spi->info->xfer.tx_cnt++];
        }
        spi->reg->REG_DATA.all = data;
        i++;
        if (++count >= xn)
            break;
    }

    return count;
}


// export SPI API function: SPI_Send
//NOTE: start address (and size? NOT num!) of the buffer pointed by 'data'
//      SHOULD be aligned with 4 if SPI_FLAG_8BIT_MERGE is set!!
int32_t
QSPI_LCD_Send(void *spi_dev, const void *data, uint32_t num)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((data == NULL) || (num > MAX_TRANCNT))
        return CSK_DRIVER_ERROR_PARAMETER;

    if (num == 0U){
        spi->reg->REG_TRANSCTRL.bit.TRANSMODE = SPI_TRANSMODE_NONEDATA;
        spi->reg->REG_CMD.bit.CMD = spi->info->command;
        //Todo:fix return value
        return CSK_DRIVER_ERROR_PARAMETER;
    }else{
        spi->reg->REG_TRANSCTRL.bit.TRANSMODE = SPI_TRANSMODE_WRONLY;
    }

    if (!(spi->info->flags & SPI_FLAG_CONFIGURED))
        return CSK_DRIVER_ERROR;

    //CLOG("[%s:%d] busy=%d", __func__, __LINE__, spi->info->status.bit.busy);

    if (spi->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    // set busy flag
    spi->info->status.all = 0;
    spi->info->status.bit.busy = 1;
    spi->info->xfer.tx_buf = data;
    spi->info->xfer.tx_cnt = 0U;
    spi->info->xfer.req_tx_cnt = num;
    spi->info->xfer.cur_op = QSPI_LCD_SEND;

    // wait prior transfer finish
    spi_polling_spiactive(spi);

    // set transfer count for write data
    spi->reg->REG_LCD_TX.all = WR_TRANCNT(num);

    // set transfer mode to write only and transfer count for write data
    //spi->reg->REG_TRANSCTRL = (SPI_TRANSMODE_WRONLY | WR_TRANCNT(num) |
    //            (spi->reg->REG_TRANSCTRL & SPI_TRANSMODE_CMD_EN));

    //spi_set_tx_fifo_threshold(spi, spi->txfifo_depth >> 1);
    //spi_set_tx_fifo_threshold(spi, 1);
    //spi_set_tx_fifo_threshold(spi, num);
    //CLOG("[%s:%d] spi->txfifo_depth=%d", __func__, __LINE__, spi->txfifo_depth);

    uint32_t intren = 0, count = num;
    bool xfer_started = false;
    bool is_master = ((spi->info->txrx_mode & CSK_QSPI_LCD_MODE_Msk) == CSK_QSPI_LCD_MODE_MASTER);

    // DMA mode
    if(!xfer_started && TX_DMA(spi->info->txrx_mode)){
        //CLOG("[%s:%d] num=%d", __func__, __LINE__, num);

        // fill the TX FIFO in DMA mode, before DMA request
        //spi_fill_tx_fifo(spi, spi->txfifo_depth);
        //spi_fill_tx_fifo(spi, 1);

        spi_fill_tx_fifo(spi, num);

        // enable interrupts
        intren = (SPI_ENDINT);

        spi->info->status.bit.tx_mode = CSK_QSPI_LCD_DMA_IO;
        xfer_started = true;
    }

    if (!xfer_started && TX_PIO(spi->info->txrx_mode)) { // PIO TX (interrupt mode)
        // fill the TX FIFO if PIO mode
        //CLOG("[%s:%d] num=%d", __func__, __LINE__, num);

        spi_fill_tx_fifo(spi, spi->txfifo_depth >> 1); // /2

        // enable interrupts
        intren = (SPI_TXFIFOINT | SPI_ENDINT);

        spi->info->status.bit.tx_mode = CSK_QSPI_LCD_PIO_IO;
        xfer_started = true;
    }

    // neither DMA nor PIO transfer is started
    if (!xfer_started) {
        LOGD("%s: Neither DMA nor PIO is started!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    // enable interrupts
    // (Interrupt could be triggered soon once enabled, so enable interrupts at the end!!)
    spi->reg->REG_INTREN.all = intren;

    // trigger transfer when SPI master mode
    if (is_master) {
        // wait until there's at least 1 data in the TX FIFO??
        //while(SPI_TXFIFO_ENTRIES(spi) == 0);
        //while(SPI_TXFIFO_EMPTY(spi)) ;
        spi->reg->REG_CMD.bit.CMD = spi->info->command;
    }

    return CSK_DRIVER_OK;
}


// Wait for SPI operation (Send/Receive/Transfer) done...
void QSPI_LCD_Wait_Done(void *spi_dev)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);

    // wait until Active bit is cleared in STATUS register and ENDINT ISR is called
    //while ( (spi->reg->STATUS & 0x1) || spi->info->status.bit.busy);

    // wait ENDINT ISR is called if ENDINT is enabled, otherwise
    // wait all TX data is fetched from RAM into TX FIFO (NOT sent out on SPI data line), or
    // wait all RX data is fetched from RX FIFO into RAM (already received in on SPI data line)
#if 1
    while (spi->info->status.bit.busy);
#else
    uint32_t timeout = 60000;
    while (spi->info->status.bit.busy)
    {
        if (--timeout == 0)
        {
            CLOGE("QSPI LCD wait done timeout");
            break;
        }
    }
#endif
}


// meaningful only in SPI master mode
static bool spi_set_bus_speed(SPI_DEV *spi, uint32_t arg)
{
    assert(spi != NULL && arg != 0);

    uint32_t clk, cs2sclk;
    int32_t sclk_div;
    clk = SPI_CLK(spi);

    if (clk == arg) { // output = input
        sclk_div = 0xFF;
    } else {
        sclk_div = (clk / (2 * arg)) - 1;
        if (sclk_div >= 0xFF || sclk_div < 0) {
            CLOGW("%s: CANNOT support SCLK = %d (SPI_CLK = %d)! ", __func__, arg, clk);
            return false;
        }
        clk /= (sclk_div + 1) * 2;
    }

    //cs2sclk = 0;
    cs2sclk = (0x1 << 12); // 2 bits, can be 0x0 ~ 0x3

    spi->reg->REG_TIMING.all &= ~0x30ffUL;
    spi->reg->REG_TIMING.all |= (sclk_div & 0xff) | (cs2sclk & 0x3000);

    LOGD("%s: sclk_div = %d (sclk: %d), cs2sclk = %d\r\n",
        __func__, (spi->reg->TIMING & 0xFF), clk, ((spi->reg->TIMING >> 12) & 0x3));
    return true;
}


static inline int32_t spi_get_bus_speed(SPI_DEV *spi)
{
    assert(spi != NULL);
    uint32_t sclk_div, spi_clk;
    spi_clk = SPI_CLK(spi);
    sclk_div = spi->reg->REG_TIMING.bit.SCLK_DIV;
    // NO divider when sclk_div is 0xFF
    return ( sclk_div == 0xFF ? spi_clk : spi_clk / ((sclk_div + 1) * 2));
}


// export SPI API function: SPI_Control
int32_t
QSPI_LCD_Control(void *spi_dev, uint32_t control, uint32_t arg)
{
    uint32_t val, format, transctl=0;
    SPI_DEV *spi = safe_spi_dev(spi_dev);

    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(spi->info->flags & SPI_FLAG_POWERED))
        return CSK_DRIVER_ERROR;

    // exclusive MISC OP
    switch (control & CSK_QSPI_LCD_EXCL_OP_Msk) {
        // NO MISC OP
        case CSK_QSPI_LCD_EXCL_OP_UNSET:
            break;

        // set bus speed in bps; arg = value
        case CSK_QSPI_LCD_SET_BUS_SPEED:
            if (arg == 0U)
                return CSK_DRIVER_ERROR;
            return (spi_set_bus_speed(spi, arg) ? CSK_DRIVER_OK : CSK_DRIVER_ERROR);

        // get bus speed in bps
        case CSK_QSPI_LCD_GET_BUS_SPEED:
            return spi_get_bus_speed(spi);

        // reset SPI RX/TX FIFO
        case CSK_QSPI_LCD_RESET_FIFO:
        {
            spi->reg->REG_CTRL.bit.TXFIFORST = 1;
            spi->reg->REG_CTRL.bit.RXFIFORST = 1;
            return CSK_DRIVER_OK;
        }

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    } // end exclusive MISC_OP

    if (spi->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    // SPI Mode
    switch (control & CSK_QSPI_LCD_MODE_Msk) {
    // Keep mode unchanged
    case CSK_QSPI_LCD_MODE_UNSET:
        break;

    // SPI master (output on MOSI, input on MISO); arg = bus speed in bps
    case CSK_QSPI_LCD_MODE_MASTER:
        // set master mode and disable data merge mode
        spi->reg->REG_TRANSFMT.bit.DATAMERGE = 0;
        spi->reg->REG_TRANSFMT.bit.SLVMODE = 0; //note: 0:master mode   1: slave mode
        spi->info->txrx_mode &= ~CSK_QSPI_LCD_MODE_Msk;
        spi->info->txrx_mode |= CSK_QSPI_LCD_MODE_MASTER;
        spi->info->flags |= SPI_FLAG_CONFIGURED;
        if (arg != 0) {
            if (!spi_set_bus_speed(spi, arg))
                return CSK_DRIVER_ERROR_PARAMETER;
        }

        if(control & CSK_QSPI_LCD_CMD_PHASE_Msk){
            //master mode only
            spi->reg->REG_TRANSCTRL.bit.CMDEN = 1;
        }else{
            spi->reg->REG_TRANSCTRL.bit.CMDEN = 0;
        }
        if(control & CSK_QSPI_LCD_ADDR_PHASE_Msk){
            //master mode only
            spi->reg->REG_TRANSCTRL.bit.ADDREN = 1;
            spi->reg->REG_TRANSCTRL.bit.ADDRFMT = (control & CSK_QSPI_LCD_ADDR_PHASE_FMT_Msk)>>CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos;
        }else{
            spi->reg->REG_TRANSCTRL.bit.ADDREN = 0;
        }
        break;

    // SPI slave (output on MISO, input on MOSI)
    case CSK_QSPI_LCD_MODE_SLAVE:
        // set slave mode and disable data merge mode
        spi->reg->REG_TRANSFMT.bit.SLVMODE = 1; //note: 0:master mode   1: slave mode
        spi->info->txrx_mode &= ~CSK_QSPI_LCD_MODE_Msk;
        spi->info->txrx_mode |= CSK_QSPI_LCD_MODE_SLAVE;
        spi->info->flags |= SPI_FLAG_CONFIGURED;
        break;

    default:
        return CSK_QSPI_LCD_ERROR_MODE;
    } // end SPI Mode

    // SPI TX IO
    val = control & CSK_QSPI_LCD_TXIO_Msk;
    spi->info->txrx_mode &= ~CSK_QSPI_LCD_TXIO_Msk;
    spi->info->txrx_mode |= val;
    switch (val) {
    case CSK_QSPI_LCD_TXIO_UNSET:
        break;
    case CSK_QSPI_LCD_TXIO_DMA:
        spi->reg->REG_CTRL.bit.TXDMAEN = 1;
        break;
    case CSK_QSPI_LCD_TXIO_PIO:
    case CSK_QSPI_LCD_TXIO_BOTH:
        spi->reg->REG_CTRL.bit.TXDMAEN = 0;
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // SPI RX IO
    val = control & CSK_QSPI_LCD_RXIO_Msk;
    spi->info->txrx_mode &= ~CSK_QSPI_LCD_RXIO_Msk;
    spi->info->txrx_mode |= val;
    switch (val) {
    case CSK_QSPI_LCD_RXIO_UNSET:
        break;
    case CSK_QSPI_LCD_RXIO_DMA:
        spi->reg->REG_CTRL.bit.RXDMAEN = 1;
        break;
    case CSK_QSPI_LCD_RXIO_PIO:
    case CSK_QSPI_LCD_RXIO_BOTH:
        spi->reg->REG_CTRL.bit.RXDMAEN = 0;
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // SPI frame format
    switch (control & CSK_QSPI_LCD_FRAME_FORMAT_Msk) {
    case CSK_QSPI_LCD_FRM_FMT_UNSET:
        break;
    case CSK_QSPI_LCD_CPOL0_CPHA0:
        spi->reg->REG_TRANSFMT.bit.CPOL = 0;
        spi->reg->REG_TRANSFMT.bit.CPHA = 0;
        break;
    case CSK_QSPI_LCD_CPOL0_CPHA1:
        spi->reg->REG_TRANSFMT.bit.CPOL = 0;
        spi->reg->REG_TRANSFMT.bit.CPHA = 1;
        break;
    case CSK_QSPI_LCD_CPOL1_CPHA0:
        spi->reg->REG_TRANSFMT.bit.CPOL = 1;
        spi->reg->REG_TRANSFMT.bit.CPHA = 0;
        break;
    case CSK_QSPI_LCD_CPOL1_CPHA1:
        spi->reg->REG_TRANSFMT.bit.CPOL = 1;
        spi->reg->REG_TRANSFMT.bit.CPHA = 1;
        break;
    default:
        return CSK_QSPI_LCD_ERROR_FRAME_FORMAT;
    }

    // SPI Data Bits
    val = ((control & CSK_QSPI_LCD_DATA_BITS_Msk) >> CSK_QSPI_LCD_DATA_BITS_Pos);
    if (val != 0) {
        if ((val < 1U) || (val > 32U)) {
            return CSK_QSPI_LCD_ERROR_DATA_BITS;
        } else {
            spi->info->data_bits = val;
            spi->info->flags &= ~SPI_FLAG_8BIT_MERGE;
            spi->reg->REG_TRANSFMT.bit.DATAMERGE = 0;
        }
        spi->reg->REG_TRANSFMT.bit.DATALEN = DATA_BITS(val);
    } else {
        if (spi->info->data_bits == 0) {
            //CLOGW("%s: DATA_BITS NOT Set!!\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // SPI Bit Order
    switch (control & CSK_QSPI_LCD_BIT_ORDER_Msk) {
    case CSK_QSPI_LCD_BIT_ORDER_UNSET:
        break;
    case CSK_QSPI_LCD_LSB_MSB:
        spi->reg->REG_TRANSFMT.bit.LSB = 1;
        break;
    case CSK_QSPI_LCD_MSB_LSB:
        spi->reg->REG_TRANSFMT.bit.LSB = 0;
        break;
    default:
        return CSK_QSPI_LCD_ERROR_BIT_ORDER;
    }

    return CSK_DRIVER_OK;
}


int32_t
QSPI_LCD_SetDMASize(void *spi_dev, uint32_t num)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);

    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    spi->reg->REG_LCD_TX.all = num;            // byte
    spi->reg->REG_TRANSCTRL.bit.RDTRANCNT = 511;
    spi->reg->REG_TRANSCTRL.bit.WRTRANCNT = 511;
    spi->reg->REG_CMD.all = 0x12;

    return CSK_DRIVER_OK;
}


int32_t
QSPI_LCD_DMAEnable(void *spi_dev)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);

    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    spi->reg->REG_TRANSFMT.bit.DATAMERGE = 1;    // data merge enable
    spi->reg->REG_CTRL.bit.RXFIFORST = 1;
    spi->reg->REG_CTRL.bit.TXFIFORST = 1;
    spi->reg->REG_CTRL.bit.TXDMAEN = 1;          // DMA enable

    return CSK_DRIVER_OK;
}


int32_t
QSPI_LCD_DMADisable(void *spi_dev)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);

    if (spi == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    spi->reg->REG_LCD_TX.all = 0;                // byte
    spi->reg->REG_TRANSFMT.bit.DATAMERGE = 0;    // data merge disable
    spi->reg->REG_CTRL.bit.TXDMAEN = 0;          // DMA disable
    spi->reg->REG_CTRL.bit.RXFIFORST = 1;
    spi->reg->REG_CTRL.bit.TXFIFORST = 1;
    spi->reg->REG_CTRL.bit.SPIRST = 1;

    return CSK_DRIVER_OK;
}


// export SPI API function: SPI_GetStatus
int32_t
QSPI_LCD_GetStatus(void *spi_dev, CSK_QSPI_LCD_STATUS *status)
{
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL || status == NULL) {
        LOGD("%s: invalid SPI device (0x%08x)!", __func__, spi_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    status->all = spi->info->status.all;
    return CSK_DRIVER_OK;
}


_FAST_FUNC_RO static void
spi_irq_handler(SPI_DEV *spi)
{
    uint32_t i = 0;
    uint32_t status = 0;
    uint32_t event = 0;

    // read status register
    status = spi->reg->REG_INTRST.all;
    //CLOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    // RX FIFO overrun
    if (status & SPI_RXFIFOORINT)
    {
        spi->reg->REG_INTREN.bit.RXFIFOORINTEN = 0;
        spi->info->status.bit.data_ovf = 1U;
        event |= CSK_QSPI_LCD_EVENT_DATA_LOST;
    }

    // TX FIFO underrun
    if (status & SPI_TXFIFOURINT)
    {
        spi->reg->REG_INTREN.bit.TXFIFOURINTEN = 0;
        spi->info->status.bit.data_unf = 1U;
        event |= CSK_QSPI_LCD_EVENT_DATA_LOST;
    }

    if (status & SPI_TXFIFOINT)
    {
        spi_fill_tx_fifo(spi, spi->txfifo_depth >> 1); //
        //CLOG("[%s:%d] tx_cnt=%d req_tx_cnt=%d", __func__, __LINE__, spi->info->xfer.tx_cnt, spi->info->xfer.req_tx_cnt);

        if (spi->info->xfer.tx_cnt == spi->info->xfer.req_tx_cnt)
        {
            spi->reg->REG_INTREN.bit.TXFIFOINTEN = 0;
            spi->reg->REG_INTREN.bit.TXFIFOURINTEN = 0;
            spi->info->status.bit.tx_mode = CSK_QSPI_LCD_NO_IO;
        }
    }

    if (status & SPI_ENDINT)
    {
        volatile uint32_t count;

        // disable SPI interrupts
        spi->reg->REG_INTREN.all = 0;

        //BSD: clear the Ready bit in the SPI Slave Status Register
        if((spi->info->txrx_mode & CSK_QSPI_LCD_MODE_Msk) == CSK_QSPI_LCD_MODE_SLAVE)
        {
            spi->reg->REG_SLVST.bit.READY = 0;
        }

        if (spi->info->xfer.cur_op != QSPI_LCD_RECEIVE)    // QSPI_LCD_SEND
        {
            // Check if there is remaining data left in TX FIFO!!
            i = SPI_TXFIFO_ENTRIES(spi);
            if (i > 0)
            {
                // The Data remained in TX FIFO is deducted from the total xfer count
                spi->info->xfer.tx_cnt -= i;
                LOGD("%s: Warning!! %d Data items remain in TX FIFO!\n", __func__, i);
            }

            if ((spi->info->txrx_mode & CSK_QSPI_LCD_MODE_Msk) == CSK_QSPI_LCD_MODE_SLAVE)   // && spi->info->data_bits > 1
            {
                count = SLV_WRCNT(spi);
                CLR_SLV_WRCNT(spi); // clear WCnt on updated IP
                LOGD("%s: SLVDATACNT.WCnt = %d Data\n", __func__, count);
                spi->info->xfer.tx_cnt = count;
            }

            if (spi->info->xfer.tx_cnt < spi->info->xfer.req_tx_cnt)
            {
                LOGD("%s: Warning!! %d Data items have not been sent!\n",
                        __func__, spi->info->xfer.req_tx_cnt - spi->info->xfer.tx_cnt);
            }
        }

        // clear TX/RX FIFOs
//        spi->reg->REG_CTRL.bit.TXDMAEN = 0;
//        spi->reg->REG_CTRL.bit.RXDMAEN = 0;
        spi->reg->REG_CTRL.bit.TXFIFORST = 1;
        spi->reg->REG_CTRL.bit.RXFIFORST = 1;

        spi->info->status.bit.busy = 0;
        spi->info->status.bit.rx_sync = 0;
        spi->info->status.bit.tx_mode = CSK_QSPI_LCD_NO_IO;
        spi->info->status.bit.rx_mode = CSK_QSPI_LCD_NO_IO;
        event |= CSK_QSPI_LCD_EVENT_TRANSFER_COMPLETE;
    }

    // clear interrupt status
    spi->reg->REG_INTRST.all = status;
    // make sure "write 1 clear" take effect before iret
    //spi->reg->REG_INTRST.all;

    if ((spi->info->cb_event != NULL) && (event != 0))
    {
        // disable SPI interrupts //FIXME: remove it??
        if (event & CSK_QSPI_LCD_EVENT_TRANSFER_COMPLETE)
        {
            spi->reg->REG_INTREN.all = 0;
        }

        spi->info->cb_event(event, spi->info->usr_param);
    }
}


//lane_num: CSK_QSPI_LCD_LANE_NUM_QUAD/ CSK_QSPI_LCD_LANE_NUM_DUAL/ CSK_QSPI_LCD_LANE_NUM_QUAD
uint32_t
QSPI_LCD_SetLaneNum(void *spi_dev, em_CSK_QSPI_LCD_LANE_NUM lane_num){
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    spi->reg->REG_TRANSCTRL.bit.DUALQUAD = lane_num;

    return CSK_DRIVER_OK;
}


uint32_t
QSPI_LCD_SetDatLength(void *spi_dev, uint8_t length){
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    spi->reg->REG_TRANSFMT.bit.DATALEN = DATA_BITS(length);

    spi->info->data_bits = length;
    if(length > 16){
        spi->info->dma_width_shift = DMA_WIDTH_WORD;
    }else{
        spi->info->dma_width_shift = DMA_WIDTH_BYTE;
    }
    return CSK_DRIVER_OK;
}


uint32_t
QSPI_LCD_SetLcdMode(void *spi_dev, em_CSK_QSPI_LCD_Mode mode){
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    spi->reg->REG_CTRL.bit.LCD_MODE = mode;

    return CSK_DRIVER_OK;
}


uint32_t
QSPI_LCD_SetDmaMode(void *spi_dev, uint8_t mode, uint8_t enable){
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint32_t val;

    if(mode == 0){
        spi->reg->REG_CTRL.bit.TXDMAEN = enable;
        if(enable){
            // SPI TX DMA
            val = CSK_QSPI_LCD_TXIO_DMA & CSK_QSPI_LCD_TXIO_Msk;
            spi->info->txrx_mode &= ~CSK_QSPI_LCD_TXIO_Msk;
            spi->info->txrx_mode |= val;
        }else{
            val = CSK_QSPI_LCD_TXIO_PIO & CSK_QSPI_LCD_TXIO_Msk;
            spi->info->txrx_mode &= ~CSK_QSPI_LCD_TXIO_Msk;
            spi->info->txrx_mode |= val;
        }
    }else if(mode == 1){
        spi->reg->REG_CTRL.bit.RXDMAEN = enable;
    }else if(mode == 2){
        spi->reg->REG_CTRL.bit.TXDMAEN = enable;
        spi->reg->REG_CTRL.bit.RXDMAEN = enable;
    }else{
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    return CSK_DRIVER_OK;
}


uint32_t
QSPI_LCD_GetDmaMode(void *spi_dev){
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    return (spi->info->txrx_mode & CSK_QSPI_LCD_TXIO_Msk);
}


uint32_t
QSPI_LCD_FifoReset(void *spi_dev){
    SPI_DEV *spi = safe_spi_dev(spi_dev);
    if (spi == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    spi->reg->REG_CTRL.bit.SPIRST = 1;
    while(spi->reg->REG_CTRL.bit.SPIRST);
    spi->reg->REG_CTRL.bit.TXFIFORST = 1;
    while(spi->reg->REG_CTRL.bit.TXFIFORST);
    spi->reg->REG_CTRL.bit.RXFIFORST = 1;
    while(spi->reg->REG_CTRL.bit.RXFIFORST);

    return CSK_DRIVER_OK;
}




