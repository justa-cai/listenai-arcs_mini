/**
 * @file lv_gpu_csk_dma2d.h
 *
 */

#ifndef LV_GPU_CSK_DMA2D_H
#define LV_GPU_CSK_DMA2D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../src/misc/lv_color.h"
#include "../src/hal/lv_hal_disp.h"
#include "../src/draw/sw/lv_draw_sw.h"

#if CONFIG_LV_USE_GPU_CSK_DMA2D

/*********************
 *      INCLUDES
 *********************/


/*********************
 *      DEFINES
 *********************/


/**********************
 *      TYPEDEFS
 **********************/
typedef lv_draw_sw_ctx_t lv_draw_csk_dma2d_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void lv_draw_csk_dma2d_init(void);
void lv_draw_csk_dma2d_ctx_init(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);
void lv_draw_csk_dma2d_ctx_deinit(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

/**********************
 *      MACROS
 **********************/

#endif  /*CONFIG_LV_USE_GPU_CSK_DMA2D*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GPU_STM32_DMA2D_H*/
