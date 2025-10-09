/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "nvs.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(void *, platform_get_flash_dev);

void mock_platform_dev_init(void);
void mock_platform_dev_reset(void);


#ifdef __cplusplus
}
#endif

