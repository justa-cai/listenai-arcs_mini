/**
 * @file board.h
 * @brief 板级接口定义
 *
 * 本文件定义了板级支持包必须实现的标准接口。
 * 所有板型都应该提供这些接口，以确保系统能够正确初始化硬件。
 */

#pragma once

#include "pinmux.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取板型名称
 *
 * @return 指向板型名称字符串的指针（静态字符串，无需释放）
 */
const char* board_get_name(void);

#ifdef __cplusplus
}
#endif
