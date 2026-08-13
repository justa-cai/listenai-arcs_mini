/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arcs_rust_dev.h"

const void *arcs_rust_dev_get_api(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return 0;
    }
    return dev->api;
}
