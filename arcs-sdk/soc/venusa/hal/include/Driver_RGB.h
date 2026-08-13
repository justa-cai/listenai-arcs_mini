/**
  ******************************************************************************
  * @file    Driver_RGB.h
  * @brief   Header file for RGB (Digital Video Processor) driver
  * @version V1.0.0
  * @date    2025.01.01
  *
  * @copyright Copyright (c) ListenAI
  *            All rights reserved.
  *
  * This file contains the definitions and prototypes for the RGB driver interface.
  * It provides configuration options and control functions for RGB video output.
  ******************************************************************************
  */
#ifndef _DRIVER_RGB_H
#define _DRIVER_RGB_H

#include "Driver_Common.h"

/** @defgroup RGB
  * @brief RGB HAL module driver
  * @{
  */
/** @defgroup RGB_Exported_Constants RGB Exported Constants
  * @{
  */
/** @defgroup RGB_DRV_Version RGB API Version
  * @{
  */
#define CSK_RGB_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)         /*!< API version */
/** @} */ /* End of group RGB_DRV_Version */
/** @} */ /* End of group RGB_Exported_Constants */

/** @defgroup RGB_Exported_Types RGB Exported Types
  * @{
  */
/**
  * @brief RGB frame mode enumeration
  * @details Defines the operation mode for frame output
  */
typedef enum
{
    RGB_FRAME_ONCE            = 0x00U,    /*!< Output single frame */
    RGB_FRAME_CONTINUE        = 0x01U,    /*!< Continuously output frames */
    RGB_FRAME_BUTT                        /*!< Boundary value */
}rgb_emFrms;

/**
  * @brief RGB output wires configuration enumeration
  * @details Specifies the number of data wires used for output
  */
typedef enum
{
    RGB_OUTPUT_WIRES_24       = 0x00U,    /*!< 24-bit parallel output */
    RGB_OUTPUT_WIRES_8        = 0x01U,    /*!< 8-bit parallel output */
    RGB_OUTPUT_WIRES_BUTT                 /*!< Boundary value */
}rgb_emWires;

/**
  * @brief RGB synchronization mode enumeration
  * @details Configures the synchronization signal mode
  */
typedef enum
{
    RGB_SYNC_MODE_SYNC        = 0x00U,    /*!< Sync mode with HSYNC and VSYNC */
    RGB_SYNC_MODE_SYNC_DE     = 0x01U,    /*!< Sync mode with Data Enable signal */
    RGB_SYNC_MODE_BUTT                    /*!< Boundary value */
}rgb_emSync;

/**
  * @brief RGB input format enumeration
  * @details Specifies the color format of input data
  */
typedef enum
{
    RGB_INPUT_FORMAT_RGB888   = 0x00U,    /*!< 24-bit RGB format */
    RGB_INPUT_FORMAT_XRGB8888 = 0x01U,    /*!< 32-bit RGB format with unused byte */
    RGB_INPUT_FORMAT_RGB565   = 0x02U,    /*!< 16-bit RGB format */
    RGB_INPUT_FORMAT_BUTT                 /*!< Boundary value */
}rgb_emFormatIn;

/**
  * @brief RGB output format enumeration
  * @details Specifies the color format of output data
  */
typedef enum
{
    RGB_OUTPUT_FORMAT_RGB888  = 0x00U,    /*!< 24-bit RGB output format */
    RGB_OUTPUT_FORMAT_RGB666  = 0x01U,    /*!< 18-bit RGB output format */
    RGB_OUTPUT_FORMAT_RGB565  = 0x02U,    /*!< 16-bit RGB output format */
    RGB_OUTPUT_FORMAT_BGR888  = 0x03U,    /*!< 24-bit BGR output format */
    RGB_OUTPUT_FORMAT_BGR666  = 0x04U,    /*!< 18-bit BGR output format */
    RGB_OUTPUT_FORMAT_BGR565  = 0x05U,    /*!< 16-bit BGR output format */
    RGB_OUTPUT_FORMAT_BUTT                /*!< Boundary value */
}rgb_emFormatOut;

/**
  * @brief RGB signal polarity enumeration
  * @details Configures the polarity of control signals
  */
typedef enum
{
    RGB_POLARITY_POSITIVE     = 0x00U,    /*!< Active high polarity */
    RGB_POLARITY_NEGATIVE     = 0x01U,    /*!< Active low polarity */
    RGB_POLARITY_BUTT                     /*!< Boundary value */
}rgb_emPol;

/**
  * @brief RGB interrupt status enumeration
  * @details Defines possible interrupt events from the RGB controller
  */
