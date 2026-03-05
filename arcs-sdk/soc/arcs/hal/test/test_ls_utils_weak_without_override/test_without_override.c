/**
 * @file test_without_override.c
 * @brief 单元测试: 验证不覆盖 weak 符号时会触发 __builtin_trap()
 *
 * 测试目标:
 * 1. 验证 ls_read_temp_voltage 的 weak 实现包含 __builtin_trap()
 * 2. 验证调用原始 weak 函数会导致程序异常终止
 */

#include "unity.h"
#include "mock_gpadc.h"
#include "ls_utils.h"
#include <signal.h>
#include <setjmp.h>

// 定义 FFF 全局变量
DEFINE_FFF_GLOBALS;

// 用于 signal handler 的跳转点
static jmp_buf jump_buffer;
static volatile int signal_caught = 0;

/**
 * @brief 信号处理函数 - 捕获 SIGILL (非法指令)
 */
static void signal_handler(int sig)
{
    if (sig == SIGILL) {
        signal_caught = 1;
        longjmp(jump_buffer, 1);  // 跳回 setjmp 位置
    }
}

/**
 * @brief 每个测试用例执行前的初始化
 */
void setUp(void)
{
    mock_gpadc_init();
    signal_caught = 0;
}

/**
 * @brief 每个测试用例执行后的清理
 */
void tearDown(void)
{
    mock_gpadc_reset();
    signal(SIGILL, SIG_DFL);  // 恢复默认信号处理
}

// ========== 测试用例 ==========

/**
 * @brief 测试用例1: 验证调用 weak 函数会触发 __builtin_trap()
 *
 * 测试原理:
 * - 原始 weak 函数第一行是 __builtin_trap()
 * - __builtin_trap() 会触发 SIGILL 信号
 * - 使用 signal handler 和 setjmp/longjmp 捕获这个信号
 */
void test_weak_function_triggers_trap(void)
{
    float voltage = 0.0f;

    // 设置信号处理器
    signal(SIGILL, signal_handler);

    // 设置跳转点
    if (setjmp(jump_buffer) == 0) {
        // 第一次执行: 调用 weak 函数,期望触发 trap
        ls_read_temp_voltage(&voltage);

        // 如果执行到这里,说明没有触发 trap,测试失败
        TEST_FAIL_MESSAGE("Expected __builtin_trap() to be triggered");
    } else {
        // 从 longjmp 跳回来,说明捕获到了 SIGILL
        TEST_ASSERT_EQUAL_MESSAGE(1, signal_caught,
            "SIGILL signal should be caught");
    }
}

/**
 * @brief 测试用例2: 验证 GPADC 函数在 trap 之前不会被调用
 *
 * 因为 __builtin_trap() 在函数开头,所以 GPADC 相关代码不应该执行
 */
void test_gpadc_not_called_before_trap(void)
{
    float voltage = 0.0f;

    // 设置信号处理器
    signal(SIGILL, signal_handler);

    // 重置 mock 计数
    mock_gpadc_reset();

    // 设置跳转点
    if (setjmp(jump_buffer) == 0) {
        ls_read_temp_voltage(&voltage);
        TEST_FAIL_MESSAGE("Expected trap");
    }

    // 验证 GPADC 函数没有被调用 (因为 trap 在函数开头)
    TEST_ASSERT_EQUAL_MESSAGE(0, HAL_GPADC_Initialize_fake.call_count,
        "HAL_GPADC_Initialize should not be called before trap");
}

// ========== 主函数 ==========

/**
 * @brief 测试入口
 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_weak_function_triggers_trap);
    RUN_TEST(test_gpadc_not_called_before_trap);

    return UNITY_END();
}
