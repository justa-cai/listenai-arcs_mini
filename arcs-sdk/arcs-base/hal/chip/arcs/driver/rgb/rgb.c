#include <string.h>
#include <stdint.h>
#include "arcs_ap.h"
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
        0,
};


//------------------------------------------------------------------------------------
static void RGB_Reset(void)
{
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_RGB_CLK = 1;    // bit23
    IP_AP_CFG->REG_SW_RESET.bit.RGB_RESET = 0x1;    // bit12
    IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0;         // bit0  0:rgb  1:qspi_out
}


static RGB_DEV * safe_rgb_dev(void *rgb_dev)
{
    //TODO: safe check of RGB device parameter
    if (rgb_dev == RGB0()) {
        if (((RGB_DEV *)rgb_dev)->Instance != ((RGB_INTERFACE_RegDef *)(RGB_BASE))) {
            //CLOGW("RGB0 device context has been tampered illegally!!\n");
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
_FAST_FUNC_RO void RGB_IRQ_Handler()
{
    RGB_DEV *rgb_dev = (RGB_DEV *)RGB0();
    uint32_t status = rgb_dev->Instance->REG_RGB_INTR_STATUS.all;
    DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

     /* Frame complete interrupt management */
    if((status & RGB_INTERFACE_RGB_INTR_STATUS_SOF_FLAG_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_SOF_FLAG_ISR_Msk)
    {
        rgb_dev->Instance->REG_RGB_INTR_CLR.bit.SOF_FLAG_CLR = 1;

        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_SOF, 0);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_EOF_FLAG_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_EOF_FLAG_ISR_Msk)
    {
        rgb_dev->Instance->REG_RGB_INTR_CLR.bit.EOF_FLAG_CLR = 1;

        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_EOF, 0);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_FULL_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_FULL_ISR_Msk)
    {
        rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_WR_FULL_CLR = 1;

        /* Update error code */
        rgb_dev->ErrorCode |= RGB_ERROR_FIFO_WR_FULL;

        /* Change the state */
        rgb_dev->State = RGB_STATE_ERROR;

        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_WR_FULL, 0);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_EMPTY_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_WR_EMPTY_ISR_Msk)
    {
        rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_WR_EMPTY_CLR = 1;

        /* Update error code */
        rgb_dev->ErrorCode |= RGB_ERROR_FIFO_WR_EMPTY;

        /* Change the state */
        rgb_dev->State = RGB_STATE_ERROR;

        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_WR_EMPTY, 0);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_FULL_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_FULL_ISR_Msk)
    {
        rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_RD_FULL_CLR = 1;

        /* Update error code */
        rgb_dev->ErrorCode |= RGB_ERROR_FIFO_RD_EMPTY;

        /* Change the state */
        rgb_dev->State = RGB_STATE_ERROR;

        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_RD_EMPTY, 0);
    }

    if((status & RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_EMPTY_ISR_Msk) == RGB_INTERFACE_RGB_INTR_STATUS_FIFO_RD_EMPTY_ISR_Msk)
    {
        rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_RD_EMPTY_CLR = 1;

        /* Update error code */
        rgb_dev->ErrorCode |= RGB_ERROR_FIFO_RD_FULL;

        /* Change the state */
        rgb_dev->State = RGB_STATE_ERROR;

        if(rgb_dev->cb_event)
            rgb_dev->cb_event(RGB_IRQ_EVENT_FIFO_RD_FULL, 0);
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
 * @param pRgbDev A pointer to the RGB device structure.
 * @param pCallback The callback function for RGB events.
 * @param pcfg The configuration structure for the RGB device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t RGB_Initialize(void *pRgbDev, void *pCallback, RGB_InitTypeDef *pcfg)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pRgbDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pcfg == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    RGB_Reset();

    memcpy(&rgb_dev->Init, pcfg, sizeof(RGB_InitTypeDef));

    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_SOFT_RST = 1;

    switch(pcfg->frms)
    {
        case RGB_FRAME_ONCE:
            rgb_dev->Instance->REG_RGB_CONTROL0.bit.FRMS_EN = 0;
            break;

        case RGB_FRAME_CONTINUE:
            rgb_dev->Instance->REG_RGB_CONTROL0.bit.FRMS_EN = 1;
            break;

        default:
            CLOG("[%s:%d] frms=%d is error", __func__, __LINE__, pcfg->frms);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->wires)
    {
        case RGB_OUTPUT_WIRES_24:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_SERIAL_MODE = 0;
            break;

        case RGB_OUTPUT_WIRES_8:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_SERIAL_MODE = 1;
            break;

        default:
            CLOG("[%s:%d] wires=%d is error", __func__, __LINE__, pcfg->wires);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->format_in)
    {
        case RGB_INPUT_FORMAT_RGB888:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_INPUT_FORMAT_SEL = 0;
            break;

        case RGB_INPUT_FORMAT_XRGB8888:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_INPUT_FORMAT_SEL = 1;
            break;

        case RGB_INPUT_FORMAT_RGB565:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_INPUT_FORMAT_SEL = 2;
            break;

        default:
            CLOG("[%s:%d] format_in=%d is error", __func__, __LINE__, pcfg->format_in);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->format_out)
    {
        case RGB_OUTPUT_FORMAT_RGB888:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_OUTPUT_FORMAT_SEL = 0;
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.SBGR = 0;
            break;

        case RGB_OUTPUT_FORMAT_RGB666:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_OUTPUT_FORMAT_SEL = 1;
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.SBGR = 0;
            break;

        case RGB_OUTPUT_FORMAT_RGB565:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_OUTPUT_FORMAT_SEL = 2;
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.SBGR = 0;
            break;

        case RGB_OUTPUT_FORMAT_BGR888:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_OUTPUT_FORMAT_SEL = 0;
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.SBGR = 1;
            break;

        case RGB_OUTPUT_FORMAT_BGR666:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_OUTPUT_FORMAT_SEL = 1;
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.SBGR = 1;
            break;

        case RGB_OUTPUT_FORMAT_BGR565:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_OUTPUT_FORMAT_SEL = 2;
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.SBGR = 1;
            break;

        default:
            CLOG("[%s:%d] format_out=%d is error", __func__, __LINE__, pcfg->format_out);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(true == pcfg->out_lsb) {
        rgb_dev->Instance->REG_RGB_CONTROL0.bit.DATA_ALIGN = 1;
    } else {
        rgb_dev->Instance->REG_RGB_CONTROL0.bit.DATA_ALIGN = 0;
    }

    if(true == pcfg->de_continue) {
        rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.bit.DE_SEQUENTIAL_SEL = 0;
    } else {
        rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.bit.DE_SEQUENTIAL_SEL = 1;
    }

    switch(pcfg->VSPolarity)
    {
        case RGB_POLARITY_POSITIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.VDPOL = 0;
            break;

        case RGB_POLARITY_NEGATIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.VDPOL = 1;
            break;

        default:
            CLOG("[%s:%d] VSPolarity=%d is error", __func__, __LINE__, pcfg->VSPolarity);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->HSPolarity)
    {
        case RGB_POLARITY_POSITIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.HDPOL = 0;
            break;

        case RGB_POLARITY_NEGATIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.HDPOL = 1;
            break;

        default:
            CLOG("[%s:%d] HSPolarity=%d is error", __func__, __LINE__, pcfg->HSPolarity);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->DEPolarity)
    {
        case RGB_POLARITY_POSITIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.DEPOL = 0;
            break;

        case RGB_POLARITY_NEGATIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.DEPOL = 1;
            break;

        default:
            CLOG("[%s:%d] DEPolarity=%d is error", __func__, __LINE__, pcfg->DEPolarity);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->CLKPolarity)
    {
        case RGB_POLARITY_POSITIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.DCLKPOL = 0;
            IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_INV_CLK = 1;  // bit30  0:normal  1:inv
            break;

        case RGB_POLARITY_NEGATIVE:
            rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.DCLKPOL = 1;
            IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_INV_CLK = 0;  // bit30  0:normal  1:inv
            break;

        default:
            CLOG("[%s:%d] CLKPolarity=%d is error", __func__, __LINE__, pcfg->CLKPolarity);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pcfg->sync)
    {
        case RGB_SYNC_MODE_SYNC:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_MODE_SEL = 0;
            break;

        case RGB_SYNC_MODE_SYNC_DE:
            rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_MODE_SEL = 1;
            break;

        default:
            CLOG("[%s:%d] sync=%d is error", __func__, __LINE__, pcfg->sync);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

#if 0
    CLOG("pcfg->img_width=%d 0x%x", pcfg->img_width, pcfg->img_width);
    CLOG("pcfg->img_height=%d 0x%x", pcfg->img_height, pcfg->img_height);
    CLOG("pcfg->v_pulse_width=%d 0x%x", pcfg->v_pulse_width, pcfg->v_pulse_width);
    CLOG("pcfg->h_pulse_width=%d 0x%x", pcfg->h_pulse_width, pcfg->h_pulse_width);
    CLOG("pcfg->v_front_blanking=%d 0x%x", pcfg->v_front_blanking, pcfg->v_front_blanking);
    CLOG("pcfg->h_front_blanking=%d 0x%x", pcfg->h_front_blanking, pcfg->h_front_blanking);
    CLOG("pcfg->v_back_blanking=%d 0x%x", pcfg->v_back_blanking, pcfg->v_back_blanking);
    CLOG("pcfg->h_back_blanking=%d 0x%x", pcfg->h_back_blanking, pcfg->h_back_blanking);
#endif

#if 0
    rgb_dev->Instance->REG_IMAGE_SIZE.bit.WIDTH_IN = pcfg->img_width & 0xFFFF;
    rgb_dev->Instance->REG_IMAGE_SIZE.bit.HEIGHT_IN = pcfg->img_height & 0xFFFF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.V_PULSE_WIDTH = pcfg->v_pulse_width & 0xF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.H_PULSE_WIDTH = pcfg->h_pulse_width & 0xF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.bit.V_FRONT_BLANKING = (uint32_t)pcfg->v_front_blanking & 0xFF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.bit.H_FRONT_BLANKING = (uint32_t)pcfg->h_front_blanking & 0xFF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.V_BLANKING = pcfg->v_back_blanking & 0xFF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.H_BLANKING = pcfg->h_back_blanking & 0xFF;
#else
    uint32_t value = 0;
    value = (pcfg->v_pulse_width << 20) | (pcfg->h_pulse_width << 16) | (pcfg->v_back_blanking << 8) | pcfg->h_back_blanking;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.all &= ~0xFFFFFF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.all |= (value & 0xFFFFFF);

    value = (pcfg->v_front_blanking << 8) | pcfg->h_front_blanking;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.all &= ~0xFFFF;
    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL1.all |= (value & 0xFFFF);

    value = (pcfg->img_height << 16) | pcfg->img_width;
    rgb_dev->Instance->REG_IMAGE_SIZE.all = value;
#endif

    rgb_dev->Instance->REG_SEQUENTIAL_CONTROL0.bit.RGB_BURST_THD = 3;      // 0:1byte  1:2byte  2:4byte  3:8byte
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.REG_CLK_FORCE_ON = 0;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_WR_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_RD_CLR = 1;

    /* IRQ mask, but enable sof and eof */
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.EOF_FLAG_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.SOF_FLAG_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_RD_EMPTY_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_RD_FULL_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_WR_EMPTY_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_WR_FULL_MSK = 1;

    /* IRQ Clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.EOF_FLAG_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.SOF_FLAG_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_RD_EMPTY_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_RD_FULL_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_WR_EMPTY_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_WR_FULL_CLR = 1;

    /* Set up callback */
    rgb_dev->cb_event = (CSK_RGB_SignalEvent_t)pCallback;

    /* Update error code */
    rgb_dev->ErrorCode = RGB_ERROR_NONE;

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
 * @param pRgbDev A pointer to the RGB device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t RGB_Uninitialize(void *pRgbDev)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pRgbDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    /* Disable RGB */
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.FRMS_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 0;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_WR_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_RD_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_SOFT_RST = 1;

    disable_IRQ(IRQ_RGB_VECTOR);

    /* IRQ mask, but enable sof and eof */
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.EOF_FLAG_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.SOF_FLAG_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_RD_EMPTY_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_RD_FULL_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_WR_EMPTY_MSK = 1;
    rgb_dev->Instance->REG_RGB_INTR_MSK.bit.FIFO_WR_FULL_MSK = 1;

    /* IRQ Clear */
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.EOF_FLAG_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.SOF_FLAG_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_RD_EMPTY_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_RD_FULL_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_WR_EMPTY_CLR = 1;
    rgb_dev->Instance->REG_RGB_INTR_CLR.bit.FIFO_WR_FULL_CLR = 1;

    RGB_Reset();

    rgb_dev->ErrorCode = RGB_ERROR_NONE;
    rgb_dev->State  = RGB_STATE_RESET;

    return CSK_DRIVER_OK;
}


/**
 * @brief Start the RGB to capture video frames.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_Start(void *pRgbDev)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pRgbDev);

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

    //DEV_LOG("[%s:%d] 0x%x", __func__, __LINE__, &rgb_dev->Instance->REG_RGB_CONTROL0.all);

    /* Enable RGB */
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_WR_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_RD_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_EN = 1;
    rgb_dev->Instance->REG_RGB_CONTROL0.bit.RGB_START = 1;

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stops the RGB and releases its resources.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_Stop(void *pRgbDev)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pRgbDev);

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
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_WR_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.FIFO_RD_CLR = 1;
    rgb_dev->Instance->REG_RGB_CONTROL1.bit.RGB_SOFT_RST = 1;

    /* Update error code */
    rgb_dev->ErrorCode = RGB_ERROR_NONE;

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
 * @param freq_hz The desired frequency of the RGB clock output in Hz.
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

    div_100MHz = clk_div_get(RGB_CLK_OUT_SEL_100MHz, freq_hz);
    freq_100MHz = RGB_CLK_OUT_SEL_100MHz / div_100MHz;

    if(freq_24MHz >= freq_hz) {
        freq_24MHz = freq_24MHz - freq_hz;
    } else {
        freq_24MHz = freq_hz - freq_24MHz;
    }

    if(freq_100MHz >= freq_hz) {
        freq_100MHz = freq_100MHz - freq_hz;
    } else {
        freq_100MHz = freq_hz - freq_100MHz;
    }

    if(freq_24MHz <= freq_100MHz) {
        IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK = 0;                    // bit24  0:24MHz  1:syspll_peri_clk
        IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_M = (div_24MHz & 0xF);  // bit25~28
        //CLOG("[%s:%d] src=24MHz div=%d clk=%dHz", __func__, __LINE__, div_24MHz, (RGB_CLK_OUT_SEL_24MHz / div_24MHz));
    } else {
        IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK = 1;  // bit24  0:24MHz  1:syspll_peri_clk
        IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_M = (div_100MHz & 0xF);  // bit25~28
        //CLOG("[%s:%d] src=100MHz div=%d clk=%dHz", __func__, __LINE__, div_100MHz, (RGB_CLK_OUT_SEL_100MHz / div_100MHz));
    }
    IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_LD = 0x1;  // bit29  0:normal  1:inv

    return CSK_DRIVER_OK;
}


/**
 * @brief Disables the RGB clock output.
 *
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_DisableClockout(void)
{
    return CSK_DRIVER_OK;
}


/**
 * @brief Get the current state of the RGB device.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @param pState A pointer to a RGB_emState variable where the current state will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_GetState(void *pRgbDev, RGB_emState *pState)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pRgbDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pState == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pState is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *pState = rgb_dev->State;

    return CSK_DRIVER_OK;
}


/**
 * @brief Get the error code from a RGB device.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @param pError A pointer to a variable where the error code will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_GetError(void *pRgbDev, uint32_t *pError)
{
    RGB_DEV *rgb_dev = safe_rgb_dev(pRgbDev);

    /* Check the RGB instance */
    if(rgb_dev == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pRgbDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pError == NULL)
    {
        DEV_LOG("[%s:%d] Error input: pError is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *pError = rgb_dev->ErrorCode;

    return CSK_DRIVER_OK;
}







