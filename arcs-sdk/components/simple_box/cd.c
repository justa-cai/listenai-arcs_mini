/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell.h"
#include "lvfs.h"
#include "util/utils.h"
#include "lisa_log.h"
#include "sysheap.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#define PATH_BUF_SIZE 512

/**
 * @brief Implementation of cd command for Letter-Shell
 *
 * @param argc argument count
 * @param argv argument array
 * @return int 0 on success, negative on error
 */
int cd_command(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    int ret;

    if (argc < 2) {
        shellPrint(shell, "Usage: cd <directory>\r\n");
        return -1;
    }

    char *path = exram_malloc(4, PATH_BUF_SIZE);
    if (path == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory\r\n");
        return -ENOMEM;
    }

    strncpy(path, argv[1], PATH_BUF_SIZE - 1);
    path[PATH_BUF_SIZE - 1] = '\0';

    if (strchr(path, ':') != NULL) {
        ret = lvfs_chdrive(path);
        if (ret != 0) {
            shellPrint(shell, "Error: Cannot change to drive '%s' (error %d)\r\n", path, ret);
            exram_free(path);
            return -1;
        }
    }

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
        LOGI("cd path: %s", full_path);
        ret = lvfs_chdir(full_path);
    } else {
        ret = lvfs_chdir(path);
    }
    // Handle error
    switch (ret) {
    case ENOENT:
        shellPrint(shell, "Error: Directory '%s' not found\r\n", path);
        return -1;
    case ENOTDIR:
        shellPrint(shell, "Error: '%s' is not a directory\r\n", path);
        return -1;
    case EACCES:
        shellPrint(shell, "Error: Permission denied to access '%s'\r\n", path);
        return -1;
    default:
        if (ret != 0) {
            shellPrint(shell, "Error: Failed to change directory (error %d)\r\n", ret);
            exram_free(path);
            exram_free(full_path);
            return -1;
        }
    }

    exram_free(path);
    exram_free(full_path);

    char *current_dir = exram_malloc(4, PATH_BUF_SIZE);
    if (current_dir == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory\r\n");
        return -ENOMEM;
    }

    ret = lvfs_getcwd(CONFIG_SIMPLE_BOX_DEFAULT_MOUNT_PATH, current_dir, PATH_BUF_SIZE);
    if (ret == 0) {
        shellPrint(shell, "Current directory: %s\r\n", current_dir);
    } else {
        shellPrint(shell, "Error: Failed to get current directory, error: %d\r\n", ret);
    }

    exram_free(current_dir);

    return 0;
}

// Export the cd command to the shell system
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), cd, cd_command,
                 change current directory);
