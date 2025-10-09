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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#define PATH_BUF_SIZE 512

static int copy_file(const char *src_path, const char *dst_path)
{
    int src_fd, dst_fd;
    char *buffer = NULL;
    ssize_t bytes_read, bytes_written;
    int ret = 0;

    src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        return -errno;
    }

    dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dst_fd < 0) {
        close(src_fd);
        return -errno;
    }

    buffer = exram_malloc(4, CONFIG_SIMPLE_BOX_COPY_BUF_SIZE);
    if (!buffer) {
        close(src_fd);
        close(dst_fd);
        return -ENOMEM;
    }

    while ((bytes_read = read(src_fd, buffer, CONFIG_SIMPLE_BOX_COPY_BUF_SIZE)) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            ret = -errno;
            break;
        }
    }

    exram_free(buffer);

    if (bytes_read < 0) {
        ret = -errno;
    }

    close(src_fd);
    close(dst_fd);

    return ret;
}

static char *path_basename(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return (char *)(last_slash + 1);
    }
    return (char *)path;
}

// 判断路径是否以斜杠结尾
static int path_ends_with_slash(const char *path)
{
    size_t len = strlen(path);
    return (len > 0 && path[len - 1] == '/') ? 1 : 0;
}

static int create_directory(const char *path, Shell *shell)
{
    int ret = mkdir(path, 0777);
    if (ret != 0) {
        if (ret == -EEXIST) {

            return 0;
        }

        shellPrint(shell, "Error: [cp] Failed to create directory '%s': %d\r\n", path, ret);
        return -errno;
    }
    return 0;
}

static int copy_directory_contents(const char *src_path, const char *dst_path, Shell *shell)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    int ret = 0;

    ret = create_directory(dst_path, shell);
    if (ret != 0) {
        return ret;
    }

    dir = opendir(src_path);
    if (dir == NULL) {
        shellPrint(shell, "Error: Failed to open directory '%s': %s\r\n", src_path, strerror(errno));
        return -errno;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *src_item_path = exram_malloc(4, PATH_BUF_SIZE);
        if (!src_item_path) {
            shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
            closedir(dir);
            return -ENOMEM;
        }
        snprintf(src_item_path, PATH_BUF_SIZE, "%s/%s", src_path, entry->d_name);

        // Build destination file/directory path
        char *dst_item_path = exram_malloc(4, PATH_BUF_SIZE);
        if (!dst_item_path) {
            shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
            exram_free(src_item_path);
            closedir(dir);
            return -ENOMEM;
        }
        snprintf(dst_item_path, PATH_BUF_SIZE, "%s/%s", dst_path, entry->d_name);

        if (stat(src_item_path, &st) != 0) {
            shellPrint(shell, "Warning: Failed to get status for '%s': %s\r\n", src_item_path, strerror(errno));
            exram_free(src_item_path);
            exram_free(dst_item_path);
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            // Copy regular file
            ret = copy_file(src_item_path, dst_item_path);
            if (ret != 0) {
                shellPrint(shell, "Warning: Failed to copy file '%s' to '%s' (error %d)\r\n", src_item_path,
                           dst_item_path, -ret);
                // Continue processing other files, do not interrupt the entire copy process
            }
        } else if (S_ISDIR(st.st_mode)) {
            // Recursively copy subdirectory
            ret = create_directory(dst_item_path, shell);
            if (ret == 0) {
                ret = copy_directory_contents(src_item_path, dst_item_path, shell);
                if (ret != 0) {
                    shellPrint(shell, "Warning: Failed to copy directory '%s' to '%s' (error %d)\r\n", src_item_path,
                               dst_item_path, -ret);
                }
            }
        }

        exram_free(src_item_path);
        exram_free(dst_item_path);
    }

    closedir(dir);

    return 0;
}

