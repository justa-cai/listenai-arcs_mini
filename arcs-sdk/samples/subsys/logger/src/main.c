/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 日志组件使用示例
 *
 * 本示例演示如何使用 LISA Log 组件实现以下功能：
 * 1. 使用不同级别的日志输出（ERROR、WARN、INFO、DEBUG、VERBOSE）
 * 2. 使用带 TAG 的日志宏（LOGE、LOGW、LOGI、LOGD、LOGV）
 * 3. 使用带标签参数的日志宏（LISA_LOGI、LISA_LOGD 等）
 * 4. 输出十六进制数据转储
 * 5. 动态调整日志级别
 */

/* 定义 LOG_TAG (必须在包含 lisa_log.h 之前) */
#define LOG_TAG "logger_sample"

/* 日志头文件 */
#include <lisa_log.h>

/* 标准库头文件 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* 系统头文件 */
#include "FreeRTOS.h"
#include "task.h"

/* ========== 示例1: 基本日志输出 ========== */

/**
 * @brief 演示基本日志输出
 *
 * 使用不同级别的日志宏输出信息
 */
static void demo_basic_logging(void)
{
    LOGI("---------- Basic Logging Demo ----------");

    /* 信息级别日志 */
    LOGI("This is an INFO level log");

    /* 警告级别日志 */
    LOGW("This is a WARNING level log");

    /* 错误级别日志 */
    LOGE("This is an ERROR level log");

    /* 调试级别日志（需要配置 DEBUG 级别才能看到）*/
    LOGD("This is a DEBUG level log (may not visible with INFO level)");

    /* 详细级别日志（需要配置 VERBOSE 级别才能看到）*/
    LOGV("This is a VERBOSE level log (may not visible with INFO level)");

    LOGI("Basic logging demo completed");
}

/* ========== 示例2: 格式化日志输出 ========== */

/**
 * @brief 演示格式化日志输出
 *
 * 使用格式化字符串输出变量信息
 */
static void demo_formatted_logging(void)
{
    LOGI("---------- Formatted Logging Demo ----------");

    /* 输出整数 */
    int temperature = 25;
    LOGI("Current temperature: %d°C", temperature);

    /* 输出浮点数（注意：EasyLogger 可能不完全支持浮点数）*/
    LOGI("Voltage: %d.%02d V", 3, 30);

    /* 输出字符串 */
    const char *device_name = "UART0";
    LOGI("Device name: %s", device_name);

    /* 输出十六进制 */
    uint32_t address = 0x20001000;
    LOGI("Memory address: 0x%08X", address);

    /* 输出指针 */
    void *ptr = (void *)0x20005A100;
    LOGI("Allocated memory at: %p", ptr);

    LOGI("Formatted logging demo completed");
}

/* ========== 示例3: 带标签参数的日志 ========== */

/**
 * @brief 演示带标签参数的日志
 *
 * 使用 LISA_LOG* 系列宏，可以动态指定标签
 */
static void demo_tagged_logging(void)
{
    LOGI("---------- Tagged Logging Demo ----------");

    /* 使用不同的标签 */
    LISA_LOGI("network", "Connection established");
    LISA_LOGI("network", "IP: 192.168.1.100");

    LISA_LOGI("storage", "Filesystem mounted");
    LISA_LOGI("storage", "Free space: 1024 KB");

    LISA_LOGI("sensor", "Temperature sensor initialized");
    LISA_LOGD("sensor", "Reading temperature: %d°C", 25);

    LOGI("Tagged logging demo completed");
}

/* ========== 示例4: 十六进制数据转储 ========== */

/**
 * @brief 演示十六进制数据转储
 *
 * 使用 LOGH 和 LISA_LOGH 宏输出二进制数据
 */
static void demo_hex_dump(void)
{
    LOGI("---------- Hex Dump Demo ----------");

    /* 准备测试数据 */
    uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t data2[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
                       0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00};

    /* 使用基于 TAG 的转储 */
    LOGI("Dumping small data buffer:");
    LOGH("data1", data1, sizeof(data1));

    /* 使用带标签的转储 */
    LOGI("Dumping large data buffer:");
    LISA_LOGH("hex_dump", data2, sizeof(data2), "data2");

    LOGI("Hex dump demo completed");
}

/* ========== 示例5: 动态调整日志级别 ========== */

/**
 * @brief 演示动态调整日志级别
 *
 * 在运行时改变日志输出级别
 */
static void demo_dynamic_log_level(void)
{
    LOGI("---------- Dynamic Log Level Demo ----------");

    /* 输出所有级别的日志（当前级别）*/
    LOGI("Current log level, all logs visible:");
    LOGI("INFO: Application running");
    LOGD("DEBUG: Debug information");
    LOGV("VERBOSE: Very detailed information");

    /* 切换到 WARN 级别 - 只显示警告和错误 */
    LOGI("Switching to WARN level...");
    lisa_log_set_level(LISA_LOG_LEVEL_WARN);

    LOGI("INFO: This log will NOT be visible");
    LOGW("WARN: This warning IS visible");
    LOGE("ERROR: This error IS visible");

    /* 切换到 VERBOSE 级别 - 显示所有日志 */
    lisa_log_set_level(LISA_LOG_LEVEL_VERBOSE);
    LOGI("Switched to VERBOSE level");

    LOGI("INFO: All logs visible now");
    LOGD("DEBUG: Debug information visible");
    LOGV("VERBOSE: Very detailed information visible");

    /* 恢复到 INFO 级别 */
    lisa_log_set_level(LISA_LOG_LEVEL_INFO);
    LOGI("Restored to INFO level");

    LOGI("Dynamic log level demo completed");
}

/* ========== 示例6: 异步日志刷新 ========== */

/**
 * @brief 演示异步日志刷新
 *
 * 在关键操作前刷新日志缓冲区
 */
static void demo_log_flush(void)
{
    LOGI("---------- Log Flush Demo ----------");

    /* 输出多条日志 */
    for (int i = 0; i < 5; i++) {
        LOGI("Log message %d", i + 1);
    }

    /* 在重要操作前刷新日志，确保所有日志已输出 */
    LOGI("Flushing log buffer...");
    log_flush();

    LOGI("Log buffer flushed, all logs have been output");
    LOGI("Log flush demo completed");
}

/* ========== 主函数 ========== */

/**
 * @brief 主函数
 *
 * 依次运行所有日志示例
 */
int main(int argc, char **argv)
{
    LOGI("=== LISA Log Component Example ===");
    LOGI("This example demonstrates various logging features");
    LOGI("");

    /* 等待系统稳定 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 示例 1: 基本日志输出 */
    demo_basic_logging();
    LOGI("");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 示例 2: 格式化日志输出 */
    demo_formatted_logging();
    LOGI("");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 示例 3: 带标签参数的日志 */
    demo_tagged_logging();
    LOGI("");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 示例 4: 十六进制数据转储 */
    demo_hex_dump();
    LOGI("");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 示例 5: 动态调整日志级别 */
    demo_dynamic_log_level();
    LOGI("");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 示例 6: 异步日志刷新 */
    demo_log_flush();
    LOGI("");

    LOGI("=== All examples completed ===");
    LOGI("System will keep running...");

    /* 进入主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
