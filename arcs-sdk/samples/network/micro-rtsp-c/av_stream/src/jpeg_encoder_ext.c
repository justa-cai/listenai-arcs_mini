/* JPEG硬件编码实现
 * 参考ls_jpeg.c但直接调用Driver_JPEG.h和DMA2D原生函数
 * 避免每次编码都重复初始化硬件和DMA
 */

 #include <stdlib.h>
#include "Driver_DMA2D.h"
#include "Driver_GPDMA.h"
#include "Driver_JPEG.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "jpeg_encoder_ext.h"
#include "arcs_ap.h"
#include "cache.h"
#include "lisa_log.h"
#include "log_print.h"

#define CSK_JPEG_GPDMA_ADDR_ALIGNMENT    32
#define CSK_JPEG_BLOCK_ALIGN(a,b)        ((((a)+(b)-1)/(b))*(b))
#define CSK_JPEG_BLOCK_BASIC_PIXEL_NUM   8

/* 使用软件YUV格式转换（设置为1）或硬件DMA2D转换（设置为0）*/
#define USE_SOFTWARE_YUV_CONVERSION      1

/* JPEG编码哈夫曼表 */
static const uint32_t jpeg_enc_htable_golden[384] = {
    0x100, 0x101, 0x204, 0x30b, 0x41a, 0x678, 0x7f8, 0x9f6, 0xf82, 0xf83,
    0x30c, 0x41b, 0x679, 0x8f6, 0xaf6, 0xf84, 0xf85, 0xf86, 0xf87, 0xf88,
    0x41c, 0x7f9, 0x9f7, 0xbf4, 0xf89, 0xf8a, 0xf8b, 0xf8c, 0xf8d, 0xf8e,
    0x53a, 0x8f7, 0xbf5, 0xf8f, 0xf90, 0xf91, 0xf92, 0xf93, 0xf94, 0xf95,
    0x53b, 0x9f8, 0xf96, 0xf97, 0xf98, 0xf99, 0xf9a, 0xf9b, 0xf9c, 0xf9d,
    0x67a, 0xaf7, 0xf9e, 0xf9f, 0xfa0, 0xfa1, 0xfa2, 0xfa3, 0xfa4, 0xfa5,
    0x67b, 0xbf6, 0xfa6, 0xfa7, 0xfa8, 0xfa9, 0xfaa, 0xfab, 0xfac, 0xfad,
    0x7fa, 0xbf7, 0xfae, 0xfaf, 0xfb0, 0xfb1, 0xfb2, 0xfb3, 0xfb4, 0xfb5,
    0x8f8, 0xec0, 0xfb6, 0xfb7, 0xfb8, 0xfb9, 0xfba, 0xfbb, 0xfbc, 0xfbd,
    0x8f9, 0xfbe, 0xfbf, 0xfc0, 0xfc1, 0xfc2, 0xfc3, 0xfc4, 0xfc5, 0xfc6,
    0x8fa, 0xfc7, 0xfc8, 0xfc9, 0xfca, 0xfcb, 0xfcc, 0xfcd, 0xfce, 0xfcf,
    0x9f9, 0xfd0, 0xfd1, 0xfd2, 0xfd3, 0xfd4, 0xfd5, 0xfd6, 0xfd7, 0xfd8,
    0x9fa, 0xfd9, 0xfda, 0xfdb, 0xfdc, 0xfdd, 0xfde, 0xfdf, 0xfe0, 0xfe1,
    0xaf8, 0xfe2, 0xfe3, 0xfe4, 0xfe5, 0xfe6, 0xfe7, 0xfe8, 0xfe9, 0xfea,
    0xfeb, 0xfec, 0xfed, 0xfee, 0xfef, 0xff0, 0xff1, 0xff2, 0xff3, 0xff4,
    0xff5, 0xff6, 0xff7, 0xff8, 0xff9, 0xffa, 0xffb, 0xffc, 0xffd, 0xffe,
    0x30a, 0xaf9, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfd0, 0xfd1,
    0xfd2, 0xfd3, 0xfd4, 0xfd5, 0xfd6, 0xfd7, 0x101, 0x204, 0x30a, 0x418,
    0x419, 0x538, 0x678, 0x8f4, 0x9f6, 0xbf4, 0x30b, 0x539, 0x7f6, 0x8f5,
    0xaf6, 0xbf5, 0xf88, 0xf89, 0xf8a, 0xf8b, 0x41a, 0x7f7, 0x9f7, 0xbf6,
    0xec2, 0xf8c, 0xf8d, 0xf8e, 0xf8f, 0xf90, 0x41b, 0x7f8, 0x9f8, 0xbf7,
    0xf91, 0xf92, 0xf93, 0xf94, 0xf95, 0xf96, 0x53a, 0x8f6, 0xf97, 0xf98,
    0xf99, 0xf9a, 0xf9b, 0xf9c, 0xf9d, 0xf9e, 0x53b, 0x9f9, 0xf9f, 0xfa0,
    0xfa1, 0xfa2, 0xfa3, 0xfa4, 0xfa5, 0xfa6, 0x679, 0xaf7, 0xfa7, 0xfa8,
    0xfa9, 0xfaa, 0xfab, 0xfac, 0xfad, 0xfae, 0x67a, 0xaf8, 0xfaf, 0xfb0,
    0xfb1, 0xfb2, 0xfb3, 0xfb4, 0xfb5, 0xfb6, 0x7f9, 0xfb7, 0xfb8, 0xfb9,
    0xfba, 0xfbb, 0xfbc, 0xfbd, 0xfbe, 0xfbf, 0x8f7, 0xfc0, 0xfc1, 0xfc2,
    0xfc3, 0xfc4, 0xfc5, 0xfc6, 0xfc7, 0xfc8, 0x8f8, 0xfc9, 0xfca, 0xfcb,
    0xfcc, 0xfcd, 0xfce, 0xfcf, 0xfd0, 0xfd1, 0x8f9, 0xfd2, 0xfd3, 0xfd4,
    0xfd5, 0xfd6, 0xfd7, 0xfd8, 0xfd9, 0xfda, 0x8fa, 0xfdb, 0xfdc, 0xfdd,
    0xfde, 0xfdf, 0xfe0, 0xfe1, 0xfe2, 0xfe3, 0xaf9, 0xfe4, 0xfe5, 0xfe6,
    0xfe7, 0xfe8, 0xfe9, 0xfea, 0xfeb, 0xfec, 0xde0, 0xfed, 0xfee, 0xfef,
    0xff0, 0xff1, 0xff2, 0xff3, 0xff4, 0xff5, 0xec3, 0xff6, 0xff7, 0xff8,
    0xff9, 0xffa, 0xffb, 0xffc, 0xffd, 0xffe, 0x100, 0x9fa, 0xfff, 0xfff,
    0xfff, 0xfff, 0xfff, 0xfff, 0xfd0, 0xfd1, 0xfd2, 0xfd3, 0xfd4, 0xfd5,
    0xfd6, 0xfd7, 0x100, 0x202, 0x203, 0x204, 0x205, 0x206, 0x30e, 0x41e,
    0x53e, 0x67e, 0x7fe, 0x8fe, 0xfff, 0xfff, 0xfff, 0xfff, 0x100, 0x101,
    0x102, 0x206, 0x30e, 0x41e, 0x53e, 0x67e, 0x7fe, 0x8fe, 0x9fe, 0xafe,
    0xfff, 0xfff, 0xfff, 0xfff
};

