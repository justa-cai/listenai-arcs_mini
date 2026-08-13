/*
 * Copyright PeakRacing
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "nes_default.h"

#include <string.h>

/*
 * 控制日志中打印的文件路径：
 * - 默认：打印编译器给出的 __FILE__（通常是绝对路径）
 * - 定义 NES_LOG_PATH_PREFIX 为字符串字面量（必须带结尾 '/' 或 '\\'），会在打印时裁剪掉该前缀，得到相对路径
 *   例如：-DNES_LOG_PATH_PREFIX=\"/home/user/arcs-sdk/\"
 * - 定义 NES_LOG_FILE_NAME_ONLY=1：只打印文件名（basename），不打印路径
 */
#ifndef NES_LOG_PATH_PREFIX
#  define NES_LOG_PATH_PREFIX ""
#endif

#ifndef NES_LOG_FILE_NAME_ONLY
#  define NES_LOG_FILE_NAME_ONLY 0
#endif

static inline const char *nes_log__basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *sep = slash;

    if (sep == NULL || (backslash != NULL && backslash > sep)) {
        sep = backslash;
    }

    return sep ? (sep + 1) : path;
}

static inline const char *nes_log__file_short(const char *file)
{
#if NES_LOG_FILE_NAME_ONLY
    (void)NES_LOG_PATH_PREFIX;
    return nes_log__basename(file);
#else
    const size_t prefix_len = sizeof(NES_LOG_PATH_PREFIX) - 1;

    if (prefix_len > 0 && strncmp(file, NES_LOG_PATH_PREFIX, prefix_len) == 0) {
        return file + prefix_len;
    }

    return file;
#endif
}

/* 统一用于日志打印的文件字段 */
#ifndef NES_LOG_FILE
#  if (NES_LOG_FILE_NAME_ONLY) && defined(__FILE_NAME__)
#    define NES_LOG_FILE __FILE_NAME__
#  else
#    define NES_LOG_FILE nes_log__file_short(__FILE__)
#  endif
#endif

#ifdef __cplusplus
    extern "C" {
#endif

/*
 * The color for terminal (foreground)
 * BLACK    30
 * RED      31
 * GREEN    32
 * YELLOW   33
 * BLUE     34
 * PURPLE   35
 * CYAN     36
 * WHITE    37
 */

#define NES_LOG_LEVEL_NONE      0    /* Do not log anything. */
#define NES_LOG_LEVEL_ERROR     1    /* Log error. */
#define NES_LOG_LEVEL_WARN      2    /* Log warning. */
#define NES_LOG_LEVEL_INFO      3    /* Log infomation. */
#define NES_LOG_LEVEL_DEBUG     4    /* Log debug. */

#ifndef NES_LOG_LEVEL
#  define NES_LOG_LEVEL        NES_LOG_LEVEL_INFO
#endif

#  if NES_LOG_LEVEL >= NES_LOG_LEVEL_ERROR
#    define NES_LOG_ERROR(format, ...) nes_log_printf("\033[31m[ERROR][%s:%d(%s)]: \033[0m" format, NES_LOG_FILE, __LINE__, __func__, ##__VA_ARGS__)
#  else
#    define NES_LOG_ERROR(format, ...) do {}while(0)
#  endif

#  if NES_LOG_LEVEL >= NES_LOG_LEVEL_WARN
#    define NES_LOG_WARN(format, ...) nes_log_printf("\033[33m[WARN][%s:%d(%s)]: \033[0m" format, NES_LOG_FILE, __LINE__, __func__, ##__VA_ARGS__)
#  else
#    define NES_LOG_WARN(format, ...) do {}while(0)
#  endif

#  if NES_LOG_LEVEL >= NES_LOG_LEVEL_INFO
#    define NES_LOG_INFO(format, ...) nes_log_printf("\033[32m[INFO][%s:%d(%s)]: \033[0m" format, NES_LOG_FILE, __LINE__, __func__, ##__VA_ARGS__)
#  else
#    define NES_LOG_INFO(format, ...) do {}while(0)
#  endif

#  if NES_LOG_LEVEL >= NES_LOG_LEVEL_DEBUG
#    define NES_LOG_DEBUG(format, ...) nes_log_printf("\033[0m[DEBUG][%s:%d(%s)]: \033[0m" format, NES_LOG_FILE, __LINE__, __func__, ##__VA_ARGS__)
#  else
#    define NES_LOG_DEBUG(format, ...) do {}while(0)
#  endif

#ifdef __cplusplus
    }
#endif
