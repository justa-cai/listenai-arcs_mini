/**
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

int pwd_command(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    char *cwd = exram_malloc(4, PATH_BUF_SIZE);

    // 获取当前工作目录
    int ret = lvfs_getcwd(CONFIG_SIMPLE_BOX_DEFAULT_MOUNT_PATH, cwd, PATH_BUF_SIZE);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to get current working directory: %s\r\n", strerror(errno));
        exram_free(cwd);
        return -1;
    }
    LOGI("Current working directory: %s", cwd);
    shellPrint(shell, "%s\r\n", cwd);
    exram_free(cwd);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, pwd,
                 pwd_command, show current working directory);
