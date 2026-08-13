/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sys/chip_id.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static bool is_hex_lower(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static bool is_all_zero(const char *s)
{
    for (size_t i = 0; i < 16; ++i) {
        if (s[i] != '0') {
            return false;
        }
    }
    return true;
}

void test_chip_id_length_is_16(void)
{
    const char *s = sys_chip_id_get();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(16u, strlen(s));
}

void test_chip_id_charset_lowercase_hex(void)
{
    const char *s = sys_chip_id_get();
    for (size_t i = 0; i < 16; ++i) {
        char msg[64];
        snprintf(msg, sizeof(msg), "char[%zu]='%c' not in [0-9a-f]", i, s[i]);
        TEST_ASSERT_TRUE_MESSAGE(is_hex_lower(s[i]), msg);
    }
}

void test_chip_id_stable_across_calls(void)
{
    char first[17];
    strncpy(first, sys_chip_id_get(), sizeof(first));
    first[16] = '\0';
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL_STRING(first, sys_chip_id_get());
    }
}

void test_chip_id_all_zero_is_pass_but_logged(void)
{
    const char *s = sys_chip_id_get();
    char msg[64];
    snprintf(msg, sizeof(msg), "chip_id=%s", s);
    TEST_MESSAGE(msg);
    if (is_all_zero(s)) {
        TEST_MESSAGE("chip_id is all zeros, EFUSE likely unfused");
    }
    TEST_PASS();
}

int main(void)
{
    UnityBegin("test/framework/sys_chip_id/test_sys_chip_id.c");
    RUN_TEST(test_chip_id_length_is_16, __LINE__);
    RUN_TEST(test_chip_id_charset_lowercase_hex, __LINE__);
    RUN_TEST(test_chip_id_stable_across_calls, __LINE__);
    RUN_TEST(test_chip_id_all_zero_is_pass_but_logged, __LINE__);
    return UnityEnd();
}