/* JPEG编码量化表 */
static const uint32_t jpeg_enc_qtable_golden[128] = {
    0x0100, 0x0155, 0x0155, 0x0a49, 0x0155, 0x0b33, 0x0100, 0x0a49,
    0x0a49, 0x0a49, 0x09c7, 0x09c7, 0x0100, 0x00cd, 0x0955, 0x08cd,
    0x093b, 0x0955, 0x12e9, 0x12e9, 0x0955, 0x251f, 0x11c7, 0x11af,
    0x0911, 0x08cd, 0x1a35, 0x113b, 0x2421, 0x1111, 0x1a35, 0x113b,
    0x1a49, 0x1a49, 0x0880, 0x19c7, 0x10b2, 0x10d2, 0x0880, 0x10f1,
    0x10ba, 0x10ea, 0x1a49, 0x1a49, 0x10cd, 0x1095, 0x231f, 0x10ba,
    0x1955, 0x229d, 0x193b, 0x193b, 0x193b, 0x2421, 0x10d2, 0x223f,
    0x2219, 0x2249, 0x10a4, 0x1911, 0x10b2, 0x2d05, 0x193b, 0x10a4,
    0x09c7, 0x09c7, 0x09c7, 0x0955, 0x12e9, 0x0955, 0x1155, 0x093b,
    0x093b, 0x1155, 0x10a4, 0x23e1, 0x1a49, 0x23e1, 0x10a4, 0x10a4,
    0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4,
    0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4,
    0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4,
    0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4,
    0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4,
    0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4, 0x10a4
};

/* 全局变量用于跟踪DMA完成状态 */
static SemaphoreHandle_t jpeg_dma_sem = NULL;

