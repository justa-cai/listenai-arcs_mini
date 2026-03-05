/**
 * @file mock_gpadc.h
 * @brief Mock implementation for GPADC driver and related dependencies
 */

#ifndef MOCK_GPADC_H
#define MOCK_GPADC_H

#include <stdint.h>
#include "fff.h"

// Mock functions declarations
DECLARE_FAKE_VOID_FUNC(HAL_GPADC_Initialize, void*);
DECLARE_FAKE_VOID_FUNC(HAL_GPADC_Control, void*, uint32_t);
DECLARE_FAKE_VOID_FUNC(HAL_GPADC_SetTriggerNum, void*, uint32_t);
DECLARE_FAKE_VOID_FUNC(HAL_GPADC_SetVrefSel, void*, uint32_t);
DECLARE_FAKE_VOID_FUNC(HAL_GPADC_Start, void*);
DECLARE_FAKE_VOID_FUNC(HAL_GPADC_PollForConversion, void*, uint32_t);
DECLARE_FAKE_VALUE_FUNC(uint32_t, HAL_GPADC_GetValue, void*, uint32_t);

/**
 * @brief 初始化所有 Mock 函数
 */
void mock_gpadc_init(void);

/**
 * @brief 重置所有 Mock 函数调用记录
 */
void mock_gpadc_reset(void);

#endif // MOCK_GPADC_H
