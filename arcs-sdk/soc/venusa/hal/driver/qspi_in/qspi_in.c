/**
 * @file qspi_in.c
 * @brief Driver implementation for the ChipSky QSPI Image Sensor interface.
 *        This file contains the low-level hardware interaction code,
 *        interrupt service routine (ISR), and the public API for the QSPI_IN peripheral.
 * @details
 * The driver manages clock generation, reset sequences, register configuration,
 * and data transfer via Direct memory Access (DMA). It supports various operational modes
 * configured through the `QSPI_IN_InitTypeDef` structure. Key components include:
 *        - Hardware reset and clock enable sequence (@ref QSPI_IN_Reset)
 *        - Interrupt handling for frame, line, and error events (@ref QSPI_IN_IRQ_Handler)
 *        - Core API functions: Initialize(), Uninitialize(), Start(), Stop()
 *        - Singleton device instance management.
 * @author USER
 * @date 2024-10-26 Created on 2024-10-26, last modified 2024-11-15
 * @version 1.0.0 (defined by CSK_QSPI_IN_DRV_VERSION)
 * @copyright Copyright (c) 2020-2025 ListenAI Technology. All rights reserved.
 * @license Proprietary / Internal Use Only
 */

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "venusa_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "qspi_in.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG


// driver version
#define CSK_QSPI_IN_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION qspi_in_driver_version = { CSK_QSPI_IN_API_VERSION, CSK_QSPI_IN_DRV_VERSION };

//------------------------------------------------------------------------------------------

/**
 * @brief Global QSPI_IN device instance (singleton pattern)
 *        Preallocated static device structure mapped to hardware base address
 */
_FAST_DATA_VI static QSPI_IN_DEV qspi_in0_dev = {
        ((QSPI_SENSOR_IN_RegDef *)(QSPI_IN_BASE)),
        {0},
        NULL,
};


//------------------------------------------------------------------------------------
/**
 * @brief Full reset sequence for QSPI subsystem
 *        Coordinates clock enable + reset pulse execution
 */
static inline void QSPI_IN_Reset(void)
{
    __HAL_CRM_QSPI0_CLK_ENABLE();  //IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_QSPI0_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.QSPI0_RESET = 1;

//    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK      = 1; // bit 0~0  0:24MHz  1:syspll peri clk
//    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_M    = 2; // bit 2~5
//    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_N    = 1; // bit 6~8
//    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_LD   = 1; // bit 1~1
}


/**
 * @brief Validate QSPI_IN device pointer integrity
 *        Checks if provided device pointer matches predefined singleton instance
 * @param[in] qspi_in_dev Pointer to device structure to validate
 * @return Valid QSPI_IN_DEV* on success, NULL on validation failure
 */
_FAST_FUNC_RO static QSPI_IN_DEV * safe_qspi_in_dev(void *qspi_in_dev)
{
    //TODO: safe check of QSPI_IN device parameter
    if (qspi_in_dev == QSPI_IN0()) {
        if (((QSPI_IN_DEV *)qspi_in_dev)->Instance != ((QSPI_SENSOR_IN_RegDef *)(QSPI_IN_BASE))) {
            //DEV_LOG("QSPI_IN0 device context has been tampered illegally!!\n");
            DEV_LOG("[%s:%d] Error qspi_in_dev=%#x ", __func__, __LINE__, qspi_in_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error qspi_in_dev=%#x ", __func__, __LINE__, qspi_in_dev);
        return NULL;
    }

    return (QSPI_IN_DEV *)qspi_in_dev;
}

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Handles QSPI_IN interrupt request.
 *        Processes hardware interrupt status and triggers registered callbacks
 * @note Called automatically by interrupt vector table
 */
_FAST_FUNC_RO void QSPI_IN_IRQ_Handler(void)
{
    QSPI_IN_DEV *qspi_in_dev = (QSPI_IN_DEV *)QSPI_IN0();
    uint32_t status = qspi_in_dev->Instance->REG_INTRST.all;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    /* Clear the corresponding interrupt flags */
    qspi_in_dev->Instance->REG_INTRST.all= status;

    if((status & QSPI_IN_INTRST_FRAME_START_INT_Msk) == QSPI_IN_INTRST_FRAME_START_INT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_FRAME_START, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_FRAME_END_INT_Msk) == QSPI_IN_INTRST_FRAME_END_INT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_FRAME_END, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_LINE_END_INT_Msk) == QSPI_IN_INTRST_LINE_END_INT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_LINE_END, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_LINE_START_INT_Msk) == QSPI_IN_INTRST_LINE_START_INT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_LINE_START, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_CRC_ERR_INT_Msk) == QSPI_IN_INTRST_CRC_ERR_INT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_CRC_ERR, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_SLVCMDINT_Msk) == QSPI_IN_INTRST_SLVCMDINT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_SLVCMD, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_ENDINT_Msk) == QSPI_IN_INTRST_ENDINT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_END, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_MTK_TRANS_ERR_INT_Msk) == QSPI_IN_INTRST_MTK_TRANS_ERR_INT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_MTK_TRANS_ERR, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_RXFIFOINT_Msk) == QSPI_IN_INTRST_RXFIFOINT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_RXFIFO, (uint32_t)&qspi_in_dev->Init);
    }

    if((status & QSPI_IN_INTRST_RXFIFOORINT_Msk) == QSPI_IN_INTRST_RXFIFOORINT_Msk)
    {
        if(qspi_in_dev->cb_event)
            qspi_in_dev->cb_event(QSPI_IN_IRQ_EVENT_RXFIFOOR, (uint32_t)&qspi_in_dev->Init);
    }
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Return QSPI_IN driver version information
 *        Returns combined API and driver versions as defined in header files
 * @return Version structure containing major/minor version numbers
 */
