/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "arcs_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "qspi_sensor_in.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG


// driver version
#define CSK_QSPI_SENSOR_IN_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION qspi_sensor_in_driver_version = { CSK_QSPI_SENSOR_IN_API_VERSION, CSK_QSPI_SENSOR_IN_DRV_VERSION };

//------------------------------------------------------------------------------------------


_FAST_DATA_VI static QSPI_SENSOR_IN_DEV qspi_sensor_in0_dev = {
        ((QSPI_SENSOR_IN_RegDef *)(QSPI_SENSOR_IN_BASE)),
        {0},
        NULL,
};


//------------------------------------------------------------------------------------
static void ap_cfg_video_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<15);
}

static void ap_cfg_vic_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;  // bit21
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<21);
}

static void ap_cfg_qspi0_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI0_CLK = 0x1;  // bit11
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<11);
}

static void ap_cfg_dma_sel_qspi_in(void)
{
    IP_AP_CFG->REG_DMA_SEL.all &= ~(1<<1);     // bit1  0:qspi_in  1:dvp
}

static void ap_cfg_qspi0_clk_sel(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI0_CLK = 0x1;  // bit10  0:XTAL  1:syspll_peri_clk
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<10);
}

static void ap_cfg_qspi0_clk_div_m(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_M = 0x1;  // bit12~15
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0xF << 12);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0xF) << 12);
}

static void ap_cfg_qspi0_clk_div_n(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_N = 0x1;  // bit16~18
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0x7 << 16);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0x7) << 16);
}

static void ap_cfg_qspi0_clk_inv(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_LD = 0x1;  // bit19  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<19);
}

static void ap_cfg_vic_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 0x1;        // bit9
    IP_AP_CFG->REG_SW_RESET.all |= (1<<9);
}

static void ap_cfg_qspi0_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.QSPI0_RESET = 0x1;      // bit10
    IP_AP_CFG->REG_SW_RESET.all |= (1<<10);
}

static void QSPI_SENSOR_IN_Reset(void)
{
    ap_cfg_video_clk_enable();
    ap_cfg_vic_clk_enable();
    ap_cfg_dma_sel_qspi_in();
    ap_cfg_qspi0_clk_enable();
    ap_cfg_qspi0_clk_sel();
    ap_cfg_qspi0_clk_div_m(2);
    ap_cfg_qspi0_clk_div_n(1);
    ap_cfg_qspi0_clk_inv();
    ap_cfg_vic_reset();
    ap_cfg_qspi0_reset();
}



static QSPI_SENSOR_IN_DEV * safe_qspi_sensor_in_dev(void *qspi_sensor_in_dev)
{
    //TODO: safe check of QSPI_SENSOR_IN device parameter
    if (qspi_sensor_in_dev == QSPI_SENSOR_IN0()) {
        if (((QSPI_SENSOR_IN_DEV *)qspi_sensor_in_dev)->Instance != ((QSPI_SENSOR_IN_RegDef *)(QSPI_SENSOR_IN_BASE))) {
            //CLOGW("QSPI_SENSOR_IN0 device context has been tampered illegally!!\n");
            DEV_LOG("[%s:%d] Error qspi_sensor_in_dev=%#x ", __func__, __LINE__, qspi_sensor_in_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error qspi_sensor_in_dev=%#x ", __func__, __LINE__, qspi_sensor_in_dev);
        return NULL;
    }

    return (QSPI_SENSOR_IN_DEV *)qspi_sensor_in_dev;
}

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Handles QSPI_SENSOR_IN interrupt request.
  * @retval None
  */
_FAST_FUNC_RO void QSPI_SENSOR_IN_IRQ_Handler()
{
    QSPI_SENSOR_IN_DEV *qspi_sensor_in_dev = (QSPI_SENSOR_IN_DEV *)QSPI_SENSOR_IN0();
    uint32_t status = qspi_sensor_in_dev->Instance->REG_INTRST.all;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    /* Clear the corresponding interrupt flags */
    qspi_sensor_in_dev->Instance->REG_INTRST.all= status;

    if((status & QSPI_SENSOR_IN_INTRST_FRAME_START_INT_Msk) == QSPI_SENSOR_IN_INTRST_FRAME_START_INT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_FRAME_START, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_FRAME_END_INT_Msk) == QSPI_SENSOR_IN_INTRST_FRAME_END_INT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_FRAME_END, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_LINE_END_INT_Msk) == QSPI_SENSOR_IN_INTRST_LINE_END_INT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_LINE_END, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_LINE_START_INT_Msk) == QSPI_SENSOR_IN_INTRST_LINE_START_INT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_LINE_START, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_ERRINT_Msk) == QSPI_SENSOR_IN_INTRST_ERRINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_ERR, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_SLVCMDINT_Msk) == QSPI_SENSOR_IN_INTRST_SLVCMDINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_SLVCMD, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_ENDINT_Msk) == QSPI_SENSOR_IN_INTRST_ENDINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_END, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_TXFIFOINT_Msk) == QSPI_SENSOR_IN_INTRST_TXFIFOINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_TXFIFO, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_RXFIFOINT_Msk) == QSPI_SENSOR_IN_INTRST_RXFIFOINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_RXFIFO, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_TXFIFOURINT_Msk) == QSPI_SENSOR_IN_INTRST_TXFIFOURINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_TXFIFOUR, 0);
    }

    if((status & QSPI_SENSOR_IN_INTRST_RXFIFOORINT_Msk) == QSPI_SENSOR_IN_INTRST_RXFIFOORINT_Msk)
    {
        if(qspi_sensor_in_dev->cb_event)
            qspi_sensor_in_dev->cb_event(QSPI_SENSOR_IN_IRQ_EVENT_RXFIFOOR, 0);
    }
}

