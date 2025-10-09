/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_Driver_EFUSE.h"

DEFINE_FAKE_VALUE_FUNC(uint64_t, efuse_read_uuid);

void mock_driver_efuse_init(void)
{
    RESET_FAKE(efuse_read_uuid);
}

void mock_driver_efuse_reset(void)
{
    mock_driver_efuse_init();
}