CSK_DRIVER_VERSION
QSPI_IN_GetVersion(void)
{
    return qspi_in_driver_version;
}

/**
 * @brief Obtain handle to primary QSPI_IN device instance
 *        Provides access to preconfigured singleton device structure
 * @return Pointer to global qspi_in0_dev structure
 */
void* QSPI_IN0(void)
{
    return &qspi_in0_dev;
}

/**
 * @brief Get physical address of QSPI_IN buffer memory region
 *        Direct access to hardware-managed frame buffer storage
 * @return Absolute address of pixel buffer area
 */
uint32_t QSPI_IN0_Buf(void)
{
    return CSK_QSPI_IN_BUF;
}


/**
 * @brief Configure QSPI_IN system clock frequency
 * @param[in] freq_hz Desired system clock frequency in Hz
 * @return Status code indicating success or failure
 * @details Selects 24MHz/50MHz/100MHz clock sources based on closest frequency match after clamping to valid range
 */
static inline int32_t qspi_in_sysclk_set(uint32_t freq_hz)
{
    if(freq_hz > 50000000) {
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK = 1;    // 0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_N = 1;
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_M = 1;  // div=1 clk=100MHz
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_LD = 1;
    } else if(freq_hz > 24000000) {
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK = 1;    // 0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_N = 1;
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_M = 2;  // div=2 clk=50MHz
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_LD = 1;
    } else {
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK = 0;    // 0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_N = 1;
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_M = 1;  // div=1 clk=24MHz
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_LD = 1;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Initialize the QSPI_IN (Digital Video Processor) device.
 *        Configures operational mode, register settings, and interrupt handling
 * @param pDev Pointer to the QSPI_IN device structure (must be valid)
 * @param callback Event notification callback function
 * @param pCfg Configuration parameters for initialization
 * @return CSK_DRIVER_OK on success, error code on failure
 * @note Must be called before any QSPI operations can be performed
 */
int32_t QSPI_IN_Initialize(void *pDev, QSPI_IN_SignalEvent_t callback, QSPI_IN_InitTypeDef *pCfg)
{
    QSPI_IN_DEV *qspi_in_dev = safe_qspi_in_dev(pDev);
    uint32_t value = 0;

    /* Check the QSPI_IN instance */
    if(qspi_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    QSPI_IN_Reset();
    qspi_in_sysclk_set(pCfg->clk_in_hz * 4);

    memcpy(&qspi_in_dev->Init, pCfg, sizeof(QSPI_IN_InitTypeDef));

    /* Disable QSPI first before configuration */
    qspi_in_dev->Instance->REG_CTRL.all = (1 << QSPI_SENSOR_IN_CTRL_SPI_CS_FROM_REG_Pos) | \
                                          (1 << QSPI_SENSOR_IN_CTRL_SPI_CS_REG_CFG_EN_Pos) | \
                                          (1 << QSPI_SENSOR_IN_CTRL_RXDMAEN_Pos) | \
                                          (1 << QSPI_SENSOR_IN_CTRL_RXFIFORST_Pos) | \
                                          (1 << QSPI_SENSOR_IN_CTRL_SPIRST_Pos) | \
                                          ((qspi_in_dev->Init.BurstThreshold << QSPI_SENSOR_IN_CTRL_RXTHRES_Pos) & QSPI_SENSOR_IN_CTRL_RXTHRES_Msk);

    qspi_in_dev->Instance->REG_TRANSCTRL.all &= (~(QSPI_SENSOR_IN_TRANSCTRL_DUALQUAD_Msk | QSPI_SENSOR_IN_TRANSCTRL_TRANSMODE_Msk));
    switch (qspi_in_dev->Init.lane_num)
    {
        case QSPI_IN_DATA_1LANE:
            qspi_in_dev->Instance->REG_TRANSCTRL.all |= (CSK_QSPI_IN_TRANSMODE_SINGLE << QSPI_SENSOR_IN_TRANSCTRL_DUALQUAD_Pos) | \
                                                        (CSK_QSPI_IN_TRANSMODE_READ_ONLY << QSPI_SENSOR_IN_TRANSCTRL_TRANSMODE_Pos);
            qspi_in_dev->Instance->REG_TRANSFMT.all = (0 << QSPI_SENSOR_IN_TRANSFMT_FIFO_WDATA_4BIT_IN_BYTE_SWAP_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_PAYLOAD_Pos) | \
                                                      (CSK_QSPI_IN_TRANSFORM_1P2B << QSPI_SENSOR_IN_TRANSFMT_TRANS_FORM_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_3LINE_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_LSB_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SYNC_SWAP_Pos) | \
                                                      (QSPI_SENSOR_IN_TRANSFMT_DATALEN_Msk) | (qspi_in_dev->Init.cp & 0x3);
            //qspi_in_dev->Instance->REG_TRANSFMT.all = 0x00001f20;
            break;

        case QSPI_IN_DATA_2LANE:
            qspi_in_dev->Instance->REG_TRANSCTRL.all |= (CSK_QSPI_IN_TRANSMODE_DUAL << QSPI_SENSOR_IN_TRANSCTRL_DUALQUAD_Pos) | \
                                                        (CSK_QSPI_IN_TRANSMODE_READ_ONLY << QSPI_SENSOR_IN_TRANSCTRL_TRANSMODE_Pos);
            qspi_in_dev->Instance->REG_TRANSFMT.all = (0 << QSPI_SENSOR_IN_TRANSFMT_FIFO_WDATA_4BIT_IN_BYTE_SWAP_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_PAYLOAD_Pos) | \
                                                      (CSK_QSPI_IN_TRANSFORM_1P2B << QSPI_SENSOR_IN_TRANSFMT_TRANS_FORM_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_3LINE_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_LSB_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SYNC_SWAP_Pos) | \
                                                      (QSPI_SENSOR_IN_TRANSFMT_DATALEN_Msk) | (qspi_in_dev->Init.cp & 0x3);
            //qspi_in_dev->Instance->REG_TRANSFMT.all = 0x00001f20;
            break;

        case QSPI_IN_DATA_4LANE:
            qspi_in_dev->Instance->REG_TRANSCTRL.all |= (CSK_QSPI_IN_TRANSMODE_QUAD << QSPI_SENSOR_IN_TRANSCTRL_DUALQUAD_Pos) | \
                                                        (CSK_QSPI_IN_TRANSMODE_READ_ONLY << QSPI_SENSOR_IN_TRANSCTRL_TRANSMODE_Pos);
            qspi_in_dev->Instance->REG_TRANSFMT.all = (1 << QSPI_SENSOR_IN_TRANSFMT_FIFO_WDATA_4BIT_IN_BYTE_SWAP_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_PAYLOAD_Pos) | \
                                                      (CSK_QSPI_IN_TRANSFORM_1P2B << QSPI_SENSOR_IN_TRANSFMT_TRANS_FORM_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SPI_3LINE_Pos) | \
                                                      (1 << QSPI_SENSOR_IN_TRANSFMT_SPI_LSB_Pos) | \
                                                      (0 << QSPI_SENSOR_IN_TRANSFMT_SYNC_SWAP_Pos) | \
                                                      (QSPI_SENSOR_IN_TRANSFMT_DATALEN_Msk) | (qspi_in_dev->Init.cp & 0x3);
            //qspi_in_dev->Instance->REG_TRANSFMT.all = 0x00003f28;
            break;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

        // ToDo
    //qspi_in_dev->Instance->REG_TRANSFMT.bit.SPI_LSB = 1;  // 4lane:4bit->8bit
        //qspi_in_dev->Instance->REG_TRANSFMT.bit.SYNC_SWAP = 0;  // bit2 SYNC_SWAP 4bit->8bit
        //qspi_in_dev->Instance->REG_TRANSFMT.bit.FIFO_WDATA_4BIT_IN_BYTE_SWAP = 1;  // bit13 fifo 4bit->8bit
        //qspi_in_dev->Instance->REG_TRANSFMT.bit.SPI_PAYLOAD = 0;  // bit7
        //qspi_in_dev->Instance->REG_CMD.bit.WIRE_TYPE = 1;  /* 0x24 bit17~21 wire_type 8bit->16bit */
        //qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.SYNC_CODE_LSB = 0;  // 8bit->16bit, when SPI_PAYLOAD=1
    if (true == qspi_in_dev->Init.wire_order) {
        qspi_in_dev->Instance->REG_CMD.bit.WIRE_TYPE = 1;     // bit0~4 -> bit4~0
    }
    if (true == qspi_in_dev->Init.is_lsb) {
        qspi_in_dev->Instance->REG_TRANSFMT.bit.SPI_LSB = 1;  // 4lane:4bit->8bit
    }
    if (true == qspi_in_dev->Init.data_merge) {
        qspi_in_dev->Instance->REG_TRANSFMT.bit.FIFO_WDATA_4BIT_IN_BYTE_SWAP = 1;  // fifo 4bit->8bit
    }

    qspi_in_dev->Instance->REG_SYNC_CODE0.all = qspi_in_dev->Init.sync_code.sof;
    qspi_in_dev->Instance->REG_SYNC_CODE1.all = qspi_in_dev->Init.sync_code.sol;
    qspi_in_dev->Instance->REG_SYNC_CODE2.all = qspi_in_dev->Init.sync_code.eol;  // or data packet
    qspi_in_dev->Instance->REG_SYNC_CODE3.all = qspi_in_dev->Init.sync_code.eof;
    qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.all = 0;

    /* SCLK_DIV 0:div2  1:div4  0xff:div1 */
    qspi_in_dev->Instance->REG_TIMING.all = 0x2FF;

    qspi_in_dev->Instance->REG_PIXEL_FRAME.all = (qspi_in_dev->Init.FrameWidth << QSPI_SENSOR_IN_PIXEL_FRAME_RG_PIXEL_WIDTH_Pos) | qspi_in_dev->Init.FrameHeight;
    qspi_in_dev->Instance->REG_VER_CROP.all = (qspi_in_dev->Init.PixelOffset << QSPI_SENSOR_IN_VER_CROP_RG_CROP_LEFT_Pos) | qspi_in_dev->Init.WindowWidth;
    qspi_in_dev->Instance->REG_HOR_CROP.all = (qspi_in_dev->Init.LineOffset << QSPI_SENSOR_IN_HOR_CROP_RG_CROP_UP_Pos) | qspi_in_dev->Init.WindowHeight;

    /* IRQ disable */
    qspi_in_dev->Instance->REG_INTREN.all = 0x0;

    /* IRQ Clear */
    qspi_in_dev->Instance->REG_INTRST.all = 0x7FF;

    qspi_in_dev->cb_event = callback;

    /* Init the low level hardware and interrupt */
    register_ISR(IRQ_QSPI_IN_VECTOR, (ISR)QSPI_IN_IRQ_Handler, NULL);
    clear_IRQ(IRQ_QSPI_IN_VECTOR);
    enable_IRQ(IRQ_QSPI_IN_VECTOR);

    return CSK_DRIVER_OK;
}


/**
 * @brief Uninitialize QSPI_IN device and release resources
 *        Cleanly disables all functionality and removes interrupt handlers
 * @param pDev Pointer to device structure to uninitialize
 * @return CSK_DRIVER_OK on success, error code on failure
 * @note Should be called before system powerdown or reconfiguration
 */
int32_t QSPI_IN_Uninitialize(void *pDev)
{
    QSPI_IN_DEV *qspi_in_dev = safe_qspi_in_dev(pDev);

    /* Check the QSPI_IN instance */
    if(qspi_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable QSPI_IN */
    qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.all = 0;
    qspi_in_dev->Instance->REG_CTRL.bit.SPI_CS_FROM_REG = 1;
    qspi_in_dev->Instance->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;
    qspi_in_dev->Instance->REG_CTRL.bit.RXFIFORST = 1;

    /* IRQ disable */
    qspi_in_dev->Instance->REG_INTREN.all = 0x0;

    /* IRQ Clear */
    qspi_in_dev->Instance->REG_INTRST.all = 0x7FF;

    disable_IRQ(IRQ_QSPI_IN_VECTOR);
    clear_IRQ(IRQ_QSPI_IN_VECTOR);

    //QSPI_IN_Reset();

    return CSK_DRIVER_OK;
}


/**
 * @brief Start QSPI_IN to capture video frames
 *        Triggers hardware execution of configured operation
 * @param pDev Pointer to initialized device structure
 * @return CSK_DRIVER_OK on success, error code on failure
 * @note Caller must ensure proper initialization was performed first
 */
int32_t QSPI_IN_Start(void *pDev)
{
    QSPI_IN_DEV *qspi_in_dev = safe_qspi_in_dev(pDev);

    /* Check the QSPI_IN instance */
    if(qspi_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* IRQ enable */
    if (qspi_in_dev->cb_event) {
        qspi_in_dev->Instance->REG_INTREN.all = (0 << QSPI_SENSOR_IN_INTREN_LINE_START_EN_Pos) | \
                                                (0 << QSPI_SENSOR_IN_INTREN_LINE_END_EN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_FRAME_END_EN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_FRAME_START_EN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_CRC_ERR_INTEN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_SLVCMDEN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_ENDINTEN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_MTK_TRANS_ERR_INTEN_Pos) | \
                                                (0 << QSPI_SENSOR_IN_INTREN_RXFIFOINTEN_Pos) | \
                                                (1 << QSPI_SENSOR_IN_INTREN_RXFIFOORINTEN_Pos);
    } else {
        qspi_in_dev->Instance->REG_INTREN.all = 0x0;
    }

    /* IRQ Clear */
    qspi_in_dev->Instance->REG_INTRST.all = 0x7FF;

    qspi_in_dev->Instance->REG_CTRL.bit.RXFIFORST = 1;
    qspi_in_dev->Instance->REG_CTRL.bit.SPI_CS_FROM_REG = 0;
    qspi_in_dev->Instance->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;

    switch (qspi_in_dev->Init.mode)
    {
        case QSPI_IN_SYNC_MODE_SPRD:
            qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.all = (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_LINE_END_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_LINE_START_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_FRAME_END_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_FRAME_START_EN_Pos);
            break;

        case QSPI_IN_SYNC_MODE_MTK:
            qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.all = (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_LINE_START_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_FRAME_END_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_FRAME_START_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_DATA_PACKET_DET_EN_Pos) | \
                                                             (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_MTK_Pos);
            break;

        case QSPI_IN_SYNC_MODE_ALL:
            qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.all = (1 << QSPI_SENSOR_IN_SPI_CAMERA_CTRL_ALL_DATA_REC_EN_Pos);
            break;

        default:
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stop ongoing QSPI_IN processing operation
 *        Halts current operation and disables hardware engine
 * @param pDev Pointer to active device structure
 * @return CSK_DRIVER_OK on success, error code on failure
 * @note Safe to call even when no operation is currently running
 */
int32_t QSPI_IN_Stop(void *pDev)
{
    QSPI_IN_DEV *qspi_in_dev = safe_qspi_in_dev(pDev);

    /* Check the QSPI_IN instance */
    if(qspi_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    qspi_in_dev->Instance->REG_SPI_CAMERA_CTRL.all = 0;

    qspi_in_dev->Instance->REG_CTRL.bit.SPI_CS_FROM_REG = 1;
    qspi_in_dev->Instance->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;
    qspi_in_dev->Instance->REG_CTRL.bit.RXFIFORST = 1;

    /* IRQ disable */
    qspi_in_dev->Instance->REG_INTREN.all = 0x0;

    /* IRQ Clear */
    qspi_in_dev->Instance->REG_INTRST.all = 0x7FF;

    return CSK_DRIVER_OK;
}


/*
| Venus-A  | QSPI_IN:func=18     | Sensor  |
| -------- | ------------------- | ------- |
| GPIOA_13 | vic_spi0_mosi       | SDO_0   |
| GPIOA_12 | vic_spi0_miso       | SDO_1   |
| GPIOA_15 | vic_spi0_wp_n       | SDO_2   |
| GPIOA_14 | vic_spi0_hold_n     | SDO_3   |
| GPIOA_16 | vic_spi0_clk        | PCLK    |
| GPIOA_20 | vic_clk_out func=13 | MCLK    |
*/
