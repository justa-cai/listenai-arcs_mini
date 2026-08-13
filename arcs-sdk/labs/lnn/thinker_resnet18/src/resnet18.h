/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

int resnet18_init(void);

int resnet18_process(const void *data, uint32_t width, uint32_t height, bool rotated, const char **result_label,
                     int8_t *result_score);

int resnet18_deinit(void);
