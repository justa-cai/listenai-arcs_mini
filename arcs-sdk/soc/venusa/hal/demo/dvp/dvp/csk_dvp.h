#ifndef __CSK_DVP_H
#define __CSK_DVP_H

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include "queue.h"

int32_t dvp_init(SemaphoreHandle_t DoneSemaphore, SemaphoreHandle_t ErrSemaphore);
void dvp_buf_add(void *buf_addr);
void dvp_buf_get(void **buf_addr);
void dvp_buf_num_get(uint8_t *done_num, uint8_t *used_num, uint8_t *unused_num);


#ifdef __cplusplus
}
#endif

#endif /* __CSK_DVP_H */

