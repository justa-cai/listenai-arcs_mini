/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CSK_QSPI_IN_H__
#define __CSK_QSPI_IN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Driver_QSPI_SENSOR_IN.h"
#include "Driver_GPDMA.h"


void qspi_in_reset(void);
int32_t qspi_in_init(QSPI_SENSOR_IN_InitTypeDef *pcfg, uint32_t clk_hz);
int32_t qspi_in_start(void);
int32_t qspi_in_stop(void);
uint32_t qspi_in_sof_cnt_get(void);
uint32_t qspi_in_eof_cnt_get(void);
void qspi_in_msg_dump(void);
void qspi_in_reg_dump(void);

int32_t qspi_in_gpdma_init(csk_gpdma_ch_t gpdma_ch);
int32_t qspi_in_gpdma_start(csk_gpdma_ch_t gpdma_ch, void *pbuf, uint32_t size_word);
int32_t qspi_in_gpdma_stop(csk_gpdma_ch_t gpdma_ch);
uint32_t qspi_in_gpdma_get_cnt(void);


#ifdef __cplusplus
}
#endif

#endif /* __CSK_QSPI_IN_H__ */
