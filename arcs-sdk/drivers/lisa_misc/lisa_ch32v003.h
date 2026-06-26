/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_ch32v003.h
 * @brief LISA CH32V003 external MCU bridge driver
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LISA_CH32V003_DEVICE_NAME "ch32v003"

int lisa_ch32v003_read_reg(lisa_device_t *dev, uint8_t reg, uint8_t *value);
int lisa_ch32v003_write_reg(lisa_device_t *dev, uint8_t reg, uint8_t value);
uint8_t lisa_ch32v003_get_version(void);

#ifdef __cplusplus
}
#endif
