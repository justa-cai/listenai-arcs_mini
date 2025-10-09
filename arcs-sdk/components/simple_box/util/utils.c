/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils.h"
#include "lvfs.h"
#include "sysheap.h"
#include "lisa_log.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

#define PATH_BUF_SIZE 512

/**
 * @brief 将相对路径转换为绝对路径
 *
 * @param path 输入路径，可能是相对路径或绝对路径
 * @param full_path 输出缓冲区，存放转换后的绝对路径
 * @param size 输出缓冲区大小
 * @return int 0成功，非0失败
 */
int convert_to_absolute_path(const char *path, char *full_path, size_t size)
{
    // 检查是否包含驱动器名称（如“/SD:”）
    if (strchr(path, ':') != NULL) {
        strncpy(full_path, path, size - 1);
        full_path[size - 1] = '\0';
        return 0;
    }

    // 绝对路径处理
    if (path[0] == '/') {
        strncpy(full_path, path, size - 1);
        full_path[size - 1] = '\0';
        return 0;
    }

    char *current_dir = exram_malloc(4, PATH_BUF_SIZE);
    if (current_dir == NULL) {
        return -ENOMEM;
    }

    int ret = lvfs_getcwd(CONFIG_SIMPLE_BOX_DEFAULT_MOUNT_PATH, current_dir, PATH_BUF_SIZE);
    if (ret != 0) {
        exram_free(current_dir);
        return ret;
    }

    // 特殊处理 "."
    if (strcmp(path, ".") == 0) {
        strncpy(full_path, current_dir, size - 1);
        full_path[size - 1] = '\0';
        exram_free(current_dir);
        return 0;
    }

    int len = strlen(current_dir);
    if (len > 0 && current_dir[len - 1] == '/') {
        snprintf(full_path, size, "%s%s", current_dir, path);
    } else {
        snprintf(full_path, size, "%s/%s", current_dir, path);
    }

    exram_free(current_dir);

    return 0;
}