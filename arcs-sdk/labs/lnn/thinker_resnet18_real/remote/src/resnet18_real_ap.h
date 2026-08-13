/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

int resnet18_real_init(void);
int resnet18_real_run_tensor(const int8_t *input_data, const char **result_label, int8_t *result_score,
                             int32_t *result_index);
int resnet18_real_deinit(void);
