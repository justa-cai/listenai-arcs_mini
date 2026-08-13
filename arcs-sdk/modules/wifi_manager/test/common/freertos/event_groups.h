/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef void *EventGroupHandle_t;
typedef uint32_t EventBits_t;

/* Mock control for xEventGroupWaitBits return value.
 * Defined in mock_event_groups.c. */
extern bool mock_event_group_override;
extern EventBits_t mock_event_group_wait_return;

static inline void mock_event_group_reset(void)
{
    mock_event_group_override = false;
    mock_event_group_wait_return = 0;
}

static inline void mock_event_group_set_wait_return(EventBits_t bits)
{
    mock_event_group_override = true;
    mock_event_group_wait_return = bits;
}

static inline EventGroupHandle_t xEventGroupCreate(void)
{
    return (EventGroupHandle_t)1;
}

static inline EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits)
{
    (void)group;
    return bits;
}

static inline EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits)
{
    (void)group;
    return bits;
}

static inline EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bits_to_wait_for,
                                              int clear_on_exit, int wait_for_all_bits, uint32_t timeout)
{
    (void)group;
    (void)clear_on_exit;
    (void)wait_for_all_bits;
    (void)timeout;
    if (mock_event_group_override) {
        return mock_event_group_wait_return;
    }
    return bits_to_wait_for;
}

static inline void vEventGroupDelete(EventGroupHandle_t group)
{
    (void)group;
}

#ifdef __cplusplus
}
#endif
