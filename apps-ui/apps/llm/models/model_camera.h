/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MODEL_CAMERA_H__
#define __MODEL_CAMERA_H__

#include <stdint.h>
#include <stdbool.h>

int model_camera_init(void);
int model_camera_capture(uint8_t *in, uint32_t len);
int model_camera_get_framesize(uint16_t *width, uint16_t *height);
bool model_camera_is_running(void);
bool model_camera_is_inited(void);

#endif // __MODEL_CAMERA_H__