/* DMA2D输出完成回调 */
static void jpeg_dma_output_callback(uint32_t event, void* workspace)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(jpeg_dma_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* 发送Huffman表到JPEG硬件 */
static void jpeg_send_huffman_table(const uint32_t *htable, uint32_t size_word)
{
    const uint32_t *src = htable;
    uint32_t *dst = (uint32_t *)Jpeg0_EncodeHuffBuf();
    for (uint32_t i = 0; i < size_word; i++) {
        *dst++ = *src++;
    }
}

/* 发送量化表到JPEG硬件 */
static void jpeg_send_quantization_table(const uint32_t *qtable, uint32_t size_word)
{
    const uint32_t *src = qtable;
    uint32_t *dst = (uint32_t *)Jpeg0_EncodeQuanBuf();
    for (uint32_t i = 0; i < size_word; i++) {
        *dst++ = *src++;
    }
}

/* 计算图像大小 */
static uint32_t jpeg_image_size(uint16_t width, uint16_t height, Jpeg_emFormatIn format)
{
    uint32_t size;
    switch(format) {
        case JPEG_DECODE_IN_FORMAT_YUV444:
            size = width * height * 3;
            break;
        case JPEG_DECODE_IN_FORMAT_YUV422:
            size = width * height * 2;
            break;
        case JPEG_DECODE_IN_FORMAT_YUV420:
        case JPEG_DECODE_IN_FORMAT_YUV411:
            size = width * height * 3 / 2;
            break;
        case JPEG_DECODE_IN_FORMAT_GRAY:
            size = width * height;
            break;
        default:
            size = width * height * 3 / 2;
            break;
    }
    return size;
}

/* 图像格式转换辅助函数 */
static void jpeg_img_format_switch(csk_dma_2d_image_cfg_t *img_cfg, uint8_t input_format)
{
    switch (input_format) {
        case 0x00: /* JPEG_PIXEL_FORMAT_YUV422 */
            img_cfg->img_input_format = csk_image_format_yuv422;
            img_cfg->img_yuv422_format = csk_image_yuv422_format_y0cby1cr;
            break;
        case 0x01: /* JPEG_PIXEL_FORMAT_YUV420 */
        case 0x03: /* JPEG_PIXEL_FORMAT_YUV411 */
            img_cfg->img_input_format = csk_image_format_yuv420;
            img_cfg->img_yuv420_format = csk_image_yuv420_format_y0cby1;
            break;
        case 0x02: /* JPEG_PIXEL_FORMAT_YUV444 */
            img_cfg->img_input_format = csk_image_format_yuv444;
            break;
        case 0x05: /* JPEG_PIXEL_FORMAT_RGB888 */
            img_cfg->img_input_format = csk_image_format_xrgb;
            img_cfg->img_rgb888_format = csk_image_rgb888_format;
            break;
        default:
            LOGW("[%s:%d] unsupport pixel format=%d", __func__, __LINE__, input_format);
            break;
    }
}

/**
 * @brief 将 YUYV 格式转换为 JPEG MCU 块格式（软件实现，参考ls_jpeg.c）
 * @param yuyv_data 输入的 YUYV 交叉数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param width_align 对齐后的宽度
 * @param height_align 对齐后的高度
 * @param sampling_h 水平采样块大小（YUV422为16）
 * @param sampling_v 垂直采样块大小（YUV422为8）
 * @param mcu_data 输出的MCU块数据
 */
static void yuyv_to_jpeg_mcu(const uint8_t *yuyv_data, int width, int height, 
                             int width_align, int height_align,
                             int sampling_h, int sampling_v, uint8_t *mcu_data)
{
    int32_t i, j, w, h;
    uint8_t pixel_bw = 2; /* YUV422 YUYV格式每个像素2字节 */
    int32_t yb[8][16], cbb[8][8], crb[8][8];
    int32_t *c0 = NULL;
    int32_t *c1 = NULL;
    int32_t *c2 = NULL;
    uint32_t pix_index;
    uint32_t mcu_index = 0;
    uint32_t block_offset = (width_align - sampling_h) * pixel_bw;
    uint16_t padding_v = height_align - height;

    /* 处理每个8x16 MCU块 (YUV422格式) */
    for(h = 0; h < height_align; h += sampling_v)
    {
        /* 按8x16块处理 (2个8x8 Y块 + 1个8x8 Cb块 + 1个8x8 Cr块) */
        for(w = 0; w < width_align; w += sampling_h)
        {
            pix_index = (h * width + w) * pixel_bw;

            /* 提取8x16块的YUV数据 */
            for(i = 0; i < sampling_v;)
            {
                c0 = &yb[i][0];
                c1 = &cbb[i][0];
                c2 = &crb[i][0];
                for(j = 0; j < sampling_h; j++)
                {
                    /* YUYV格式: Y0 U0 Y1 V0 | Y2 U1 Y3 V1 | ... */
                    *c0++ = yuyv_data[pix_index];  /* Y */
                    
                    /* 色度分量每2个像素采样一次 */
                    if (0 == j % 2)
                        *c1++ = yuyv_data[pix_index + 1];  /* U */
                    else
                        *c2++ = yuyv_data[pix_index + 1];  /* V */
                    
                    if (w < width)
                    {
                        pix_index += pixel_bw;
                    }
                }
                i++;
                
                /* 处理垂直边界padding */
                if (padding_v && (h + i) >= height)
                {
                    for (; i < sampling_v; i++)
                    {
                        memcpy(&yb[i][0], &yb[i-1][0], sizeof(yb[0]));
                        memcpy(&cbb[i][0], &cbb[i-1][0], sizeof(cbb[0]));
                        memcpy(&crb[i][0], &crb[i-1][0], sizeof(crb[0]));
                    }
                    break;
                }
                pix_index += block_offset;
            }

            /* 输出第一个Y块 (8x8) */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }
            
            /* 输出第二个Y块 (8x8) */
            for(i = 0; i < 8; i++)
            {
                for(j = 8; j < 16; j++)
                {
                    mcu_data[mcu_index++] = yb[i][j];
                }
            }

            /* 输出Cb块 (8x8) */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = cbb[i][j];
                }
            }

            /* 输出Cr块 (8x8) */
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    mcu_data[mcu_index++] = crb[i][j];
                }
            }
        }
    }
}

/**
 * @brief 初始化JPEG编码器
 */