static int copy_directory(const char *src_path, const char *dst_path, int create_subdir, Shell *shell)
{
    int ret = 0;
    char *actual_dst_path = NULL;

    LOGI("copy_directory: src='%s', dst='%s', create_subdir=%d", src_path, dst_path, create_subdir);

    if (create_subdir) {
        char *src_basename = path_basename(src_path);
        LOGI("Source directory name: %s", src_basename);

        actual_dst_path = exram_malloc(4, PATH_BUF_SIZE);
        if (!actual_dst_path) {
            shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
            return -ENOMEM;
        }

        if (path_ends_with_slash(dst_path)) {
            snprintf(actual_dst_path, PATH_BUF_SIZE, "%s%s", dst_path, src_basename);
        } else {
            snprintf(actual_dst_path, PATH_BUF_SIZE, "%s/%s", dst_path, src_basename);
        }

        LOGI("Create subdirectory: %s", actual_dst_path);

        ret = create_directory(actual_dst_path, shell);
        if (ret != 0) {
            exram_free(actual_dst_path);
            return ret;
        }
    } else {
        actual_dst_path = exram_malloc(4, PATH_BUF_SIZE);
        if (!actual_dst_path) {
            shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
            return -ENOMEM;
        }

        strncpy(actual_dst_path, dst_path, PATH_BUF_SIZE - 1);
        actual_dst_path[PATH_BUF_SIZE - 1] = '\0';

        LOGI("Directly use destination path: %s", actual_dst_path);

        ret = create_directory(actual_dst_path, shell);
        if (ret != 0) {
            exram_free(actual_dst_path);
            return ret;
        }
    }

    LOGI("copy_directory: copying content to: %s", actual_dst_path);

    ret = copy_directory_contents(src_path, actual_dst_path, shell);

    exram_free(actual_dst_path);
    return ret;
}

static int parse_cp_arguments(int argc, char *argv[], char **src_path_out, char **dst_path_out, int *recursive_out)
{
    char *src_path = *src_path_out;
    char *dst_path = *dst_path_out;
    int recursive = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'r') != NULL) {
                recursive = 1;
            }
        } else if (src_path[0] == '\0') {
            strncpy(src_path, argv[i], PATH_BUF_SIZE - 1);
            src_path[PATH_BUF_SIZE - 1] = '\0';
        } else if (dst_path[0] == '\0') {
            strncpy(dst_path, argv[i], PATH_BUF_SIZE - 1);
            dst_path[PATH_BUF_SIZE - 1] = '\0';
        }
    }

    if (src_path[0] == '\0' || dst_path[0] == '\0') {
        return -1;
    }

    *recursive_out = recursive;
    return 0;
}

static int copy_file_to_directory(const char *src_path, const char *dst_dir, Shell *shell)
{
    char *basename = path_basename(src_path);

    char *target_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!target_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        return -ENOMEM;
    }

    int dst_len = strlen(dst_dir);
    if (dst_dir[dst_len - 1] == '/') {
        snprintf(target_path, PATH_BUF_SIZE, "%s%s", dst_dir, basename);
    } else {
        snprintf(target_path, PATH_BUF_SIZE, "%s/%s", dst_dir, basename);
    }

    int ret = copy_file(src_path, target_path);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to copy file (error %d)\r\n", -ret);
        exram_free(target_path);
        return ret;
    }

    shellPrint(shell, "Copied '%s' to '%s'\r\n", src_path, target_path);
    exram_free(target_path);
    return 0;
}

static int handle_directory_copy(const char *src_path, const char *dst_path, int dst_exists, Shell *shell)
{
    int ret;

    if (dst_exists) {
        LOGI("destination exists, copying source directory as subdirectory");
        ret = copy_directory(src_path, dst_path, 1, shell);
    } else {
        LOGI("destination does not exist, creating directory and copying contents");
        char *clean_dst_path = exram_malloc(4, PATH_BUF_SIZE);
        if (!clean_dst_path) {
            shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
            return -ENOMEM;
        }

        strncpy(clean_dst_path, dst_path, PATH_BUF_SIZE - 1);
        clean_dst_path[PATH_BUF_SIZE - 1] = '\0';

        // Remove trailing slash from destination path
        size_t len = strlen(clean_dst_path);
        if (len > 0 && clean_dst_path[len - 1] == '/') {
            clean_dst_path[len - 1] = '\0';
            LOGI("remove trailing slash from destination path, now path is: %s", clean_dst_path);
        }

        ret = create_directory(clean_dst_path, shell);
        if (ret == 0) {
            ret = copy_directory_contents(src_path, clean_dst_path, shell);
        }

        exram_free(clean_dst_path);
    }

    return ret;
}

static int process_file_copy(const char *src_path, const char *dst_path, Shell *shell)
{
    int ret;
    struct stat dst_st;

    if (stat(dst_path, &dst_st) == 0 && S_ISDIR(dst_st.st_mode)) {
        ret = copy_file_to_directory(src_path, dst_path, shell);
    } else {
        ret = copy_file(src_path, dst_path);
        if (ret == 0) {
            shellPrint(shell, "Copied '%s' to '%s'\r\n", src_path, dst_path);
        } else {
            shellPrint(shell, "Error: Failed to copy file (error %d)\r\n", -ret);
        }
    }

    return ret;
}

