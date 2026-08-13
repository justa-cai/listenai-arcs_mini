#include <string.h>
#include <stdint.h>
#include "venusa_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "rgb.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG


// driver version
#define CSK_RGB_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION rgb_driver_version = { CSK_RGB_API_VERSION, CSK_RGB_DRV_VERSION };

//------------------------------------------------------------------------------------------


_FAST_DATA_VI static RGB_DEV rgb0_dev = {
        ((RGB_INTERFACE_RegDef *)(RGB_BASE)),
        {0},
        NULL,
        0,
};


//------------------------------------------------------------------------------------
static inline void RGB_Reset(void)
{
    __HAL_CRM_RGB_CLK_ENABLE();     // IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_RGB_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.RGB_RESET = 1;
}


_FAST_FUNC_RO static RGB_DEV * safe_rgb_dev(void *rgb_dev)
{
    //TODO: safe check of RGB device parameter
    if (rgb_dev == RGB0()) {
        if (((RGB_DEV *)rgb_dev)->Instance != ((RGB_INTERFACE_RegDef *)(RGB_BASE))) {
            //DEV_LOG("RGB0 device context has been tampered illegally!!\n");
            DEV_LOG("[%s:%d] Error rgb_dev=%#x ", __func__, __LINE__, rgb_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error rgb_dev=%#x ", __func__, __LINE__, rgb_dev);
        return NULL;
    }

    return (RGB_DEV *)rgb_dev;
}

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Handles RGB interrupt request.
  * @retval None
  */
_FAST_FUNC_RO void RGB_IRQ_Handler(void)
{
    RGB_DEV *rgb_dev = (RGB_DEV *)RGB0();
    uint32_t status = rgb_dev->Instance->REG_RGB_INTR_STATUS.all;

    /* clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.all = status;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

     /* Frame complete interrupt management */
    if((status & RGB_INTERFACE_RGB_INTR_STATUS_SOF_FLAG_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_SOF_FLAG_ISR_Msk)
    {
        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_SOF, (uint32_t)&rgb_dev->Init);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_EOF_FLAG_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_EOF_FLAG_ISR_Msk)
    {
        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_EOF, (uint32_t)&rgb_dev->Init);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_FULL_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_FULL_ISR_Msk)
    {
        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_WR_FULL, (uint32_t)&rgb_dev->Init);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_EMPTY_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_EMPTY_ISR_Msk)
    {
        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_WR_EMPTY, (uint32_t)&rgb_dev->Init);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_FULL_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_FULL_ISR_Msk)
    {
        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_RD_FULL, (uint32_t)&rgb_dev->Init);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_EMPTY_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_EMPTY_ISR_Msk)
    {
        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_RD_EMPTY, (uint32_t)&rgb_dev->Init);
    }
}

/* Exported functions --------------------------------------------------------*/


/**
  * @brief  Return RGB driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
RGB_GetVersion(void)
{
    return rgb_driver_version;
}


/**
  * @brief  Return RGB instance.
  *
  * @return Instance of RGB
  */
void* RGB0(void)
{
    return &rgb0_dev;
}


/**
  * @brief  Return RGB BUF instance.
  *
  * @return Instance of RGB
  */
uint32_t RGB0_Buf(void)
{
    return RGB_BUF;
}


