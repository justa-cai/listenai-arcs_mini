#ifndef __INCLUDE_DRIVER_BLENDER_H
#define __INCLUDE_DRIVER_BLENDER_H

#include "Driver_Common.h"

#define CSK_BLENDER_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)         /*!< API version */


/*
| blender_mode | alpha_mode |           parameter          |
| ------------ | ---------- | ---------------------------- |
| 0            | 0          | error                        |
| 0            | 1          | back + color + mask + alpha  |
| 0            | 2          | back + color + alpha         |
| 1            | 0          | back + fore + fore_A + alpha |
| 1            | 1          | back + fore + mask + alpha   |
| 1            | 2          | back + fore + alpha          |

back/fore/mask is come from mem
color/alpha is come from register
fore_A is come from ARGB8888/ARGB1555/ARGB4444
*/

typedef enum
{
    BLENDER_MODE_FILL       = 0x00U,  /*!< fill mode(alpha_mode !=0 && fore_color_mode  invalid)  the force is come from color_reg  */
    BLENDER_MODE_MAP        = 0x01U,  /*!< map mode */
    BLENDER_MODE_BUTT
}Blender_emMode;

typedef enum
{
    BLENDER_ALPHA_MODE_0    = 0x00U,  /*!< ALPHA_MODE=0 : alpha = (A * ALPHA_REG) >> 8  */
    BLENDER_ALPHA_MODE_1    = 0x01U,  /*!< ALPHA_MODE=1 : alpha = (mask_buf * ALPHA_REG) >> 8 */
    BLENDER_ALPHA_MODE_2    = 0x02U,  /*!< ALPHA_MODE=2 : alpha = ALPHA_REG */
    BLENDER_ALPHA_MODE_BUTT
}Blender_emAlphaMode;

typedef enum
{
    BLENDER_BACK_FORMAT_RGB565    = 0x00U,
    BLENDER_BACK_FORMAT_RGB888    = 0x01U,
    BLENDER_BACK_FORMAT_ARGB8888  = 0x02U,
    BLENDER_BACK_FORMAT_BUTT
}Blender_emBackFormat;

typedef enum
{
    BLENDER_FORE_FORMAT_ARGB8888  = 0x00U,
    BLENDER_FORE_FORMAT_RGB888    = 0x01U,
    BLENDER_FORE_FORMAT_RGB565    = 0x02U,
    BLENDER_FORE_FORMAT_ARGB1555  = 0x03U,
    BLENDER_FORE_FORMAT_ARGB4444  = 0x04U,
    BLENDER_FORE_FORMAT_L8        = 0x05U,
    BLENDER_FORE_FORMAT_BUTT
}Blender_emForeFormat;

typedef struct
{
    Blender_emMode  blender_mode;
    Blender_emAlphaMode  alpha_mode;
    Blender_emBackFormat back_format;
    Blender_emForeFormat fore_format;
    uint16_t img_width;
    uint16_t img_height;
    uint32_t color;         // Blue:bit[23:16]  Green:bit[15:8]  Red:bit[7:0]
    uint8_t alpha;
    uint8_t burst_thd;      // word
}Blender_InitTypeDef;



//------------------------------------------------------------------------------------------
/**
  * @brief  Return Blender driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
Blender_GetVersion(void);

/**
 * @brief Initialize the Blender (Digital Video Processor) device.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @param pCfg The configuration structure for the Blender device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t
Blender_Initialize(void *pBlenderDev, Blender_InitTypeDef *pCfg);

/**
 * @brief Uninitializes the Blender device.
 *
 * This function uninitializes the Blender device by performing a reset and disabling the VI.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t
Blender_Uninitialize(void *pBlenderDev);

/**
 * @brief Start the Blender to capture video frames.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
Blender_Start(void *pBlenderDev);

/**
 * @brief Stops the Blender and releases its resources.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
Blender_Stop(void *pBlenderDev);


/**
  * @brief  Return Blender instance.
  *
  * @return Instance of Blender
  */
void* Blender0(void);


#endif /* __DRIVER_BLENDER_H */