static int process_directory_copy(const char *src_path, const char *dst_path, int recursive, Shell *shell)
{
    int ret;
    struct stat dst_stat;
    int dst_exists = (stat(dst_path, &dst_stat) == 0);

    if (!recursive) {
        shellPrint(shell, "Error: '%s' is a directory. Use -r option to copy directories.\r\n", src_path);
        return -1;
    }

    if (dst_exists && !S_ISDIR(dst_stat.st_mode)) {
        shellPrint(shell, "Error: Cannot overwrite non-directory '%s' with directory '%s'\r\n", dst_path, src_path);
        return -1;
    }

    LOGI("src_path: %s, dst_path: %s, dst_exists: %d", src_path, dst_path, dst_exists);

    ret = handle_directory_copy(src_path, dst_path, dst_exists, shell);

    if (ret == 0) {
        shellPrint(shell, "Copied directory '%s' to '%s'\r\n", src_path, dst_path);
    } else {
        shellPrint(shell, "Error: Failed to copy directory (error %d)\r\n", -ret);
    }

    return ret;
}

static int prepare_paths(const char *src_path_arg, const char *dst_path_arg, char **full_src_path_out,
                         char **full_dst_path_out, Shell *shell)
{
    int ret;
    char *full_src_path = *full_src_path_out;
    char *full_dst_path = *full_dst_path_out;

    full_src_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!full_src_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        return -ENOMEM;
    }

    ret = convert_to_absolute_path(src_path_arg, full_src_path, PATH_BUF_SIZE);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to resolve source path '%s' (error %d)\r\n", src_path_arg, ret);
        exram_free(full_src_path);
        return -1;
    }
    LOGI("Using source path: %s", full_src_path);

    full_dst_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!full_dst_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        exram_free(full_src_path);
        return -ENOMEM;
    }

    ret = convert_to_absolute_path(dst_path_arg, full_dst_path, PATH_BUF_SIZE);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to resolve destination path '%s' (error %d)\r\n", dst_path_arg, ret);
        exram_free(full_src_path);
        exram_free(full_dst_path);
        return -1;
    }
    LOGI("Using destination path: %s", full_dst_path);

    *full_src_path_out = full_src_path;
    *full_dst_path_out = full_dst_path;
    return 0;
}

int cp_command(int argc, char *argv[])
{
    int recursive = 0;
    char *src_path = NULL;
    char *dst_path = NULL;
    char *full_src_path = NULL;
    char *full_dst_path = NULL;
    struct stat st;
    int ret = 0;

    Shell *shell = shellGetCurrent();

    if (argc < 3) {
        shellPrint(shell, "Usage: cp [-r] SOURCE DEST\r\n");
        shellPrint(shell, "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\r\n");
        shellPrint(shell, "\r\n");
        shellPrint(shell, "  -r\trecursively copy directories\r\n");
        return -1;
    }

    src_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!src_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        ret = -ENOMEM;
        goto cleanup;
    }
    src_path[0] = '\0';

    dst_path = exram_malloc(4, PATH_BUF_SIZE);
    if (!dst_path) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        ret = -ENOMEM;
        goto cleanup;
    }
    dst_path[0] = '\0';

    ret = parse_cp_arguments(argc, argv, &src_path, &dst_path, &recursive);
    if (ret != 0) {
        shellPrint(shell, "Error: Source and destination paths must be specified\r\n");
        ret = -1;
        goto cleanup;
    }

    ret = prepare_paths(src_path, dst_path, &full_src_path, &full_dst_path, shell);
    if (ret != 0) {
        goto cleanup;
    }

    if (stat(full_src_path, &st) != 0) {
        shellPrint(shell, "Error: Source '%s' does not exist\r\n", full_src_path);
        ret = -1;
        goto cleanup;
    }

    if (S_ISDIR(st.st_mode)) {
        ret = process_directory_copy(full_src_path, full_dst_path, recursive, shell);
    } else {
        ret = process_file_copy(full_src_path, full_dst_path, shell);
    }

cleanup:
    if (src_path) {
        exram_free(src_path);
    }
    if (dst_path) {
        exram_free(dst_path);
    }
    if (full_src_path) {
        exram_free(full_src_path);
    }
    if (full_dst_path) {
        exram_free(full_dst_path);
    }
    return ret;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), cp, cp_command,
                 copy files and directories);
