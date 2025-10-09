/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "task.h"

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(unsigned int, xTaskGetTickCount);

void mock_task_init(void);
void mock_task_reset(void);

#ifdef __cplusplus
}
#endif