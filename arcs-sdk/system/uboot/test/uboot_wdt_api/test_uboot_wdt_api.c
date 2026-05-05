#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdint.h>

#include "uboot_wdt_api.h"

#define UBOOT_WDT_ENABLE_CTRL_VALUE 0x78Du

static uint32_t g_wren;
static uint32_t g_ctrl;
static uint32_t g_restart;

volatile uint32_t *uboot_wdt_api_wren_reg(void)
{
    return &g_wren;
}

volatile uint32_t *uboot_wdt_api_ctrl_reg(void)
{
    return &g_ctrl;
}

volatile uint32_t *uboot_wdt_api_restart_reg(void)
{
    return &g_restart;
}

void setUp(void)
{
    g_wren = 0;
    g_ctrl = 0;
    g_restart = 0;
}

void tearDown(void)
{
}

void test_enable_writes_watchdog_unlock_and_default_ctrl_value(void)
{
    TEST_ASSERT_EQUAL_INT(0, uboot_wdt_enable());
    TEST_ASSERT_EQUAL_UINT32(0x5AA5u, g_wren);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_WDT_ENABLE_CTRL_VALUE, g_ctrl);
}

void test_disable_clears_watchdog_enable_bit(void)
{
    g_ctrl = UBOOT_WDT_ENABLE_CTRL_VALUE;

    TEST_ASSERT_EQUAL_INT(0, uboot_wdt_disable());
    TEST_ASSERT_EQUAL_UINT32(0x5AA5u, g_wren);
    TEST_ASSERT_EQUAL_UINT32(UBOOT_WDT_ENABLE_CTRL_VALUE & ~0x1u, g_ctrl);
}

void test_feed_writes_watchdog_unlock_and_restart_magic(void)
{
    TEST_ASSERT_EQUAL_INT(0, uboot_wdt_feed());
    TEST_ASSERT_EQUAL_UINT32(0x5AA5u, g_wren);
    TEST_ASSERT_EQUAL_UINT32(0xCAFEu, g_restart);
}

int main(void)
{
    UnityBegin("system/uboot/test/uboot_wdt_api/test_uboot_wdt_api.c");

    RUN_TEST(test_enable_writes_watchdog_unlock_and_default_ctrl_value, __LINE__);
    RUN_TEST(test_disable_clears_watchdog_enable_bit, __LINE__);
    RUN_TEST(test_feed_writes_watchdog_unlock_and_restart_magic, __LINE__);

    return UnityEnd();
}
