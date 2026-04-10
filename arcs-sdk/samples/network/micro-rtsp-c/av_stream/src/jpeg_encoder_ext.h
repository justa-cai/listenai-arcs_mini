/* JPEG硬件编码接口
 * 将初始化、编码和停止分离，避免每次编码都重复初始化硬件
 */

#ifndef __JPEG_ENCODER_EXT_H__
#define __JPEG_ENCODER_EXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "Driver_JPEG.h"
#include "Driver_DMA2D.h"
#include "Driver_GPDMA.h"

/* JPEG编码器配置 */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t input_format;  /* JPEG_PIXEL_FORMAT_YUV422, JPEG_PIXEL_FORMAT_RGB888等 */
} jpeg_enc_cfg_t;

/* JPEG编码器上下文 */
typedef struct {
    Jpeg_InitTypeDef jpeg_cfg;
    csk_dma2d_ch_t in_ch;
    csk_dma2d_ch_t transfer_ch;
    csk_dma2d_ch_t out_ch;
    uint8_t *pipo_buf[2];
    uint8_t *pipo_buf_ori[2];
    uint8_t *ecs_output_buffer;
    uint8_t *ecs_output_buffer_ori;
    uint8_t *mcu_buffer;            /* 软件MCU块转换缓冲区 */
    uint8_t *mcu_buffer_ori;        /* 软件MCU块转换缓冲区（原始指针）*/
    uint8_t *planar_buffer;         /* 硬件平面格式转换缓冲区 */
    uint8_t *planar_buffer_ori;     /* 硬件平面格式转换缓冲区（原始指针）*/
    uint32_t pipo_buf_len;
    uint32_t ecs_len;
    uint8_t initialized;
} jpeg_enc_ctx_t;

/**
 * @brief 初始化JPEG编码器（仅调用一次）
 *
 * @param ctx 编码器上下文
 * @param cfg 编码器配置
 * @return int32_t 0表示成功，其他表示失败
 */
int32_t jpeg_encoder_init(jpeg_enc_ctx_t *ctx, jpeg_enc_cfg_t *cfg);

/**
 * @brief 编码一帧图像（快速编码，无重复初始化）
 *
 * @param ctx 编码器上下文
 * @param in_buf 输入图像数据
 * @param out_buf 输出JPEG数据缓冲区
 * @param out_size 输出JPEG数据大小
 * @return int32_t 0表示成功，其他表示失败
 */
int32_t jpeg_encoder_encode(jpeg_enc_ctx_t *ctx, void *in_buf, void *out_buf, uint32_t *out_size);

/**
 * @brief 停止并释放JPEG编码器资源
 *
 * @param ctx 编码器上下文
 * @return int32_t 0表示成功，其他表示失败
 */
int32_t jpeg_encoder_stop(jpeg_enc_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __JPEG_ENCODER_EXT_H__ */
