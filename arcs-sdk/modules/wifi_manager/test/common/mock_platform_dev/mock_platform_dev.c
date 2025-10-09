/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_platform_dev.h"

DEFINE_FAKE_VALUE_FUNC(void *, platform_get_flash_dev);


void mock_platform_dev_init(void)
{
    RESET_FAKE(platform_get_flash_dev);
}

void mock_platform_dev_reset(void)
{
    mock_platform_dev_init();
}