int32_t jpeg_encoder_init(jpeg_enc_ctx_t *ctx, jpeg_enc_cfg_t *cfg)
{
    int32_t ret = 0;
    uint8_t pixel_bw = 3;

    if (!ctx || !cfg) {
        LOGE("[%s:%d] Invalid parameters", __func__, __LINE__);
        return -1;
    }

    memset(ctx, 0, sizeof(jpeg_enc_ctx_t));
    
    if (jpeg_dma_sem == NULL) {
        jpeg_dma_sem = xSemaphoreCreateBinary();
        if (jpeg_dma_sem == NULL) {
            LOGE("[%s:%d] Failed to create semaphore", __func__, __LINE__);
            return -1;
        }
    }

    /* 配置JPEG参数 */
    ctx->jpeg_cfg.mode = JPEG_MODE_ENCODE;
    ctx->jpeg_cfg.format_in = JPEG_DECODE_IN_FORMAT_YUV420;
    ctx->jpeg_cfg.ecs_size = 0;
    ctx->jpeg_cfg.rst_enable = 0;
    ctx->jpeg_cfg.rst_num = 0;
    ctx->jpeg_cfg.sampling_h = 2 * CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
    ctx->jpeg_cfg.sampling_v = 2 * CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
    ctx->jpeg_cfg.qt_index[0] = 0x00;
    ctx->jpeg_cfg.qt_index[1] = 0x01;
    ctx->jpeg_cfg.qt_index[2] = 0x01;
    ctx->jpeg_cfg.qt_index[3] = 0x00;
    ctx->jpeg_cfg.ht_index[0] = 0x00;
    ctx->jpeg_cfg.ht_index[1] = 0x11;
    ctx->jpeg_cfg.ht_index[2] = 0x11;
    ctx->jpeg_cfg.ht_index[3] = 0x00;

    /* 根据输入格式调整参数 */
    if (cfg->input_format == 0x04) { /* GRAY */
        ctx->jpeg_cfg.format_in = JPEG_DECODE_IN_FORMAT_GRAY;
        ctx->jpeg_cfg.sampling_h = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
        ctx->jpeg_cfg.sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
        pixel_bw = 1;
    } else if (cfg->input_format == 0x00) { /* YUV422 */
        ctx->jpeg_cfg.format_in = JPEG_DECODE_IN_FORMAT_YUV422;
        ctx->jpeg_cfg.sampling_h = 2 * CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
        ctx->jpeg_cfg.sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
        pixel_bw = 2;
    } else if (cfg->input_format == 0x02) { /* YUV444 */
        ctx->jpeg_cfg.format_in = JPEG_DECODE_IN_FORMAT_YUV444;
        ctx->jpeg_cfg.sampling_h = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
        ctx->jpeg_cfg.sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
        pixel_bw = 3;
    }

    ctx->jpeg_cfg.img_width = cfg->width;
    ctx->jpeg_cfg.img_height = cfg->height;
    ctx->jpeg_cfg.img_width_align = CSK_JPEG_BLOCK_ALIGN(ctx->jpeg_cfg.img_width, ctx->jpeg_cfg.sampling_h);
    ctx->jpeg_cfg.img_height_align = CSK_JPEG_BLOCK_ALIGN(ctx->jpeg_cfg.img_height, ctx->jpeg_cfg.sampling_v);
    ctx->jpeg_cfg.pixel_size = jpeg_image_size(ctx->jpeg_cfg.img_width_align,
                                                     ctx->jpeg_cfg.img_height_align,
                                                     ctx->jpeg_cfg.format_in);
    ctx->jpeg_cfg.src_size = ctx->jpeg_cfg.pixel_size;

    LOGI("[%s:%d] JPEG config: %dx%d, align=%dx%d, format=%d, pixel_size=%u",
          __func__, __LINE__, cfg->width, cfg->height,
          ctx->jpeg_cfg.img_width_align, ctx->jpeg_cfg.img_height_align,
          ctx->jpeg_cfg.format_in, ctx->jpeg_cfg.pixel_size);

    /* 初始化JPEG硬件 */
    void *jpeg_dev = Jpeg0();
    ret = Jpeg_Initialize(jpeg_dev, NULL, &ctx->jpeg_cfg);
    if (ret != 0) {
        LOGE("[%s:%d] Jpeg_Initialize failed: %d", __func__, __LINE__, ret);
        return ret;
    }

    /* 发送Huffman和量化表 */
    jpeg_send_huffman_table(jpeg_enc_htable_golden, sizeof(jpeg_enc_htable_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_enc_qtable_golden, sizeof(jpeg_enc_qtable_golden)/sizeof(uint32_t));

#if !USE_SOFTWARE_YUV_CONVERSION
    /* 硬件模式：分配PIPO缓冲区 */
    ctx->pipo_buf_len = ctx->jpeg_cfg.img_width_align * pixel_bw * ctx->jpeg_cfg.sampling_v;
    for (uint8_t i = 0; i < 2; i++) {
        ctx->pipo_buf_ori[i] = (uint8_t*)malloc(ctx->pipo_buf_len + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1);
        if (!ctx->pipo_buf_ori[i]) {
            LOGE("[%s:%d] malloc pipo_buf[%d] failed: size=%u", __func__, __LINE__, i, ctx->pipo_buf_len);
            goto error_cleanup;
        }
        ctx->pipo_buf[i] = (uint8_t*)(((size_t)ctx->pipo_buf_ori[i] + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1) &
                                       ~(CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1));
        memset(ctx->pipo_buf[i], 0, ctx->pipo_buf_len);
    }
#endif

    /* 分配ECS输出缓冲区 */
    ctx->ecs_len = ctx->jpeg_cfg.pixel_size;
    ctx->ecs_output_buffer_ori = (uint8_t*)malloc(ctx->ecs_len + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1);
    if (!ctx->ecs_output_buffer_ori) {
        LOGE("[%s:%d] malloc ecs_output_buffer failed: size=%u", __func__, __LINE__, ctx->ecs_len);
        goto error_cleanup;
    }
    ctx->ecs_output_buffer = (uint8_t*)(((size_t)ctx->ecs_output_buffer_ori + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1) &
                                         ~(CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1));
    memset(ctx->ecs_output_buffer, 0, ctx->ecs_len);

#if USE_SOFTWARE_YUV_CONVERSION
    /* 软件模式：分配MCU块转换缓冲区 */
    ctx->mcu_buffer_ori = (uint8_t*)malloc(ctx->jpeg_cfg.pixel_size + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1);
    if (!ctx->mcu_buffer_ori) {
        LOGE("[%s:%d] malloc mcu_buffer failed: size=%u", __func__, __LINE__, ctx->jpeg_cfg.pixel_size);
        goto error_cleanup;
    }
    ctx->mcu_buffer = (uint8_t*)(((size_t)ctx->mcu_buffer_ori + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1) &
                                  ~(CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1));
    memset(ctx->mcu_buffer, 0, ctx->jpeg_cfg.pixel_size);
    LOGI("[%s:%d] 软件模式：已分配MCU转换缓冲区: %u bytes", __func__, __LINE__, ctx->jpeg_cfg.pixel_size);
#else
    /* 硬件模式：分配YUV平面格式缓冲区 */
    ctx->planar_buffer_ori = (uint8_t*)malloc(ctx->jpeg_cfg.pixel_size + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1);
    if (!ctx->planar_buffer_ori) {
        LOGE("[%s:%d] malloc planar_buffer failed: size=%u", __func__, __LINE__, ctx->jpeg_cfg.pixel_size);
        goto error_cleanup;
    }
    ctx->planar_buffer = (uint8_t*)(((size_t)ctx->planar_buffer_ori + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1) &
                                     ~(CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1));
    memset(ctx->planar_buffer, 0, ctx->jpeg_cfg.pixel_size);
    LOGI("[%s:%d] 硬件模式：已分配YUV平面转换缓冲区: %u bytes", __func__, __LINE__, ctx->jpeg_cfg.pixel_size);
#endif

    /* 初始化DMA2D */
    ret = DMA2D_Initialize();
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Initialize failed: %d", __func__, __LINE__, ret);
        goto error_cleanup;
    }

    if (ctx->transfer_ch == 0)
        ctx->transfer_ch = dma_2d_ch7;
    if (ctx->out_ch == 0)
        ctx->out_ch = dma_2d_ch8;

#if !USE_SOFTWARE_YUV_CONVERSION
    /* 配置DMA2D通道 */
    if (ctx->in_ch == 0)
        ctx->in_ch = dma_2d_ch6;

    /* 配置DMA2D输入通道 */
    csk_dma2d_init_t jpeg_enc_input = {
        .dma_ch = ctx->in_ch,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_pipo,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = dma2d_sample_unit_word,
        .handshake = qspi_hs_num0,
        .rd_done_ack = read_done_ack_enable,
    };

    csk_dma_2d_image_cfg_t jpeg_enc_input_img = {
        .img_output_fromat_transfer = csk_image_format_transfer_bypass,
        .image_yuv422_rotate_mode = csk_image_yuv422_no_rotate,
        .img_zoom_scale = csk_image_zoom_scale_1_to_1,
        .img_jpeg_2d_type = csk_jpeg_enc_unpack,
        .img_width = ctx->jpeg_cfg.img_width,
        .img_height = ctx->jpeg_cfg.img_height,
    };
    jpeg_img_format_switch(&jpeg_enc_input_img, cfg->input_format);

    /* 硬件模式：配置DMA2D输入通道用于格式转换 */
    ret = DMA2D_Config(&jpeg_enc_input, NULL, NULL);
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Config input failed: %d", __func__, __LINE__, ret);
        goto error_cleanup;
    }

    ret = DMA2D_Image_Config_Extend(ctx->in_ch, &jpeg_enc_input_img);
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Image_Config_Extend input failed: %d", __func__, __LINE__, ret);
        goto error_cleanup;
    }
