#include <string.h>
#include <stdint.h>
#include "arcs_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "dvp.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG


// driver version
#define CSK_DVP_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION dvp_driver_version = { CSK_DVP_API_VERSION, CSK_DVP_DRV_VERSION };

//------------------------------------------------------------------------------------------


_FAST_DATA_VI static DVP_DEV dvp0_dev = {
        ((IMAGE_VIC_RegDef *)(DVP_BASE)),
        {0},
        NULL,
        0,
        0,
};


//------------------------------------------------------------------------------------
void DVP_Reset(void)
{
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;    // bit21
    IP_AP_CFG->REG_DMA_SEL.bit.SEL_DVP_QSPI = 1;      // bit1  0:qspi_in  1:dvp
    IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 1;        // bit9
}


static DVP_DEV * safe_dvp_dev(void *dvp_dev)
{
    //TODO: safe check of DVP device parameter
    if (dvp_dev == DVP0()) {
        if (((DVP_DEV *)dvp_dev)->Instance != ((IMAGE_VIC_RegDef *)(DVP_BASE))) {
            //CLOGW("DVP0 device context has been tampered illegally!!\n");
            DEV_LOG("[%s:%d] Error dvp_dev=%#x ", __func__, __LINE__, dvp_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error dvp_dev=%#x ", __func__, __LINE__, dvp_dev);
        return NULL;
    }

    return (DVP_DEV *)dvp_dev;
}

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Handles DVP interrupt request.
  * @retval None
  */
_FAST_FUNC_RO void DVP_IRQ_Handler()
{
    DVP_DEV *dvp_dev = (DVP_DEV *)DVP0();
    uint32_t status = dvp_dev->Instance->REG_VIC_INT_STATUS.all;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

     /* Frame complete interrupt management */
    if((status & IMAGE_VIC_VIC_INT_STATUS_SOF_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_SOF_ISR_Msk)
    {
//        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_WR_CLR = 1;
//        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_CLR = 1;
        dvp_dev->Instance->REG_INTR_CLR.bit.SOF_CLR = 1;
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_SOF, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_EOF_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_EOF_ISR_Msk)
    {
//        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_WR_CLR = 1;
//        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_CLR = 1;
        dvp_dev->Instance->REG_INTR_CLR.bit.EOF_CLR = 1;
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_EOF, 0);
    }


    if((status & IMAGE_VIC_VIC_INT_STATUS_EOF_CNT_ABNOR_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_EOF_CNT_ABNOR_ISR_Msk)
    {
//        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_WR_CLR = 1;
//        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_CLR = 1;
        dvp_dev->Instance->REG_INTR_CLR.bit.EOF_CNT_ABNOR_CLR = 1;

        /* Update error code */
        dvp_dev->ErrorCode |= DVP_ERROR_EOF_CNT_ABNOR;

        /* Change the state */
        dvp_dev->State = DVP_STATE_ERROR;

        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_EOF_CNT_ABNOR, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_DMA_VIC_SINGLE_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_DMA_VIC_SINGLE_ISR_Msk)
    {
        dvp_dev->Instance->REG_INTR_CLR.bit.DMA_VIC_SINGLE_CLR = 1;
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_DMA_VIC_SINGLE, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_DMA_VIC_REQ_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_DMA_VIC_REQ_ISR_Msk)
    {
        dvp_dev->Instance->REG_INTR_CLR.bit.DMA_VIC_REQ_CLR = 1;
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_DMA_VIC_REQ, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_FIFO_UNFLOW_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_FIFO_UNFLOW_ISR_Msk)
    {
        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_UNFLOW_CLR = 1;
        /* Update error code */
        dvp_dev->ErrorCode |= DVP_ERROR_FIFO_UNFLOW;

        /* Change the state */
        dvp_dev->State = DVP_STATE_ERROR;

        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_UNFLOW, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_FIFO_OVFLOW_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_FIFO_OVFLOW_ISR_Msk)
    {
        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_OVFLOW_CLR = 1;
        /* Update error code */
        dvp_dev->ErrorCode |= DVP_ERROR_FIFO_OVFLOW;

        /* Change the state */
        dvp_dev->State = DVP_STATE_ERROR;

        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_OVFLOW, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_FIFO_RD_EMPTY_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_FIFO_RD_EMPTY_ISR_Msk)
    {
        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_EMPTY_CLR = 1;
        /* Update error code */
        dvp_dev->ErrorCode |= DVP_ERROR_FIFO_RD_EMPTY;

        /* Change the state */
        dvp_dev->State = DVP_STATE_ERROR;

        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_RD_EMPTY, 0);
    }

    if((status & IMAGE_VIC_VIC_INT_STATUS_FIFO_RD_FULL_ISR_Msk) == IMAGE_VIC_VIC_INT_STATUS_FIFO_RD_FULL_ISR_Msk)
    {
        dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_FULL_CLR = 1;
        /* Update error code */
        dvp_dev->ErrorCode |= DVP_ERROR_FIFO_RD_FULL;

        /* Change the state */
        dvp_dev->State = DVP_STATE_ERROR;

        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_RD_FULL, 0);
    }
}

/* Exported functions --------------------------------------------------------*/


/**
  * @brief  Return DVP driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
DVP_GetVersion(void)
{
    return dvp_driver_version;
}


/**
  * @brief  Return DVP instance.
  *
  * @return Instance of DVP
  */
void* DVP0(void)
{
    return &dvp0_dev;
}


/**
  * @brief  Return DVP BUF instance.
  *
  * @return Instance of DVP
  */
uint32_t DVP0_Buf(void)
{
    return VIC_BUF;
}


/**
 * @brief Initialize the DVP (Digital Video Processor) device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pCallback The callback function for DVP events.
 * @param pCfg The configuration structure for the DVP device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t DVP_Initialize(void *pDvpDev, void *pCallback, DVP_InitTypeDef *pCfg)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    DVP_Reset();

    memcpy(&dvp_dev->Init, pCfg, sizeof(DVP_InitTypeDef));

//    DEV_LOG("[%s:%d] FrameWidth=%d", __func__, __LINE__, dvp_dev->Init.FrameWidth);
//    DEV_LOG("[%s:%d] FrameHeight=%d", __func__, __LINE__, dvp_dev->Init.FrameHeight);
//    DEV_LOG("[%s:%d] InputFormat=%d", __func__, __LINE__, dvp_dev->Init.InputFormat);

    /* Check if a valid width or height */
    if ((dvp_dev->Init.FrameWidth == 0) || (dvp_dev->Init.FrameHeight == 0))
    {
        DEV_LOG("[%s:%d] Error input: FrameWidth is %d, FrameHeight is %d", __func__, __LINE__, dvp_dev->Init.FrameWidth, dvp_dev->Init.FrameHeight);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Check if a valid width - multiplier of 16 */
    switch(dvp_dev->Init.InputFormat)
    {
        case DVP_INPUT_FORM_YUV422_Y0CBY1CR:
        case DVP_INPUT_FORM_YUV422_CBY0CRY1:
        case DVP_INPUT_FORM_YUV422_Y0CRY1CB:
        case DVP_INPUT_FORM_YUV422_CRY0CBY1:
            if ((dvp_dev->Init.FrameWidth % 8) != 0)
            {
                DEV_LOG("[%s:%d] Error input: FrameWidth must be divisible by 8, but now is %d", __func__, __LINE__, dvp_dev->Init.FrameWidth);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        case DVP_INPUT_FORM_YUV420_Y0CBY1Y2CRY3:
        case DVP_INPUT_FORM_YUV420_CBY0Y1CRY2Y3:
        case DVP_INPUT_FORM_YUV420_Y0Y1CBY2Y3CR:
        case DVP_INPUT_FORM_YUV444_Y0CBCR:
        case DVP_INPUT_FORM_LUMINA_12BIT:
        case DVP_INPUT_FORM_LUMINA_10BIT:
        case DVP_INPUT_FORM_LUMINA_8BIT:
            if ((dvp_dev->Init.FrameWidth % 16) != 0)
            {
                DEV_LOG("[%s:%d] Error input: FrameWidth must be divisible by 16, but now is %d", __func__, __LINE__, dvp_dev->Init.FrameWidth);
                return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        default:
            DEV_LOG("[%s:%d] Error input: InputFormat is %#x", __func__, __LINE__, dvp_dev->Init.InputFormat);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable VIC first before configuration */
    dvp_dev->Instance->REG_VI_EN.bit.VI_EN = 0;

    /* IRQ mask, but enable sof and eof */
    dvp_dev->Instance->REG_INTR_MSK.bit.DMA_VIC_SINGLE_MASK  = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.DMA_VIC_REQ_MASK     = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_UNFLOW_MASK     = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_OVFLOW_MASK     = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_RD_EMPTY_MASK   = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_RD_FULL_MASK    = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.EOF_MASK             = 0;
    dvp_dev->Instance->REG_INTR_MSK.bit.SOF_MASK             = 0;
    dvp_dev->Instance->REG_INTR_MSK.bit.EOF_CNT_ABNOR_MASK   = 1;

    /* IRQ Clear */
    dvp_dev->Instance->REG_INTR_CLR.bit.DMA_VIC_SINGLE_CLR   = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.DMA_VIC_REQ_CLR      = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_UNFLOW_CLR      = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_OVFLOW_CLR      = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_EMPTY_CLR    = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_FULL_CLR     = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.EOF_CLR              = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.SOF_CLR              = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_WR_CLR          = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_CLR          = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.EOF_CNT_ABNOR_CLR    = 1;

    if(dvp_dev->State == DVP_STATE_RESET)
    {
        /* Init the low level hardware and interrupt */
        register_ISR(IRQ_DVP_VECTOR, (ISR)DVP_IRQ_Handler, NULL);
        clear_IRQ(IRQ_DVP_VECTOR);
        enable_IRQ(IRQ_DVP_VECTOR);
    }

    /* Change the DVP state */
    dvp_dev->State = DVP_STATE_BUSY;

    /* Configure the VIC input format */
    switch(dvp_dev->Init.InputFormat)
    {
        case DVP_INPUT_FORM_YUV422_Y0CBY1CR:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV422_Y0CBY1CR;
            break;

        case DVP_INPUT_FORM_YUV422_CBY0CRY1:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV422_CBY0CRY1;
            break;

        case DVP_INPUT_FORM_YUV422_Y0CRY1CB:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV422_Y0CRY1CB;
            break;

        case DVP_INPUT_FORM_YUV422_CRY0CBY1:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV422_CRY0CBY1;
            break;

        case DVP_INPUT_FORM_YUV420_Y0CBY1Y2CRY3:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV420_Y0CBY1Y2CRY3;
            break;

        case DVP_INPUT_FORM_YUV420_CBY0Y1CRY2Y3:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV420_CBY0Y1CRY2Y3;
            break;

        case DVP_INPUT_FORM_YUV420_Y0Y1CBY2Y3CR:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV420_Y0Y1CBY2Y3CR;
            break;

        case DVP_INPUT_FORM_YUV444_Y0CBCR:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV444_Y0CBCR;
            break;

        case DVP_INPUT_FORM_LUMINA_12BIT:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_LUMINA_12BIT;
            break;

        case DVP_INPUT_FORM_LUMINA_10BIT:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_LUMINA_10BIT;
            break;

        case DVP_INPUT_FORM_LUMINA_8BIT:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_LUMINA_8BIT;
            break;

        default:
            DEV_LOG("[%s:%d] Error input: InputFormat is %#x", __func__, __LINE__, dvp_dev->Init.InputFormat);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Configure the VIC frame */
    dvp_dev->Instance->REG_F_HOR.all = (uint32_t)(dvp_dev->Init.FrameWidth);
    dvp_dev->Instance->REG_F_VER.all = (uint32_t)(dvp_dev->Init.FrameHeight);

    /* Configure the VIC input offset */
    dvp_dev->Instance->REG_P_OFFSET.all = (uint32_t)(dvp_dev->Init.PixelOffset);
    dvp_dev->Instance->REG_L_OFFSET.all = (uint32_t)(dvp_dev->Init.LineOffset);

    /* Configure the out clock polarity */
    if (DVP_POL_RISING == dvp_dev->Init.PCKPolarity) {
        dvp_dev->Instance->REG_POL_CNTL.bit.SAMPLE_EDGE_SEL = CSK_DVP_POL_CNTL_CLOCK_RISING;
    } else if (DVP_POL_FALLING == dvp_dev->Init.PCKPolarity) {
        dvp_dev->Instance->REG_POL_CNTL.bit.SAMPLE_EDGE_SEL = CSK_DVP_POL_CNTL_CLOCK_FALLING;
    } else {
        DEV_LOG("[%s:%d] Error input: PCKPolarity is %#x", __func__, __LINE__, dvp_dev->Init.PCKPolarity);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Configure the VSync polarity */
    if (DVP_POL_RISING == dvp_dev->Init.VSPolarity) {
        dvp_dev->Instance->REG_POL_CNTL.bit.VSEL_V_SYNC = CSK_DVP_POL_CNTL_VSYNC_RISING;
    } else if (DVP_POL_FALLING == dvp_dev->Init.VSPolarity) {
        dvp_dev->Instance->REG_POL_CNTL.bit.VSEL_V_SYNC = CSK_DVP_POL_CNTL_VSYNC_FALLING;
    } else {
        DEV_LOG("[%s:%d] Error input: VSPolarity is %#x", __func__, __LINE__, dvp_dev->Init.VSPolarity);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Configure the HSync polarity */
    if (DVP_POL_RISING == dvp_dev->Init.HSPolarity) {
        dvp_dev->Instance->REG_POL_CNTL.bit.VSEL_H_SYNC = CSK_DVP_POL_CNTL_HSYNC_RISING;
    } else if (DVP_POL_FALLING == dvp_dev->Init.HSPolarity) {
        dvp_dev->Instance->REG_POL_CNTL.bit.VSEL_H_SYNC = CSK_DVP_POL_CNTL_HSYNC_FALLING;
    } else {
        DEV_LOG("[%s:%d] Error input: HSPolarity is %#x", __func__, __LINE__, dvp_dev->Init.HSPolarity);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Configure the data alignment */
    if (DVP_DATA_ALIGN_RIGHT == dvp_dev->Init.DataAlign) {            //LSB
        dvp_dev->Instance->REG_CLK_OUTEN.bit.DATA_BUS_ALIGN = CSK_DVP_DATA_BUS_ALIGN_LSB;
    } else if (DVP_DATA_ALIGN_LEFT == dvp_dev->Init.DataAlign) {      //MSB
        dvp_dev->Instance->REG_CLK_OUTEN.bit.DATA_BUS_ALIGN = CSK_DVP_DATA_BUS_ALIGN_MSB;
    } else {
        DEV_LOG("[%s:%d] Error input: DataAlign is %#x", __func__, __LINE__, dvp_dev->Init.DataAlign);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Configure the DMA burst threshold (0~63) */
    dvp_dev->Instance->REG_DMA_BURST_THD.bit.DMA_BURST_THD = CSK_DVP_DMA_BURST_THD;

    /* external_rstn_mask 0:useful 1:mask */
    //dvp_dev->Instance->REG_JLB_HANSHK_SEL.bit.EXTERNAL_RSTN_MASK = 0;

    /* Set up callback */
    dvp_dev->cb_event = (CSK_DVP_SignalEvent_t)pCallback;

    /* Update error code */
    dvp_dev->ErrorCode = DVP_ERROR_NONE;

    /* Initialize the DVP state*/
    dvp_dev->State  = DVP_STATE_READY;

    return CSK_DRIVER_OK;
}


/**
 * @brief Uninitializes the DVP device.
 *
 * This function uninitializes the DVP device by performing a reset and disabling the VI.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t DVP_Uninitialize(void *pDvpDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable VIC */
    dvp_dev->Instance->REG_VI_EN.bit.VI_EN = 0;

    disable_IRQ(IRQ_DVP_VECTOR);

    /* IRQ mask, but enable sof and eof */
    dvp_dev->Instance->REG_INTR_MSK.bit.DMA_VIC_SINGLE_MASK  = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.DMA_VIC_REQ_MASK     = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_UNFLOW_MASK     = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_OVFLOW_MASK     = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_RD_EMPTY_MASK   = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.FIFO_RD_FULL_MASK    = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.EOF_MASK             = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.SOF_MASK             = 1;
    dvp_dev->Instance->REG_INTR_MSK.bit.EOF_CNT_ABNOR_MASK   = 1;

    /* IRQ Clear */
    dvp_dev->Instance->REG_INTR_CLR.bit.DMA_VIC_SINGLE_CLR   = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.DMA_VIC_REQ_CLR      = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_UNFLOW_CLR      = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_OVFLOW_CLR      = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_EMPTY_CLR    = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_FULL_CLR     = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.EOF_CLR              = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.SOF_CLR              = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_WR_CLR          = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.FIFO_RD_CLR          = 1;
    dvp_dev->Instance->REG_INTR_CLR.bit.EOF_CNT_ABNOR_CLR    = 1;

    DVP_Reset();

    dvp_dev->ErrorCode = DVP_ERROR_NONE;
    dvp_dev->State  = DVP_STATE_RESET;

    return CSK_DRIVER_OK;
}


/**
 * @brief Start the DVP to capture video frames.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t DVP_Start(void *pDvpDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(dvp_dev->State != DVP_STATE_READY)
    {
        DEV_LOG("[%s:%d] Error state: dvp state not ready, now is %d, ", __func__, __LINE__, dvp_dev->State);
        return CSK_DRIVER_ERROR;
    }

    /* Lock the DVP peripheral state */
    dvp_dev->State = DVP_STATE_BUSY;

    /* Enable Capture */
    dvp_dev->Instance->REG_VI_EN.bit.VI_EN = 1;

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stops the DVP and releases its resources.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t DVP_Stop(void *pDvpDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Lock the DVP peripheral state */
    dvp_dev->State = DVP_STATE_BUSY;

    /* Disable VIC */
    dvp_dev->Instance->REG_VI_EN.bit.VI_EN = 0;

    /* Update error code */
    dvp_dev->ErrorCode = DVP_ERROR_NONE;

    /* Change DVP state */
    dvp_dev->State = DVP_STATE_READY;

    return CSK_DRIVER_OK;
}


/**
 * @brief Enables the DVP clock output.
 *
 * This function enables the DVP clock output with a specified frequency.
 * The clock output frequency is set to hclk/2/(divider+1), hclk=300MHz, divider=0~63.
 * If the frequency setting is greater than 150000000Hz, it is limited to 150000000Hz;
 * If the frequency setting is less than 2343750Hz, it is limited to 2343750Hz;
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param freq_hz The desired frequency of the DVP clock output in Hz.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t DVP_EnableClockout(void *pDvpDev, uint32_t freq_hz)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);
    uint32_t divider = 0;
    uint32_t freq_big = 0;
    uint32_t freq_lit = 0;

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* If the frequency setting is greater than 150000000Hz, it is limited to 150000000Hz; */
    /* If the frequency setting is less than 2343750Hz, it is limited to 2343750Hz; */
    if(freq_hz > CSK_DVP_OUTPUT_CLK_HZ_MAX)
    {
        freq_hz = CSK_DVP_OUTPUT_CLK_HZ_MAX;
    } else if(freq_hz < CSK_DVP_OUTPUT_CLK_HZ_MIN)
    {
        freq_hz = CSK_DVP_OUTPUT_CLK_HZ_MIN;
    } else {
    }

    /* Set the clock output frequency to hclk/2/(divider+1), hclk=300MHz, divider=0~63. */
    divider = (((CSK_DVP_HCLK_HZ / 2)) / freq_hz) - 1;
    freq_big = (CSK_DVP_HCLK_HZ / 2) / (divider + 1);
    freq_lit = (CSK_DVP_HCLK_HZ / 2) / (divider + 2);

    if(((freq_big - freq_hz) > (freq_hz - freq_lit))) {
            divider += 1;
    }

    if(divider > CSK_DVP_DIV_MAX) {
        divider = CSK_DVP_DIV_MAX;
    }

    //DEV_LOG("[%s:%d] DVP out clock freq=%dHz, div=%d", __func__, __LINE__, (CSK_DVP_HCLK_HZ >> 1) / (divider + 1), divider);

    dvp_dev->Instance->REG_JLB_HANSHK_SEL.bit.FREQ_OUT = divider;
    dvp_dev->Instance->REG_CLK_OUTEN.bit.DVP_CLK_OUTEN = 1;

    return CSK_DRIVER_OK;
}


/**
 * @brief Disables the DVP clock output.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t DVP_DisableClockout(void *pDvpDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    dvp_dev->Instance->REG_CLK_OUTEN.bit.DVP_CLK_OUTEN = 0;

    return CSK_DRIVER_OK;
}


/**
 * @brief Get the current state of the DVP device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pState A pointer to a DVP_emState variable where the current state will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t DVP_GetState(void *pDvpDev, DVP_emState *pState)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pState == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pState is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *pState = dvp_dev->State;

    return CSK_DRIVER_OK;
}


/**
 * @brief Get the error code from a DVP device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pError A pointer to a variable where the error code will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t DVP_GetError(void *pDvpDev, uint32_t *pError)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDvpDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDvpDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pError == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pError is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *pError = dvp_dev->ErrorCode;

    return CSK_DRIVER_OK;
}







