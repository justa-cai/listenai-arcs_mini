/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __GC0310_H
#define __GC0310_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "sensor.h"


/**
 * @brief Detect sensor pid
 *
 * @param slv_addr SCCB address
 * @param id Detection result
 * @return
 *     0:       Can't detect this sensor
 *     Nonzero: This sensor has been detected
 */
int gc0310_detect(int slv_addr, sensor_id_t *id);

/**
 * @brief initialize sensor function pointers
 *
 * @param sensor pointer of sensor
 * @return
 *      Always 0
 */
int gc0310_init(sensor_t *sensor);



#ifdef __cplusplus
}
#endif

#endif /* __GC0310_H */