#endif

    /* 配置DMA2D传输通道 */
    csk_dma2d_init_t jpeg_enc_transfer = {
        .dma_ch = ctx->transfer_ch,
        .burst_len = dma2d_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = dma2d_sample_unit_word,
        .handshake = jpg_p_hs_num7,
#if !USE_SOFTWARE_YUV_CONVERSION
        .flow_ctrl = dma2d_flow_ctrl_peripheral,
        .rd_done_ack = read_done_ack_enable,
        .trigger_ch = ctx->in_ch,
#endif
    };

    csk_dma_2d_image_cfg_t jpeg_enc_transfer_img = {
        .img_output_fromat_transfer = csk_image_format_transfer_bypass,
        .image_yuv422_rotate_mode = csk_image_yuv422_no_rotate,
        .img_zoom_scale = csk_image_zoom_scale_1_to_1,
        .img_jpeg_2d_type = csk_jpeg_enc_2d_transfer,
        .img_width = ctx->jpeg_cfg.img_width,
        .img_height = ctx->jpeg_cfg.img_height,
    };
    jpeg_img_format_switch(&jpeg_enc_transfer_img, cfg->input_format);

    ret = DMA2D_Config(&jpeg_enc_transfer, NULL, NULL);
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Config transfer failed: %d", __func__, __LINE__, ret);
        goto error_cleanup;
    }

