/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "Driver_CALENDAR.h"
#include "PowerManager.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool set_calendar_sec_value();
bool set_calendar_min_value();
bool set_calendar_hour_value();
bool set_calendar_alarm_value();

#ifdef __cplusplus
}
#endif