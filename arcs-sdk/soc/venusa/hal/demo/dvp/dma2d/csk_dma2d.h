#ifndef __CSK_DMA2D_H
#define __CSK_DMA2D_H

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include "queue.h"

typedef struct _csk_dma2d_sacler_crop {
    uint16_t img_width_in;
    uint16_t img_height_in;
    uint16_t img_width_out;
    uint16_t img_height_out;
    uint16_t crop_x;
    uint16_t crop_y;
    uint8_t *img_buf_in;
    uint8_t *img_buf_out;
} csk_dma2d_scaler_crop_t;


int32_t dma2d_scaler_start(SemaphoreHandle_t Semaphore, csk_dma2d_scaler_crop_t *cfg);
int32_t dma2d_crop_start(SemaphoreHandle_t Semaphore, csk_dma2d_scaler_crop_t *cfg);


#ifdef __cplusplus
}
#endif

#endif /* __CSK_DMA2D_H */

