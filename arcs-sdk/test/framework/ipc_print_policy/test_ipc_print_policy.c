/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>

#include "ipc_print_policy.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_notify_threshold_uses_configured_value(void)
{
    TEST_ASSERT_EQUAL_UINT32(64, ipc_print_notify_threshold_get());
}

void test_poll_timeout_uses_configured_value(void)
{
    TEST_ASSERT_EQUAL_INT32(1234, ipc_print_poll_timeout_ms_get());
}

void test_should_notify_when_pending_exceeds_threshold_and_reader_sleeping(void)
{
    TEST_ASSERT_TRUE(ipc_print_should_notify(65, false, true));
}

void test_should_not_notify_when_pending_does_not_exceed_threshold(void)
{
    TEST_ASSERT_FALSE(ipc_print_should_notify(64, false, true));
}

void test_should_not_notify_when_reader_is_active(void)
{
    TEST_ASSERT_FALSE(ipc_print_should_notify(65, true, true));
}

void test_should_not_notify_without_reader_progress_since_last_notify(void)
{
    TEST_ASSERT_FALSE(ipc_print_should_notify(65, false, false));
}

int main(void)
{
    UnityBegin("test/framework/ipc_print_policy/test_ipc_print_policy.c");
    RUN_TEST(test_notify_threshold_uses_configured_value, __LINE__);
    RUN_TEST(test_poll_timeout_uses_configured_value, __LINE__);
    RUN_TEST(test_should_notify_when_pending_exceeds_threshold_and_reader_sleeping, __LINE__);
    RUN_TEST(test_should_not_notify_when_pending_does_not_exceed_threshold, __LINE__);
    RUN_TEST(test_should_not_notify_when_reader_is_active, __LINE__);
    RUN_TEST(test_should_not_notify_without_reader_progress_since_last_notify, __LINE__);
    return UnityEnd();
}