typedef enum
{
    RGB_IRQ_EVENT_EOF             = 0x00U,  /*!< End of frame interrupt */
    RGB_IRQ_EVENT_SOF             = 0x01U,  /*!< Start of frame interrupt */
    RGB_IRQ_EVENT_FIFO_RD_EMPTY   = 0x02U,  /*!< Read FIFO empty interrupt */
    RGB_IRQ_EVENT_FIFO_RD_FULL    = 0x03U,  /*!< Read FIFO full interrupt */
    RGB_IRQ_EVENT_FIFO_WR_EMPTY   = 0x04U,  /*!< Write FIFO empty interrupt */
    RGB_IRQ_EVENT_FIFO_WR_FULL    = 0x05U,  /*!< Write FIFO full interrupt */
    RGB_IRQ_EVENT_BUTT                      /*!< Boundary value */
}RGB_emIrqEvent;

/**
  * @brief RGB Initialization structure
  * @details Contains all configurable parameters for RGB controller initialization
  */
typedef struct
{
    rgb_emFrms      frms;               /*!< Frame output mode */
    rgb_emWires     wires;              /*!< Number of data wires */
    rgb_emSync      sync;               /*!< Synchronization mode */
    bool            de_continue;        /*!< Data enable continuous mode */
    rgb_emFormatIn  format_in;          /*!< Input data format */
    rgb_emFormatOut format_out;         /*!< Output data format */
    bool            out_lsb;            /*!< LSB first output */
    rgb_emPol       VSPolarity;         /*!< VSYNC polarity */
    rgb_emPol       HSPolarity;         /*!< HSYNC polarity */
    rgb_emPol       DEPolarity;         /*!< Data Enable polarity */
    rgb_emPol       CLKPolarity;        /*!< Clock polarity */
    uint32_t        clk_hz;             /*!< Pixel clock frequency in Hz (1600000~200000000) */
    uint16_t        img_width;          /*!< Image width in pixels (0~0xFFFF) */
    uint16_t        img_height;         /*!< Image height in pixels (0~0xFFFF) */
    uint8_t         h_pulse_width;      /*!< Horizontal pulse width (0~0xF) */
    uint8_t         h_back_blanking;    /*!< Horizontal back porch (0~0xFF) */
    uint8_t         h_front_blanking;   /*!< Horizontal front porch (0~0xFF) */
    uint8_t         v_pulse_width;      /*!< Vertical pulse width (0~0xF) */
    uint8_t         v_back_blanking;    /*!< Vertical back porch (0~0xFF) */
    uint8_t         v_front_blanking;   /*!< Vertical front porch (0~0xFF) */
    uint8_t         BurstThreshold;     /*!< DMA burst threshold 1/2/4/8 */
}RGB_InitTypeDef;

/**
 \fn          void CSK_RGB_SignalEvent_t (RGB_emIrqEvent event, uint32_t usr_param)
 \brief       Signal RGB Events.
 \param[in]   event        RGB event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void (*RGB_SignalEvent_t)(RGB_emIrqEvent event, uint32_t param);
/** @} */ /* End of group RGB_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup RGB_Exported_Functions RGB Exported Functions
  * @{
  */
/**
  * @brief  Return RGB driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION RGB_GetVersion(void);

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

/**
 * @brief Initialize the RGB (Digital Video Processor) device.
 *
 * @param pDev A pointer to the RGB device structure.
 * @param callback The callback function for RGB events.
 * @param pCfg The configuration structure for the RGB device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t RGB_Initialize(void *pDev, RGB_SignalEvent_t callback, RGB_InitTypeDef *pCfg);

/**
 * @brief Uninitializes the RGB device.
 *
 * This function uninitializes the RGB device by performing a reset and disabling the VI.
 *
 * @param pDev A pointer to the RGB device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t RGB_Uninitialize(void *pDev);

/**
 * @brief Start the RGB to capture video frames.
 *
 * @param pDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_Start(void *pDev);

/**
 * @brief Stops the RGB and releases its resources.
 *
 * @param pDev A pointer to the RGB device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_Stop(void *pDev);

/**
 * @brief Enables the RGB clock output.
 *
 * @param freq_hz The desired frequency of the RGB clock output in Hz. (1600000~200000000)
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_EnableClockout(uint32_t freq_hz);

/**
 * @brief Disables the RGB clock output.
 *
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t RGB_DisableClockout(void);

/** @} */ /* End of group RGB_Exported_Functions */
/**
  * @}
  */ /* End of group RGB */
#endif /* __DRIVER_RGB_H */


