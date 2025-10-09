/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_log.h"
#include "shell.h"
#include "lvfs.h"
#include "sysheap.h"
#include "util/utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define PATH_BUF_SIZE 512

static void print_entry(Shell *shell, struct dirent *entry, int show_details, const char *path);

static int parse_ls_arguments(int argc, char *argv[], char *path, int *show_details)
{
    path[0] = '.';
    path[1] = '\0';
    *show_details = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'l') != NULL) {
                *show_details = 1;
            }
        } else {
            strncpy(path, argv[i], PATH_BUF_SIZE - 1);
            path[PATH_BUF_SIZE - 1] = '\0';
        }
    }

    return 0;
}

static int display_directory_contents(const char *path, int show_details, Shell *shell)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        shellPrint(shell, "Error: Cannot open directory '%s'\r\n", path);
        return -1;
    }

    struct dirent *entry;

    if (show_details) {

#if CONFIG_SIMPLE_BOX_LS_SHOW_TIME
        shellPrint(shell, "%-10s %-12s %-20s %s\r\n", "Size", "Type", "Last Modified", "Name");
        shellPrint(shell, "--------------------------------------------------------------\r\n");
#else
        shellPrint(shell, "%-10s %-12s %s\r\n", "Size", "Type", "Name");
        shellPrint(shell, "-------------------------------------------\r\n");
#endif
    }

    while ((entry = readdir(dir)) != NULL) {
        print_entry(shell, entry, show_details, path);
    }

    if (!show_details) {
        shellPrint(shell, "\r\n");
    }

    closedir(dir);
    return 0;
}

static void print_entry(Shell *shell, struct dirent *entry, int show_details, const char *path)
{
    if (!show_details) {

        shellPrint(shell, "%s  ", entry->d_name);
        return;
    }


    struct stat st;
    char *full_path = exram_malloc(4, PATH_BUF_SIZE);


    snprintf(full_path, PATH_BUF_SIZE, "%s/%s", path, entry->d_name);

    if (stat(full_path, &st) != 0) {

        exram_free(full_path);
        shellPrint(shell, "%-10s %-12s %s\r\n", "?", "?", entry->d_name);
        return;
    }

    exram_free(full_path);


    char type_str[13] = "Unknown";
    if (S_ISREG(st.st_mode)) {
        strcpy(type_str, "File");
    } else if (S_ISDIR(st.st_mode)) {
        strcpy(type_str, "Directory");
    } else if (S_ISLNK(st.st_mode)) {
        strcpy(type_str, "Link");
    }


    char size_str[11];
    snprintf(size_str, sizeof(size_str), "%lu", (unsigned long)st.st_size);

#if CONFIG_SIMPLE_BOX_LS_SHOW_TIME

    char time_str[21];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    shellPrint(shell, "%-10s %-12s %-20s %s\r\n", size_str, type_str, time_str, entry->d_name);
#else

    shellPrint(shell, "%-10s %-12s %s\r\n", size_str, type_str, entry->d_name);
#endif
}

int ls_command(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    int show_details = 0;
    int ret;


    char *path = exram_malloc(4, PATH_BUF_SIZE);
    if (path == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory for path\r\n");
        return -ENOMEM;
    }


    ret = parse_ls_arguments(argc, argv, path, &show_details);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to parse arguments\r\n");
        exram_free(path);
        return ret;
    }


    char *full_path = exram_malloc(4, PATH_BUF_SIZE);
    if (full_path == NULL) {
        shellPrint(shell, "Error: Failed to allocate memory for path conversion\r\n");
        exram_free(path);
        return -ENOMEM;
    }

    ret = convert_to_absolute_path(path, full_path, PATH_BUF_SIZE);
    if (ret != 0) {
        shellPrint(shell, "Error: Failed to convert path to absolute path, error: %d\r\n", ret);
        exram_free(full_path);
        exram_free(path);
        return ret;
    }


    strncpy(path, full_path, PATH_BUF_SIZE - 1);
    path[PATH_BUF_SIZE - 1] = '\0';
    LOGI("ls path: %s", path);

    exram_free(full_path);


    ret = display_directory_contents(path, show_details, shell);


    exram_free(path);

    return ret;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ls,
                 ls_command, list directory contents);
