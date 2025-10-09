#ifndef __INCLUDE_DRIVER_DVP_H
#define __INCLUDE_DRIVER_DVP_H

#include "Driver_Common.h"

#define CSK_DVP_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)         /*!< API version */


/**
  * @brief  DVP clk/hsync/vhync polarity status enum definition
  */
typedef enum
{
    DVP_POL_RISING         = 0x00U,  /*!< Signal polarity is effective on the rising edge  */
    DVP_POL_FALLING        = 0x01U,  /*!< Signal polarity is effective on the falling edge */
    DVP_POL_BUTT
}DVP_emPol;


/**
  * @brief  DVP_JLB_HANSHK_SEL enum definition
  *
  * Only software mode is supported now.
  */
typedef enum
{
    DVP_JLB_HANSHK_HARD    = 0x00U,  /*!< Hardware mode */
    DVP_JLB_HANSHK_SOFT    = 0x01U,  /*!< Software mode */
    DVP_JLB_HANSHK_BUTT
}DVP_emJlbHanshk;


/**
  * @brief  DVP data bus sel enum definition
  */
typedef enum
{
    DVP_DATA_ALIGN_RIGHT   = 0x00U,  /*!< Keep data right aligned (bit7~0) */
    DVP_DATA_ALIGN_LEFT    = 0x01U,  /*!< Keep data left aligned (bit11~4) */
    DVP_DATA_ALIGN_BUTT
}DVP_emDataAlign;


/**
  * @brief  DVP input frame format enum definition
  */
typedef enum
{
    DVP_INPUT_FORM_YUV422_Y0CBY1CR       = 0x00U,  /*!< YUV422 YUYV   */
    DVP_INPUT_FORM_YUV422_CBY0CRY1       = 0x01U,  /*!< YUV422 UYVY   */
    DVP_INPUT_FORM_YUV422_Y0CRY1CB       = 0x02U,  /*!< YUV422 YVYU   */
    DVP_INPUT_FORM_YUV422_CRY0CBY1       = 0x03U,  /*!< YUV422 VYUY   */
    DVP_INPUT_FORM_YUV420_Y0CBY1Y2CRY3   = 0x04U,  /*!< YUV420 YUYYVY */
    DVP_INPUT_FORM_YUV420_CBY0Y1CRY2Y3   = 0x05U,  /*!< YUV420 UYYVYY */
    DVP_INPUT_FORM_YUV420_Y0Y1CBY2Y3CR   = 0x06U,  /*!< YUV420 YYUYYV */
    DVP_INPUT_FORM_YUV444_Y0CBCR         = 0x07U,  /*!< YUV444 YUV    */
    DVP_INPUT_FORM_LUMINA_12BIT          = 0x08U,  /*!< RAW12         */
    DVP_INPUT_FORM_LUMINA_10BIT          = 0x09U,  /*!< RAW10         */
    DVP_INPUT_FORM_LUMINA_8BIT           = 0x0AU,  /*!< RAW8          */
    DVP_INPUT_FORM_BUTT
}DVP_emInputFormat;

/**
  * @brief  DVP State enum definition
  */
typedef enum
{
    DVP_STATE_RESET             = 0x00U,  /*!< DVP not yet initialized or disabled  */
    DVP_STATE_READY             = 0x01U,  /*!< DVP initialized and ready for use    */
    DVP_STATE_BUSY              = 0x02U,  /*!< DVP internal processing is ongoing   */
    DVP_STATE_TIMEOUT           = 0x03U,  /*!< DVP timeout state                    */
    DVP_STATE_ERROR             = 0x04U,  /*!< DVP error state                      */
    DVP_STATE_SUSPENDED         = 0x05U,  /*!< DVP suspend state                    */
    DVP_STATE_BUTT
}DVP_emState;


/**
  * @brief  DVP error enum definition
  */
typedef enum
{
    DVP_ERROR_NONE              = 0x00U,  /*!< DVP not yet initialized or disabled  */
    DVP_ERROR_FIFO_UNFLOW       = 0x01U,  /*!< DVP fifo_unflow                      */
    DVP_ERROR_FIFO_OVFLOW       = 0x02U,  /*!< DVP fifo_ovflow                      */
    DVP_ERROR_FIFO_RD_EMPTY     = 0x04U,  /*!< DVP fifo_rd_empty                    */
    DVP_ERROR_FIFO_RD_FULL      = 0x08U,  /*!< DVP fifo_rd_full                     */
    DVP_ERROR_EOF_CNT_ABNOR     = 0x10U,  /*!< DVP EOF_CNT_ABNOR   */
    DVP_ERROR_BUTT
}DVP_emError;


/**
  * @brief  DVP interrupt status enum definition
  */
