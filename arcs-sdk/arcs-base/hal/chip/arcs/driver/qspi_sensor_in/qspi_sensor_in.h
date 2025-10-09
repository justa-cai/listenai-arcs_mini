/*
 * qspi_sensor_in.h
 *
 *  Created on: Oct 09, 2023
 *
 */

#ifndef __DRIVER_QSPI_SENSOR_IN_INTERNAL_H
#define __DRIVER_QSPI_SENSOR_IN_INTERNAL_H

#include "arcs_ap.h"
#include "Driver_QSPI_SENSOR_IN.h"

/*----- QSPI Transfer mode -----*/
#define CSK_QSPI_SENSOR_IN_TRANSMODE_SINGLE       (0x0)
#define CSK_QSPI_SENSOR_IN_TRANSMODE_DUAL         (0x1)
#define CSK_QSPI_SENSOR_IN_TRANSMODE_QUAD         (0x2)

#define CSK_QSPI_SENSOR_IN_BUF      (QSPI_SENSOR_IN_BASE + 0x2C)

/**
  * @brief  QSPI_SENSOR_IN handle Structure definition
  */
typedef struct __QSPI_SENSOR_IN_DEV
{
    QSPI_SENSOR_IN_RegDef             *Instance;           /*!< QSPI_SENSOR_IN Register base address  */

    QSPI_SENSOR_IN_InitTypeDef        Init;                /*!< QSPI_SENSOR_IN parameters             */

    QSPI_SENSOR_IN_SignalEvent_t      cb_event;            /*!< QSPI_SENSOR_IN callback               */
}QSPI_SENSOR_IN_DEV;


#endif /* __DRIVER_QSPI_SENSOR_IN_INTERNAL_H */
