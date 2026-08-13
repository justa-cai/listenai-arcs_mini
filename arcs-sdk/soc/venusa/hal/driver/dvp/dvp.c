/** @file dvp.c
 *  @brief Digital Video Processor (DVP) driver implementation file.
 *         Contains interrupt handlers, initialization/finalization routines,
 *         and core control functions for the DVP peripheral.
 *  @details This file implements low-level interfaces to the Image VIC hardware block,
 *           including frame capture control, clock management, and error handling.
 *  @author Your Name
 *  @version 1.0
 *  @date YYYY-MM-DD
 *  @copyright Company Name Reserved
 */

#include <string.h>
#include <stdint.h>
#include "venusa_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "dvp.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG

/** @brief Driver version number definition */
#define CSK_DVP_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

/** @brief Global driver version structure */
static const
CSK_DRIVER_VERSION dvp_driver_version = { CSK_DVP_API_VERSION, CSK_DVP_DRV_VERSION };

//------------------------------------------------------------------------------------------

/** @brief DVP device instance structure with memory attributes */
_FAST_DATA_VI static DVP_DEV dvp0_dev = {
        ((IMAGE_VIC_RegDef *)(DVP_BASE)),
        {0},
        NULL,
        0,
};

//------------------------------------------------------------------------------------
/** @brief Resets the DVP peripheral
 *  @details Currently disabled reset sequence (placeholder for future implementation)
 */
static inline void DVP_Reset(void)
{
    __HAL_CRM_VIC_CLK_ENABLE();  // IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_VIC_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.DVP_RESET = 1;
}

/** @brief Safely validates DVP device pointer
 *  @param[in] dvp_dev Device context pointer to validate
 *  @return Validated DVP_DEV pointer or NULL if invalid
 *  @note Verifies both magic number matching and proper instance registration
 */
