#ifndef __CAMERA_H
#define __CAMERA_H

#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "sensor.h"


/**
 * @brief Configuration structure for camera initialization
 */
typedef struct {
    uint8_t sccb_i2c_port;          /*!< If pin_sccb_sda is -1, use the already configured I2C bus by number */
    uint32_t xclk_freq_hz;          /*!< Frequency of XCLK signal, in Hz. */
    pixformat_t pixel_format;       /*!< Format of the pixel data: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG  */
    framesize_t frame_size;         /*!< Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA  */
    int jpeg_quality;               /*!< Quality of JPEG output. 0-63 lower means higher quality  */
    uint8_t colorbar;
} camera_config_t;


/**
 * @brief Initialize the camera driver
 *
 * @note call camera_probe before calling this function
 *
 * This function detects and configures camera over I2C interface.
 *
 * @param config  Camera configuration parameters
 *
 * @return 0 on success
 */
int camera_init(const camera_config_t* config);

/**
 * @brief Deinitialize the camera driver
 *
 * @return 0 on success
 */
int camera_deinit(void);








#if 0


#define CAMERA_MODE_YUV422              0x00   /* YUV422                               */
#define CAMERA_MODE_YUV420              0x01   /* YUV420                               */
#define CAMERA_MODE_Y_ONLY              0x02   /* Y only                               */
#define CAMERA_MODE_RAW_DATA            0x03   /* Raw data                             */
#define CAMERA_MODE_RGB565              0x04   /* RGB565                               */
#define CAMERA_MODE_YUV422_FIX_UV       0x05   /* YUV422 with fixed UV                 */

#define CAMERA_R2560x512                0x0E   /* Customized Resolution                */
#define CAMERA_R1280x240                0x0D   /* Customized Resolution                */
#define CAMERA_R320x120                 0x0C   /* Customized Resolution                */
#define CAMERA_R64x40                   0x0B   /* Customized Resolution                */
#define CAMERA_R80x240                  0x0A   /* Customized Resolution                */
#define CAMERA_R320x480                 0x09   /* Customized Resolution                */
#define CAMERA_R80x480                  0x08   /* Customized Resolution                */
#define CAMERA_R160x480                 0x07   /* Customized Resolution                */
#define CAMERA_R160x240                 0x06   /* Customized Resolution                */
#define CAMERA_R160x80                  0x05   /* Customized Resolution                */
#define CAMERA_R80x40                   0x04   /* Customized Resolution                */
#define CAMERA_R160x120                 0x00   /* QQVGA Resolution                     */
#define CAMERA_R320x240                 0x01   /* QVGA Resolution                      */
#define CAMERA_R480x272                 0x02   /* 480x272 Resolution                   */
#define CAMERA_R640x480                 0x03   /* VGA Resolution                       */

#define CAMERA_CONTRAST_BRIGHTNESS      0x00   /* Camera contrast brightness features  */
#define CAMERA_BLACK_WHITE              0x01   /* Camera black white feature           */
#define CAMERA_COLOR_EFFECT             0x03   /* Camera color effect feature          */

#define CAMERA_BRIGHTNESS_LEVEL0        0x00   /* Brightness level -2         */
#define CAMERA_BRIGHTNESS_LEVEL1        0x01   /* Brightness level -1         */
#define CAMERA_BRIGHTNESS_LEVEL2        0x02   /* Brightness level 0          */
#define CAMERA_BRIGHTNESS_LEVEL3        0x03   /* Brightness level +1         */
#define CAMERA_BRIGHTNESS_LEVEL4        0x04   /* Brightness level +2         */

#define CAMERA_CONTRAST_LEVEL0          0x05   /* Contrast level -2           */
#define CAMERA_CONTRAST_LEVEL1          0x06   /* Contrast level -1           */
#define CAMERA_CONTRAST_LEVEL2          0x07   /* Contrast level  0           */
#define CAMERA_CONTRAST_LEVEL3          0x08   /* Contrast level +1           */
#define CAMERA_CONTRAST_LEVEL4          0x09   /* Contrast level +2           */

#define CAMERA_BLACK_WHITE_BW           0x00   /* Black and white effect      */
#define CAMERA_BLACK_WHITE_NEGATIVE     0x01   /* Negative effect             */
#define CAMERA_BLACK_WHITE_BW_NEGATIVE  0x02   /* BW and Negative effect      */
#define CAMERA_BLACK_WHITE_NORMAL       0x03   /* Normal effect               */

#define CAMERA_COLOR_EFFECT_NONE        0x00   /* No effects                  */
#define CAMERA_COLOR_EFFECT_BLUE        0x01   /* Blue effect                 */
#define CAMERA_COLOR_EFFECT_GREEN       0x02   /* Green effect                */
#define CAMERA_COLOR_EFFECT_RED         0x03   /* Red effect                  */
#define CAMERA_COLOR_EFFECT_ANTIQUE     0x04   /* Antique effect              */


#define CAMERA_I2C_WRITE(addr_7bit, reg, value)        CSK_I2C_WRITE_BYTE(addr_7bit, reg, value)
#define CAMERA_I2C_READ(addr_7bit, reg)                CSK_I2C_READ_BYTE(addr_7bit, reg)


typedef enum {
    CAMERA_GC0328 = 0,
    CAMERA_TYPE_BUTT,
} camera_type_e;

typedef enum {
    CAMERA_FORMAT_RGB565 = 0,
    CAMERA_FORMAT_YUV422_CRY0CBY1,
    CAMERA_FORMAT_BUTT,
} camera_format_e;

typedef struct
{
    camera_type_e type;
    camera_format_e format;
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint32_t clk_hz;
}camera_attr_t;

#endif


#ifdef __cplusplus
}
#endif

#endif /* __CAMERA_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
