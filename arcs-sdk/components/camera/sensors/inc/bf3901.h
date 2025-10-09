#ifndef __DRV_BF3901_H__
#define __DRV_BF3901_H__

#include <stdint.h>
#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAM_FRAME_WIDTH			96
#define CAM_FRAME_HEIGHT		240

int bf3901_detect(int slv_addr, sensor_id_t *id);
int bf3901_init(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
#endif
