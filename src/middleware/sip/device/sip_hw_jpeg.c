/*
 * sip_hw_jpeg.c - JPEG 硬件编码（常驻式，DMA2D 路线）
 *
 * 路线取舍（本文件为何存在）：
 *  - lisa_jpeg 组件 jpeg_encoder()/jpeg_encoder_ext()：每次调用都
 *    jpeg_init + malloc pipo/ecs（~150KB）+ 轮询 —— GPDMA 通道全占的
 *    机器上编码 DMA 永远等不到，重复 malloc 还把 6MB 池吃到 OOM 断言。
 *  - 骨架取自 micro-rtsp-c 的常驻实现（一次 Jpeg_Initialize + DMA2D
 *    配置，每帧只 Start/DMA/信号量）：编码数据通路全走 DMA2D
 *    （transfer=ch7、out=ch8），与解码的 GPDMA ch0/ch5 零冲突。
 *    前提：关掉 LVGL GPU（blender 占用 DMA2D ch6~9，见 prj.conf 注释）。
 *
 * 数据流：RGB565 → 软件 YUYV → MCU 块格式（YUV422）→ DMA2D 喂 Jpeg0
 * → ECS 熵数据 → 拼头输出完整 JFIF。
 *
 * Jpeg0 编/解码共用外设：调用方持 s_jpeg_lock 串行化；解码帧的
 * Jpeg_Initialize(DECODE) 会覆盖编码配置，因此 encode 每帧重新
 * Jpeg_Initialize(ENCODE) —— 只写 Jpeg0 寄存器，DMA2D 配置不动。
 */
#define TAG "sip_hw_jpeg"

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "Driver_DMA2D.h"
#include "Driver_JPEG.h"
#include "arcs_ap.h" /* IP_JPEG：ECS 结果长度寄存器 */
#include "cache.h"

#include "lisa_log.h"
#include "sysheap.h"

#include "sip_hw_jpeg.h"

/* golden 表在 sip_jpeg_enc_tables.c（数据取自 micro-rtsp-c 示例）。 */
extern const uint32_t jpeg_enc_htable_golden[384];
extern const uint32_t jpeg_enc_qtable_golden[128];

#define CSK_JPEG_BLOCK_BASIC_PIXEL_NUM 8
#define CSK_JPEG_GPDMA_ADDR_ALIGNMENT  32
#define CSK_JPEG_BLOCK_ALIGN(a, b)     ((((a) + (b) - 1) / (b)) * (b))

#define HWJ_TRANSFER_CH dma_2d_ch7 /* m2p：MCU 缓冲 -> Jpeg0 像素口 */
#define HWJ_OUT_CH      dma_2d_ch8 /* p2m：Jpeg0 ECS -> 输出缓冲 */

static SemaphoreHandle_t s_done_sem;
static Jpeg_InitTypeDef s_cfg;
static uint16_t s_w, s_h;
static int s_ready;
static volatile int s_decode_used; /* 下行解码动过 Jpeg0：编码前需重配 */

static uint8_t *s_yuyv;        /* w*h*2：RGB565 -> YUYV 中间帧 */
static uint8_t *s_mcu;         /* MCU 块格式（对齐后 pixel_size） */
static uint8_t *s_mcu_raw;
static uint8_t *s_ecs_raw;     /* ECS 输出（对齐后 pixel_size） */
static uint8_t *s_ecs;

static void hwj_dma_done(uint32_t event, void *workspace)
{
    (void)event;
    (void)workspace;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(s_done_sem, &wake);
    portYIELD_FROM_ISR(wake);
}

/* BT.601 整数近似；每两像素一对（YUYV 结构），色度取两像素均值。
 * 注意 RGB565 分量是 5/6bit，必须先升到 8bit 再套标准系数 —— 曾
 * 直接乘 5bit 值，亮度上限只有 199、色度量级全错，画面表现为绿色
 * 调竖条纹（把升位漏了当"硬件花屏"排查了一圈）。 */
