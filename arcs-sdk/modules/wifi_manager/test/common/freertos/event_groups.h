/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void *EventGroupHandle_t;
typedef uint32_t EventBits_t;

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
    (void)bits_to_wait_for;
    (void)clear_on_exit;
    (void)wait_for_all_bits;
    (void)timeout;
    return bits_to_wait_for;
}

static inline void vEventGroupDelete(EventGroupHandle_t group)
{
    (void)group;
}

#ifdef __cplusplus
}
#endif
