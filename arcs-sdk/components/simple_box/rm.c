/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell.h"
#include "lvfs.h"
#include "lisa_log.h"
#include "sysheap.h"
#include "util/utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_BUF_SIZE 512

static int parse_rm_arguments(int argc, char *argv[], char **path_out, int *recursive_out);
static int process_rm_path(const char *path, char *full_path, size_t full_path_size);
static int remove_file_or_directory(const char *path, int recursive, Shell *shell);
static int remove_directory_recursive(const char *dir_path, Shell *shell);


static int parse_rm_arguments(int argc, char *argv[], char **path_out, int *recursive_out) {
    *recursive_out = 0;
    

    char *path = exram_malloc(4, PATH_BUF_SIZE);
    if (path == NULL) {
        return -ENOMEM;
    }
    
    path[0] = '\0';
    

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'r') != NULL) {
                *recursive_out = 1;
            }
        } else {
            strncpy(path, argv[i], PATH_BUF_SIZE - 1);
            path[PATH_BUF_SIZE - 1] = '\0';
        }
    }
    

    if (path[0] == '\0') {
        exram_free(path);
        return -1;
    }
    
    *path_out = path;
    return 0;
}


static int process_rm_path(const char *path, char *full_path, size_t full_path_size) {
    int ret;
    

    if (path[0] != '/' && strchr(path, ':') == NULL) {
        ret = convert_to_absolute_path(path, full_path, full_path_size);
        if (ret != 0) {
            return ret;
        }
        LOGI("rm path: %s", full_path);
    } else {
        strncpy(full_path, path, full_path_size - 1);
        full_path[full_path_size - 1] = '\0';
    }
    

    if (strchr(full_path, ':') != NULL) {
        ret = lvfs_chdrive(full_path);
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}


static int remove_file_or_directory(const char *path, int recursive, Shell *shell) {
    struct stat st;
    int ret;
    

    if (stat(path, &st) != 0) {
        return -errno;
    }
    

    if (S_ISDIR(st.st_mode)) {

        if (!recursive) {
            return -EISDIR;
        }
        
        LOGI("Attempting to remove directory: %s", path);
        ret = remove_directory_recursive(path, shell);
    } else {

        ret = unlink(path);
    }
    
    if (ret == 0) {
        LOGI("Removed: %s", path);
    }
    
    return ret;
}


int rm_command(int argc, char *argv[]) {
    Shell *shell = shellGetCurrent();
    int ret;
    int recursive = 0;
    char *path = NULL;

    if (argc < 2) {
        shellPrint(shell, "Usage: rm [-r] <file/directory>\r\n");
        shellPrint(shell, "  -r  Recursively remove directories and their contents\r\n");
        return -1;
    }


    ret = parse_rm_arguments(argc, argv, &path, &recursive);
    if (ret != 0) {
        shellPrint(shell, "Error: No file or directory specified\r\n");
        return -1;
    }
    

    char *full_path = exram_malloc(4, PATH_BUF_SIZE);
    if (full_path == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory\r\n");
        exram_free(path);
        return -ENOMEM;
    }
    

    ret = process_rm_path(path, full_path, PATH_BUF_SIZE);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to process path '%s' (error %d)\r\n", path, ret);
        exram_free(path);
        exram_free(full_path);
        return -1;
    }
    

    ret = remove_file_or_directory(full_path, recursive, shell);
    

    if (ret != 0) {
        if (ret == -EISDIR) {
            shellPrint(shell, "Error: '%s' is a directory. Use -r option to remove directories.\r\n", full_path);
        } else if (ret == -ENOENT) {
            shellPrint(shell, "Error: '%s' not found\r\n", full_path);
        } else if (ret == -EACCES) {
            shellPrint(shell, "Error: Permission denied to access '%s'\r\n", full_path);
        } else {
            shellPrint(shell, "Error: Failed to remove '%s': %s\r\n", full_path, strerror(-ret));
        }
        
        exram_free(path);
        exram_free(full_path);
        return -1;
    }
    
    exram_free(path);
    exram_free(full_path);
    return 0;
}

// 递归删除目录及其内容
static int remove_directory_recursive(const char *dir_path, Shell *shell) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    int ret = 0;
    

    dir = opendir(dir_path);
    if (!dir) {
        shellPrint(shell, "Error: Cannot open directory '%s': %s\r\n", dir_path, strerror(errno));
        return -errno;
    }
    
    char *item_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!item_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        closedir(dir);
        return -ENOMEM;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(item_path, PATH_BUF_SIZE, "%s/%s", dir_path, entry->d_name);
        

        if (stat(item_path, &st) != 0) {
            LOGW("Cannot stat '%s': %s, skipping...", item_path, strerror(errno));
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {

            ret = remove_directory_recursive(item_path, shell);
            if (ret != 0) {
                LOGW("Failed to remove directory '%s': %d, continuing with other items...", item_path, ret);
    
            }
        } else {

            ret = unlink(item_path);
            if (ret != 0) {
                LOGW("Failed to remove file '%s': %s, continuing with other items...", 
                     item_path, strerror(errno));
    
            }
        }
    }
    

    closedir(dir);
    exram_free(item_path);
    
    // 在嵌入式系统(FatFS)中使用unlink删除目录
    LOGI("Attempting to remove directory itself: %s", dir_path);
    ret = unlink(dir_path);
    if (ret != 0) {
        int err = errno;
        LOGI("Failed to remove directory '%s': %s (errno=%d)", dir_path, strerror(err), err);
        shellPrint(shell, "Warning: Failed to remove empty directory '%s': %s\r\n", 
                  dir_path, strerror(err));
        // 目录内容已清空，即使目录本身未被删除也算部分成功
    } else {
        LOGI("Successfully removed directory: %s", dir_path);
    }
    
    return 0;
}


SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), rm, rm_command, Remove files or directories);