/* Exported functions --------------------------------------------------------*/


/**
  * @brief  Return QSPI_SENSOR_IN driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
QSPI_SENSOR_IN_GetVersion(void)
{
    return qspi_sensor_in_driver_version;
}


/**
  * @brief  Return QSPI_SENSOR_IN instance.
  *
  * @return Instance of QSPI_SENSOR_IN
  */
void* QSPI_SENSOR_IN0(void)
{
    return &qspi_sensor_in0_dev;
}


/**
  * @brief  Return QSPI_SENSOR_IN BUF instance.
  *
  * @return Instance of QSPI_SENSOR_IN
  */
uint32_t QSPI_SENSOR_IN0_Buf(void)
{
    return CSK_QSPI_SENSOR_IN_BUF;
}


/**
 * @brief Initialize the QSPI_SENSOR_IN (Digital Video Processor) device.
 *
 * @param pDev A pointer to the QSPI_SENSOR_IN device structure.
 * @param pCallback The callback function for QSPI_SENSOR_IN events.
 * @param pCfg The configuration structure for the QSPI_SENSOR_IN device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t QSPI_SENSOR_IN_Initialize(void *pDev, void *pCallback, QSPI_SENSOR_IN_InitTypeDef *pCfg)
{
    QSPI_SENSOR_IN_DEV *qspi_sensor_in_dev = safe_qspi_sensor_in_dev(pDev);

    /* Check the QSPI_SENSOR_IN instance */
    if(qspi_sensor_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    QSPI_SENSOR_IN_Reset();

    memcpy(&qspi_sensor_in_dev->Init, pCfg, sizeof(QSPI_SENSOR_IN_InitTypeDef));

    /* Disable QSPI first before configuration */
    IP_QSPI_SENSOR_IN->REG_CTRL.bit.SPI_CS_FROM_REG = 1;
    IP_QSPI_SENSOR_IN->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;
    IP_QSPI_SENSOR_IN->REG_SLVST.bit.READY = 0;

    /* IRQ enable */
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.RXFIFOORINTEN      = 1;  // bit 0~0
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.TXFIFOURINTEN      = 0;  // bit 1~1
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.RXFIFOINTEN        = 0;  // bit 2~2
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.TXFIFOINTEN        = 0;  // bit 3~3
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.ENDINTEN           = 1;  // bit 4~4
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.SLVCMDEN           = 1;  // bit 5~5
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.ERRINTEN           = 1;  // bit 6~6
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.FRAME_START_INT_EN = 1;  // bit 7~7
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.FRAME_END_INT_EN   = 1;  // bit 8~8
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.LINE_END_INT_EN    = 0;  // bit 9~9
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.LINE_START_INT_EN  = 0;  // bit 10~10

    /* IRQ Clear */
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.RXFIFOORINT        = 1;  // bit 0~0
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.TXFIFOURINT        = 1;  // bit 1~1
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.RXFIFOINT          = 1;  // bit 2~2
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.TXFIFOINT          = 1;  // bit 3~3
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.ENDINT             = 1;  // bit 4~4
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.SLVCMDINT          = 1;  // bit 5~5
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.ERRINT             = 1;  // bit 6~6
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.FRAME_START_INT    = 1;  // bit 7~7
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.FRAME_END_INT      = 1;  // bit 8~8
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.LINE_END_INT       = 1;  // bit 9~9
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.LINE_START_INT     = 1;  // bit 10~10

    switch (pCfg->lane_num)
    {
        case 1:
            qspi_sensor_in_dev->Instance->REG_TRANSCTRL.bit.DUALQUAD = CSK_QSPI_SENSOR_IN_TRANSMODE_SINGLE;
            break;

        case 2:
            qspi_sensor_in_dev->Instance->REG_TRANSCTRL.bit.DUALQUAD = CSK_QSPI_SENSOR_IN_TRANSMODE_DUAL;
            break;

        case 4:
            qspi_sensor_in_dev->Instance->REG_TRANSCTRL.bit.DUALQUAD = CSK_QSPI_SENSOR_IN_TRANSMODE_QUAD;
            break;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    switch (pCfg->cp)
    {
        case QSPI_SENSOR_IN_CPOL0_CPOH0:
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPOL = 0;
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPHA = 0;
            break;

        case QSPI_SENSOR_IN_CPOL0_CPOH1:
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPOL = 0;
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPHA = 1;
            break;

        case QSPI_SENSOR_IN_CPOL1_CPOH0:
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPOL = 1;
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPHA = 0;
            break;

        case QSPI_SENSOR_IN_CPOL1_CPOH1:
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPOL = 1;
            qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.CPHA = 1;
            break;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    switch (pCfg->mode)
    {
        case QSPI_SENSOR_IN_SYNC_MODE_SPRD:
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.MTK = 0;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.ALL_DATA_REC_EN = 0;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.FRAME_START_EN = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.FRAME_END_EN = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.LINE_START_EN = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.LINE_END_EN = 1;
            //DEV_LOG("[%s:%d] QSPI_IN_SYNC_MODE_SPRD", __func__, __LINE__);
            break;

        case QSPI_SENSOR_IN_SYNC_MODE_MTK:
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.MTK = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.ALL_DATA_REC_EN = 0;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.FRAME_START_EN = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.FRAME_END_EN = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.LINE_START_EN = 1;
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.LINE_END_EN = 1;
            //DEV_LOG("[%s:%d] QSPI_IN_SYNC_MODE_MTK", __func__, __LINE__);
            break;

        case QSPI_SENSOR_IN_SYNC_MODE_ALL:
            qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.ALL_DATA_REC_EN = 1;
            //DEV_LOG("[%s:%d] QSPI_IN_SYNC_MODE_ALL", __func__, __LINE__);
            break;

        default:
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    qspi_sensor_in_dev->Instance->REG_SYNC_CODE.all = pCfg->sync_code.sync_code;
    qspi_sensor_in_dev->Instance->REG_PACKET_ID.all = (pCfg->sync_code.eof << 24) | (pCfg->sync_code.eol << 16) | \
                                            (pCfg->sync_code.sol << 8) | (pCfg->sync_code.sof);
    // sync_code = {0xFF0000AB, 0xFF0000B6, 0xFF000080, 0xFF00009D}, // sof/eof/sol/eol

    qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.DATALEN = 31;

    if (true == pCfg->is_lsb) {
        qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.LSB = 1;
    } else {
        qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.LSB = 0;
    }

    if (true == pCfg->data_merge) {
        qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.DATAMERGE = 1;
    } else {
        qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.DATAMERGE = 0;
    }

    if (true == pCfg->wire_order) {
        qspi_sensor_in_dev->Instance->REG_CMD.bit.WIRE_TYPE = 1;
    } else {
        qspi_sensor_in_dev->Instance->REG_CMD.bit.WIRE_TYPE = 0;
    }

    /* 0x70 bit7 sync_code_lsb */
    qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.SYNC_CODE_LSB = 1;

    qspi_sensor_in_dev->Instance->REG_TRANSFMT.bit.SLVMODE = 1;
    qspi_sensor_in_dev->Instance->REG_TRANSCTRL.bit.TRANSMODE = 2;
    qspi_sensor_in_dev->Instance->REG_CTRL.bit.RXDMAEN = 1;
    qspi_sensor_in_dev->Instance->REG_CTRL.bit.RXTHRES = 8;

    qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.ERR_INT_EN = 1;

    qspi_sensor_in_dev->cb_event = pCallback;

    /* Init the low level hardware and interrupt */
    register_ISR(IRQ_QSPI_IN_VECTOR, (ISR)QSPI_SENSOR_IN_IRQ_Handler, NULL);
    clear_IRQ(IRQ_QSPI_IN_VECTOR);
    enable_IRQ(IRQ_QSPI_IN_VECTOR);

    return CSK_DRIVER_OK;
}


/**
 * @brief Uninitializes the QSPI_SENSOR_IN device.
 *
 * This function uninitializes the QSPI_SENSOR_IN device by performing a reset and disabling the VI.
 *
 * @param pDev A pointer to the QSPI_SENSOR_IN device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t QSPI_SENSOR_IN_Uninitialize(void *pDev)
{
    QSPI_SENSOR_IN_DEV *qspi_sensor_in_dev = safe_qspi_sensor_in_dev(pDev);

    /* Check the QSPI_SENSOR_IN instance */
    if(qspi_sensor_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable QSPI_SENSOR_IN */
    qspi_sensor_in_dev->Instance->REG_SPI_CAMERA_CTRL.bit.ERR_INT_EN = 0;
    IP_QSPI_SENSOR_IN->REG_CTRL.bit.SPI_CS_FROM_REG = 1;
    IP_QSPI_SENSOR_IN->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;
    IP_QSPI_SENSOR_IN->REG_SLVST.bit.READY = 0;

    disable_IRQ(IRQ_QSPI_IN_VECTOR);

    /* IRQ disable */
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.RXFIFOORINTEN      = 0;  // bit 0~0
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.TXFIFOURINTEN      = 0;  // bit 1~1
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.RXFIFOINTEN        = 0;  // bit 2~2
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.TXFIFOINTEN        = 0;  // bit 3~3
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.ENDINTEN           = 0;  // bit 4~4
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.SLVCMDEN           = 0;  // bit 5~5
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.ERRINTEN           = 0;  // bit 6~6
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.FRAME_START_INT_EN = 0;  // bit 7~7
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.FRAME_END_INT_EN   = 0;  // bit 8~8
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.LINE_END_INT_EN    = 0;  // bit 9~9
    qspi_sensor_in_dev->Instance->REG_INTREN.bit.LINE_START_INT_EN  = 0;  // bit 10~10

    /* IRQ Clear */
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.RXFIFOORINT        = 1;  // bit 0~0
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.TXFIFOURINT        = 1;  // bit 1~1
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.RXFIFOINT          = 1;  // bit 2~2
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.TXFIFOINT          = 1;  // bit 3~3
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.ENDINT             = 1;  // bit 4~4
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.SLVCMDINT          = 1;  // bit 5~5
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.ERRINT             = 1;  // bit 6~6
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.FRAME_START_INT    = 1;  // bit 7~7
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.FRAME_END_INT      = 1;  // bit 8~8
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.LINE_END_INT       = 1;  // bit 9~9
    qspi_sensor_in_dev->Instance->REG_INTRST.bit.LINE_START_INT     = 1;  // bit 10~10

    //QSPI_SENSOR_IN_Reset();

    return CSK_DRIVER_OK;
}


/**
 * @brief Start the QSPI_SENSOR_IN to capture video frames.
 *
 * @param pDev A pointer to the QSPI_SENSOR_IN device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t QSPI_SENSOR_IN_Start(void *pDev)
{
    QSPI_SENSOR_IN_DEV *qspi_sensor_in_dev = safe_qspi_sensor_in_dev(pDev);

    /* Check the QSPI_SENSOR_IN instance */
    if(qspi_sensor_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    qspi_sensor_in_dev->Instance->REG_CTRL.bit.SPI_CS_FROM_REG = 0;
    qspi_sensor_in_dev->Instance->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;
    qspi_sensor_in_dev->Instance->REG_SLVST.bit.READY = 1;

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stops the QSPI_SENSOR_IN and releases its resources.
 *
 * @param pDev A pointer to the QSPI_SENSOR_IN device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t QSPI_SENSOR_IN_Stop(void *pDev)
{
    QSPI_SENSOR_IN_DEV *qspi_sensor_in_dev = safe_qspi_sensor_in_dev(pDev);

    /* Check the QSPI_SENSOR_IN instance */
    if(qspi_sensor_in_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    qspi_sensor_in_dev->Instance->REG_CTRL.bit.SPI_CS_FROM_REG = 1;
    qspi_sensor_in_dev->Instance->REG_CTRL.bit.SPI_CS_REG_CFG_EN = 1;
    qspi_sensor_in_dev->Instance->REG_SLVST.bit.READY = 0;

    return CSK_DRIVER_OK;
}


