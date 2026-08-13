#ifndef __LS_JPEG_H__
#define __LS_JPEG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "venusa_ap.h"
#include "mmio.h"
#include "cache.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_DMA2D.h"
#include "Driver_JPEG.h"
#include "log_print.h"
//#include "csk_timer.h"
#include "systick.h"
//#include "csk_driver.h"
#include "venusa_ap.h"

//#include "csk_clk_reset.h"
//#include "check.h"

#define CSK_JPEG_BLOCK_BASIC_PIXEL_NUM                  8        /* codec basic block pixel num */

typedef enum
{
    JPEG_PIXEL_FORMAT_YUV422 = 0x00U,     /* reserved */
    JPEG_PIXEL_FORMAT_YUV420 = 0x01U,     /* reserved */
    JPEG_PIXEL_FORMAT_YUV444 = 0x02U,     /* reserved */
    JPEG_PIXEL_FORMAT_YUV411 = 0x03U,     /* reserved */
    JPEG_PIXEL_FORMAT_GRAY   = 0x04U,     /* reserved apply to enc-input and dec-output */
    JPEG_PIXEL_FORMAT_RGB888 = 0x05U,     /* apply to enc-input and dec-output */
    JPEG_PIXEL_FORMAT_RGB565 = 0x06U,     /* reserved apply to enc-input and dec-output */

    JPEG_PIXEL_FORMAT_BUTT
}Jpeg_emPixelFormat;

typedef struct
{
    uint16_t width;
    uint16_t height;
    Jpeg_emPixelFormat pixel_format;
    uint16_t q_factor;  /* (encoder valid)quantization coefficient [1,99], the larger the number of values, the smaller the quantization coefficient, the better the image quality and the lower the compression rate */
}Jpeg_CodecCfg;


int32_t jpeg_init(Jpeg_InitTypeDef *pcfg);
int32_t jpeg_deinit(void);
int32_t jpeg_start(void);
int32_t jpeg_stop(void);
void jpeg_reg_dump(void);
void jpeg_reset(void);
void jpeg_refresh_quantization_table(uint32_t *qtable, uint16_t q_factor);
void jpeg_send_huffman_table(const uint32_t *htable, uint32_t size_word);
void jpeg_send_quantization_table(const uint32_t *qtable, uint32_t size_word);
void jpeg_send_base_table(const uint32_t *base_table, uint32_t size_word);
void jpeg_send_min_table(const uint32_t *min_table, uint32_t size_word);
void jpeg_send_symbol_table(const uint32_t *symbol_table, uint32_t size_word);

int32_t jpeg_encode_dma2d_init(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch, Jpeg_CodecCfg *enc_cfg);
int32_t jpeg_encode_dma2d_start(csk_dma2d_ch_t in_ch, void *in_buf, uint32_t in_size, csk_dma2d_ch_t out_ch, void *out_buf, uint32_t out_size);
int32_t jpeg_encode_dma2d_stop(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch);
int32_t jpeg_decode_dma2d_init(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch, Jpeg_CodecCfg *dec_cfg);
int32_t jpeg_decode_dma2d_start(csk_dma2d_ch_t in_ch, void *in_buf, uint32_t in_size, csk_dma2d_ch_t out_ch, void *out_buf, uint32_t out_size);
int32_t jpeg_decode_dma2d_stop(csk_dma2d_ch_t in_ch, csk_dma2d_ch_t out_ch);
uint32_t jpeg_dma2d_output_finish_cnt_get(void);
uint32_t jpeg_encoder(void *in_buf, void *out_buf, uint32_t *out_size, Jpeg_CodecCfg *enc_cfg);
uint32_t jpeg_encoder_ext(void *in_buf, void *out_buf, uint32_t *out_size, Jpeg_CodecCfg *enc_cfg);
uint32_t jpeg_decoder(const uint8_t *in_buf, uint32_t in_size, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg);
uint32_t jpeg_decoder_ext(const uint8_t *in_buf, uint32_t in_size, uint8_t *out_buf, Jpeg_CodecCfg *dec_cfg);


#ifdef __cplusplus
}
#endif

#endif /* __LS_JPEG_H__ */
