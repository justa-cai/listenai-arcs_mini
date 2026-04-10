/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Shell 使用示例
 *
 * 本示例演示如何使用 LISA Shell 组件创建各种类型的命令：
 * 1. 简单命令 - 不带参数的基础命令
 * 2. 带参数命令 - 处理用户输入参数
 * 3. 可选参数 - 必选参数+可选参数的组合
 * 4. 命令组 - 组织多个相关子命令
 * 5. 命令选项 - 处理命令行选项参数
 */
#define LOG_TAG "shell_sample"
#include <lisa_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "shell.h"
#include "lisa_shell.h"
#include "FreeRTOS.h"
#include "task.h"

/* ========== 示例1: 简单命令 - 不带参数 ========== */

/**
 * @brief 显示版本信息命令
 *
 * 演示: 如何实现一个简单的无参数命令
 */
static int cmd_version(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    shellPrint(shell, "ARCS SDK Version: 1.0.0\n");
    shellPrint(shell, "Build Date: %s %s\n", __DATE__, __TIME__);

    LISA_LOGI(LOG_TAG, "Display version information\n");
    return 0;
}

/* 注册 version 命令 */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    version,
    cmd_version,
    show system version
);

/* ========== 示例2: 带参数的命令 ========== */

/**
 * @brief echo 命令 - 回显输入的参数
 *
 * 演示: 如何处理命令参数
 * @param argc 参数个数
 * @param argv 参数数组,argv[0]是第一个参数
 */
static int cmd_echo(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    /* 检查参数 */
    if (argc < 1) {
        shellPrint(shell, "Usage: echo <message>\n");
        return -1;
    }

    /* 输出所有参数 */
    for (int i = 0; i < argc; i++) {
        shellPrint(shell, "%s ", argv[i]);
    }
    shellPrint(shell, "\n");

    LISA_LOGI(LOG_TAG, "Echo %d arguments\n", argc);
    return 0;
}

/* 注册 echo 命令 */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    echo,
    cmd_echo,
    echo the input message
);

/* ========== 示例3: 带可选参数的命令 ========== */

/**
 * @brief 内存读取命令
 *
 * 演示: 如何处理必选参数和可选参数
 * @param argc 参数个数
 * @param argv argv[0]=地址(必选), argv[1]=长度(可选,默认64)
 */
static int cmd_memread(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    uint32_t addr;
    uint32_t len = 64;  /* 默认长度 */
    uint8_t *ptr;

    /* 检查必选参数 */
    if (argc < 1) {
        shellPrint(shell, "Usage: memread <address> [length]\n");
        shellPrint(shell, "  address : memory address (hex format supported)\n");
        shellPrint(shell, "  length  : bytes to read (optional, default 64)\n");
        return -1;
    }

    /* 解析地址(支持十进制和十六进制) */
    addr = strtoul(argv[0], NULL, 0);

    /* 解析可选的长度参数 */
    if (argc >= 2) {
        len = strtoul(argv[1], NULL, 0);
        if (len == 0 || len > 256) {
            shellPrint(shell, "Error: length must be 1-256\n");
            LISA_LOGE(LOG_TAG, "Invalid length parameter: %u\n", len);
            return -1;
        }
    }

    ptr = (uint8_t *)addr;

    /* 显示内存内容 */
    shellPrint(shell, "\nMemory at 0x%08X (%u bytes):\n", addr, len);
    shellPrint(shell, "         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    shellPrint(shell, "-------------------------------------------------------\n");

    for (uint32_t i = 0; i < len; i += 16) {
        shellPrint(shell, "%08X:", addr + i);
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            shellPrint(shell, " %02X", ptr[i + j]);
        }
        shellPrint(shell, "\n");
    }
    shellPrint(shell, "\n");

    LISA_LOGI(LOG_TAG, "Read %u bytes from 0x%08X\n", len, addr);
    return 0;
}

/* 注册 memread 命令 */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    memread,
    cmd_memread,
    read memory content
);

/* ========== 示例4: 命令组 - 组织相关命令 ========== */

/* 定义命令表结构 */
struct shell_cmd_entry {
    const char *name;                        /* 子命令名称 */
    int (*handler)(int argc, char **argv);   /* 子命令处理函数 */
    const char *help;                        /* 子命令帮助信息 */
};

/**
 * @brief info 命令组的子命令: version
 *
 * 演示: 命令组中的子命令实现
 */
static int subcmd_info_version(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    shellPrint(shell, "Version: 1.0.0\n");
    LISA_LOGI(LOG_TAG, "Display version via info command\n");
    return 0;
}

/**
 * @brief info 命令组的子命令: tasks
 *
 * 演示: 显示 FreeRTOS 任务列表
 */
static int subcmd_info_tasks(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    char *buffer;

    /* 分配缓冲区 */
    buffer = pvPortMalloc(1024);
    if (buffer == NULL) {
        shellPrint(shell, "Error: out of memory\n");
        LISA_LOGE(LOG_TAG, "Failed to allocate memory for task list\n");
        return -1;
    }

    /* 获取任务列表 */
    shellPrint(shell, "\nTask List:\n");
    shellPrint(shell, "Name                State  Pri  Stack  Num\n");
    shellPrint(shell, "---------------------------------------------\n");
    vTaskList(buffer);
    shellPrint(shell, "%s", buffer);
    shellPrint(shell, "---------------------------------------------\n");

    vPortFree(buffer);
    LISA_LOGI(LOG_TAG, "Display task list\n");
    return 0;
}

