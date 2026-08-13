/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 * @file     qspi_out.c
 * @author
 * @brief    QSPI Output Driver Implementation
 *           This file contains the implementation of the QSPI output driver,
 *           including initialization, configuration, and operational functions.
 */

#include "qspi_out.h"
#include "ClockManager.h" // for CRM_GetSrcFreq() etc.
#include "log_print.h"
#include <assert.h>

//----------------------------------------------------------------
// Whether output log information via UART port or RTT ICD
#define DEBUG_LOG   1

#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)   ((void)0)
#endif // DEBUG_LOG

//----------------------------------------------------------------

#define CSK_QSPI_OUT_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

// driver version
static const
CSK_DRIVER_VERSION spi_driver_version = { CSK_QSPI_OUT_API_VERSION, CSK_QSPI_OUT_DRV_VERSION };

_FAST_FUNC_RO static void QSPI_OUT_IRQ_Handler(void);

//------------------------------------------------------------------------------------

_FAST_DATA_VI static QSPI_OUT_DEV qspi_out_dev = {
        .Instance = ((QSPI_LCD_RegDef *)  QSPI_OUT_BASE),
        .cb_event = NULL,
        .usr_param = 0,
        .state = QSPI_OUT_STATE_RESET,
        .txfifo_depth = QSPI_OUT_TXFIFO_DEPTH,
        .data_bits = 8,
        .tx_buf = NULL,
        .tx_cnt = 0,
        .req_tx_cnt = 0
};


// export SPI API function: QSPI_OUT
/**
 * @brief Get QSPI output device instance
 *  @return Pointer to QSPI output device structure
 */
void* QSPI_OUT()
{
    return &qspi_out_dev;
}

/**
 * @brief Get QSPI output buffer address
 *  @return Base address of QSPI output buffer
 */
uint32_t QSPI_OUT_Buf(void)
{
    return QSPI_OUT_TX_BUF;
}

/**
 * @brief Reset QSPI output peripheral
 *  This function performs necessary clock and reset configurations for QSPI output
 */
static inline void QSPI_OUT_Reset(void)
{
    __HAL_CRM_QSPI1_CLK_ENABLE();  //IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_QSPI1_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.QSPI1_RESET = 1;

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_QSPI1_CLK      = 1; // bit 10~10  0:24MHz  1:syspll peri clk
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_M    = 1; // bit 12~15
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_N    = 1; // bit 16~18
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_LD   = 1; // bit 11~11
}

//------------------------------------------------------------------------------------

/**
 * @brief Safely validate SPI device pointer
 *  @param[in] spi_dev Pointer to SPI device structure
 *  @return Valid SPI device pointer or NULL if invalid
 */
_FAST_FUNC_RO static QSPI_OUT_DEV * safe_spi_dev(void *spi_dev)
{
    if (!spi_dev)
        return NULL;

    QSPI_OUT_DEV *dev = (QSPI_OUT_DEV *)spi_dev;
    if (dev->Instance != ((QSPI_LCD_RegDef *)(QSPI_OUT_BASE))) {
        DEV_LOG("[%s:%d] Error spi_dev=%#x ", __func__, __LINE__, spi_dev);
        return NULL;
    }
    return dev;
}


/**
 * @brief Get driver version information
 *  @return Driver version structure
 */
// export SPI API function: SPI_GetVersion
CSK_DRIVER_VERSION QSPI_OUT_GetVersion()
{
    return spi_driver_version;
}


/**
 * @brief Initialize QSPI output interface
 *  @param[in] pDev Pointer to SPI device structure
 *  @param[in] callback Event callback function
 *  @param[in] param User parameter passed to callback
 *  @return CSK_DRIVER_OK on success, error code otherwise
 */
