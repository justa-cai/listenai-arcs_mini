#ifndef __INCLUDE_DRIVER_JPEG_H
#define __INCLUDE_DRIVER_JPEG_H

#include "Driver_Common.h"

#define CSK_JPEG_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)         /*!< API version */

typedef enum
{
    JPEG_MODE_ENCODE  = 0x00U,
    JPEG_MODE_DECODE  = 0x01U,
    JPEG_MODE_BUTT
}Jpeg_emMode;

typedef enum
{
    JPEG_DECODE_IN_FORMAT_YUV422  = 0x00U,
    JPEG_DECODE_IN_FORMAT_YUV420  = 0x01U,
    JPEG_DECODE_IN_FORMAT_YUV444  = 0x02U,
    JPEG_DECODE_IN_FORMAT_YUV411  = 0x03U,
    JPEG_DECODE_IN_FORMAT_GRAY    = 0x04U,
    JPEG_DECODE_IN_FORMAT_BUTT
}Jpeg_emFormatIn;

typedef enum
{
    JPEG_DECODE_OUT_FORMAT_YUV422  = 0x00U,
    JPEG_DECODE_OUT_FORMAT_RGB     = 0x01U,
    JPEG_DECODE_OUT_FORMAT_BUTT
}Jpeg_emtFormatOut;

/**
  * @brief  DVP interrupt status enum definition
  */
typedef enum
{
    JPEG_IRQ_EVENT_CODEC_DONE      = 0x00U,  /*!< JPEG dma_vic_single  */
    JPEG_IRQ_EVENT_PDMA_DONE       = 0x01U,  /*!< JPEG dma_vic_req     */
    JPEG_IRQ_EVENT_EDMA_DONE       = 0x02U,  /*!< JPEG fifo_unflow     */
    JPEG_IRQ_EVENT_BUTT
}Jpeg_emIrqEvent;

typedef struct
{
    Jpeg_emMode  mode;
    Jpeg_emFormatIn  format_in;
    Jpeg_emtFormatOut  format_out;
    uint32_t pixel_size;            // byte
    uint32_t ecs_size;              // byte
    uint32_t src_size;              // byte
    uint16_t img_width;
    uint16_t img_height;
    uint16_t img_width_align;
    uint16_t img_height_align;
    uint8_t rst_enable;
    uint8_t rst_num;                //number of MCUs between two restart markers
    uint8_t sampling_h;             //y horizontal sampling piexl num per block(8/16)
    uint8_t sampling_v;             //y vertical sampling piexl num per block(8/16)
    uint8_t qt_index[4];            //indicates the quantization table for the colour coomponent(0/1)
    uint8_t ht_index[4];            //indicates the huffman table DC&AC for the colour coomponent(AC:0~3bit  DC:4~7bit)
}Jpeg_InitTypeDef;


/**
 \fn          void Jpeg_SignalEvent_t (JPEG_emIrqEvent event, uint32_t usr_param)
 \brief       Signal JPEG Events.
 \param[in]   event        JPEG event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void (*Jpeg_SignalEvent_t)(Jpeg_emIrqEvent event, uint32_t param);


//------------------------------------------------------------------------------------------
/**
  * @brief  Return Jpeg driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
Jpeg_GetVersion(void);

/**
 * @brief Initialize the Jpeg device.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @param pCallback The callback function for Jpeg events.
 * @param pCfg The configuration structure for the Jpeg device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t
Jpeg_Initialize(void *pJpegDev, void *pCallback, Jpeg_InitTypeDef *pCfg);


/**
 * @brief Uninitializes the Jpeg device.
 *
 * This function uninitializes the Jpeg device by performing a reset and disabling the VI.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t
Jpeg_Uninitialize(void *pJpegDev);

/**
 * @brief Start the Jpeg to capture video frames.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
Jpeg_Start(void *pJpegDev);

/**
 * @brief Stops the Jpeg and releases its resources.
 *
 * @param pJpegDev A pointer to the Jpeg device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
Jpeg_Stop(void *pJpegDev);


/**
  * @brief  Return Jpeg instance.
  *
  * @return Instance of Jpeg
  */
void* Jpeg0(void);


/**
  * @brief  Return addr:
  *
  * @return Jpeg_EncodeIn_DecodeOut_Address
  */
uint32_t Jpeg0_PixelBuf(void);

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeIn_EncodeOut_Address
  */
uint32_t Jpeg0_ECSBuf(void);

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_EncodeQuanTable_Address
  */
uint32_t Jpeg0_EncodeQuanBuf(void);

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_EncodeHuffTable_Address
  */
uint32_t Jpeg0_EncodeHuffBuf(void);

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeMinTable_Address
  */
uint32_t Jpeg0_DecodeMinBuf(void);

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeBaseTable_Address
  */
uint32_t Jpeg0_DecodeBaseBuf(void);

/**
  * @brief  Return Addr:
  *
  * @return Jpeg_DecodeSymTable_Address
  */
uint32_t Jpeg0_DecodeSymBuf(void);


#endif /* __DRIVER_JPEG_H */


