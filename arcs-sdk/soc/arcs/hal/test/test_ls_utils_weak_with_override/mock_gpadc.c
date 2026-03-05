/**
 * @file mock_gpadc.c
 * @brief Mock implementation for GPADC driver
 */

#include "mock_gpadc.h"

// Define fake functions
DEFINE_FAKE_VOID_FUNC(HAL_GPADC_Initialize, void*);
DEFINE_FAKE_VOID_FUNC(HAL_GPADC_Control, void*, uint32_t);
DEFINE_FAKE_VOID_FUNC(HAL_GPADC_SetTriggerNum, void*, uint32_t);
DEFINE_FAKE_VOID_FUNC(HAL_GPADC_SetVrefSel, void*, uint32_t);
DEFINE_FAKE_VOID_FUNC(HAL_GPADC_Start, void*);
DEFINE_FAKE_VOID_FUNC(HAL_GPADC_PollForConversion, void*, uint32_t);
DEFINE_FAKE_VALUE_FUNC(uint32_t, HAL_GPADC_GetValue, void*, uint32_t);

void mock_gpadc_init(void)
{
    // 初始化所有 fake 函数
    RESET_FAKE(HAL_GPADC_Initialize);
    RESET_FAKE(HAL_GPADC_Control);
    RESET_FAKE(HAL_GPADC_SetTriggerNum);
    RESET_FAKE(HAL_GPADC_SetVrefSel);
    RESET_FAKE(HAL_GPADC_Start);
    RESET_FAKE(HAL_GPADC_PollForConversion);
    RESET_FAKE(HAL_GPADC_GetValue);
}

void mock_gpadc_reset(void)
{
    mock_gpadc_init();
}
