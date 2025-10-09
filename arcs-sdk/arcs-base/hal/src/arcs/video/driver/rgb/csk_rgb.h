/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CSK_RGB_H__
#define __CSK_RGB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Driver_RGB.h"
#include "Driver_GPDMA.h"


typedef struct
{
    volatile uint32_t sof;
    volatile uint32_t eof;
    volatile uint32_t fifo_read_empty;
    volatile uint32_t fifo_read_full;
    volatile uint32_t fifo_write_empty;
    volatile uint32_t fifo_write_full;
}RGB_irq_cnt_t;


void rgb_reset(void);
int32_t rgb_init(RGB_InitTypeDef *pcfg);
int32_t rgb_deinit(void);
int32_t rgb_start(void);
int32_t rgb_stop(void);
void rgb_reg_dump(void);
RGB_irq_cnt_t* rgb_irq_cnt_get(void);

int32_t rgb_gpdma_init(csk_gpdma_ch_t gpdma_ch);
int32_t rgb_gpdma_start(csk_gpdma_ch_t gpdma_ch, void* pbuf, uint32_t size_word);
int32_t rgb_gpdma_stop(csk_gpdma_ch_t gpdma_ch);
uint32_t rgb_gpdma_get_cnt(void);

#ifdef __cplusplus
}
#endif

#endif /* __CSK_DVP_H__ */
