/**
 * @file test_ls_utils_weak.c
 * @brief 单元测试: 验证 ls_read_temp_voltage 是否为 weak 符号
 *
 * 测试目标:
 * 1. 验证 ls_read_temp_voltage 可以被用户自定义实现覆盖
 * 2. 验证 weak 符号机制正常工作
 */

#include "unity.h"
#include "mock_gpadc.h"
#include "ls_utils.h"
#include <string.h>

// 定义 FFF 全局变量
DEFINE_FFF_GLOBALS;

// ========== 自定义实现(用于覆盖 weak 函数) ==========

// 标志:用于确认是否调用了自定义实现
static int custom_function_called = 0;
static float custom_voltage_value = 0.0f;

/**
 * @brief 用户自定义的 ls_read_temp_voltage 实现
 *
 * 这个实现会覆盖 ls_utils.c 中的 weak 符号版本
 */
int ls_read_temp_voltage(float *vout)
{
    custom_function_called = 1;

    if (vout == NULL) {
        return -1;
    }

    *vout = custom_voltage_value;
    return 0;
}

// ========== 测试辅助函数 ==========

/**
 * @brief 每个测试用例执行前的初始化
 */
void setUp(void)
{
    mock_gpadc_init();
    custom_function_called = 0;
    custom_voltage_value = 0.0f;
}

/**
 * @brief 每个测试用例执行后的清理
 */
void tearDown(void)
{
    mock_gpadc_reset();
}

// ========== 测试用例 ==========

/**
 * @brief 测试用例: 验证 weak 符号可以被覆盖
 *
 * 测试步骤:
 * 1. 调用 ls_read_temp_voltage
 * 2. 验证调用的是自定义实现,而不是原始 weak 实现
 * 3. 验证 GPADC 相关函数没有被调用(因为使用了自定义实现)
 */
void test_weak_symbol_can_be_overridden(void)
{
    float voltage = 0.0f;

    // 设置自定义返回值
    custom_voltage_value = 1.23f;

    // 调用函数
    int ret = ls_read_temp_voltage(&voltage);

    // 验证:调用成功
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "ls_read_temp_voltage should return 0");

    // 验证:使用了自定义实现
    TEST_ASSERT_EQUAL_MESSAGE(1, custom_function_called,
        "Custom implementation should be called");

    // 验证:返回了自定义的电压值
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.23f, voltage,
        "Should return custom voltage value");

    // 验证:GPADC 函数没有被调用(因为 weak 符号被覆盖)
    TEST_ASSERT_EQUAL_MESSAGE(0, HAL_GPADC_Initialize_fake.call_count,
        "HAL_GPADC_Initialize should not be called when using custom implementation");
}

// ========== 主函数 ==========

/**
 * @brief 测试入口
 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_weak_symbol_can_be_overridden);

    return UNITY_END();
}