// export SPI API function: SPI_Initialize
int32_t QSPI_OUT_Initialize(void *pDev, QSPI_OUT_SignalEvent_t callback, uint32_t param)
{
    QSPI_OUT_DEV *spi = safe_spi_dev(pDev);
    if (spi == NULL) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    QSPI_OUT_Reset();

    // set TX FIFO threshold and RX FIFO threshold to half of TX/RX FIFO depth
    spi->Instance->REG_CTRL.all &= (~(QSPI_LCD_CTRL_TXTHRES_Msk | QSPI_LCD_CTRL_RXTHRES_Msk));
    spi->Instance->REG_CTRL.all |= (8 << QSPI_LCD_CTRL_TXTHRES_Pos) | (1 << QSPI_LCD_CTRL_RXTHRES_Pos);

    // set master mode and disable data merge mode
    // SLVMODE= 0:master mode   1: slave mode
    // write only
    spi->Instance->REG_TRANSCTRL.all &= (~(QSPI_LCD_TRANSFMT_DATAMERGE_Msk | QSPI_LCD_TRANSFMT_SLVMODE_Msk | \
            QSPI_LCD_TRANSCTRL_CMDEN_Msk | QSPI_LCD_TRANSCTRL_ADDREN_Msk | QSPI_LCD_TRANSCTRL_TRANSMODE_Msk | \
            QSPI_LCD_TRANSCTRL_WRTRANCNT_Msk | QSPI_LCD_TRANSCTRL_RDTRANCNT_Msk));
    spi->Instance->REG_TRANSCTRL.all |= QSPI_LCD_TRANSCTRL_WRTRANCNT_Msk | QSPI_LCD_TRANSCTRL_RDTRANCNT_Msk | (1 << QSPI_LCD_TRANSCTRL_TRANSMODE_Pos);

    /* IRQ disable */
    spi->Instance->REG_INTREN.all = 0;

    /* IRQ clear */
    spi->Instance->REG_INTRST.all = QSPI_LCD_INTRST_ENDINT_Msk | QSPI_LCD_INTRST_TXFIFOINT_Msk | QSPI_LCD_INTRST_RXFIFOINT_Msk;

    // initialize SPI run-time resources
    spi->cb_event = callback;
    spi->usr_param = param;
    spi->state = QSPI_OUT_STATE_READY;
    spi->txfifo_depth = QSPI_OUT_TXFIFO_DEPTH;
    spi->data_bits = 8;
    spi->tx_buf = NULL;
    spi->tx_cnt = 0;
    spi->req_tx_cnt = 0;

    register_ISR(IRQ_QSPI_OUT_VECTOR, QSPI_OUT_IRQ_Handler, NULL); // register SPI's ISR
    clear_IRQ(IRQ_QSPI_OUT_VECTOR);
    enable_IRQ(IRQ_QSPI_OUT_VECTOR); // enable SPI IRQ

    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitialize QSPI output interface
 *  @param[in] pDev Pointer to SPI device structure
 *  @return CSK_DRIVER_OK on success, error code otherwise
 */
// export SPI API function: SPI_Uninitialize
int32_t QSPI_OUT_Uninitialize(void *pDev)
{
    QSPI_OUT_DEV *spi = safe_spi_dev(pDev);
    if (spi == NULL) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* IRQ disable */
    spi->Instance->REG_INTREN.all = 0;

    /* IRQ clear */
    spi->Instance->REG_INTRST.all = QSPI_LCD_INTRST_ENDINT_Msk | QSPI_LCD_INTRST_TXFIFOINT_Msk | QSPI_LCD_INTRST_RXFIFOINT_Msk;

    // disable SPI IRQ
    disable_IRQ(IRQ_QSPI_OUT_VECTOR);
    clear_IRQ(IRQ_QSPI_OUT_VECTOR);

    QSPI_OUT_Reset();
    spi->state = QSPI_OUT_STATE_RESET; // SPI is uninitialized

    return CSK_DRIVER_OK;
}


/**
 * @brief Fill transmit FIFO with data
 *  @param[in] spi Pointer to SPI device structure
 *  @param[in] xn Number of items to fill
 *  @return Number of items actually filled
 */
_FAST_FUNC_RO static uint32_t spi_fill_tx_fifo(QSPI_OUT_DEV *spi, uint32_t xn)
{
    uint32_t count = 0;
    uint32_t data = 0;
    uint8_t i = spi->Instance->REG_STATUS.bit.TXNUM;  // the remaining data left in TX FIFO

//    DEV_LOG("[%s:%d] xn=%d i=%d limit=%d", __func__, __LINE__, xn, i, spi->txfifo_depth);
//    DEV_LOG("[%s:%d] tx_cnt=%d req_tx_cnt=%d", __func__, __LINE__, spi->tx_cnt, spi->req_tx_cnt);

    while (i < spi->txfifo_depth && spi->tx_cnt < spi->req_tx_cnt)
    {
        if (spi->data_bits <= 8) { // data bits = 1....8
            uint8_t *tx_buf8 = (uint8_t *)spi->tx_buf;
            data = tx_buf8[spi->tx_cnt++];
        } else if (spi->data_bits <= 16) { // data bits = 9....16
            uint16_t *tx_buf16 = (uint16_t *)spi->tx_buf;
            data = tx_buf16[spi->tx_cnt++];
        } else { // data bits = 17....32
            uint32_t *tx_buf32 = (uint32_t *)spi->tx_buf;
            data = tx_buf32[spi->tx_cnt++];
        }
        spi->Instance->REG_DATA.all = data;
        i++;
        if (++count >= xn)
            break;
    }

    return count;
}

/**
 * @brief Send data through QSPI interface
 *  @param[in] pDev Pointer to SPI device structure
 *  @param[in] data Pointer to data buffer
 *  @param[in] num Number of bytes to send
 *  @return CSK_DRIVER_OK on success, error code otherwise
 */
// export SPI API function: SPI_Send
//NOTE: start address (and size? NOT num!) of the buffer pointed by 'data'
int32_t QSPI_OUT_Send(void *pDev, const void *data, uint32_t num)
{
    QSPI_OUT_DEV *spi = safe_spi_dev(pDev);
    if (spi == NULL) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((data == NULL) || (num == 0))
        return CSK_DRIVER_ERROR_PARAMETER;

    if (spi->state != QSPI_OUT_STATE_READY)
        return CSK_DRIVER_ERROR;

    // set busy flag
    spi->state = QSPI_OUT_STATE_BUSY;
    spi->tx_buf = data;
    spi->tx_cnt = 0;
    spi->req_tx_cnt = num;

    /* data merge disable */
    spi->Instance->REG_TRANSFMT.all &= (~QSPI_LCD_TRANSFMT_DATAMERGE_Msk);

    /* write only */
    spi->Instance->REG_TRANSCTRL.all &= (~QSPI_LCD_TRANSCTRL_TRANSMODE_Msk);
    spi->Instance->REG_TRANSCTRL.all |= (1 << QSPI_LCD_TRANSCTRL_TRANSMODE_Pos);

    /* irq send, DMA disable */
    spi->Instance->REG_CTRL.all &= (~QSPI_LCD_CTRL_TXDMAEN_Msk);

    /* reset and fifo clear */
    spi->Instance->REG_CTRL.all |= QSPI_LCD_CTRL_TXFIFORST_Msk | QSPI_LCD_CTRL_RXFIFORST_Msk | QSPI_LCD_CTRL_SPIRST_Msk;

    // wait prior transfer finish
    //while ((spi->Instance->REG_STATUS.bit.SPIACTIVE) & 0x1);

    // set transfer count for write data
    spi->Instance->REG_LCD_TX.all = (num - 1);          // byte

    // fill the TX FIFO
    spi_fill_tx_fifo(spi, spi->txfifo_depth >> 1);

    /* IRQ enable */
    spi->Instance->REG_INTREN.all = QSPI_LCD_INTREN_ENDINTEN_Msk | QSPI_LCD_INTREN_TXFIFOINTEN_Msk;

    // trigger transfer when SPI master mode
    spi->Instance->REG_CMD.all = 0x00;

    return CSK_DRIVER_OK;
}

/**
 * @brief Set SPI bus speed
 *  @param[in] spi Pointer to SPI device structure
 *  @param[in] freq Desired bus speed in bps
 *  @return true if successful, false otherwise
 */
static bool qspi_set_bus_speed(QSPI_OUT_DEV *spi, uint32_t freq)
{
    int32_t sclk_div = 0;  // 0~0xFF
    uint32_t clk = CRM_GetQspi1Freq();

    if (clk == freq) { // output = input
        sclk_div = 0xFF;
    } else {
        sclk_div = (clk / (2 * freq)) - 1;
        if ((sclk_div >= 0xFF) || (sclk_div < 0)) {
            DEV_LOG("%s: CANNOT support SCLK = %d (SPI_CLK = %d)! ", __func__, freq, clk);
            return false;
        }
        clk /= ((sclk_div + 1) * 2);
    }

    spi->Instance->REG_TIMING.all &= (~(QSPI_LCD_TIMING_SCLK_DIV_Msk | QSPI_LCD_TIMING_CS2SCLK_Msk));
    spi->Instance->REG_TIMING.all |= (sclk_div & QSPI_LCD_TIMING_SCLK_DIV_Msk) | ((1 << QSPI_LCD_TIMING_CS2SCLK_Pos) & QSPI_LCD_TIMING_CS2SCLK_Msk);

//    DEV_LOG("%s: sclk_div=%d cs2sclk=%d clk_out=%dHz", __func__, (spi->Instance->REG_TIMING.all & QSPI_LCD_TIMING_SCLK_DIV_Msk), \
//            ((spi->Instance->REG_TIMING.all & QSPI_LCD_TIMING_CS2SCLK_Msk) >> QSPI_LCD_TIMING_CS2SCLK_Pos), clk);

    return true;
}

/**
 * @brief Control SPI interface parameters
 *  @param[in] pDev Pointer to SPI device structure
 *  @param[in] control Control flags
 *  @param[in] arg Control argument
 *  @return CSK_DRIVER_OK on success, error code otherwise
 */
// export SPI API function: SPI_Control
int32_t QSPI_OUT_Control(void *pDev, QSPI_OUT_emControl control, uint32_t arg)
{
    QSPI_OUT_DEV *spi = safe_spi_dev(pDev);

    if (spi == NULL) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (control & QSPI_OUT_CONTROL_CLK_OUT_HZ) {
        if ((arg == 0) || (qspi_set_bus_speed(spi, arg) == false)) {
            DEV_LOG("[%s:%d] Error input: set clk out is %d, must be 200000~100000000Hz", __func__, __LINE__, arg);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    if (control & QSPI_OUT_CONTROL_BURST_THD) {
        if((arg == 8) || (arg == 4) || (arg == 1))  {
            spi->Instance->REG_CTRL.all &= (~QSPI_LCD_CTRL_TXTHRES_Msk);
            spi->Instance->REG_CTRL.all |= ((arg << QSPI_LCD_CTRL_TXTHRES_Pos) & QSPI_LCD_CTRL_TXTHRES_Msk);
        } else {
            DEV_LOG("[%s:%d] Error input: burst thd is %d, must be 1/4/8", __func__, __LINE__, arg);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    if (control & QSPI_OUT_CONTROL_DMA_SIZE) {
        if (arg) {
            spi->tx_buf = NULL;
            spi->tx_cnt = 0;
            spi->req_tx_cnt = 0;

            spi->Instance->REG_TRANSFMT.all |= QSPI_LCD_TRANSFMT_DATAMERGE_Msk;
            spi->Instance->REG_CTRL.all |= QSPI_LCD_CTRL_TXDMAEN_Msk | QSPI_LCD_CTRL_TXFIFORST_Msk | QSPI_LCD_CTRL_RXFIFORST_Msk | QSPI_LCD_CTRL_SPIRST_Msk;

            spi->Instance->REG_LCD_TX.all = arg;            // byte
            spi->Instance->REG_INTREN.all = QSPI_LCD_INTREN_ENDINTEN_Msk;
            spi->Instance->REG_CMD.all = 0x12;
        } else {
            DEV_LOG("[%s:%d] Error input: dma size is 0", __func__, __LINE__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    if (control & QSPI_OUT_CONTROL_FIFO_CLEAR) {
        spi->Instance->REG_CTRL.all |= QSPI_LCD_CTRL_TXFIFORST_Msk | QSPI_LCD_CTRL_RXFIFORST_Msk | QSPI_LCD_CTRL_SPIRST_Msk;
    }

    if (control & QSPI_OUT_CONTROL_DATA_1LANE) {
        spi->Instance->REG_TRANSCTRL.bit.DUALQUAD = QSPI_OUT_LANE_NUM_SINGLE;
    } else if(control & QSPI_OUT_CONTROL_DATA_2LANE) {
        spi->Instance->REG_TRANSCTRL.bit.DUALQUAD = QSPI_OUT_LANE_NUM_DUAL;
    } else if(control & QSPI_OUT_CONTROL_DATA_4LANE) {
        spi->Instance->REG_TRANSCTRL.bit.DUALQUAD = QSPI_OUT_LANE_NUM_QUAD;
    } else {
    }

    if (control & QSPI_OUT_CONTROL_CPOL0_CPHA0) {
        spi->Instance->REG_TRANSFMT.bit.CPOL = 0;
        spi->Instance->REG_TRANSFMT.bit.CPHA = 0;
    } else if(control & QSPI_OUT_CONTROL_CPOL0_CPHA1) {
        spi->Instance->REG_TRANSFMT.bit.CPOL = 0;
        spi->Instance->REG_TRANSFMT.bit.CPHA = 1;
    } else if(control & QSPI_OUT_CONTROL_CPOL1_CPHA0) {
        spi->Instance->REG_TRANSFMT.bit.CPOL = 1;
        spi->Instance->REG_TRANSFMT.bit.CPHA = 0;
    } else if(control & QSPI_OUT_CONTROL_CPOL1_CPHA1) {
        spi->Instance->REG_TRANSFMT.bit.CPOL = 1;
        spi->Instance->REG_TRANSFMT.bit.CPHA = 1;
    } else {
    }

    if (control & QSPI_OUT_CONTROL_DATA_LENGTH_8) {
        spi->Instance->REG_TRANSFMT.bit.DATALEN = 8-1;
        spi->data_bits = 8;
    } else if(control & QSPI_OUT_CONTROL_DATA_LENGTH_16) {
        spi->Instance->REG_TRANSFMT.bit.DATALEN = 16-1;
        spi->data_bits = 16;
    } else if(control & QSPI_OUT_CONTROL_DATA_LENGTH_24) {
        spi->Instance->REG_TRANSFMT.bit.DATALEN = 24-1;
        spi->data_bits = 24;
    } else if(control & QSPI_OUT_CONTROL_DATA_LENGTH_32) {
        spi->Instance->REG_TRANSFMT.bit.DATALEN = 32-1;
        spi->data_bits = 32;
    } else {
    }

    if (control & QSPI_OUT_CONTROL_BIT_LSB) {
        spi->Instance->REG_TRANSFMT.bit.LSB = 1;
    } else if(control & QSPI_OUT_CONTROL_BIT_MSB) {
        spi->Instance->REG_TRANSFMT.bit.LSB = 0;
    } else {
    }

    if (control & QSPI_OUT_CONTROL_HALFWORD_LSB) {
        spi->Instance->REG_TRANSFMT.bit.RGB565_BYTE_CONV_EN = 0;
    } else if(control & QSPI_OUT_CONTROL_HALFWORD_MSB) {
        spi->Instance->REG_TRANSFMT.bit.RGB565_BYTE_CONV_EN = 1;
    } else {
    }

    if (control & QSPI_OUT_CONTROL_GET_FIFO_EMPTY) {
        if (arg) {
            uint32_t *value = (uint32_t *)arg;
            if(spi->Instance->REG_STATUS.bit.TXEMPTY == 1) {
                *value = 1;
            } else {
                *value = 0;
            }
        } else {
            DEV_LOG("[%s:%d] Error input: arg is 0", __func__, __LINE__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    return CSK_DRIVER_OK;
}


/**
 * @brief SPI interrupt handler
 *  @param[in] spi Pointer to SPI device structure
 */
_FAST_FUNC_RO static void QSPI_OUT_IRQ_Handler(void)
{
    uint32_t status = 0;
    uint32_t event = 0;
    QSPI_OUT_DEV *spi = &qspi_out_dev;

    status = spi->Instance->REG_INTRST.all;
    spi->Instance->REG_INTRST.all = status;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    /* irq send */
    if (status & QSPI_LCD_INTRST_TXFIFOINT_Msk)
    {
        /* send remaining data */
        spi_fill_tx_fifo(spi, spi->txfifo_depth >> 1); //
        //DEV_LOG("[%s:%d] tx_cnt=%d req_tx_cnt=%d", __func__, __LINE__, spi->tx_cnt, spi->req_tx_cnt);

        if (spi->tx_cnt == spi->req_tx_cnt) {
            spi->Instance->REG_INTREN.bit.TXFIFOINTEN = 0;
        }
    }

    if (status & QSPI_LCD_INTRST_ENDINT_Msk)
    {
        // disable SPI interrupts
        spi->Instance->REG_INTREN.all = 0;

        // Check if there is remaining data left in TX FIFO!!
        uint32_t i = spi->Instance->REG_STATUS.bit.TXNUM;
        if (i > 0) {
            // The Data remained in TX FIFO is deducted from the total xfer count
            spi->tx_cnt -= i;
            //DEV_LOG("%s: Warning!! %d Data items remain in TX FIFO!\n", __func__, i);
        }

        if (spi->tx_cnt < spi->req_tx_cnt) {
            //DEV_LOG("%s: Warning!! %d Data items have not been sent!\n", __func__, spi->req_tx_cnt - spi->tx_cnt);
        }

        spi->state = QSPI_OUT_STATE_READY;

        if (spi->cb_event) {
            spi->cb_event(QSPI_OUT_IRQ_EVENT_TRANSFER_COMPLETE, spi->usr_param);
        }
    }
}


/*
| Venus-A  | QSPI_OUT:func=19 | LCD  |
| -------- | ---------------- | ---- |
| GPIOA_03 | voc_spi1_cs_n    | CS   |
| GPIOA_02 | voc_spi1_clk     | CLK  |
| GPIOA_05 | voc_spi1_mosi    | D0   |
| GPIOA_04 | voc_spi1_miso    | D1   |
| GPIOA_07 | voc_spi1_wp_n    | D2   |
| GPIOA_06 | voc_spi1_hold_n  | D3   |
*/
