/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sysheap.h"
#include "mac_manager.h"


static void *malloc(size_t size) {
    return exram_malloc(4, size);
}

static void free(void *ptr) {
    exram_free(ptr);
}

mac_manager_mem_ops_t mac_manager_mem_ops_sys_heap = {
    .malloc = malloc,
    .free = free
};