#if !USE_SOFTWARE_YUV_CONVERSION
    /* 硬件模式：配置DMA2D传输通道的图像处理 */
    ret = DMA2D_Image_Config_Extend(ctx->transfer_ch, &jpeg_enc_transfer_img);
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Image_Config_Extend transfer failed: %d", __func__, __LINE__, ret);
        goto error_cleanup;
    }
#endif

    /* 配置DMA2D输出通道 */
    csk_dma2d_init_t jpeg_enc_output = {
        .dma_ch = ctx->out_ch,
        .burst_len = dma2d_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = dma2d_sample_unit_word,
        .handshake = jpg_e_hs_num6,
        .flow_ctrl = dma2d_flow_ctrl_peripheral,
    };

    ret = DMA2D_Config(&jpeg_enc_output, jpeg_dma_output_callback, NULL);
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Config output failed: %d", __func__, __LINE__, ret);
        goto error_cleanup;
    }

    ctx->initialized = 1;
    LOGI("[%s:%d] JPEG encoder initialized successfully", __func__, __LINE__);
    return 0;

error_cleanup:
    /* 清理已分配的资源 */
    for (uint8_t i = 0; i < 2; i++) {
        if (ctx->pipo_buf_ori[i]) {
            free(ctx->pipo_buf_ori[i]);
            ctx->pipo_buf_ori[i] = NULL;
        }
    }
    if (ctx->ecs_output_buffer_ori) {
        free(ctx->ecs_output_buffer_ori);
        ctx->ecs_output_buffer_ori = NULL;
    }
    Jpeg_Stop(jpeg_dev);
    Jpeg_Uninitialize(jpeg_dev);
    return -1;
}

