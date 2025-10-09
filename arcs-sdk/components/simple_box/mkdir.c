/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell.h"
#include "lvfs.h"
#include "lisa_log.h"
#include "util/utils.h"
#include "sysheap.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define PATH_BUF_SIZE 512
int mkdir_command(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    int ret;

    if (argc < 2) {
        shellPrint(shell, "Usage: mkdir <directory>\r\n");
        return -1;
    }

    char *path = exram_malloc(4, PATH_BUF_SIZE);
    if (path == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory\r\n");
        return -ENOMEM;
    }

    strncpy(path, argv[1], PATH_BUF_SIZE - 1);
    path[PATH_BUF_SIZE - 1] = '\0';

    char *full_path = exram_malloc(4, PATH_BUF_SIZE);
    if (full_path == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory\r\n");
        exram_free(path);
        return -ENOMEM;
    }

    if (path[0] != '/' && strchr(path, ':') == NULL) {
        ret = convert_to_absolute_path(path, full_path, PATH_BUF_SIZE);
        if (ret != 0) {
            shellPrint(shell, "Error: Failed to resolve path '%s' (error %d)\r\n", path, ret);
            exram_free(path);
            exram_free(full_path);
            return -1;
        }
        LOGI("mkdir path: %s", full_path);
    } else {
        strncpy(full_path, path, PATH_BUF_SIZE - 1);
        full_path[PATH_BUF_SIZE - 1] = '\0';
    }

    if (strchr(full_path, ':') != NULL) {
        ret = lvfs_chdrive(full_path);
        if (ret != 0) {
            shellPrint(shell, "Error: Cannot change to drive in path '%s' (error %d)\r\n", full_path, ret);
            exram_free(path);
            exram_free(full_path);
            return -1;
        }
    }

    ret = mkdir(full_path, 0777);
    if (ret != 0) {
        switch (ret) {
        case -ENOENT:
            shellPrint(shell, "Error: Parent directory does not exist for '%s'\r\n", full_path);
            break;
        case -EEXIST:
            shellPrint(shell, "Error: Directory '%s' already exists\r\n", full_path);
            break;
        case -EACCES:
            shellPrint(shell, "Error: Permission denied to create directory '%s'\r\n", full_path);
            break;
        default:
            shellPrint(shell, "Error: Failed to create directory (error %d)\r\n", ret);
            break;
        }
        exram_free(path);
        exram_free(full_path);
        return -1;
    }

    exram_free(path);
    exram_free(full_path);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, mkdir,
                 mkdir_command, make a directory);
