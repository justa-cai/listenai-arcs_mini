/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include "ipc_mem.h"
#include "ipc_shared.h"
#include "ipc_queue.h"

uint8_t _ipcshram[sizeof(struct ipc_shared_env_tag)];

static bool header_published_before_queue_init;
static uint32_t queue_init_calls;

void ipc_queue_ring_init(volatile struct vring_hdr *vring, volatile void *buf, int32_t item_size,
                         int32_t item_num)
{
    struct ipc_shared_env_tag *mem = (struct ipc_shared_env_tag *)_ipcshram;

    (void)vring;
    (void)buf;
    (void)item_size;
    (void)item_num;

    queue_init_calls++;

    if ((mem->hdr.pattern1 != 0) || (mem->hdr.pattern2 != 0) ||
        (mem->hdr.config_addr != 0) || (mem->hdr.config_len != 0)) {
        header_published_before_queue_init = true;
    }
}

void setUp(void)
{
    memset(_ipcshram, 0, sizeof(_ipcshram));
    header_published_before_queue_init = false;
    queue_init_calls = 0;
}

void tearDown(void)
{
}

void test_ipc_mem_init_publishes_header_after_queue_init(void)
{
    struct ipc_shared_env_tag *mem = (struct ipc_shared_env_tag *)_ipcshram;

    ipc_mem_init(0);

    TEST_ASSERT_EQUAL_UINT32(2, queue_init_calls);
    TEST_ASSERT_FALSE(header_published_before_queue_init);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, mem->hdr.pattern1);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, mem->hdr.pattern2);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)mem->config, mem->hdr.config_addr);
    TEST_ASSERT_EQUAL_UINT32(sizeof(mem->config), mem->hdr.config_len);
}

int main(void)
{
    UnityBegin("test/framework/ipc_mem_init_order/test_ipc_mem_init_order.c");
    RUN_TEST(test_ipc_mem_init_publishes_header_after_queue_init, __LINE__);
    return UnityEnd();
}