/* JPEG构建函数 - 从ECS数据构建完整的JPEG文件 */
static uint32_t jpeg_build_jpeg(const uint8_t *ecs_buf, uint32_t ecs_size,
                                     uint8_t *out_buf, uint32_t *out_size,
                                     Jpeg_InitTypeDef *jpeg_cfg)
{
    uint32_t jpeg_index = 0;
    uint8_t app0_jfif[18] = {0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,0x01,0x01,0x00,0x78,0x00,0x78,0x00,0x00};
    uint8_t qt0[69] = {0xff,0xdb,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,0x07,0x09,0x09,0x08,0x0a,0x0c,0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,
                       0x1c,0x1c,0x20,0x24,0x2e,0x27,0x20,0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,0x39,0x3d,0x38,0x32,0x3c,0x2e,0x33,0x34,0x32};
    uint8_t qt1[69] = {0xff,0xdb,0x00,0x43,0x01,0x09,0x09,0x09,0x0c,0x0b,0x0c,0x18,0x0d,0x0d,0x18,0x32,0x21,0x1c,0x21,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
                       0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32};
    uint8_t huffm_dc0[33]={0xff,0xc4,0x00,0x1f,0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b};
    uint8_t huffm_dc1[33]={0xff,0xc4,0x00,0x1f,0x01,0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b};

    /* SOI */
    out_buf[jpeg_index++] = 0xff;
    out_buf[jpeg_index++] = 0xd8;

    /* APP0-JFIF */
    memcpy(&out_buf[jpeg_index], app0_jfif, sizeof(app0_jfif));
    jpeg_index += sizeof(app0_jfif);

    /* DQT0 */
    memcpy(&out_buf[jpeg_index], qt0, sizeof(qt0));
    jpeg_index += sizeof(qt0);

    /* DQT1 */
    memcpy(&out_buf[jpeg_index], qt1, sizeof(qt1));
    jpeg_index += sizeof(qt1);

    /* SOF0 - 帧头 */
    if (JPEG_DECODE_IN_FORMAT_GRAY == jpeg_cfg->format_in) {
        uint8_t sof0_gray[13] = {0xff,0xc0,0x00,0x0b,0x08,0x00,0x00,0x00,0x00,0x01,0x01,0x11,0x00};
        sof0_gray[5] = (jpeg_cfg->img_height>>8)&0xff;
        sof0_gray[6] = jpeg_cfg->img_height&0xff;
        sof0_gray[7] = (jpeg_cfg->img_width>>8)&0xff;
        sof0_gray[8] = jpeg_cfg->img_width&0xff;
        memcpy(&out_buf[jpeg_index], sof0_gray, sizeof(sof0_gray));
        jpeg_index += sizeof(sof0_gray);
    } else {
        uint8_t sof0_yuv[19] = {0xff,0xc0,0x00,0x11,0x08,0x00,0x00,0x00,0x00,0x03,0x01,0x22,0x00,0x02,0x11,0x01,0x03,0x11,0x01};
        sof0_yuv[5] = (jpeg_cfg->img_height>>8)&0xff;
        sof0_yuv[6] = jpeg_cfg->img_height&0xff;
        sof0_yuv[7] = (jpeg_cfg->img_width>>8)&0xff;
        sof0_yuv[8] = jpeg_cfg->img_width&0xff;
        if (JPEG_DECODE_IN_FORMAT_YUV444 == jpeg_cfg->format_in) {
            sof0_yuv[11] = 0x11;
        } else if (JPEG_DECODE_IN_FORMAT_YUV422 == jpeg_cfg->format_in) {
            sof0_yuv[11] = 0x21;
        }
        memcpy(&out_buf[jpeg_index], sof0_yuv, sizeof(sof0_yuv));
        jpeg_index += sizeof(sof0_yuv);
    }

    /* DHT - Huffman表 */
    memcpy(&out_buf[jpeg_index], huffm_dc0, sizeof(huffm_dc0));
    jpeg_index += sizeof(huffm_dc0);
    memcpy(&out_buf[jpeg_index], huffm_dc1, sizeof(huffm_dc1));
    jpeg_index += sizeof(huffm_dc1);

    /* AC Huffman表 */
    uint8_t huffm_ac0[183]={0xff,0xc4,0x00,0xb5,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7d,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
                            0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
                            0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
                            0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
                            0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
                            0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa};
    uint8_t huffm_ac1[183]={0xff,0xc4,0x00,0xb5,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
                            0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
                            0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
                            0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
                            0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
                            0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa};

    memcpy(&out_buf[jpeg_index], huffm_ac0, sizeof(huffm_ac0));
    jpeg_index += sizeof(huffm_ac0);
    memcpy(&out_buf[jpeg_index], huffm_ac1, sizeof(huffm_ac1));
    jpeg_index += sizeof(huffm_ac1);

    /* SOS - 扫描开始 */
    if (JPEG_DECODE_IN_FORMAT_GRAY == jpeg_cfg->format_in) {
        uint8_t sos_gray[10] = {0xff,0xda,0x00,0x08,0x01,0x01,0x00,0x00,0x3f,0x00};
        memcpy(&out_buf[jpeg_index], sos_gray, sizeof(sos_gray));
        jpeg_index += sizeof(sos_gray);
    } else {
        uint8_t sos_yuv[14] = {0xff,0xda,0x00,0x0c,0x03,0x01,0x00,0x02,0x11,0x03,0x11,0x00,0x3f,0x00};
        memcpy(&out_buf[jpeg_index], sos_yuv, sizeof(sos_yuv));
        jpeg_index += sizeof(sos_yuv);
    }

    /* ECS数据 */
    memcpy(&out_buf[jpeg_index], ecs_buf, ecs_size);
    jpeg_index += ecs_size;

    /* EOI - 图像结束 */
    out_buf[jpeg_index++] = 0xff;
    out_buf[jpeg_index++] = 0xd9;

    *out_size = jpeg_index;
    return 0;
}

/**
 * @brief 编码一帧图像（快速编码，无重复初始化）
 */
int32_t jpeg_encoder_encode(jpeg_enc_ctx_t *ctx, void *in_buf, void *out_buf, uint32_t *out_size)
{
    int32_t ret = 0;

    if (!ctx || !ctx->initialized) {
        LOGE("[%s:%d] Context not initialized", __func__, __LINE__);
        return -1;
    }
    void *jpeg_dev = Jpeg0();
    ret = Jpeg_Start(jpeg_dev);
    if (ret != 0) {
        LOGE("[%s:%d] Jpeg_Start failed: %d", __func__, __LINE__, ret);
        Jpeg_Uninitialize(Jpeg0());
        return ret;
    }

    /* 等待DMA完成 - 改为使用信号量 */
    /* 重置DMA完成计数 - 这里不需要了，直接Reset信号量即可 */
    xSemaphoreTake(jpeg_dma_sem, 0);

#if USE_SOFTWARE_YUV_CONVERSION
    /* 软件模式：CPU将 YUYV 格式转换为 JPEG MCU 块格式 */
    yuyv_to_jpeg_mcu((const uint8_t *)in_buf, 
                     ctx->jpeg_cfg.img_width, 
                     ctx->jpeg_cfg.img_height,
                     ctx->jpeg_cfg.img_width_align,
                     ctx->jpeg_cfg.img_height_align,
                     ctx->jpeg_cfg.sampling_h,
                     ctx->jpeg_cfg.sampling_v,
                     ctx->mcu_buffer);
    
    /* 刷新Cache，确保数据写入内存 */
    HAL_FlushDCache_by_Addr((uint32_t*)ctx->mcu_buffer, ctx->jpeg_cfg.pixel_size);
#else
    /* 硬件模式：DMA2D进行格式转换和MCU重组 */
    /* 启动DMA2D传输 - 输入通道（从YUYV到平面格式，再到PiPo缓冲区）*/
    ret = DMA2D_Start_PiPo(ctx->in_ch, in_buf, NULL,
                           ctx->pipo_buf[0], ctx->pipo_buf[1],
                           ctx->jpeg_cfg.pixel_size / sizeof(uint32_t),
                           ctx->jpeg_cfg.pixel_size / sizeof(uint32_t));
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Start_PiPo input failed: %d", __func__, __LINE__, ret);
        goto cleanup;
    }
