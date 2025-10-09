/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_sysheap.h"

#include <stdlib.h>

DEFINE_FAKE_VALUE_FUNC(void *, exram_malloc, int, size_t);
DEFINE_FAKE_VOID_FUNC(exram_free, void *);

static void *mock_exram_malloc(int heap_id, size_t size)
{
    return malloc(size);
}

static void mock_exram_free(void *ptr)
{
    free(ptr);
}

void mock_sysheap_init(void)
{
    RESET_FAKE(exram_malloc);
    RESET_FAKE(exram_free);

    exram_malloc_fake.custom_fake = mock_exram_malloc;
    exram_free_fake.custom_fake = mock_exram_free;
}

void mock_sysheap_reset(void)
{
    mock_sysheap_init();
}