typedef enum
{
    DVP_IRQ_EVENT_DMA_VIC_SINGLE      = 0x00U,  /*!< DVP dma_vic_single  */
    DVP_IRQ_EVENT_DMA_VIC_REQ         = 0x01U,  /*!< DVP dma_vic_req     */
    DVP_IRQ_EVENT_FIFO_UNFLOW         = 0x02U,  /*!< DVP fifo_unflow     */
    DVP_IRQ_EVENT_FIFO_OVFLOW         = 0x03U,  /*!< DVP fifo_ovflow     */
    DVP_IRQ_EVENT_FIFO_RD_EMPTY       = 0x04U,  /*!< DVP fifo_rd_empty   */
    DVP_IRQ_EVENT_FIFO_RD_FULL        = 0x05U,  /*!< DVP fifo_rd_full    */
    DVP_IRQ_EVENT_EOF                 = 0x06U,  /*!< DVP eof             */
    DVP_IRQ_EVENT_SOF                 = 0x07U,  /*!< DVP sof             */
    DVP_IRQ_EVENT_EOF_CNT_ABNOR       = 0x08U,  /*!< DVP EOF_CNT_ABNOR   */
    DVP_IRQ_EVENT_BUTT
}DVP_emIrqEvent;


/**
  * @brief   DVP Init structure definition
  */
typedef struct
{
    uint16_t  FrameWidth;                 /*!< Specifies the width of frame in pixels.                      */

    uint16_t  FrameHeight;                /*!< Specifies the height of frame in lines.                      */

    uint16_t  PixelOffset;                /*!< Specifies the clock cycles between deactivation and activation
                                             of the h_sync.                                                 */

    uint16_t  LineOffset;                 /*!< Specifies the first valid image line offset from the
                                             deactivation of the v_sync.                                    */

    DVP_emInputFormat  InputFormat;       /*!< Specifies the input format of image sensor.                  */

    DVP_emDataAlign  DataAlign;           /*!< Specifies the data on the bus aligned to right or left.      */

    DVP_emPol  VSPolarity;                /*!< Specifies the Vertical synchronization polarity: High or Low.
                                             This parameter can be a value of @ref DVP_VSYNC_Polarity       */

    DVP_emPol  HSPolarity;                /*!< Specifies the Horizontal synchronization polarity: High or Low.
                                             This parameter can be a value of @ref DVP_HSYNC_Polarity       */

    DVP_emPol  PCKPolarity;               /*!< Specifies the Pixel clock polarity: Falling or Rising.
                                             This parameter can be a value of @ref DVP_PIXCK_Polarity       */

    DVP_emJlbHanshk  HandshakingMode;     /*!< Specifies the handshaking mode including which user/users
                                             to enable and software/hardware handshaking selection.         */
}DVP_InitTypeDef;


/**
 \fn          void CSK_DVP_SignalEvent_t (DVP_emIrqEvent event, uint32_t usr_param)
 \brief       Signal DVP Events.
 \param[in]   event        DVP event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void (*CSK_DVP_SignalEvent_t)(DVP_emIrqEvent event, uint32_t param);



//------------------------------------------------------------------------------------------
/**
  * @brief  Return DVP driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
DVP_GetVersion(void);

/**
 * @brief Initialize the DVP (Digital Video Processor) device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pCallback The callback function for DVP events.
 * @param pCfg The configuration structure for the DVP device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t
DVP_Initialize(void *pDvpDev, void *pCallback, DVP_InitTypeDef *pCfg);

/**
 * @brief Uninitializes the DVP device.
 *
 * This function uninitializes the DVP device by performing a reset and disabling the VI.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t
DVP_Uninitialize(void *pDvpDev);

void DVP_Reset(void);

/**
 * @brief Start the DVP to capture video frames.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
DVP_Start(void *pDvpDev);

/**
 * @brief Stops the DVP and releases its resources.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
DVP_Stop(void *pDvpDev);

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
int32_t
DVP_EnableClockout(void *pDvpDev, uint32_t freq_hz);

/**
 * @brief Disables the DVP clock output.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
DVP_DisableClockout(void *pDvpDev);

/**
 * @brief Get the current state of the DVP device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pState A pointer to a DVP_emState variable where the current state will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
DVP_GetState(void *pDvpDev, DVP_emState *pState);

/**
 * @brief Get the error code from a DVP device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pError A pointer to a variable where the error code will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
DVP_GetError(void *pDvpDev, uint32_t *pError);

/**
  * @brief  Return DVP instance.
  *
  * @return Instance of DVP
  */
void* DVP0(void);

/**
  * @brief  Return DVP BUF instance.
  *
  * @return Instance of RGB
  */
uint32_t DVP0_Buf(void);


#endif /* __DRIVER_DVP_H */


