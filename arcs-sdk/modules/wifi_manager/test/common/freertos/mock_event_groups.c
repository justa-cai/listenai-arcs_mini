/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "event_groups.h"

/* Shared mock state — defined once, declared extern in event_groups.h */
bool mock_event_group_override = false;
EventBits_t mock_event_group_wait_return = 0;
