/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * TC6036 camera sensor driver header.
 */
#ifndef _TC6036_H_
#define _TC6036_H_

#include "sensor.h"

/**
 * @brief Detect TC6036 sensor
 *
 * @param slv_addr SCCB address
 * @param id Detection result
 * @return
 *     0:       Can't detect this sensor
 *     Nonzero: This sensor has been detected (returns PID)
 */
int tc6036_detect(int slv_addr, sensor_id_t *id);

/**
 * @brief Initialize TC6036 sensor function pointers
 *
 * @param sensor pointer of sensor
 * @return
 *      Always 0
 */
int tc6036_init(sensor_t *sensor);

#endif // _TC6036_H_
