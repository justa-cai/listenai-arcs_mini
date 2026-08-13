/**
 *@file i8080.c
 *@brief Intel 8080 Compatibility Mode Driver Implementation
 *@details This file contains the implementation of the I8080 peripheral driver, including initialization,
 * interrupt handling, DMA transfer control, and hardware configuration functions. The driver provides
 *          an API for configuring the I8080 interface and managing data transfers between the host system and
 *          external devices in Intel 8080 compatible mode.
*/

#include <string.h>
#include <stdint.h>
#include "venusa_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "mmio.h"
#include "i8080.h"


#define DEBUG_LOG 1  ///< Enable debug logging
#if DEBUG_LOG
    #define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
    #define DEV_LOG(...)           ///< Disabled logging
#endif

/// Driver version
#define CSK_I8080_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)


//------------------------------------------------------------------------------------------
// Static Variables
//------------------------------------------------------------------------------------------

/// @brief Driver version structure conforming to CSK driver version standard
static const CSK_DRIVER_VERSION i8080_driver_version = { CSK_I8080_API_VERSION, CSK_I8080_DRV_VERSION };

/**
 * @brief Primary device instance structure
 * @details Maintains the state and configuration of the I8080 peripheral instance, including register mappings,
 *          callback functions, and operational state machine.
 */
_FAST_DATA_VI static I8080_DEV i8080_dev0 = {
    .Instance = ((I8080_OUT_RegDef *)(I8080_BASE)),
    .cb_event = NULL,
    .info = {0},
};

//------------------------------------------------------------------------------------------
// Static Function Declarations
//------------------------------------------------------------------------------------------
static I8080_DEV *safe_i8080_dev(void *i8080_dev);
/**
 * @brief Hardware reset sequence for I8080 peripheral
 * @details Performs a complete power-on reset sequence including clock gating and reset signal activation
 */
static inline void I8080_hw_reset(void);
/**
 * @brief Clear both transmit and receive FIFOs
 * @param[in] i8080_dev Device instance pointer
 * @details Clears TX/RX FIFOs and resets buffer pointers to prevent stale data retention
 */
static inline void i8080_fifo_clr(I8080_DEV *i8080_dev);
/**
 * @brief Calculate optimal system clock divider
 * @param[in] src_clk Source clock frequency in Hz
 * @param[in] dts_clk Desired target clock frequency in Hz
 * @return Optimal divider value minimizing frequency error
 * @details Uses integer division with rounding to select best divider from available range
 */
static uint32_t sysclk_div_get(uint32_t src_clk, uint32_t dts_clk);
/**
 * @brief Configure I8080 system clock frequency
 * @param[in] freq_hz Desired system clock frequency in Hz
 * @return Status code indicating success or failure
 * @details Selects between 24MHz and 100MHz clock sources based on closest frequency match
 */
static int32_t I8080_clk_run_set(uint32_t freq_hz);
/**
 * @brief Get output clock divider settings
 * @param[in] src_clk Source clock frequency in Hz
 * @param[in] dts_clk Desired target clock frequency in Hz
 * @return Calculated divider value
 * @details Specialized version of sysclk_div_get for output clock generation path
 */
static uint32_t i8080_clk_div_get(uint32_t src_clk, uint32_t dts_clk);
/**
 * @brief Set I8080 output clock frequency
 * @param[in] i8080_dev Device instance pointer
 * @param[in] out_freq Desired output frequency in Hz
 * @return Status code indicating success or failure
 * @details Configures timing registers to generate precise output clock signals
 */
static int32_t I8080_clk_out_set(I8080_DEV *i8080_dev, uint32_t out_freq);
/**
 * @brief Validate transfer parameters before execution
 * @param[in] mode Transfer mode enumeration
 * @param[in] pReg Register address/data pointer
 * @param[in] RegSize Register size in bytes
 * @param[in] pData Data buffer pointer
 * @param[in] DataSize Data size in bytes
 * @return Status code indicating parameter validity
 * @details Enforces hardware constraints on buffer sizes, alignment requirements, and register widths
 */
static int32_t validate_transfer_params(I8080_emTransMode mode, uint8_t *pReg, uint8_t RegSize, uint8_t *pData, uint32_t DataSize);

/**
 * @brief I8080 Interrupt Handler
 * @details Central handler for all I8080 peripheral interrupt sources including transfer completion,
 *          FIFO status changes, and error conditions. Processes received data and triggers registered callbacks.
 */
