/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CSK_DVP_H__
#define __CSK_DVP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "Driver_DVP.h"
#include "Driver_GPDMA.h"

//typedef enum {
//    FALSE = 0,
//    TRUE  = 1,
//} bool_t;

int32_t dvp_init(DVP_InitTypeDef *dvp_cfg, uint32_t clk_hz);
int32_t dvp_deinit(void);
int32_t dvp_start(void);
int32_t dvp_stop(void);
uint32_t dvp_frame_get(void);
void dvp_reg_dump(void);
void dvp_reset(void);

int32_t dvp_gpdma_init(csk_gpdma_ch_t gpdma_ch, bool auto_en);
int32_t dvp_gpdma_start(csk_gpdma_ch_t gpdma_ch, void *pdst, uint32_t size_word, bool auto_en);
uint32_t dvp_gpdma_frame_get(void);
void gpdma_reg_dump(uint8_t dma_ch);


#ifdef __cplusplus
}
#endif

#endif /* __CSK_DVP_H__ */
