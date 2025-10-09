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
#include <fcntl.h>
#include <unistd.h>

#define PATH_BUF_SIZE 512

static int create_file(const char *path, Shell *shell) {
    int fd = open(path, O_CREAT | O_WRONLY, 0666);
    if (fd < 0) {
        int err = errno;
        
        switch (err) {
        case ENOENT:
            shellPrint(shell, "Error: Directory in path '%s' does not exist\r\n", path);
            break;
        case EACCES:
            shellPrint(shell, "Error: Permission denied to create '%s'\r\n", path);
            break;
        default:
            shellPrint(shell, "Error: Failed to create file (error %d)\r\n", err);
            break;
        }
        
        return -err;
    }

    close(fd);
    shellPrint(shell, "File created: %s\r\n", path);
    return 0;
}

static int prepare_path(const char *input_path, char **full_path_out, Shell *shell) {
    char *full_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!full_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        return -ENOMEM;
    }
    
    int ret = convert_to_absolute_path(input_path, full_path, PATH_BUF_SIZE);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to resolve path '%s' (error %d)\r\n", input_path, ret);
        exram_free(full_path);
        return -1;
    }
    
    LOGI("touch path: %s", full_path);
    
    if (strchr(full_path, ':') != NULL) {
        ret = lvfs_chdrive(full_path);
        if (ret != 0) {
            shellPrint(shell, "Error: Cannot change to drive in path '%s' (error %d)\r\n", full_path, ret);
            exram_free(full_path);
            return -1;
        }
    }
    
    *full_path_out = full_path;
    return 0;
}

int touch_command(int argc, char *argv[]) {
    char *path = NULL;
    char *full_path = NULL;
    int ret = 0;
    Shell *shell = shellGetCurrent();

    if (argc < 2) {
        shellPrint(shell, "Usage: touch <file>\r\n");
        return -1;
    }

    path = exram_malloc(4, PATH_BUF_SIZE);
    if (!path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        ret = -ENOMEM;
        goto cleanup;
    }
    
    strncpy(path, argv[1], PATH_BUF_SIZE - 1);
    path[PATH_BUF_SIZE - 1] = '\0';
    
    ret = prepare_path(path, &full_path, shell);
    if (ret != 0) {
        goto cleanup;
    }
    
    ret = create_file(full_path, shell);

cleanup:
    if (path) exram_free(path);
    if (full_path) exram_free(full_path);
    return ret;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, touch, touch_command,
                 create or update file timestamps);