_FAST_FUNC_RO void I8080_IRQ_Handler(void)
{
    I8080_DEV *i8080_dev = (I8080_DEV *)I8080();
    uint32_t status = i8080_dev->Instance->REG_I8080_INT_STATUS.all;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    /* Clear the corresponding interrupt flags */
    i8080_dev->Instance->REG_I8080_INTR_CLR.all = status;

    /* Transfer Complete */
    if (status & I8080_OUT_I8080_INT_STATUS_TRANS_END_ISR_Msk) {
        /* Process received data */
        if(i8080_dev->info.rx_size && i8080_dev->info.rx_buf) {
            while(i8080_dev->info.rx_size--)
            {
                *i8080_dev->info.rx_buf++ = mmio_read32(I8080_RXBUF);
            }

            i8080_dev->info.rx_buf = NULL;
            i8080_dev->info.rx_size = 0;
        }

        i8080_dev->info.state = I8080_STATE_READY;
        if (i8080_dev->cb_event) {
            i8080_dev->cb_event(I8080_IRQ_EVENT_TRANS_END, i8080_dev->usr_param);
        }
    }

    /* FIFO Empty */
    if (status & I8080_OUT_I8080_INT_STATUS_TXFIFO_RD_EMPTY_ISR_Msk) {
        if (i8080_dev->cb_event) {
            i8080_dev->cb_event(I8080_IRQ_EVENT_TXFIFO_RD_EMPTY, i8080_dev->usr_param);
        }
    }

    /* FIFO Full */
    if (status & I8080_OUT_I8080_INT_STATUS_TXFIFO_WR_FULL_ISR_Msk) {
        if (i8080_dev->cb_event) {
            i8080_dev->cb_event(I8080_IRQ_EVENT_TXFIFO_WR_FULL, i8080_dev->usr_param);
        }
    }

    /* FIFO Underflow */
    if (status & I8080_OUT_I8080_INT_STATUS_TXFIFO_UNDERFLOW_ISR_Msk) {
        i8080_dev->info.state = I8080_STATE_ERROR;
        if (i8080_dev->cb_event) {
            i8080_dev->cb_event(I8080_IRQ_EVENT_TXFIFO_UNDERFLOW, i8080_dev->usr_param);
        }
    }

    /* FIFO Overflow */
    if (status & I8080_OUT_I8080_INT_STATUS_TXFIFO_OVERFLOW_ISR_Msk) {
        i8080_dev->info.state = I8080_STATE_ERROR;
        if (i8080_dev->cb_event) {
            i8080_dev->cb_event(I8080_IRQ_EVENT_TXFIFO_OVERFLOW, i8080_dev->usr_param);
        }
    }
}



/**
 * @brief Get driver version information
 * @return CSK_DRIVER_VERSION structure containing version numbers
 * @details Returns the driver's API version and driver version as defined by the CSK driver model
 */
CSK_DRIVER_VERSION I8080_GetVersion(void)
{
    return i8080_driver_version;
}


/**
 * @brief Get device handle pointer
 * @return Pointer to the primary I8080 device instance
 * @details Returns a pointer to the static device instance structure maintained by the driver
 */
void* I8080(void)
{
    return &i8080_dev0;
}


/**
 * @brief Get TX buffer physical address
 * @return Physical address of the transmit buffer in memory map
 * @details Directly returns the base address of the hardware transmit buffer defined in the board configuration
 */
uint32_t I8080_TxBuf(void)
{
    return I8080_TXBUF;
}


/**
 * @brief Initialize I8080 peripheral
 * @param[in] pDev Device handle pointer
 * @param[in] pCallback Event callback function pointer
 * @return Status code indicating success or failure
 * @details Performs hardware reset, initializes registers, configures interrupt handlers, and sets initial state
 * @note Must be called before any other driver functions can be used
 */
int32_t I8080_Initialize(void *pDev, I8080_SignalEvent_t callback, uint32_t param)
{
    I8080_DEV *i8080_dev = safe_i8080_dev(pDev);
    if (!i8080_dev) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Hardware reset sequence */
    I8080_hw_reset();

    /* Initialize device info */
    i8080_dev->info.cs = I8080_PIN_OUTPUT_AUTO;
    i8080_dev->info.rs = I8080_PIN_OUTPUT_AUTO;
    i8080_dev->info.state = I8080_STATE_RESET;
    i8080_dev->info.rx_buf = NULL;
    i8080_dev->info.rx_size = 0;

    /* Disable I8080 */
    i8080_dev->Instance->REG_TRANSCTRL.all = (0 << I8080_OUT_TRANSCTRL_I8080_EN_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_I8080_CMD_EN_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_I8080_565_BYTE_CONV_EN_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_DIRECT_RS_IO_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_RS_OUT_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_DIRECT_CS_IO_Pos) | \
                                             (0 << I8080_OUT_TRANSCTRL_CS_N_OUT_Pos);

    /* RXTHRES=0~31, but RX_FIFO=16Byte */
    /* tXTHRES=0~63, but TX_FIFO=32word */
    i8080_dev->Instance->REG_FIFOCTRL.all = (1 << I8080_OUT_FIFOCTRL_I8080_SOFT_CLR_Pos) | \
                                            (1 << I8080_OUT_FIFOCTRL_RXFIFO_RST_Pos) | \
                                            (1 << I8080_OUT_FIFOCTRL_TXFIFO_RST_Pos) | \
                                            (0 << I8080_OUT_FIFOCTRL_TXDMAEN_Pos) | \
                                            (I8080_RXTHRES_DEFAULT << I8080_OUT_FIFOCTRL_RXTHRES_Pos) | \
                                            (I8080_TXTHRES_DEFAULT << I8080_OUT_FIFOCTRL_TXTHRES_Pos);

    /* TRANS_MODE= 0:write  1:read  2:nodata  3:dummy and read */
    /* DATA_LEN= 7:8bit  23:24bit  31:32bit */
    /* CMD_LEN= 0:1byte  1:2byte  2:3byte  3:4byte */
    i8080_dev->Instance->REG_TRANSFMT.all = (0 << I8080_OUT_TRANSFMT_I8080_LSB_Pos) | \
                                            (I8080_TRANS_MODE_WRITE << I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Pos) | \
                                            (I8080_DATA_LEN_8BIT << I8080_OUT_TRANSFMT_I8080_DATA_LEN_Pos) | \
                                            (I8080_CMD_LEN_1BYTE << I8080_OUT_TRANSFMT_I8080_CMD_LEN_Pos);

    /* bit0~19  I8080_WR_NUM=0~1048575 */
    /* bit20~23 I8080_RD_NUM=0~15 */
    /* bit24~25 I8080_DUMMY_NUM=0~3 */
    i8080_dev->Instance->REG_TRANSNUM.all = (0 << I8080_OUT_TRANSNUM_I8080_WR_NUM_Pos) | \
                                            (0 << I8080_OUT_TRANSNUM_I8080_RD_NUM_Pos) | \
                                            (0 << I8080_OUT_TRANSNUM_I8080_DUMMY_NUM_Pos);

    /* bit0~3  TIMING_SCLK_DIV=0~15 */
    /* bit4~7  TIMING_CSHT=0~15 */
    /* bit8~10 TIMING_CS2SCLK=0~7 */
    i8080_dev->Instance->REG_TIMING.all = (I8080_CLK_OUT_DIV_DEFAULT << I8080_OUT_TIMING_SCLK_DIV_Pos) | \
                                          (I8080_TIMING_CSHT_DEFAULT << I8080_OUT_TIMING_CSHT_Pos) | \
                                          (I8080_TIMING_CS2SCLK_DEFAULT << I8080_OUT_TIMING_CS2SCLK_Pos);

    /* IRQ mask */
    i8080_dev->Instance->REG_I8080_INTR_MASK.all =  (1 << I8080_OUT_I8080_INTR_MASK_DMA_SINGLE_MASK_Pos) | \
                                                    (1 << I8080_OUT_I8080_INTR_MASK_DMA_REQ_MASK_Pos) | \
                                                    (0 << I8080_OUT_I8080_INTR_MASK_TXFIFO_OVERFLOW_MASK_Pos) | \
                                                    (0 << I8080_OUT_I8080_INTR_MASK_TXFIFO_UNDERFLOW_MASK_Pos) | \
                                                    (1 << I8080_OUT_I8080_INTR_MASK_TXFIFO_WR_FULL_MASK_Pos) | \
                                                    (1 << I8080_OUT_I8080_INTR_MASK_TXFIFO_RD_EMPTY_MASK_Pos) | \
                                                    (0 << I8080_OUT_I8080_INTR_MASK_TRANS_END_MASK_Pos);

    /* IRQ clear */
    i8080_dev->Instance->REG_I8080_INTR_CLR.all = 0x7F;

    /* Enable I8080 */
    i8080_dev->Instance->REG_TRANSCTRL.bit.I8080_EN = 1;

    /* Register callback */
    i8080_dev->cb_event = callback;
    i8080_dev->usr_param = param;

    /* Register interrupt handler */
    register_ISR(IRQ_I8080_VECTOR, (ISR)I8080_IRQ_Handler, NULL);
    clear_IRQ(IRQ_I8080_VECTOR);
    enable_IRQ(IRQ_I8080_VECTOR);

    i8080_dev->info.state = I8080_STATE_READY;

    return CSK_DRIVER_OK;
}