_FAST_FUNC_RO static DVP_DEV * safe_dvp_dev(void *dvp_dev)
{
    //TODO: safe check of DVP device parameter
    if (dvp_dev == DVP0()) {
        if (((DVP_DEV *)dvp_dev)->Instance != ((IMAGE_VIC_RegDef *)(DVP_BASE))) {
            //DEV_LOG("DVP0 device context has been tampered illegally!!\n");
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

/** @brief Interrupt Service Routine for DVP events
 *  @details Handles all image processing interrupts including frame completion,
 *           line synchronization errors, and buffer status events. Clears respective
 *           interrupt flags after processing each event.
 */
_FAST_FUNC_RO void DVP_IRQ_Handler(void)
{
    DVP_DEV *dvp_dev = (DVP_DEV *)DVP0();
    uint32_t status = dvp_dev->Instance->REG_IMAGE_VIC_INT_STATUS.all;

    /* clear */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_CLR.all = status;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

     /* Frame complete interrupt management */
    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_SOF_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_SOF_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_SOF, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_FRAME_FINISH_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_FRAME_FINISH_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FRAME_FINISH, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_EOF_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_EOF_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_EOF, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_EOF_CNT_ABNOR_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_EOF_CNT_ABNOR_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_EOF_CNT_ABNOR, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_H_SYNC_ABNOR_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_H_SYNC_ABNOR_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_H_SYNC_ABNOR, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_PIXEL_ABNOR_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_PIXEL_ABNOR_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_PIXEL_ABNOR, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_DMA_VIC_SINGLE_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_DMA_VIC_SINGLE_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_DMA_VIC_SINGLE, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_DMA_VIC_REQ_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_DMA_VIC_REQ_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_DMA_VIC_REQ, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_UNFLOW_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_UNFLOW_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_UNFLOW, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_OVFLOW_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_OVFLOW_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_OVFLOW, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_RD_EMPTY_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_RD_EMPTY_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_RD_EMPTY, (uint32_t)&dvp_dev->Init);
    }

    if((status & IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_WR_FULL_ISR_Msk) == IMAGE_VIC_IMAGE_VIC_INT_STATUS_FIFO_WR_FULL_ISR_Msk)
    {
        if(dvp_dev->cb_event)
            dvp_dev->cb_event(DVP_IRQ_EVENT_FIFO_WR_FULL, (uint32_t)&dvp_dev->Init);
    }
}

/* Exported functions --------------------------------------------------------*/

/** @brief Returns DVP driver version information
 *  @return CSK_DRIVER_VERSION Structure containing API and driver versions
 */
CSK_DRIVER_VERSION DVP_GetVersion(void)
{
    return dvp_driver_version;
}

/** @brief Gets base address of primary DVP instance
 *  @return Void pointer to DVP device structure
 */
void* DVP0(void)
{
    return &dvp0_dev;
}

/** @brief Gets buffer address for DVP data transfers
 *  @return Physical address of DVP buffer region
 */
uint32_t DVP0_Buf(void)
{
    return DVP_BUF;
}

/** @brief Initializes the DVP peripheral with specified parameters
 *  @param[in] pDev DVP device context pointer
 *  @param[in] callback Event callback function
 *  @param[in] pCfg Configuration structure
 *  @return CSK_DRIVER_OK on success, error code otherwise
 *  @details Sets up video format, resolution, synchronization polarities,
 *           and DMA burst thresholds. Validates input parameters before applying configuration.
 */
int32_t DVP_Initialize(void *pDev, DVP_SignalEvent_t callback, DVP_InitTypeDef *pCfg)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDev);

    /* Check the DVP instance */
    if (dvp_dev == NULL) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (pCfg == NULL) {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Check if a valid width or height */
    if ((pCfg->FrameWidth == 0) || (pCfg->FrameHeight == 0)) {
        DEV_LOG("[%s:%d] Error input: FrameWidth is %d, FrameHeight is %d", __func__, __LINE__, dvp_dev->Init.FrameWidth, dvp_dev->Init.FrameHeight);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    DVP_Reset();
    //dvp_dev->Instance->REG_SOFT_CLR.all = 1;

    memcpy(&dvp_dev->Init, pCfg, sizeof(DVP_InitTypeDef));

    /* Disable VIC first before configuration */
    dvp_dev->Instance->REG_VIC_EN.bit.VIC_EN = 0;

    /* Configure the VIC frame */
    dvp_dev->Instance->REG_F_HOR.all = (uint32_t)(dvp_dev->Init.FrameWidth);
    dvp_dev->Instance->REG_F_VER.all = (uint32_t)(dvp_dev->Init.FrameHeight);

    /* Configure the VIC input offset */
    dvp_dev->Instance->REG_P_OFFSET.all = (uint32_t)(dvp_dev->Init.PixelOffset);
    dvp_dev->Instance->REG_L_OFFSET.all = (uint32_t)(dvp_dev->Init.LineOffset);

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

        case DVP_INPUT_FORM_YUV444_Y0CBCR:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_YUV444_Y0CBCR;
            break;

        case DVP_INPUT_FORM_RGB555_RGGB:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_RGB555;
            break;

        case DVP_INPUT_FORM_RGB555_GBRG:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_RGB555;
            break;

        case DVP_INPUT_FORM_RGB565_RGGB:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_RGB565;
            break;

        case DVP_INPUT_FORM_RGB565_GBRG:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_RGB565;
            break;

        case DVP_INPUT_FORM_RGB888:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_RGB888;
            break;

        case DVP_INPUT_FORM_LUMINA_8BIT:
            dvp_dev->Instance->REG_INPUT_FORM.all = CSK_DVP_INPUT_FORM_LUMINA_8BIT;
            break;

        default:
            DEV_LOG("[%s:%d] Error input: InputFormat is %#x", __func__, __LINE__, dvp_dev->Init.InputFormat);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint32_t ctrl_value = 0;

    /* only for RGB555/565:  0:R5G3_G3B5 1:G3B5_R5G3 */
    if ((dvp_dev->Init.InputFormat == DVP_INPUT_FORM_RGB555_GBRG) || (dvp_dev->Init.InputFormat == DVP_INPUT_FORM_RGB565_GBRG)) {
        ctrl_value |= IMAGE_VIC_IMAGE_VIC_CTRL_RG_GB_REVERSE_Msk;
    }

    /* Configure the out clock polarity */
    if (DVP_POL_FALLING == dvp_dev->Init.PCKPolarity) {
        ctrl_value |= IMAGE_VIC_IMAGE_VIC_CTRL_SAMPLE_EDGE_SEL_Msk;
    }

    /* Configure the VSync polarity */
    if (DVP_POL_FALLING == dvp_dev->Init.VSPolarity) {
        ctrl_value |= IMAGE_VIC_IMAGE_VIC_CTRL_VSEL_V_SYNC_Msk;
    }

    /* Configure the HSync polarity */
    if (DVP_POL_FALLING == dvp_dev->Init.HSPolarity) {
        ctrl_value |= IMAGE_VIC_IMAGE_VIC_CTRL_VSEL_H_SYNC_Msk;
    }

    /* Configure the data alignment */
    if (DVP_DATA_ALIGN_LEFT == dvp_dev->Init.DataAlign) {      //MSB
        ctrl_value |= IMAGE_VIC_IMAGE_VIC_CTRL_DATA_BUS_ALIGN_Msk;
    }

    dvp_dev->Instance->REG_IMAGE_VIC_CTRL.all = ctrl_value;

    /* Configure the DMA burst threshold (0~63) */
    dvp_dev->Instance->REG_DMA_BURST_THD.all = (1 << IMAGE_VIC_DMA_BURST_THD_DMA_REQ_ENABLE_Pos) | \
                                                (0 << IMAGE_VIC_DMA_BURST_THD_DMA_SINGLE_ENABLE_Pos) | \
                                                (dvp_dev->Init.BurstThreshold & IMAGE_VIC_DMA_BURST_THD_DMA_BURST_THD_Msk);

    /* IRQ mask */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_MASK.all = 0xFFF;

    /* IRQ Clear */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_CLR.all = 0xFFF;

    /* Set up callback */
    dvp_dev->cb_event = callback;

    /* Init the low level hardware and interrupt */
    register_ISR(IRQ_DVP_VECTOR, (ISR)DVP_IRQ_Handler, NULL);
    clear_IRQ(IRQ_DVP_VECTOR);
    enable_IRQ(IRQ_DVP_VECTOR);

    /* Initialize the DVP state*/
    dvp_dev->State = DVP_STATE_READY;

    return CSK_DRIVER_OK;
}

/** @brief Uninitializes the DVP peripheral
 *  @param[in] pDev DVP device context pointer
 *  @return CSK_DRIVER_OK on success, error code otherwise
 *  @details Powers down the peripheral, disables interrupts, and resets state machines.
 */
int32_t DVP_Uninitialize(void *pDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable VIC */
    dvp_dev->Instance->REG_VIC_EN.bit.VIC_EN = 0;

    /* IRQ mask */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_MASK.all = 0xFFF;

    /* IRQ Clear */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_CLR.all = 0xFFF;

    disable_IRQ(IRQ_DVP_VECTOR);
    clear_IRQ(IRQ_DVP_VECTOR);

    DVP_Reset();

    dvp_dev->State = DVP_STATE_RESET;

    return CSK_DRIVER_OK;
}

/** @brief Starts video capture operation
 *  @param[in] pDev DVP device context pointer
 *  @return CSK_DRIVER_OK on success, error code otherwise
 *  @pre The device must be in READY state
 *  @post Enables VIC engine and interrupt processing
 */
int32_t DVP_Start(void *pDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(dvp_dev->State != DVP_STATE_READY)
    {
        DEV_LOG("[%s:%d] Error state: dvp state not ready, now is %d, ", __func__, __LINE__, dvp_dev->State);
        return CSK_DRIVER_ERROR;
    }

    /* Lock the DVP peripheral state */
    dvp_dev->State = DVP_STATE_BUSY;

    /* FIFO Clear */
    //dvp_dev->Instance->REG_IMAGE_VIC_CTRL.all |= IMAGE_VIC_IMAGE_VIC_CTRL_FIFO_RD_CLR_Msk | IMAGE_VIC_IMAGE_VIC_CTRL_FIFO_WR_CLR_Msk;

    /* IRQ mask */
    if (dvp_dev->cb_event) {
        dvp_dev->Instance->REG_IMAGE_VIC_INTR_MASK.all = (1 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_DMA_VIC_SINGLE_MASK_Pos) | \
                                                         (1 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_DMA_VIC_REQ_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_FIFO_UNFLOW_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_FIFO_OVFLOW_MASK_Pos) | \
                                                         (1 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_FIFO_RD_EMPTY_MASK_Pos) | \
                                                         (1 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_FIFO_WR_FULL_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_EOF_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_SOF_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_EOF_CNT_ABNOR_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_FRAME_FINISH_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_H_SYNC_ABNOR_MASK_Pos) | \
                                                         (0 << IMAGE_VIC_IMAGE_VIC_INTR_MASK_PIXEL_ABNOR_MASK_Pos);
    } else {
        dvp_dev->Instance->REG_IMAGE_VIC_INTR_MASK.all = 0xFFF;
    }

    /* IRQ Clear */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_CLR.all = 0xFFF;

    /* Enable Capture */
    //dvp_dev->Instance->REG_SOFT_CLR.all = 1;
    dvp_dev->Instance->REG_VIC_EN.bit.VIC_EN = 1;

    /* Return function status */
    return CSK_DRIVER_OK;
}

/** @brief Stops video capture operation
 *  @param[in] pDev DVP device context pointer
 *  @return CSK_DRIVER_OK on success, error code otherwise
 *  @details Halts capture process, disables interrupts, and flushes FIFOs
 */
int32_t DVP_Stop(void *pDev)
{
    DVP_DEV *dvp_dev = safe_dvp_dev(pDev);

    /* Check the DVP instance */
    if(dvp_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Lock the DVP peripheral state */
    dvp_dev->State = DVP_STATE_BUSY;

    /* Disable VIC */
    dvp_dev->Instance->REG_VIC_EN.bit.VIC_EN = 0;

    /* IRQ mask */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_MASK.all = 0xFFF;

    /* IRQ Clear */
    dvp_dev->Instance->REG_IMAGE_VIC_INTR_CLR.all = 0xFFF;

    /* FIFO Clear */
    //dvp_dev->Instance->REG_IMAGE_VIC_CTRL.all |= IMAGE_VIC_IMAGE_VIC_CTRL_FIFO_RD_CLR_Msk | IMAGE_VIC_IMAGE_VIC_CTRL_FIFO_WR_CLR_Msk;

    /* Change DVP state */
    dvp_dev->State = DVP_STATE_READY;

    return CSK_DRIVER_OK;
}


static uint32_t clk_div_get(uint32_t src_clk, uint32_t freq_hz)
{
    uint32_t divider = 0;
    uint32_t freq_big = 0;
    uint32_t freq_lit = 0;

    divider = src_clk / freq_hz;
    if(divider == 0) {
        return 1;
    }

    freq_big = src_clk / divider;
    freq_lit = src_clk / (divider + 1);

    if(((freq_big - freq_hz) > (freq_hz - freq_lit))) {
        return (divider + 1);
    } else {
        return divider;
    }
}

/** @brief Enables DVP clock output signal
 *  @param[in] freq_hz Desired output frequency in Hertz
 *  @return CSK_DRIVER_OK on success, error code otherwise
 *  @details Frequency range clamped between min/max limits (47KHz-200MHz).
 */
int32_t DVP_EnableClockout(uint32_t freq_hz)
{
    uint32_t div_24MHz = 0;
    uint32_t freq_24MHz = 0;
    uint32_t div_100MHz = 0;
    uint32_t freq_100MHz = 0;

    if(freq_hz < DVP_CLK_OUT_HZ_MIN) {
        freq_hz = DVP_CLK_OUT_HZ_MIN;
    } else if(freq_hz > DVP_CLK_OUT_HZ_MAX) {
        freq_hz = DVP_CLK_OUT_HZ_MAX;
    } else {
    }

    div_24MHz = clk_div_get( DVP_CLK_OUT_SEL_24MHz, freq_hz);
    freq_24MHz =  DVP_CLK_OUT_SEL_24MHz / div_24MHz;
    freq_24MHz = (freq_24MHz >= freq_hz) ? (freq_24MHz - freq_hz) : (freq_hz - freq_24MHz);

    div_100MHz = clk_div_get(DVP_CLK_OUT_SEL_100MHz, freq_hz);
    freq_100MHz = DVP_CLK_OUT_SEL_100MHz / div_100MHz;
    freq_100MHz = (freq_100MHz >= freq_hz) ? (freq_100MHz - freq_hz) : (freq_hz - freq_100MHz);

    if(freq_24MHz <= freq_100MHz) {
        IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.SEL_VIC_OUT_CLK = 0;  // 0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_M = (div_24MHz & DVP_CLK_OUT_DIV_MAX);  //
        //DEV_LOG("[%s:%d] src=%dMHz div=%d clk=%dHz", __func__, __LINE__, DVP_CLK_OUT_SEL_24MHz/1000000, div_24MHz, (DVP_CLK_OUT_SEL_24MHz/div_24MHz));
    } else {
        IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.SEL_VIC_OUT_CLK = 1;  // 0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_M = (div_100MHz & DVP_CLK_OUT_DIV_MAX);  //
        //DEV_LOG("[%s:%d] src=%dMHz div=%d clk=%dHz", __func__, __LINE__, DVP_CLK_OUT_SEL_100MHz/1000000, div_100MHz, (DVP_CLK_OUT_SEL_100MHz/div_100MHz));
    }
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_VIC_CLK        = 0;   // 0:clk    1:clk_inv
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_LD = 1;   // 0:normal  1:inv
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_VIC_OUT_CLK    = 1;

    return CSK_DRIVER_OK;
}

/** @brief Disables DVP clock output signal
 *  @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DVP_DisableClockout(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_VIC_OUT_CLK    = 0; // bit 17~17

    return CSK_DRIVER_OK;
}