#endif

    /* 启动DMA2D传输 - 输出通道 */
    ret = DMA2D_Start_Normal(ctx->out_ch, (void*)(uintptr_t)Jpeg0_ECSBuf(),
                             (void*)ctx->ecs_output_buffer,
                             ctx->ecs_len / sizeof(uint32_t));
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Start_Normal output failed: %d", __func__, __LINE__, ret);
        goto cleanup;
    }

#if USE_SOFTWARE_YUV_CONVERSION
    /* 软件模式：DMA2D直接从MCU缓冲区传输到JPEG编码器 */
    ret = DMA2D_Start_Normal(ctx->transfer_ch, ctx->mcu_buffer, 
                             (void*)(uintptr_t)Jpeg0_PixelBuf(), 
                             ctx->jpeg_cfg.pixel_size / sizeof(uint32_t));
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Start_Normal transfer failed: %d", __func__, __LINE__, ret);
        goto cleanup;
    }
#else
    /* 硬件模式：DMA2D从PiPo缓冲区传输到JPEG编码器 */
    ret = DMA2D_Start_PiPo(ctx->transfer_ch, ctx->pipo_buf[0], ctx->pipo_buf[1],
                           (void*)(uintptr_t)Jpeg0_PixelBuf(), NULL,
                           ctx->pipo_buf_len / sizeof(uint32_t),
                           ctx->pipo_buf_len / sizeof(uint32_t));
    if (ret != 0) {
        LOGE("[%s:%d] DMA2D_Start_PiPo transfer failed: %d", __func__, __LINE__, ret);
        goto cleanup;
    }
#endif

    /* 等待DMA完成 */
    if (xSemaphoreTake(jpeg_dma_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        LOGE("[%s:%d] DMA timeout", __func__, __LINE__);
        ret = -1;
        goto cleanup;
    }

    /* 获取ECS长度 - 使用chip.h中定义的IP_JPEG */
    uint32_t ecs_len = IP_JPEG->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN;

    /* 刷新缓存 */
    HAL_FlushDCache_by_Addr((uint32_t*)ctx->ecs_output_buffer, ecs_len);

    /* 构建完整的JPEG文件 */
    ret = (int32_t)jpeg_build_jpeg(ctx->ecs_output_buffer, ecs_len, out_buf, out_size, &ctx->jpeg_cfg);

cleanup:
    /* 每次编码后必须停止JPEG和DMA，硬件状态机需要复位 */
    Jpeg_Stop(jpeg_dev);
    DMA2D_Stop(ctx->in_ch);
    DMA2D_Stop(ctx->transfer_ch);
    DMA2D_Stop(ctx->out_ch);
    return ret;
}

/**
 * @brief 停止并释放JPEG编码器资源
 */
int32_t jpeg_encoder_stop(jpeg_enc_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        LOGE("[%s:%d] Context not initialized", __func__, __LINE__);
        return -1;
    }

    void *jpeg_dev = Jpeg0();

    /* 停止DMA2D通道 */
    DMA2D_Stop(ctx->in_ch);
    DMA2D_Stop(ctx->transfer_ch);
    DMA2D_Stop(ctx->out_ch);

    /* 停止JPEG */
    Jpeg_Stop(jpeg_dev);
    Jpeg_Uninitialize(jpeg_dev);

    /* 释放缓冲区 */
#if !USE_SOFTWARE_YUV_CONVERSION
    for (uint8_t i = 0; i < 2; i++) {
        if (ctx->pipo_buf_ori[i]) {
            free(ctx->pipo_buf_ori[i]);
            ctx->pipo_buf_ori[i] = NULL;
            ctx->pipo_buf[i] = NULL;
        }
    }
#endif

    if (ctx->ecs_output_buffer_ori) {
        free(ctx->ecs_output_buffer_ori);
        ctx->ecs_output_buffer_ori = NULL;
        ctx->ecs_output_buffer = NULL;
    }

#if USE_SOFTWARE_YUV_CONVERSION
    if (ctx->mcu_buffer_ori) {
        free(ctx->mcu_buffer_ori);
        ctx->mcu_buffer_ori = NULL;
        ctx->mcu_buffer = NULL;
    }
#else
    if (ctx->planar_buffer_ori) {
        free(ctx->planar_buffer_ori);
        ctx->planar_buffer_ori = NULL;
        ctx->planar_buffer = NULL;
    }
#endif

    ctx->initialized = 0;
    
    if (jpeg_dma_sem) {
        vSemaphoreDelete(jpeg_dma_sem);
        jpeg_dma_sem = NULL;
    }
    
    LOGD("[%s:%d] JPEG encoder stopped successfully", __func__, __LINE__);

    return 0;
}