/**
 * @brief Deinitialize I8080 peripheral
 * @param[in] pDev Device handle pointer
 * @return Status code indicating success or failure
 * @details Reverses the effects of initialization including disabling interrupts, resetting registers,
 *          and freeing allocated resources. Puts the device back into reset state.
 */
int32_t I8080_Uninitialize(void *pDev)
{
    I8080_DEV *i8080_dev = safe_i8080_dev(pDev);
    if (!i8080_dev) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable I8080 */
    i8080_dev->Instance->REG_TRANSCTRL.all = 0;

    /* IRQ mask */
    i8080_dev->Instance->REG_I8080_INTR_MASK.all = 0x7F;

    /* IRQ clear */
    i8080_dev->Instance->REG_I8080_INTR_CLR.all = 0x7F;

    /* Disable interrupts */
    disable_IRQ(IRQ_I8080_VECTOR);
    clear_IRQ(IRQ_I8080_VECTOR);

    /* Reset hardware */
    I8080_hw_reset();

    /* Clear device info */
    i8080_dev->info.cs = I8080_PIN_OUTPUT_AUTO;
    i8080_dev->info.rs = I8080_PIN_OUTPUT_AUTO;
    i8080_dev->info.state = I8080_STATE_RESET;
    i8080_dev->info.rx_buf = NULL;
    i8080_dev->info.rx_size = 0;

    return CSK_DRIVER_OK;
}


/**
 * @brief Control device parameters and configuration
 * @param[in] pDev Device handle pointer
 * @param[in] control Control command flags
 * @param[in] arg Control argument value
 * @return Status code indicating success or failure
 * @details Allows dynamic modification of various device parameters including clock settings, FIFO thresholds,
 *          pin multiplexing modes, and byte ordering formats. Supports bitmask combinations of control commands.
 */
