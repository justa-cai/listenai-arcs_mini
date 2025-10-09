/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * @brief 将相对路径转换为绝对路径
 * 
 * @param path 输入路径，可能是相对路径或绝对路径
 * @param full_path 输出缓冲区，存放转换后的绝对路径
 * @param size 输出缓冲区大小
 * @return int 0成功，非0失败
 */
int convert_to_absolute_path(const char *path, char *full_path, size_t size);

#ifdef __cplusplus
}
#endif