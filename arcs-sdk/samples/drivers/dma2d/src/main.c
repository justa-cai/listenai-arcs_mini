#define LOG_TAG "Dma2d Sample"
#include "chip.h"
#include "Driver_DMA2D.h"
#include "nmsis_core.h"
#include "lisa_log.h"
#include <string.h>
#include <stdlib.h>
// 定义图像的宽高（64x64像素）
#define DMA2D_IMAGE_WIDTH_SIZE  64
#define DMA2D_IMAGE_HEIGHT_SIZE 64
// DMA2D传输完成事件标志
static volatile uint32_t dma2d_image_event = 0;

__USED static uint32_t golden_yuv444_format_image[DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4];

__USED static uint32_t golden_xrgb_format_image[DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4];

#define IMAGE_YUV444_COMPONENT(y, u, v) (((0x00) << 24) | (v << 16) | (u << 8) | y)

#define clip(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define YUV444ToRGB(Y, Cb, Cr)                                                                                         \
    ((clip((256 * Y + 359 * (Cr - 128)) >> 8)) | (clip((256 * Y - 183 * (Cr - 128) - 88 * (Cb - 128)) >> 8) << 8) |    \
     (clip((256 * Y + 444 * (Cb - 128)) >> 8) << 16))

#define DEF_Y 0xAA
#define DEF_U 0x88
#define DEF_V 0x33
/* DMA2D传输完成回调函数 */
static void dma2d_image_callback(uint32_t event, void *workspace)
{
    dma2d_image_event = event;
}

void DMA2D_Init(void)
{
    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .sample_unit = dma2d_sample_unit_word,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv444,
        .img_height = DMA2D_IMAGE_HEIGHT_SIZE,
        .img_width = DMA2D_IMAGE_WIDTH_SIZE,

        .img_output_fromat_transfer = csk_image_format_transfer_yuv444_xrgb,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
}

static void DMA2D_Image_YUV444_to_RGB888(void)
{
    LOGI("[DMA2D][IMAGE] YUV444 Transfer to RGB888 with DMA2D function, the image size[%d, %d], %s",
         DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

    dma2d_image_event = 0;

    if (golden_yuv444_format_image == NULL || golden_xrgb_format_image == NULL) {
        LOGI("Failed to allocate memory");
    }

    memset(golden_yuv444_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
    memset(golden_xrgb_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    // 生成YUV444格式图像
    for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
        golden_yuv444_format_image[i] = IMAGE_YUV444_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

    DMA2D_Start_Normal(dma_2d_ch9, golden_yuv444_format_image, golden_xrgb_format_image,
                       DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);

    while (!dma2d_image_event);

    int ret = 0;
    volatile uint32_t rgb = YUV444ToRGB(DEF_Y, DEF_U, DEF_V);
    LOGI("rgb_value = 0x%08x", rgb);

    volatile uint32_t brgb = (rgb << 24) | rgb;
    volatile uint32_t gbrg = (rgb << 16) | (rgb >> 8);
    volatile uint32_t rgbr = (rgb << 8) | (rgb >> 16);

    for (uint32_t i = 0; i < (DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4); i++) {
        // brgb
        if (i % 3 == 0) {
            if (golden_xrgb_format_image[i] != brgb) {
                ret = -1;
            }
        }
        // gbrg
        else if (i % 3 == 1) {
            if (golden_xrgb_format_image[i] != gbrg) {
                ret = -2;
            }
        }
        // rgbr
        else if (i % 3 == 2) {
            if (golden_xrgb_format_image[i] != rgbr) {
                ret = -3;
            }
        }

        if (ret == -1) {
            LOGI("[DMA2D][FORMAT] Index->%d  YUV444->XRGB compare error: 0x%x != 0x%x\n", i, brgb,
                 golden_xrgb_format_image[i]);
            return;
        } else if (ret == -2) {
            LOGI("[DMA2D][FORMAT] Index->%d  YUV444->XRGB compare error: 0x%x != 0x%x\n", i, gbrg,
                 golden_xrgb_format_image[i]);
            return;
        } else if (ret == -3) {
            LOGI("[DMA2D][FORMAT] Index->%d  YUV444->XRGB compare error: 0x%x != 0x%x\n", i, rgbr,
                 golden_xrgb_format_image[i]);
            return;
        }
        ret = 0;
    }
    LOGI("YUV444 Transfer to RGB888 with DMA2D function Success!\r\n");
}

int main(int argc, char **argv)
{
    LOGI("Hello, world! Dma2d");

    enable_GINT();

    DMA2D_Init();

    DMA2D_Image_YUV444_to_RGB888();
}