static void rgb565_to_yuyv(const uint8_t *rgb565, uint8_t *yuyv,
                           uint32_t w, uint32_t h)
{
    const uint16_t *src = (const uint16_t *)rgb565;
    for (uint32_t y = 0; y < h; ++y) {
        uint8_t *d = yuyv + y * w * 2u;
        for (uint32_t x = 0; x + 1 < w; x += 2) {
            uint16_t p0 = src[y * w + x];
            uint16_t p1 = src[y * w + x + 1];
            int r0 = ((p0 >> 11) & 0x1F) * 255 / 31;
            int g0 = ((p0 >> 5) & 0x3F) * 255 / 63;
            int b0 = (p0 & 0x1F) * 255 / 31;
            int r1 = ((p1 >> 11) & 0x1F) * 255 / 31;
            int g1 = ((p1 >> 5) & 0x3F) * 255 / 63;
            int b1 = (p1 & 0x1F) * 255 / 31;
            int y0 = (77 * r0 + 150 * g0 + 29 * b0) >> 8;
            int y1 = (77 * r1 + 150 * g1 + 29 * b1) >> 8;
            int rr = (r0 + r1) / 2, gg = (g0 + g1) / 2, bb = (b0 + b1) / 2;
            int u = 128 + ((-43 * rr - 85 * gg + 128 * bb) >> 8);
            int v = 128 + ((128 * rr - 107 * gg - 21 * bb) >> 8);
            d[0] = (uint8_t)y0;
            d[1] = (uint8_t)u;
            d[2] = (uint8_t)y1;
            d[3] = (uint8_t)v;
            d += 4;
        }
        /* 奇数宽度落单的最后像素：复制前一对（相机 240/320 均为偶数，
         * 此行仅防御）。 */
        if (w & 1u) {
            d[0] = d[-2];
            d[1] = d[-1];
        }
    }
}

/*
 * YUYV -> JPEG MCU 块格式（YUV422：每 16x8 块 = Y0(8x8)+Y1(8x8)+Cb+Cr）。
 * 与 micro-rtsp-c 的 yuyv_to_jpeg_mcu 同构。色度按对取样（YUYV 每 2
 * 像素共享一对 U/V），右/下边界复制边缘像素补齐。
 */
static void yuyv_to_mcu(const uint8_t *yuyv, uint8_t *mcu)
{
    const uint32_t wa = s_cfg.img_width_align;
    const uint32_t ha = s_cfg.img_height_align;
    const uint32_t last_pair = ((s_w >= 2) ? s_w : 2) - 2; /* 最后完整 YUYV 对 */
    uint32_t idx = 0;
    for (uint32_t h = 0; h < ha; h += 8) {
        for (uint32_t w = 0; w < wa; w += 16) {
            uint8_t yb[8][16], cb[8][8], cr[8][8];
            for (int i = 0; i < 8; ++i) {
                uint32_t py = h + (uint32_t)i;
                if (py >= s_h) {
                    py = s_h - 1;
                }
                for (int j = 0; j < 16; j += 2) {
                    uint32_t px = w + (uint32_t)j;
                    if (px + 1 >= s_w) {
                        px = last_pair; /* 越界对复制最后一对 */
                    }
                    const uint8_t *p = yuyv + (py * s_w + px) * 2u;
                    yb[i][j] = p[0];
                    yb[i][j + 1] = p[2];
                    cb[i][j / 2] = p[1];
                    cr[i][j / 2] = p[3];
                }
            }
            for (int i = 0; i < 8; ++i) {
                memcpy(mcu + idx, yb[i], 8);
                idx += 8;
            }
            for (int i = 0; i < 8; ++i) {
                memcpy(mcu + idx, yb[i] + 8, 8);
                idx += 8;
            }
            memcpy(mcu + idx, cb, 64);
            idx += 64;
            memcpy(mcu + idx, cr, 64);
            idx += 64;
        }
    }
}

