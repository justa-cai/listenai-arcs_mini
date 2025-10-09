#if 1

#ifndef CSK_DMA2D_WARP_H
#define CSK_DMA2D_WARP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int32_t rgb565_dma2d_init(void);

int32_t lv_gpu_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);
int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h);
int32_t lv_gpu_dma2d_fill_mask(void *buf, uint16_t buf_w, uint32_t color, void *mask, uint8_t opa, uint16_t fill_w, uint16_t fill_h);
int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CSK_DMA2D_WARP_H*/

#endif /*Disable/Enable content*/
