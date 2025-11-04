#ifndef __INCLUDE_DRIVER_RGB_H
#define __INCLUDE_DRIVER_RGB_H

#include "Driver_Common.h"

#define CSK_RGB_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)         /*!< API version */



typedef enum
{
    RGB_FRAME_ONCE            = 0x00U,
    RGB_FRAME_CONTINUE        = 0x01U,
    RGB_FRAME_BUTT
}rgb_emFrms;

typedef enum
{
    RGB_OUTPUT_WIRES_24       = 0x00U,
    RGB_OUTPUT_WIRES_8        = 0x01U,
    RGB_OUTPUT_WIRES_BUTT
}rgb_emWires;

typedef enum
{
    RGB_SYNC_MODE_SYNC        = 0x00U,
    RGB_SYNC_MODE_SYNC_DE     = 0x01U,
    RGB_SYNC_MODE_BUTT
}rgb_emSync;

typedef enum
{
    RGB_INPUT_FORMAT_RGB888   = 0x00U,
    RGB_INPUT_FORMAT_XRGB8888 = 0x01U,
    RGB_INPUT_FORMAT_RGB565   = 0x02U,
    RGB_INPUT_FORMAT_BUTT
}rgb_emFormatIn;

typedef enum
{
    RGB_OUTPUT_FORMAT_RGB888  = 0x00U,
    RGB_OUTPUT_FORMAT_RGB666  = 0x01U,
    RGB_OUTPUT_FORMAT_RGB565  = 0x02U,
    RGB_OUTPUT_FORMAT_BGR888  = 0x03U,
    RGB_OUTPUT_FORMAT_BGR666  = 0x04U,
    RGB_OUTPUT_FORMAT_BGR565  = 0x05U,
    RGB_OUTPUT_FORMAT_BUTT
}rgb_emFormatOut;

typedef enum
{
    RGB_POLARITY_POSITIVE     = 0x00U,
    RGB_POLARITY_NEGATIVE     = 0x01U,
    RGB_POLARITY_BUTT
}rgb_emPol;

/**
  * @brief  RGB State enum definition
  */
typedef enum
{
    RGB_STATE_RESET             = 0x00U,  /*!< RGB not yet initialized or disabled  */
    RGB_STATE_READY             = 0x01U,  /*!< RGB initialized and ready for use    */
    RGB_STATE_BUSY              = 0x02U,  /*!< RGB internal processing is ongoing   */
    RGB_STATE_TIMEOUT           = 0x03U,  /*!< RGB timeout state                    */
    RGB_STATE_ERROR             = 0x04U,  /*!< RGB error state                      */
    RGB_STATE_SUSPENDED         = 0x05U,  /*!< RGB suspend state                    */
    RGB_STATE_BUTT
}RGB_emState;


/**
  * @brief  RGB error enum definition
  */
typedef enum
{
    RGB_ERROR_NONE              = 0x00U,  /*!< RGB not yet initialized or disabled  */
    RGB_ERROR_FIFO_RD_EMPTY     = 0x04U,  /*!< RGB fifo_read_empty                  */
    RGB_ERROR_FIFO_RD_FULL      = 0x08U,  /*!< RGB fifo_read_full                   */
    RGB_ERROR_FIFO_WR_EMPTY     = 0x10U,  /*!< RGB fifo_write_empty                 */
    RGB_ERROR_FIFO_WR_FULL      = 0x20U,  /*!< RGB fifo_write_full                  */
    RGB_ERROR_BUTT
}RGB_emError;


/**
  * @brief  RGB interrupt status enum definition
  */
typedef enum
{
    RGB_IRQ_EVENT_EOF                 = 0x00U,  /*!< RGB eof               */
    RGB_IRQ_EVENT_SOF                 = 0x01U,  /*!< RGB sof               */
    RGB_IRQ_EVENT_FIFO_RD_EMPTY       = 0x02U,  /*!< RGB fifo_read_empty   */
    RGB_IRQ_EVENT_FIFO_RD_FULL        = 0x03U,  /*!< RGB fifo_read_full    */
    RGB_IRQ_EVENT_FIFO_WR_EMPTY       = 0x04U,  /*!< RGB fifo_write_empty  */
    RGB_IRQ_EVENT_FIFO_WR_FULL        = 0x05U,  /*!< RGB fifo_write_full   */
    RGB_IRQ_EVENT_BUTT
}RGB_emIrqEvent;


/**
  * @brief   RGB Init structure definition
  */
typedef struct
{
    rgb_emFrms      frms;
    rgb_emWires     wires;
    rgb_emSync      sync;
    bool            de_continue;
    rgb_emFormatIn  format_in;
    rgb_emFormatOut format_out;
    bool            out_lsb;
    rgb_emPol       VSPolarity;
    rgb_emPol       HSPolarity;
    rgb_emPol       DEPolarity;
    rgb_emPol       CLKPolarity;
    uint32_t        clk_hz;
    uint16_t        img_width;          // 0~0xFFFF
    uint16_t        img_height;         // 0~0xFFFF
    uint8_t         v_pulse_width;      // 0~0xF
    uint8_t         h_pulse_width;      // 0~0xF
    uint8_t         v_front_blanking;   // 0~0xFF
    uint8_t         h_front_blanking;   // 0~0xFF
    uint8_t         v_back_blanking;    // 0~0xFF
    uint8_t         h_back_blanking;    // 0~0xFF
}RGB_InitTypeDef;


/**
 \fn          void CSK_RGB_SignalEvent_t (RGB_emIrqEvent event, uint32_t usr_param)
 \brief       Signal RGB Events.
 \param[in]   event        RGB event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void (*CSK_RGB_SignalEvent_t)(RGB_emIrqEvent event, uint32_t param);



//------------------------------------------------------------------------------------------
/**
  * @brief  Return RGB driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
RGB_GetVersion(void);

/**
 * @brief Initialize the RGB (Digital Video Processor) device.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @param pCallback The callback function for RGB events.
 * @param pCfg The configuration structure for the RGB device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t
RGB_Initialize(void *pRgbDev, void *pCallback, RGB_InitTypeDef *pcfg);

/**
 * @brief Uninitializes the RGB device.
 *
 * This function uninitializes the RGB device by performing a reset and disabling the VI.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t
RGB_Uninitialize(void *pRgbDev);

/**
 * @brief Start the RGB to capture video frames.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
RGB_Start(void *pRgbDev);

/**
 * @brief Stops the RGB and releases its resources.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
RGB_Stop(void *pRgbDev);

/**
 * @brief Enables the RGB clock output.
 *
 * @param freq_hz The desired frequency of the RGB clock output in Hz.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
RGB_EnableClockout(uint32_t freq_hz);

/**
 * @brief Disables the RGB clock output.
 *
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
RGB_DisableClockout(void);

/**
 * @brief Get the current state of the RGB device.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @param pState A pointer to a RGB_emState variable where the current state will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
RGB_GetState(void *pRgbDev, RGB_emState *pState);

/**
 * @brief Get the error code from a RGB device.
 *
 * @param pRgbDev A pointer to the RGB device structure.
 * @param pError A pointer to a variable where the error code will be stored.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
RGB_GetError(void *pRgbDev, uint32_t *pError);

/**
  * @brief  Return RGB instance.
  *
  * @return Instance of RGB
  */
void* RGB0(void);

/**
  * @brief  Return RGB BUF instance.
  *
  * @return Instance of RGB
  */
uint32_t RGB0_Buf(void);


#endif /* __DRIVER_RGB_H */