int32_t I8080_Control(void *pDev, I8080_emControl control, uint32_t arg)
{
    I8080_DEV *i8080_dev = safe_i8080_dev(pDev);
    uint32_t reg_trans_ctrl_mask = 0;
    uint32_t reg_trans_ctrl = 0;

    /* Check the I8080 handle */
    if (!i8080_dev) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Check the I8080 initialized */
    if(i8080_dev->info.state == I8080_STATE_RESET)
    {
        DEV_LOG("[%s:%d] Error I8080 is not initialized", __func__, __LINE__);
        return CSK_DRIVER_ERROR;
    }

    /* FIFO control */
    if(control & I8080_CONTROL_FIFO_CLEAR) {
        i8080_fifo_clr(i8080_dev);
    }

    /* sys run clock configuration */
    if(control & I8080_CONTROL_CLK_RUN_HZ) {
        if(arg != 0) {
            I8080_clk_run_set(arg);
        } else {
            DEV_LOG("[%s:%d] Error input: clk is 0", __func__, __LINE__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    /* output clock configuration */
    /* div=(sclk_div+1)*2  sclk_div=0~15 */
    if(control & I8080_CONTROL_CLK_OUT_HZ) {
        if(arg != 0) {
            I8080_clk_out_set(i8080_dev, arg);
        } else {
            DEV_LOG("[%s:%d] Error input: clk is 0", __func__, __LINE__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    /* arg=0~15 */
    if(control & I8080_CONTROL_CSHT) {
        if(arg <= I8080_TIMING_CSHT_MAX)  {
            i8080_dev->Instance->REG_TIMING.bit.CSHT = arg;
        } else {
            DEV_LOG("[%s:%d] Error input: TIMING_CSHT is %d, must be 0~15", __func__, __LINE__, arg);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    /* arg=0~7 */
    if(control & I8080_CONTROL_CS2SCLK) {
        if(arg <= I8080_TIMING_CS2SCLK_MAX)  {
            i8080_dev->Instance->REG_TIMING.bit.CS2SCLK = arg;
        } else {
            DEV_LOG("[%s:%d] Error input: TIMING_CS2SCLK is %d, must be 0~7", __func__, __LINE__, arg);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    /* arg=1/4/8/16, TX_FIFO=32word */
    if(control & I8080_CONTROL_BURST_THD) {
        if((arg == 16) || (arg == 8) || (arg == 4) || (arg == 1))  {
            i8080_dev->Instance->REG_FIFOCTRL.bit.TXTHRES = arg;
        } else {
            DEV_LOG("[%s:%d] Error input: burst thd is %d, must be 1/4/8/16", __func__, __LINE__, arg);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    /* normal */
    if(control & I8080_CONTROL_BIT_LSB) {
        i8080_dev->Instance->REG_TRANSFMT.bit.I8080_LSB = 0;
    }

    /* if I8080_DATA_LEN=31 bit31~0 -> bit0~31 */
    /* if I8080_DATA_LEN=7   bit7~0 -> bit0~7 */
    if(control & I8080_CONTROL_BIT_MSB) {
        i8080_dev->Instance->REG_TRANSFMT.bit.I8080_LSB = 1;
    }

    /* normal */
    if(control & I8080_CONTROL_HALFWORD_LSB) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_I8080_565_BYTE_CONV_EN_Msk;
    }

    /* {byte3,byte2,byte1,byte0} -> {byte2,byte3,byte0,byte1} */
    if(control & I8080_CONTROL_HALFWORD_MSB) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_I8080_565_BYTE_CONV_EN_Msk;
        reg_trans_ctrl |= (1 << I8080_OUT_TRANSCTRL_I8080_565_BYTE_CONV_EN_Pos);
    }

    /* normal */
    if(control & I8080_CONTROL_WORD_LSB) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk;
    }

    /* {byte3,byte2,byte1,byte0} -> {byte0,byte1,byte2,byte3} */
    if(control & I8080_CONTROL_WORD_MSB) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk;
        reg_trans_ctrl |= (1 << I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Pos);
    }

    /***************** CS and RS Output ***********************/
    if((control & I8080_CONTROL_CS_OUTPUT_LOW) && (i8080_dev->info.cs != I8080_PIN_OUTPUT_LOW)) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_CS_N_OUT_Msk | I8080_OUT_TRANSCTRL_DIRECT_CS_IO_Msk;
        reg_trans_ctrl |= (1 << I8080_OUT_TRANSCTRL_DIRECT_CS_IO_Pos);
        i8080_dev->info.cs = I8080_PIN_OUTPUT_LOW;
    }

    if((control & I8080_CONTROL_CS_OUTPUT_HIGH) && (i8080_dev->info.cs != I8080_PIN_OUTPUT_HIGH)) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_CS_N_OUT_Msk | I8080_OUT_TRANSCTRL_DIRECT_CS_IO_Msk;
        reg_trans_ctrl |= (1 << I8080_OUT_TRANSCTRL_CS_N_OUT_Pos) | (1 << I8080_OUT_TRANSCTRL_DIRECT_CS_IO_Pos);
        i8080_dev->info.cs = I8080_PIN_OUTPUT_HIGH;
    }

    if((control & I8080_CONTROL_CS_OUTPUT_AUTO) && (i8080_dev->info.cs != I8080_PIN_OUTPUT_AUTO)) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_DIRECT_CS_IO_Msk;
        i8080_dev->info.cs = I8080_PIN_OUTPUT_AUTO;
    }

    if((control & I8080_CONTROL_RS_OUTPUT_LOW) && (i8080_dev->info.rs != I8080_PIN_OUTPUT_LOW)) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_RS_OUT_Msk | I8080_OUT_TRANSCTRL_DIRECT_RS_IO_Msk;
        reg_trans_ctrl |= (1 << I8080_OUT_TRANSCTRL_DIRECT_RS_IO_Pos);
        i8080_dev->info.rs = I8080_PIN_OUTPUT_LOW;
    }

    if((control & I8080_CONTROL_RS_OUTPUT_HIGH) && (i8080_dev->info.rs != I8080_PIN_OUTPUT_HIGH)) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_RS_OUT_Msk | I8080_OUT_TRANSCTRL_DIRECT_RS_IO_Msk;
        reg_trans_ctrl |= (1 << I8080_OUT_TRANSCTRL_RS_OUT_Pos) | (1 << I8080_OUT_TRANSCTRL_DIRECT_RS_IO_Pos);
        i8080_dev->info.rs = I8080_PIN_OUTPUT_HIGH;
    }

    if((control & I8080_CONTROL_RS_OUTPUT_AUTO) && (i8080_dev->info.rs != I8080_PIN_OUTPUT_AUTO)) {
        reg_trans_ctrl_mask |= I8080_OUT_TRANSCTRL_DIRECT_RS_IO_Msk;
        i8080_dev->info.rs = I8080_PIN_OUTPUT_AUTO;
    }

    /* Apply register changes */
    if(reg_trans_ctrl_mask) {
        i8080_dev->Instance->REG_TRANSCTRL.all &= ~reg_trans_ctrl_mask;
        i8080_dev->Instance->REG_TRANSCTRL.all |= reg_trans_ctrl;
    }

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Get current device status
 * @param[in] pDev Device handle pointer
 * @param[out] pStatus Status structure pointer
 * @return Status code indicating success or failure
 * @details Reads hardware status registers and reports current operational state including bus activity,
 *          FIFO levels, and error conditions. Updates the provided status structure with current values.
 */
int32_t I8080_GetStatus(void *pDev, I8080_StatusDef *pStatus)
{
    I8080_DEV *i8080_dev = safe_i8080_dev(pDev);

    /* Check the I8080 handle */
    if (!i8080_dev) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pStatus == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pStatus is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(i8080_dev->info.state == I8080_STATE_RESET)
    {
        DEV_LOG("[%s:%d] Error I8080 is not initialized", __func__, __LINE__);
        return CSK_DRIVER_ERROR;
    }

    /* Extract status bits */
#if 0  /* code:20003f80~20003fc6 70Byte 23Line */
    pStatus->is_busy = i8080_dev->Instance->REG_STATUS.bit.I8080_BUSY;
    pStatus->tx_full = i8080_dev->Instance->REG_STATUS.bit.TXFULL;
    pStatus->tx_empty = i8080_dev->Instance->REG_STATUS.bit.TXEMPTY;
    pStatus->tx_num = i8080_dev->Instance->REG_STATUS.bit.TXNUM;
    pStatus->rx_full = i8080_dev->Instance->REG_STATUS.bit.RXFULL;
    pStatus->rx_empty = i8080_dev->Instance->REG_STATUS.bit.RXEMPTY;
    pStatus->rx_num = i8080_dev->Instance->REG_STATUS.bit.RXNUM;
#else  /* code:20003f6e~20003fb0 66Byte 18Line */
    uint32_t status = i8080_dev->Instance->REG_STATUS.all;
    pStatus->is_busy = (status & I8080_OUT_STATUS_I8080_BUSY_Msk) >> I8080_OUT_STATUS_I8080_BUSY_Pos;
    pStatus->tx_full = (status & I8080_OUT_STATUS_TXFULL_Msk) >> I8080_OUT_STATUS_TXFULL_Pos;
    pStatus->tx_empty = (status & I8080_OUT_STATUS_TXEMPTY_Msk) >> I8080_OUT_STATUS_TXEMPTY_Pos;
    pStatus->tx_num = (status & I8080_OUT_STATUS_TXNUM_Msk) >> I8080_OUT_STATUS_TXNUM_Pos;
    pStatus->rx_full = (status & I8080_OUT_STATUS_RXFULL_Msk) >> I8080_OUT_STATUS_RXFULL_Pos;
    pStatus->rx_empty = (status & I8080_OUT_STATUS_RXEMPTY_Msk) >> I8080_OUT_STATUS_RXEMPTY_Pos;
    pStatus->rx_num = (status & I8080_OUT_STATUS_RXNUM_Msk) >> I8080_OUT_STATUS_RXNUM_Pos;
#endif

    return CSK_DRIVER_OK;
}


/**
 * @brief Execute data transfer operation
 * @param[in] pDev Device handle pointer
 * @param[in] mode Transfer mode enumeration
 * @param[in] pReg Register address/data pointer
 * @param[in] RegSize Register size in bytes
 * @param[in] pData Data buffer pointer
 * @param[in] DataSize Data size in bytes
 * @return Status code indicating success or failure
 * @details Initiates a data transfer operation using either interrupt-driven or DMA modes. Supports multiple transfer types including
 *          simplex send/receive, register-modified transfers, and bulk DMA transfers. Validates parameters before execution.
 * @note Different transfer modes have specific constraints on buffer sizes and alignment requirements.
 */
int32_t I8080_Transfer(void *pDev, I8080_emTransMode mode, uint8_t *pReg, uint8_t RegSize, uint8_t *pData, uint32_t DataSize)
{
    I8080_DEV *i8080_dev = safe_i8080_dev(pDev);
    uint32_t tx_cmd = 0;

    /* Check the I8080 instance */
    if(!i8080_dev)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if((i8080_dev->info.state == I8080_STATE_RESET) || (i8080_dev->info.state == I8080_STATE_BUSY))
    {
        DEV_LOG("[%s:%d] Error state=%d", __func__, __LINE__, i8080_dev->info.state);
        return CSK_DRIVER_ERROR;
    }

    if(CSK_DRIVER_OK != validate_transfer_params(mode, pReg, RegSize, pData, DataSize)) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    i8080_fifo_clr(i8080_dev);
    i8080_dev->info.state = I8080_STATE_BUSY;

    switch(mode)
    {
        case I8080_TRANS_MODE_IRQ_TX_DATA:
#if 0
            i8080_dev->Instance->REG_FIFOCTRL.bit.TXDMAEN = 0;                 // bit 3~3
            i8080_dev->Instance->REG_TRANSCTRL.bit.I8080_CMD_EN = 0;           // bit1
            i8080_dev->Instance->REG_TRANSCTRL.bit.I8080_888_BYTE_CONV_EN = 0;
            i8080_dev->Instance->REG_TRANSFMT.bit.I8080_TRANS_MODE = 0;        // bit 2~3  0:write  1:read  2:nodata  3:dummy and read
            i8080_dev->Instance->REG_TRANSFMT.bit.I8080_DATA_LEN = 7;          // bit 4~8  7:8bit  23:24bit  31:32bit
            i8080_dev->Instance->REG_TRANSFMT.bit.I8080_CMD_LEN = 0;           // bit 12~13  0:1byte  1:2byte  2:3byte  3:4byte
            i8080_dev->Instance->REG_TRANSNUM.bit.I8080_WR_NUM = DataSize;    // bit 0~19
            i8080_dev->Instance->REG_TRANSNUM.bit.I8080_RD_NUM = 0;            // bit 20~23
            i8080_dev->Instance->REG_TRANSNUM.bit.I8080_DUMMY_NUM = 0;         // bit 24~25
#else
            i8080_dev->Instance->REG_FIFOCTRL.all &= ~I8080_OUT_FIFOCTRL_TXDMAEN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all &= ~(I8080_OUT_TRANSCTRL_I8080_CMD_EN_Msk | I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all &= ~(I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Msk | I8080_OUT_TRANSFMT_I8080_DATA_LEN_Msk | I8080_OUT_TRANSFMT_I8080_CMD_LEN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all |= (7 << I8080_OUT_TRANSFMT_I8080_DATA_LEN_Pos);
            i8080_dev->Instance->REG_TRANSNUM.all = (DataSize << I8080_OUT_TRANSNUM_I8080_WR_NUM_Pos);
#endif
            while(DataSize--)
            {
                mmio_write32(I8080_TXBUF, *pData++);
            }
            break;

        case I8080_TRANS_MODE_IRQ_RX_DATA:
            i8080_dev->info.rx_buf = pData;
            i8080_dev->info.rx_size = DataSize;
            i8080_dev->Instance->REG_FIFOCTRL.all &= ~I8080_OUT_FIFOCTRL_TXDMAEN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all &= ~(I8080_OUT_TRANSCTRL_I8080_CMD_EN_Msk | I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all &= ~(I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Msk | I8080_OUT_TRANSFMT_I8080_DATA_LEN_Msk | I8080_OUT_TRANSFMT_I8080_CMD_LEN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all |= (1 << I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Pos) | (31 << I8080_OUT_TRANSFMT_I8080_DATA_LEN_Pos);
            i8080_dev->Instance->REG_TRANSNUM.all = (DataSize << I8080_OUT_TRANSNUM_I8080_RD_NUM_Pos);
            break;

        case I8080_TRANS_MODE_IRQ_TX_REG_TX_DATA:
            i8080_dev->Instance->REG_FIFOCTRL.all &= ~I8080_OUT_FIFOCTRL_TXDMAEN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all &= ~I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all |= I8080_OUT_TRANSCTRL_I8080_CMD_EN_Msk;
            i8080_dev->Instance->REG_TRANSFMT.all &= ~(I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Msk | I8080_OUT_TRANSFMT_I8080_DATA_LEN_Msk | I8080_OUT_TRANSFMT_I8080_CMD_LEN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all |= (7 << I8080_OUT_TRANSFMT_I8080_DATA_LEN_Pos) | ((RegSize-1) << I8080_OUT_TRANSFMT_I8080_CMD_LEN_Pos);
            i8080_dev->Instance->REG_TRANSNUM.all = (DataSize << I8080_OUT_TRANSNUM_I8080_WR_NUM_Pos);
            while(DataSize--)
            {
                mmio_write32(I8080_TXBUF, *pData++);
            }
            memcpy(&tx_cmd, pReg, RegSize);
            break;

        case I8080_TRANS_MODE_IRQ_TX_REG_RX_DATA:
            i8080_dev->info.rx_buf = pData;
            i8080_dev->info.rx_size = DataSize;
            i8080_dev->Instance->REG_FIFOCTRL.all &= ~I8080_OUT_FIFOCTRL_TXDMAEN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all &= ~I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all |= I8080_OUT_TRANSCTRL_I8080_CMD_EN_Msk;
            i8080_dev->Instance->REG_TRANSFMT.all &= ~(I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Msk | I8080_OUT_TRANSFMT_I8080_DATA_LEN_Msk | I8080_OUT_TRANSFMT_I8080_CMD_LEN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all |= (1 << I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Pos) | (31 << I8080_OUT_TRANSFMT_I8080_DATA_LEN_Pos) | ((RegSize-1) << I8080_OUT_TRANSFMT_I8080_CMD_LEN_Pos);
            i8080_dev->Instance->REG_TRANSNUM.all = (DataSize << I8080_OUT_TRANSNUM_I8080_RD_NUM_Pos);
            memcpy(&tx_cmd, pReg, RegSize);
            break;

        case I8080_TRANS_MODE_DMA_TX_DATA:
            i8080_dev->Instance->REG_FIFOCTRL.all |= I8080_OUT_FIFOCTRL_TXDMAEN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all &= ~I8080_OUT_TRANSCTRL_I8080_CMD_EN_Msk;
            i8080_dev->Instance->REG_TRANSCTRL.all |= I8080_OUT_TRANSCTRL_I8080_888_BYTE_CONV_EN_Msk;
            i8080_dev->Instance->REG_TRANSFMT.all &= ~(I8080_OUT_TRANSFMT_I8080_TRANS_MODE_Msk | I8080_OUT_TRANSFMT_I8080_DATA_LEN_Msk | I8080_OUT_TRANSFMT_I8080_CMD_LEN_Msk);
            i8080_dev->Instance->REG_TRANSFMT.all |= (31 << I8080_OUT_TRANSFMT_I8080_DATA_LEN_Pos);
            i8080_dev->Instance->REG_TRANSNUM.all = ((DataSize / sizeof(uint32_t)) << I8080_OUT_TRANSNUM_I8080_WR_NUM_Pos);
            break;

        default:
            i8080_dev->info.state = I8080_STATE_READY;
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Start transfer */
    i8080_dev->Instance->REG_CMD.all = tx_cmd;

    return CSK_DRIVER_OK;
}


//------------------------------------------------------------------------------------------
// Static Function Implementations
//------------------------------------------------------------------------------------------

/**
 * @brief Validate transfer parameters against hardware constraints
 * @param[in] mode Transfer mode enumeration
 * @param[in] pReg Register address/data pointer
 * @param[in] RegSize Register size in bytes
 * @param[in] pData Data buffer pointer
 * @param[in] DataSize Data size in bytes
 * @return Status code indicating parameter validity
 * @details Enforces hardware constraints on buffer sizes, alignment requirements, and register widths for each transfer mode.
 */
static int32_t validate_transfer_params(I8080_emTransMode mode,
                                        uint8_t *pReg,
                                        uint8_t RegSize,
                                        uint8_t *pData,
                                        uint32_t DataSize)
{
    switch (mode) {
        case I8080_TRANS_MODE_IRQ_TX_DATA:
            if ((!pData) || (DataSize == 0) || (DataSize > I8080_TXFIFO_NUM))
            {
                DEV_LOG("[%s:%d] unsupport pData=0x%x DataSize=%d", __func__, __LINE__, pData, DataSize);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        case I8080_TRANS_MODE_IRQ_RX_DATA:
            if ((!pData) || (DataSize == 0) || (DataSize > I8080_RXFIFO_NUM))
            {
                DEV_LOG("[%s:%d] unsupport", __func__, __LINE__);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        case I8080_TRANS_MODE_IRQ_TX_REG_TX_DATA:
            if ((!pData)  || (RegSize == 0) || (RegSize > sizeof(uint32_t)) || (DataSize == 0) || (DataSize > I8080_TXFIFO_NUM))
            {
                DEV_LOG("[%s:%d] unsupport", __func__, __LINE__);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        case I8080_TRANS_MODE_IRQ_TX_REG_RX_DATA:
            if ((!pData) || (RegSize == 0) || (RegSize > sizeof(uint32_t)) || (DataSize == 0) || (DataSize > I8080_RXFIFO_NUM))
            {
                DEV_LOG("[%s:%d] unsupport", __func__, __LINE__);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        case I8080_TRANS_MODE_DMA_TX_DATA:
            if (DataSize < sizeof(uint32_t))
            {
                DEV_LOG("[%s:%d] unsupport", __func__, __LINE__);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        default:
            DEV_LOG("[%s:%d] unsupport mode=%d", __func__, __LINE__, mode);
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    return CSK_DRIVER_OK;
}


/**
 * @brief Safely validate device handle pointer
 * @param[in] i8080_dev Device handle pointer
 * @return Valid device instance pointer or NULL
 * @details Verifies that the provided device handle points to a valid device instance and matches the expected memory location.
 */
_FAST_FUNC_RO static I8080_DEV *safe_i8080_dev(void *i8080_dev)
{
    if (!i8080_dev) return NULL;

    I8080_DEV *dev = (I8080_DEV *)i8080_dev;
    if (dev->Instance != ((I8080_OUT_RegDef *)(I8080_BASE))) {
        DEV_LOG("Device context corrupted");
        return NULL;
    }
    return dev;
}


/**
 * @brief Reset I8080 hardware peripheral
 * @details Performs a complete hardware reset sequence including enabling clock gate and asserting reset signal
 */
static inline void I8080_hw_reset(void)
{
    __HAL_CRM_I8080_CLK_ENABLE();   // IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_I8080_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I8080_RESET = 1;

//    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.SEL_I8080_CLK      = 1; // bit 20~20  0:24MHz  1:syspll peri clk
//    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_M    = 1; // bit 21~24
//    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_LD   = 1; // bit 25~25
}


/**
 * @brief Clear transmit and receive FIFOs
 * @param[in] i8080_dev Device instance pointer
 * @details Issues software reset commands to both FIFOs and clears receive buffer tracking variables
 */
static inline void i8080_fifo_clr(I8080_DEV *i8080_dev)
{
    if (!i8080_dev)
        return;

    i8080_dev->Instance->REG_FIFOCTRL.all = (1 << I8080_OUT_FIFOCTRL_I8080_SOFT_CLR_Pos) | \
                                            (1 << I8080_OUT_FIFOCTRL_RXFIFO_RST_Pos) | \
                                            (1 << I8080_OUT_FIFOCTRL_TXFIFO_RST_Pos);
    i8080_dev->info.rx_buf = NULL;
    i8080_dev->info.rx_size = 0;
}


/**
 * @brief Calculate optimal system clock divider
 * @param[in] src_clk Source clock frequency in Hz
 * @param[in] dts_clk Desired target clock frequency in Hz
 * @return Optimal divider value minimizing frequency error
 * @details Uses integer division with rounding to select best divider from available range (1-15)
 */
static uint32_t sysclk_div_get(uint32_t src_clk, uint32_t dts_clk)
{
    uint32_t divider = 0;

    divider = src_clk / dts_clk;
    if(divider == 0) {
        return 1;
    }

    return ((((src_clk / divider) - dts_clk) > (dts_clk - (src_clk / (divider + 1)))) ? (divider + 1) : divider);
}


/**
 * @brief Configure I8080 system clock frequency
 * @param[in] freq_hz Desired system clock frequency in Hz
 * @return Status code indicating success or failure
 * @details Selects between 24MHz and 200MHz clock sources based on closest frequency match after clamping to valid range
 */
static int32_t I8080_clk_run_set(uint32_t freq_hz)
{
    uint32_t div_24MHz = 0;
    uint32_t div_100MHz = 0;
    uint32_t freq_24MHz = 0;
    uint32_t freq_100MHz = 0;
    uint32_t error_24MHz = 0;
    uint32_t error_100MHz = 0;

    /* Clamp frequency to valid range */
    if(freq_hz < I8080_CLK_RUN_HZ_MIN) {
        freq_hz = I8080_CLK_RUN_HZ_MIN;
    } else if(freq_hz > I8080_CLK_RUN_HZ_MAX) {
        freq_hz = I8080_CLK_RUN_HZ_MAX;
    } else {
    }

    /* Calculate dividers for both clock sources */
    div_24MHz = sysclk_div_get(I8080_CLK_RUN_SEL_24MHz, freq_hz);
    div_100MHz = sysclk_div_get(I8080_CLK_RUN_SEL_100MHz, freq_hz);

    /* Calculate actual frequencies */
    freq_24MHz = I8080_CLK_RUN_SEL_24MHz / div_24MHz;
    freq_100MHz = I8080_CLK_RUN_SEL_100MHz / div_100MHz;

    /* Calculate frequency errors */
    error_24MHz = (freq_24MHz >= freq_hz) ? (freq_24MHz - freq_hz) : (freq_hz - freq_24MHz);
    error_100MHz = (freq_100MHz >= freq_hz) ? (freq_100MHz - freq_hz) : (freq_hz - freq_100MHz);

    /* Select clock source with smallest error */
    if(error_24MHz <= error_100MHz) {
        IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.SEL_I8080_CLK = 0;                    // bit20  0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_M = (div_24MHz & 0xF);  // bit21~24
        //DEV_LOG("[%s:%d] src=%dMHz div=%d clk=%dHz", __func__, __LINE__, I8080_CLK_RUN_SEL_24MHz/1000000, div_24MHz, (I8080_CLK_RUN_SEL_24MHz/div_24MHz));
    } else {
        IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.SEL_I8080_CLK = 1;  // bit20  0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_M = (div_100MHz & 0xF);  // bit21~24
        //DEV_LOG("[%s:%d] src=%dMHz div=%d clk=%dHz", __func__, __LINE__, I8080_CLK_RUN_SEL_100MHz/1000000, div_100MHz, (I8080_CLK_RUN_SEL_100MHz/div_100MHz));
    }

    /* Trigger divider load */
    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_LD = 0x1;  // bit25

    return CSK_DRIVER_OK;
}

/**
 * @brief Get output clock divider settings
 * @param[in] src_clk Source clock frequency in Hz
 * @param[in] dts_clk Desired target clock frequency in Hz
 * @return Calculated divider value
 * @details Specialized version of sysclk_div_get optimized for output clock generation path
 */
static inline uint32_t i8080_clk_div_get(uint32_t src_clk, uint32_t dts_clk)
{
    uint32_t divider = 0;

    /* Validate input parameters */
    if ((src_clk == 0) || (dts_clk == 0)) {
        return 0;
    }

    divider = src_clk / (2 * dts_clk);
    divider = (divider > 1) ? (divider - 1) : 0;
    if ((int32_t)((src_clk / (2 * (divider + 1))) - dts_clk) > (int32_t)(dts_clk - (src_clk / (2 * (divider + 2))))) {
        divider++;
    }

    return divider;
}

/**
 * @brief Set I8080 output clock frequency
 * @param[in] i8080_dev Device instance pointer
 * @param[in] out_freq Desired output frequency in Hz
 * @return Status code indicating success or failure
 * @details Configures timing registers to generate precise output clock signals based on selected clock source
 */
static int32_t I8080_clk_out_set(I8080_DEV *i8080_dev, uint32_t out_freq)
{
    uint8_t sclk_div = 0;
    uint32_t i8080_freq = CRM_GetI8080Freq();

    /* Validate device handle */
    if ((!i8080_dev) || (!out_freq) || (!i8080_freq)) {
        DEV_LOG("[%s:%d] error: i8080_dev=0x%x out_freq=%d i8080_freq=%d", __func__, __LINE__, i8080_dev, out_freq, i8080_freq);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Calculate optimal divider */
    /* div=(sclk_div+1)*2  sclk_div=0~15 */
    sclk_div = i8080_clk_div_get(i8080_freq, out_freq);
    if(sclk_div > 0xF) {
        sclk_div = 0xF;
    }

    /* Apply divider configuration */
    i8080_dev->Instance->REG_TIMING.bit.SCLK_DIV = sclk_div;

    //DEV_LOG("[%s:%d] i8080_freq=%d sclk_div=%d div=%d out_freq=%d", __func__, __LINE__, i8080_freq, sclk_div, (sclk_div+1)*2, i8080_freq/((sclk_div+1)*2));

    return CSK_DRIVER_OK;
}

/*
 * sysclk=24/2=12MHz
 * div=(sclk_div+1)*2   sclk_div=0~15
 * sclk_div=15  clk=375Khz      ok
 * sclk_div=14  clk=400Khz      ok
 * sclk_div=9   clk=600Khz      ok
 * sclk_div=5   clk=1000Khz     ok
 * sclk_div=2   clk=2000Khz     ok
 * sclk_div=1   clk=3000Khz     ok
 * sclk_div=0   clk=6000Khz     ok
 */




