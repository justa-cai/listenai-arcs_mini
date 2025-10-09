/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CSK_LVGL_DMA2D_WRAP_H__
#define __CSK_LVGL_DMA2D_WRAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "chip.h"
#include "mmio.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DMA2D.h"
#include "Driver_JPEG.h"
#include "log_print.h"
#include "csk_timer.h"
#include "systick.h"

#include "csk_clk_reset.h"
#include "check.h"

#define RGB565_PIXEL_BYTE   2
#define RGB888_PIXEL_BYTE   3

typedef enum _csk_dma2d_format {
    DMA2D_FORMAT_RGB565 = 0,
    DMA2D_FORMAT_RGB888,
    DMA2D_FORMAT_BUTT,
} csk_dma2d_format_t;

typedef enum {
    LVGL_DMA2D_COPY = 0,
    LVGL_DMA2D_FILL,
    LVGL_DMA2D_FILL_MASK,
    LVGL_DMA2D_BLEND,
    LVGL_DMA2D_BLEND_MASK,
    LVGL_DMA2D_BUTT,
} lvgl_dma2d_mode_e;

typedef struct
{
    lvgl_dma2d_mode_e mode;
    void *buf;
    void *map;
    void *mask;
    uint16_t buf_w;
    uint16_t map_w;
    uint16_t mask_w;
    uint16_t copy_w;
    uint16_t copy_h;
    uint32_t color;     // RGB888
    uint8_t opa;
}lvgl_dma2d_cfg_t;

int32_t rgb565_dma2d_init(void);
int32_t rgb565_dma2d_config(lvgl_dma2d_cfg_t *pcfg);

int32_t lv_gpu_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);
int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h);
int32_t lv_gpu_dma2d_fill_mask(void *buf, uint16_t buf_w, uint32_t color, void *mask, uint16_t mask_w, uint8_t opa, uint16_t fill_w, uint16_t fill_h);
int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);
int32_t lv_gpu_dma2d_blend_mask(void *buf, uint16_t buf_w, void *map, uint16_t map_w, void *mask, uint16_t mask_w, uint8_t opa, uint16_t copy_w, uint16_t copy_h);

void lvgl_gpdma_reg_dump(void);
void lvgl_blender_reg_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* __CSK_LVGL_DMA2D_WRAP_H__ */