/* info 命令组的子命令表 */
static const struct shell_cmd_entry info_cmds[] = {
    {"version", subcmd_info_version, "Show version"},
    {"tasks",   subcmd_info_tasks,   "Show task list"},
};

/**
 * @brief 显示 info 命令组的帮助信息
 */
static void info_show_help(void)
{
    Shell *shell = shellGetCurrent();
    int count = sizeof(info_cmds) / sizeof(info_cmds[0]);

    shellPrint(shell, "\nUsage: info <subcommand>\n");
    shellPrint(shell, "\nAvailable subcommands:\n");
    for (int i = 0; i < count; i++) {
        shellPrint(shell, "  %-12s - %s\n", info_cmds[i].name, info_cmds[i].help);
    }
    shellPrint(shell, "\n");
}

/**
 * @brief info 命令组处理函数
 *
 * 演示: 如何实现命令组,组织多个相关子命令
 * @param argc 参数个数(包括命令名)
 * @param argv argv[0]是命令名, argv[1]是子命令名
 */
static int cmd_info_handler(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    int count = sizeof(info_cmds) / sizeof(info_cmds[0]);

    /* 没有子命令,显示帮助 */
    if (argc == 1) {
        info_show_help();
        return 0;
    }

    /* 查找并执行子命令 */
    const char *subcmd = argv[1];
    for (int i = 0; i < count; i++) {
        if (strcmp(info_cmds[i].name, subcmd) == 0) {
            /* 找到子命令,执行它 */
            /* argc-2: 去除命令名和子命令名 */
            /* argv+2: 剩余的参数 */
            return info_cmds[i].handler(argc - 2, argv + 2);
        }
    }

    /* 未找到子命令 */
    shellPrint(shell, "Error: unknown subcommand '%s'\n", subcmd);
    info_show_help();
    return -1;
}

/* 注册 info 命令组 */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    info,
    cmd_info_handler,
    system information commands
);

/* ========== 示例5: 带选项的命令 ========== */

/**
 * @brief list 命令 - 演示如何处理命令选项
 *
 * 使用示例:
 *   list          - 简单列表
 *   list -l       - 详细列表
 *   list -a       - 显示所有(包括隐藏项)
 *   list -la      - 详细列表+显示所有
 */
static int cmd_list(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    int verbose = 0;   /* -l 选项 */
    int show_all = 0;  /* -a 选项 */

    /* 解析选项 */
    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'l') {
                    verbose = 1;
                } else if (argv[i][j] == 'a') {
                    show_all = 1;
                } else {
                    shellPrint(shell, "Unknown option: -%c\n", argv[i][j]);
                    return -1;
                }
            }
        }
    }

    /* 根据选项执行不同的显示 */
    shellPrint(shell, "\nFile List:\n");
    if (verbose) {
        shellPrint(shell, "%-20s %10s %s\n", "Name", "Size", "Date");
        shellPrint(shell, "------------------------------------------\n");
        shellPrint(shell, "%-20s %10s %s\n", "main.c", "1234", "2025-01-15");
        shellPrint(shell, "%-20s %10s %s\n", "shell.c", "5678", "2025-01-14");
        if (show_all) {
            shellPrint(shell, "%-20s %10s %s\n", ".config", "256", "2025-01-13");
        }
    } else {
        shellPrint(shell, "main.c  shell.c");
        if (show_all) {
            shellPrint(shell, "  .config");
        }
        shellPrint(shell, "\n");
    }
    shellPrint(shell, "\n");

    LISA_LOGI(LOG_TAG, "List files (verbose=%d, all=%d)\n", verbose, show_all);
    return 0;
}

/* 注册 list 命令 */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    list,
    cmd_list,
    list files (-l: verbose, -a: all)
);

/* ========== 应用程序主函数 ========== */

/**
 * @brief 应用程序入口
 */
int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA Shell Usage Sample ===\n");

    /* 初始化 LISA Shell */
    int ret = lisa_shell_init();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize shell (error: %d)\n", ret);
        return ret;
    }
    LISA_LOGI(LOG_TAG, "Shell initialized successfully\n");

    LISA_LOGI(LOG_TAG, "This sample demonstrates:\n");
    LISA_LOGI(LOG_TAG, "  1. Simple command (version)\n");
    LISA_LOGI(LOG_TAG, "  2. Command with parameters (echo)\n");
    LISA_LOGI(LOG_TAG, "  3. Optional parameters (memread)\n");
    LISA_LOGI(LOG_TAG, "  4. Command group (info)\n");
    LISA_LOGI(LOG_TAG, "  5. Command options (list)\n");
    LISA_LOGI(LOG_TAG, "Type 'help' in shell to see all commands\n\n");

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return 0;
}
