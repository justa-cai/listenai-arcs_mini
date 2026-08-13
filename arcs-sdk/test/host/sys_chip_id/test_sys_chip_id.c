/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sys/chip_id.h"

static uint8_t g_fake_bytes[8];
static bool g_fake_uppercase;

void sys_arch_chip_id_get(uint8_t bytes[8])
{
    memcpy(bytes, g_fake_bytes, 8);
}

bool sys_arch_chip_id_uppercase(void)
{
    return g_fake_uppercase;
}

void setUp(void)
{
    memset(g_fake_bytes, 0, sizeof(g_fake_bytes));
    g_fake_uppercase = false;
}

void tearDown(void)
{
}

static void run_case(const uint8_t bytes[8], bool uppercase, const char *expected)
{
    memcpy(g_fake_bytes, bytes, 8);
    g_fake_uppercase = uppercase;
    const char *s = sys_chip_id_get();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING(expected, s);
    TEST_ASSERT_EQUAL_size_t(16u, strlen(s));
}

void test_lowercase_canonical(void)
{
    static const uint8_t b[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    run_case(b, false, "123456789abcdef0");
}

void test_uppercase_canonical(void)
{
    static const uint8_t b[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    run_case(b, true, "123456789ABCDEF0");
}

void test_all_zero_returns_zeros_no_fake_prefix(void)
{
    static const uint8_t b[8] = {0};
    run_case(b, false, "0000000000000000");
}

void test_all_ones_lowercase(void)
{
    static const uint8_t b[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    run_case(b, false, "ffffffffffffffff");
}

void test_all_ones_uppercase(void)
{
    static const uint8_t b[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    run_case(b, true, "FFFFFFFFFFFFFFFF");
}

void test_leading_zeros_padding(void)
{
    static const uint8_t b[8] = {0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0};
    run_case(b, false, "0a000000000000b0");
}

void test_arcs_boot_serial_byte_order(void)
{
    static const uint8_t b[8] = {0x6B, 0x3B, 0xC0, 0x1B, 0x4D, 0x53, 0xC9, 0xBD};
    run_case(b, false, "6b3bc01b4d53c9bd");
}

void test_consecutive_calls_byte_equal(void)
{
    static const uint8_t b[8] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18};
    memcpy(g_fake_bytes, b, 8);
    g_fake_uppercase = false;
    char first[17];
    strcpy(first, sys_chip_id_get());
    const char *second = sys_chip_id_get();
    TEST_ASSERT_EQUAL_STRING(first, second);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d4e5f60718", second);
}

int main(void)
{
    UnityBegin("test/host/sys_chip_id/test_sys_chip_id.c");
    RUN_TEST(test_lowercase_canonical, __LINE__);
    RUN_TEST(test_uppercase_canonical, __LINE__);
    RUN_TEST(test_all_zero_returns_zeros_no_fake_prefix, __LINE__);
    RUN_TEST(test_all_ones_lowercase, __LINE__);
    RUN_TEST(test_all_ones_uppercase, __LINE__);
    RUN_TEST(test_leading_zeros_padding, __LINE__);
    RUN_TEST(test_arcs_boot_serial_byte_order, __LINE__);
    RUN_TEST(test_consecutive_calls_byte_equal, __LINE__);
    return UnityEnd();
}