/* ECS 熵数据 -> 完整 JFIF（YUV422 4:2:2 头）。 */
static void build_jfif(const uint8_t *ecs, uint32_t ecs_len,
                       uint8_t *out, uint32_t *out_len)
{
    static const uint8_t app0[18] = {0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49,
                                     0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x78,
                                     0x00, 0x78, 0x00, 0x00};
    static const uint8_t dqt0[69] = {
        0xff, 0xdb, 0x00, 0x43, 0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08,
        0x07, 0x07, 0x07, 0x09, 0x09, 0x08, 0x0a, 0x0c, 0x14, 0x0d, 0x0c, 0x0b,
        0x0b, 0x0c, 0x19, 0x12, 0x13, 0x0f, 0x14, 0x1d, 0x1a, 0x1f, 0x1e, 0x1d,
        0x1a, 0x1c, 0x1c, 0x20, 0x24, 0x2e, 0x27, 0x20, 0x22, 0x2c, 0x23, 0x1c,
        0x1c, 0x28, 0x37, 0x29, 0x2c, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1f, 0x27,
        0x39, 0x3d, 0x38, 0x32, 0x3c, 0x2e, 0x33, 0x34, 0x32};
    static const uint8_t dqt1[69] = {
        0xff, 0xdb, 0x00, 0x43, 0x01, 0x09, 0x09, 0x09, 0x0c, 0x0b, 0x0c, 0x18,
        0x0d, 0x0d, 0x18, 0x32, 0x21, 0x1c, 0x21, 0x32, 0x32, 0x32, 0x32, 0x32,
        0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
        0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
        0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
        0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32};
    static const uint8_t dht_dc0[33] = {
        0xff, 0xc4, 0x00, 0x1f, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
    static const uint8_t dht_dc1[33] = {
        0xff, 0xc4, 0x00, 0x1f, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
    static const uint8_t sos[14] = {0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00,
                                    0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00};

    uint32_t i = 0;
    out[i++] = 0xff;
    out[i++] = 0xd8; /* SOI */
    memcpy(out + i, app0, sizeof(app0));
    i += sizeof(app0);
    memcpy(out + i, dqt0, sizeof(dqt0));
    i += sizeof(dqt0);
    memcpy(out + i, dqt1, sizeof(dqt1));
    i += sizeof(dqt1);

    uint8_t sof[19] = {0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x00, 0x00, 0x00,
                       0x03, 0x01, 0x21, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01};
    sof[5] = (uint8_t)(s_h >> 8);
    sof[6] = (uint8_t)s_h;
    sof[7] = (uint8_t)(s_w >> 8);
    sof[8] = (uint8_t)s_w;
    memcpy(out + i, sof, sizeof(sof));
    i += sizeof(sof);

    memcpy(out + i, dht_dc0, sizeof(dht_dc0));
    i += sizeof(dht_dc0);
    memcpy(out + i, dht_dc1, sizeof(dht_dc1));
    i += sizeof(dht_dc1);
    /* AC Huffman：与 micro-rtsp 的 golden 头一致（标准 Annex K 表）。 */
    extern const uint8_t sip_jpeg_dht_ac0[183];
    extern const uint8_t sip_jpeg_dht_ac1[183];
    memcpy(out + i, sip_jpeg_dht_ac0, 183);
    i += 183;
    memcpy(out + i, sip_jpeg_dht_ac1, 183);
    i += 183;

    memcpy(out + i, sos, sizeof(sos));
    i += sizeof(sos);
    memcpy(out + i, ecs, ecs_len);
    i += ecs_len;
    out[i++] = 0xff;
    out[i++] = 0xd9; /* EOI */
    *out_len = i;
}

int sip_hw_jpeg_init(uint16_t width, uint16_t height)
{
    if (s_ready) {
        return 0;
    }
    if (!s_done_sem) {
        s_done_sem = xSemaphoreCreateBinary();
        if (!s_done_sem) {
            return -1;
        }
    }

    s_w = width;
    s_h = height;
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.mode = JPEG_MODE_ENCODE;
    s_cfg.format_in = JPEG_DECODE_IN_FORMAT_YUV422;
    s_cfg.sampling_h = 2 * CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
    s_cfg.sampling_v = CSK_JPEG_BLOCK_BASIC_PIXEL_NUM;
    /* 表路由（曾漏配全 0：所有分量都走量化/霍夫曼表 0，而 JFIF 头里
     * 声明色度用表 1 —— 编码实际用的表与头声明不符，解码端按头解
     * 出色度错乱 = 绿色条纹花屏）。 */
    s_cfg.qt_index[0] = 0x00;
    s_cfg.qt_index[1] = 0x01;
    s_cfg.qt_index[2] = 0x01;
    s_cfg.qt_index[3] = 0x00;
    s_cfg.ht_index[0] = 0x00;
    s_cfg.ht_index[1] = 0x11;
    s_cfg.ht_index[2] = 0x11;
    s_cfg.ht_index[3] = 0x00;
    s_cfg.img_width = width;
    s_cfg.img_height = height;
    s_cfg.img_width_align = CSK_JPEG_BLOCK_ALIGN(width, s_cfg.sampling_h);
    s_cfg.img_height_align = CSK_JPEG_BLOCK_ALIGN(height, s_cfg.sampling_v);
    /* YUV422：w*h*2 字节（对齐尺寸）。 */
    s_cfg.pixel_size = s_cfg.img_width_align * s_cfg.img_height_align * 2u;
    s_cfg.src_size = s_cfg.pixel_size;

    s_yuyv = psram_malloc((uint32_t)width * height * 2u);
    s_mcu_raw = psram_malloc(s_cfg.pixel_size + CSK_JPEG_GPDMA_ADDR_ALIGNMENT);
    s_ecs_raw = psram_malloc(s_cfg.pixel_size + CSK_JPEG_GPDMA_ADDR_ALIGNMENT);
    if (!s_yuyv || !s_mcu_raw || !s_ecs_raw) {
        LISA_LOGE(TAG, "buffer alloc failed");
        sip_hw_jpeg_stop();
        return -1;
    }
    s_mcu = (uint8_t *)(((uintptr_t)s_mcu_raw + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1) &
                        ~(uintptr_t)(CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1));
    s_ecs = (uint8_t *)(((uintptr_t)s_ecs_raw + CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1) &
                        ~(uintptr_t)(CSK_JPEG_GPDMA_ADDR_ALIGNMENT - 1));

    if (DMA2D_Initialize() != 0) {
        LISA_LOGE(TAG, "DMA2D_Initialize failed");
        sip_hw_jpeg_stop();
        return -1;
    }

    /* transfer 通道：MCU 缓冲 -> Jpeg0 像素口（m2p）。软件转换模式下
     * 参考实现（micro-rtsp）不配外设流控 —— 曾带上 flow_ctrl，首帧
     * 正常后续帧花屏。 */
    csk_dma2d_init_t tx = {
        .dma_ch = HWJ_TRANSFER_CH,
        .burst_len = dma2d_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = dma2d_sample_unit_word,
        .handshake = jpg_p_hs_num7,
    };
    if (DMA2D_Config(&tx, NULL, NULL) != 0) {
        LISA_LOGE(TAG, "DMA2D_Config transfer failed");
        sip_hw_jpeg_stop();
        return -1;
    }

    /* out 通道：Jpeg0 ECS -> 输出缓冲（p2m，完成回调给信号量）。 */
    csk_dma2d_init_t out = {
        .dma_ch = HWJ_OUT_CH,
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
    if (DMA2D_Config(&out, hwj_dma_done, NULL) != 0) {
        LISA_LOGE(TAG, "DMA2D_Config out failed");
        sip_hw_jpeg_stop();
        return -1;
    }

    s_ready = 1;
    LISA_LOGI(TAG, "hw jpeg ready: %ux%u (align %ux%u, pixel_size=%u)",
              width, height, s_cfg.img_width_align, s_cfg.img_height_align,
              (unsigned)s_cfg.pixel_size);
    return 0;
}

int sip_hw_jpeg_encode(const uint8_t *rgb565, uint8_t *jpeg, uint32_t *jpeg_len)
{
    if (!s_ready || !rgb565 || !jpeg || !jpeg_len) {
        return -1;
    }

    /* 解码帧会把 Jpeg0 切到 DECODE 模式：仅在解码用过之后重新
     * Jpeg_Initialize + 发表（参考实现从不每帧重配 —— 每帧重配时
     * 首帧正常、后续帧花屏，疑似 Initialize 与 DMA2D 常驻配置的
     * 交互问题）。 */
    void *dev = Jpeg0();
    if (s_decode_used) {
        s_decode_used = 0;
        if (Jpeg_Initialize(dev, NULL, &s_cfg) != 0) {
            return -1;
        }
        volatile uint32_t *dst = (volatile uint32_t *)Jpeg0_EncodeHuffBuf();
        for (uint32_t i = 0; i < 384; ++i) {
            dst[i] = jpeg_enc_htable_golden[i];
        }
        dst = (volatile uint32_t *)Jpeg0_EncodeQuanBuf();
        for (uint32_t i = 0; i < 128; ++i) {
            dst[i] = jpeg_enc_qtable_golden[i];
        }
    }
    if (Jpeg_Start(dev) != 0) {
        return -1;
    }

    rgb565_to_yuyv(rgb565, s_yuyv, s_w, s_h);
    yuyv_to_mcu(s_yuyv, s_mcu);
    HAL_FlushDCache_by_Addr((uint32_t *)s_mcu, s_cfg.pixel_size);

    xSemaphoreTake(s_done_sem, 0); /* 清残留 */

    if (DMA2D_Start_Normal(HWJ_OUT_CH, (void *)(uintptr_t)Jpeg0_ECSBuf(),
                           s_ecs, s_cfg.pixel_size / sizeof(uint32_t)) != 0) {
        goto fail;
    }
    if (DMA2D_Start_Normal(HWJ_TRANSFER_CH, s_mcu,
                           (void *)(uintptr_t)Jpeg0_PixelBuf(),
                           s_cfg.pixel_size / sizeof(uint32_t)) != 0) {
        goto fail;
    }

    if (xSemaphoreTake(s_done_sem, pdMS_TO_TICKS(500)) != pdTRUE) {
        LISA_LOGW(TAG, "encode dma timeout");
        goto fail;
    }

    uint32_t ecs_len = IP_JPEG->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN;
    if (ecs_len == 0 || ecs_len > s_cfg.pixel_size) {
        goto fail;
    }
    /* ECS 是 DMA 刚写入内存的：必须 Invalidate 丢弃 CPU cache 里的
     * 上一帧旧行再读。曾用 Flush（clean）——旧行是干净的不会被写回
     * 也不会失效，CPU 读 ECS 永远命中旧数据：第一帧 cache 冷正常、
     * 之后全部条纹花屏。 */
    HAL_InvalidateDCache_by_Addr((uint32_t *)s_ecs, ecs_len);
    build_jfif(s_ecs, ecs_len, jpeg, jpeg_len);

    Jpeg_Stop(dev);
    DMA2D_Stop(HWJ_TRANSFER_CH);
    DMA2D_Stop(HWJ_OUT_CH);
    return 0;

fail:
    Jpeg_Stop(dev);
    DMA2D_Stop(HWJ_TRANSFER_CH);
    DMA2D_Stop(HWJ_OUT_CH);
    return -1;
}

void sip_hw_jpeg_mark_decode(void)
{
    s_decode_used = 1;
}

void sip_hw_jpeg_stop(void)
{
    s_decode_used = 0;
    if (s_ready) {
        DMA2D_Stop(HWJ_TRANSFER_CH);
        DMA2D_Stop(HWJ_OUT_CH);
        Jpeg_Stop(Jpeg0());
        Jpeg_Uninitialize(Jpeg0());
    }
    if (s_yuyv) {
        psram_free(s_yuyv);
        s_yuyv = NULL;
    }
    if (s_mcu_raw) {
        psram_free(s_mcu_raw);
        s_mcu_raw = NULL;
        s_mcu = NULL;
    }
    if (s_ecs_raw) {
        psram_free(s_ecs_raw);
        s_ecs_raw = NULL;
        s_ecs = NULL;
    }
    s_ready = 0;
}