/**
 * @brief Initialize the RGB (Digital Video Processor) device.
 *
 * @param pDev A pointer to the RGB device structure.
 * @param pCallback The callback function for RGB events.
 * @param pCfg The configuration structure for the RGB device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t RGB_Initialize(void *pDev, RGB_SignalEvent_t callback, RGB_InitTypeDef *pCfg)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pDev);
    uint32_t value = 0;

    /* Check the RGB instance */
    if (rgb_dev == NULL) {
        DEV_LOG("[%s:%d] Error input: pDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (pCfg == NULL) {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    RGB_Reset();
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 0;

    memcpy(&rgb_dev->Init, pCfg, sizeof(RGB_InitTypeDef));

    value = 0;
    if (rgb_dev->Init.CLKPolarity == RGB_POLARITY_POSITIVE) {
        value |= RGB_INTERFACE_RGB_CONTROL0_SEL_RGB_INV_CLK_Msk;
    }
    if (rgb_dev->Init.frms == RGB_FRAME_CONTINUE) {
        value |= RGB_INTERFACE_RGB_CONTROL0_FRMS_EN_Msk;
    }
    if (rgb_dev->Init.out_lsb == true) {
        value |= RGB_INTERFACE_RGB_CONTROL0_DATA_ALIGN_Msk;
    }
    rgb_dev->Instance->REG_RGB_CONTROL0.all = value;

    value = 0;
    if (rgb_dev->Init.sync == RGB_SYNC_MODE_SYNC_DE) {
        value |= (1 << RGB_INTERFACE_RGB_CONTROL1_RGB_MODE_SEL_Pos);
    }
    if (rgb_dev->Init.wires == RGB_OUTPUT_WIRES_8) {
        value |= RGB_INTERFACE_RGB_CONTROL1_RGB_SERIAL_MODE_Msk;
    }
    switch(rgb_dev->Init.format_in)
    {
        case RGB_INPUT_FORMAT_RGB888:
            break;

        case RGB_INPUT_FORMAT_XRGB8888:
            value |= (1 << RGB_INTERFACE_RGB_CONTROL1_RGB_INPUT_FORMAT_SEL_Pos);
            break;

        case RGB_INPUT_FORMAT_RGB565:
            value |= (2 << RGB_INTERFACE_RGB_CONTROL1_RGB_INPUT_FORMAT_SEL_Pos);
            break;

        default:
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    switch(rgb_dev->Init.format_out)
    {
        case RGB_OUTPUT_FORMAT_RGB888:
        case RGB_OUTPUT_FORMAT_BGR888:
            break;

        case RGB_OUTPUT_FORMAT_RGB666:
        case RGB_OUTPUT_FORMAT_BGR666:
            value |= (1 << RGB_INTERFACE_RGB_CONTROL1_RGB_OUTPUT_FORMAT_SEL_Pos);
            break;

        case RGB_OUTPUT_FORMAT_RGB565:
        case RGB_OUTPUT_FORMAT_BGR565:
            value |= (2 << RGB_INTERFACE_RGB_CONTROL1_RGB_OUTPUT_FORMAT_SEL_Pos);
            break;

        default:
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    value |= RGB_INTERFACE_RGB_CONTROL1_FIFO_WR_CLR_Msk | RGB_INTERFACE_RGB_CONTROL1_FIFO_RD_CLR_Msk;
    value |= RGB_INTERFACE_RGB_CONTROL1_RGB_SOFT_RST_Msk;
    rgb_dev->Instance->REG_RGB_CONTROL1.all = value;

    value = 0;
    value |= ((rgb_dev->Init.h_back_blanking << RGB_INTERFACE_SEQUENTIAL_CONTROL0_H_BLANKING_Pos) & RGB_INTERFACE_SEQUENTIAL_CONTROL0_H_BLANKING_Msk);
    value |= ((rgb_dev->Init.v_back_blanking << RGB_INTERFACE_SEQUENTIAL_CONTROL0_V_BLANKING_Pos) & RGB_INTERFACE_SEQUENTIAL_CONTROL0_V_BLANKING_Msk);
    value |= ((rgb_dev->Init.h_pulse_width << RGB_INTERFACE_SEQUENTIAL_CONTROL0_H_PULSE_WIDTH_Pos) & RGB_INTERFACE_SEQUENTIAL_CONTROL0_H_PULSE_WIDTH_Msk);
    value |= ((rgb_dev->Init.v_pulse_width << RGB_INTERFACE_SEQUENTIAL_CONTROL0_V_PULSE_WIDTH_Pos) & RGB_INTERFACE_SEQUENTIAL_CONTROL0_V_PULSE_WIDTH_Msk);
    if ((rgb_dev->Init.format_out == RGB_OUTPUT_FORMAT_BGR888) || (rgb_dev->Init.format_out == RGB_OUTPUT_FORMAT_BGR666) || (rgb_dev->Init.format_out == RGB_OUTPUT_FORMAT_BGR565)) {
        value |= RGB_INTERFACE_SEQUENTIAL_CONTROL0_SBGR_Msk;
    }
    if (rgb_dev->Init.VSPolarity == RGB_POLARITY_NEGATIVE) {
        value |= RGB_INTERFACE_SEQUENTIAL_CONTROL0_VDPOL_Msk;
    }
    if (rgb_dev->Init.HSPolarity == RGB_POLARITY_NEGATIVE) {
        value |= RGB_INTERFACE_SEQUENTIAL_CONTROL0_HDPOL_Msk;
    }
    if (rgb_dev->Init.DEPolarity == RGB_POLARITY_NEGATIVE) {
        value |= RGB_INTERFACE_SEQUENTIAL_CONTROL0_DEPOL_Msk;
    }
    if (rgb_dev->Init.CLKPolarity == RGB_POLARITY_NEGATIVE) {
        value |= RGB_INTERFACE_SEQUENTIAL_CONTROL0_DCLKPOL_Msk;
    }
    switch(rgb_dev->Init.BurstThreshold)   //0:1word 1:2word 2:4word 3:8word  input_RGB565:2/4/8/16  XRGB888:1/2/4/8  RGB888:2/3/6/11
    {
        case 1:
            value |= (0 << RGB_INTERFACE_SEQUENTIAL_CONTROL0_RGB_BURST_THD_Pos);
            break;

        case 2:
            value |= (1 << RGB_INTERFACE_SEQUENTIAL_CONTROL0_RGB_BURST_THD_Pos);
            break;

        case 4:
            value |= (2 << RGB_INTERFACE_SEQUENTIAL_CONTROL0_RGB_BURST_THD_Pos);
            break;

        case 8:
            value |= (3 << RGB_INTERFACE_SEQUENTIAL_CONTROL0_RGB_BURST_THD_Pos);
            break;

        default:
            DEV_LOG("[%s:%d] Error input: BurstThreshold=%d", __func__, __LINE__, rgb_dev->Init.BurstThreshold);
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.all = value;

    value = ((rgb_dev->Init.h_front_blanking << RGB_INTERFACE_SEQUENTIAL_CONTROL1_H_FRONT_BLANKING_Pos) & RGB_INTERFACE_SEQUENTIAL_CONTROL1_H_FRONT_BLANKING_Msk);
    value |= ((rgb_dev->Init.v_front_blanking << RGB_INTERFACE_SEQUENTIAL_CONTROL1_V_FRONT_BLANKING_Pos) & RGB_INTERFACE_SEQUENTIAL_CONTROL1_V_FRONT_BLANKING_Msk);
    if(false == rgb_dev->Init.de_continue) {
        value |= RGB_INTERFACE_SEQUENTIAL_CONTROL1_DE_SEQUENTIAL_SEL_Msk;
    }
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.all = value;

    if (rgb_dev->Init.wires == RGB_OUTPUT_WIRES_8) {
        value = (((rgb_dev->Init.img_width + 1)<< RGB_INTERFACE_IMAGE_SIZE_WIDTH_IN_Pos) & RGB_INTERFACE_IMAGE_SIZE_WIDTH_IN_Msk);
    } else {
        value = ((rgb_dev->Init.img_width << RGB_INTERFACE_IMAGE_SIZE_WIDTH_IN_Pos) & RGB_INTERFACE_IMAGE_SIZE_WIDTH_IN_Msk);
    }
    value |= ((rgb_dev->Init.img_height << RGB_INTERFACE_IMAGE_SIZE_HEIGHT_IN_Pos) & RGB_INTERFACE_IMAGE_SIZE_HEIGHT_IN_Msk);
    rgb_dev->Instance->REG_IMAGE_SIZE.all = value;

    /* IRQ mask */
    rgb_dev->Instance->REG_RGB_INTR_MSK.all = 0x3F;

    /* IRQ Clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.all = 0x3F;

    /* Set up callback */
    rgb_dev->cb_event = callback;

    /* Init the low level hardware and interrupt */
    register_ISR(IRQ_RGB_VECTOR, (ISR)RGB_IRQ_Handler, NULL);
    clear_IRQ(IRQ_RGB_VECTOR);
    enable_IRQ(IRQ_RGB_VECTOR);

    /* Initialize the RGB state*/
    rgb_dev->State  = RGB_STATE_READY;

    return CSK_DRIVER_OK;
}


/**
 * @brief Uninitializes the RGB device.
 *
 * This function uninitializes the RGB device by performing a reset and disabling the VI.
 *
 * @param pDev A pointer to the RGB device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t RGB_Uninitialize(void *pDev)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable RGB */
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.FRMS_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL1.all |= RGB_INTERFACE_RGB_CONTROL1_RGB_SOFT_RST_Msk | \
                        RGB_INTERFACE_RGB_CONTROL1_FIFO_RD_CLR_Msk | RGB_INTERFACE_RGB_CONTROL1_FIFO_WR_CLR_Msk;

    /* IRQ mask */
    rgb_dev->Instance->REG_RGB_INTR_MSK.all = 0x3F;

    /* IRQ Clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.all = 0x3F;

    disable_IRQ(IRQ_RGB_VECTOR);
    clear_IRQ(IRQ_RGB_VECTOR);

    RGB_Reset();

    rgb_dev->State  = RGB_STATE_RESET;

    return CSK_DRIVER_OK;
}


/**
 * @brief Start the RGB to capture video frames.
 *
 * @param pDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_Start(void *pDev)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(rgb_dev->State != RGB_STATE_READY)
    {
        DEV_LOG("[%s:%d] Error state: rgb state not ready, now is %d, ", __func__, __LINE__, rgb_dev->State);
        return CSK_DRIVER_ERROR;
    }

    /* Lock the RGB peripheral state */
    rgb_dev->State = RGB_STATE_BUSY;

    /* IRQ mask */
    if (rgb_dev->cb_event) {
        rgb_dev->Instance->REG_RGB_INTR_MSK.all = (0 << RGB_INTERFACE_RGB_INTR_MSK_EOF_FLAG_MSK_Pos) | \
                                                  (0 << RGB_INTERFACE_RGB_INTR_MSK_SOF_FLAG_MSK_Pos) | \
                                                  (1 << RGB_INTERFACE_RGB_INTR_MSK_FIFO_RD_EMPTY_MSK_Pos) | \
                                                  (1 << RGB_INTERFACE_RGB_INTR_MSK_FIFO_RD_FULL_MSK_Pos) | \
                                                  (1 << RGB_INTERFACE_RGB_INTR_MSK_FIFO_WR_EMPTY_MSK_Pos) | \
                                                  (1 << RGB_INTERFACE_RGB_INTR_MSK_FIFO_WR_FULL_MSK_Pos);
    } else {
        rgb_dev->Instance->REG_RGB_INTR_MSK.all = 0x3F;
    }

    /* IRQ Clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.all = 0x3F;

    /* Enable RGB */
    rgb_dev->Instance->REG_RGB_CONTROL1.all |= RGB_INTERFACE_RGB_CONTROL1_RGB_SOFT_RST_Msk | \
                        RGB_INTERFACE_RGB_CONTROL1_FIFO_RD_CLR_Msk | RGB_INTERFACE_RGB_CONTROL1_FIFO_WR_CLR_Msk;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 1;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_START = 1;

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stops the RGB and releases its resources.
 *
 * @param pDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_Stop(void *pDev)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Lock the RGB peripheral state */
    rgb_dev->State = RGB_STATE_BUSY;

    /* Disable RGB */
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.FRMS_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL1.all |= RGB_INTERFACE_RGB_CONTROL1_RGB_SOFT_RST_Msk | \
                        RGB_INTERFACE_RGB_CONTROL1_FIFO_RD_CLR_Msk | RGB_INTERFACE_RGB_CONTROL1_FIFO_WR_CLR_Msk;

    /* IRQ mask */
    rgb_dev->Instance->REG_RGB_INTR_MSK.all = 0x3F;

    /* IRQ Clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.all = 0x3F;

    /* Change RGB state */
    rgb_dev->State = RGB_STATE_READY;

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

/**
 * @brief Enables the RGB clock output.
 *
 * @param freq_hz The desired frequency of the RGB clock output in Hz. (1600000~200000000)
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_EnableClockout(uint32_t freq_hz)
{
    uint32_t div_24MHz = 0;
    uint32_t freq_24MHz = 0;
    uint32_t div_100MHz = 0;
    uint32_t freq_100MHz = 0;

    if(freq_hz < RGB_CLK_OUT_HZ_MIN) {
        freq_hz = RGB_CLK_OUT_HZ_MIN;
    } else if(freq_hz > RGB_CLK_OUT_HZ_MAX) {
        freq_hz = RGB_CLK_OUT_HZ_MAX;
    } else {
    }

    div_24MHz = clk_div_get(RGB_CLK_OUT_SEL_24MHz, freq_hz);
    freq_24MHz = RGB_CLK_OUT_SEL_24MHz / div_24MHz;
    freq_24MHz = (freq_24MHz >= freq_hz) ? (freq_24MHz - freq_hz) : (freq_hz - freq_24MHz);

    div_100MHz = clk_div_get(RGB_CLK_OUT_SEL_100MHz, freq_hz);
    freq_100MHz = RGB_CLK_OUT_SEL_100MHz / div_100MHz;
    freq_100MHz = (freq_100MHz >= freq_hz) ? (freq_100MHz - freq_hz) : (freq_hz - freq_100MHz);

    __HAL_CRM_RGB_CLK_ENABLE();
    if(freq_24MHz <= freq_100MHz) {
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_RGB_CLK = 0;                    // bit24  0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_M = (div_24MHz & 0xF);  // bit25~28
        //DEV_LOG("[%s:%d] src=%dMHz div=%d clk=%dHz", __func__, __LINE__, RGB_CLK_OUT_SEL_24MHz/1000000, div_24MHz, (RGB_CLK_OUT_SEL_24MHz/div_24MHz));
    } else {
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_RGB_CLK = 1;  // bit24  0:24MHz  1:syspll_peri_clk
        IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_M = (div_100MHz & 0xF);  // bit25~28
        //DEV_LOG("[%s:%d] src=%dMHz div=%d clk=%dHz", __func__, __LINE__, RGB_CLK_OUT_SEL_100MHz/1000000, div_100MHz, (RGB_CLK_OUT_SEL_100MHz/div_100MHz));
    }
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_LD = 0x1;  // bit29  0:normal  1:inv

    return CSK_DRIVER_OK;
}


/**
 * @brief Disables the RGB clock output.
 *
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_DisableClockout(void)
{
    __HAL_CRM_RGB_CLK_DISABLE();
    return CSK_DRIVER_OK;
}

