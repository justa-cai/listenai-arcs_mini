/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "Driver_TRNG.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "nos_timer.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

bool set_trng_multigroup_value();
bool set_trng_polling_data_uniqueness();
bool set_trng_interrupt_value();

#ifdef __cplusplus
}
#endif
