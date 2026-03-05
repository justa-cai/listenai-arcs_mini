/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ACOMP Logger 使用示例
 *
 * 展示如何在 Master 端启用和使用日志传输功能。
 * Logger 组件通过 R2M 流从 Remote (AP) 端接收日志并输出到 Master 端。
 */

#include "acomp_logger.h"

#define TAG "logger_example"
#include "lisa_log.h"

/**
 * @brief 基础使用示例
 *
 * 这是最简单的使用方式，只需两步：init 和 start
 *
 * acomp_logger_start() 会自动完成：
 * - 根据 Kconfig 配置 R2M 流通道（缓冲区大小和数量）
 * - 启动 Remote 端的日志捕获
 * - 创建接收线程（根据触发策略工作）
 * - 自动接收并输出日志
 */
int acomp_logger_example_init(void)
{
    int ret;

    /* 步骤 1: 初始化 logger 组件 */
    ret = acomp_logger_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Failed to initialize logger: %d", ret);
        return ret;
    }

    /* 步骤 2: 启动 logger 组件 */
    ret = acomp_logger_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Failed to start logger: %d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Logger started successfully!");
    LISA_LOGI(TAG, "Remote logs will now be displayed automatically");

    return ACOMP_ERR_OK;
}

/**
 * @brief 停止日志传输
 *
 * 停止 logger 组件会自动：
 * - 停止接收线程
 * - 停止 Remote 端的日志捕获
 * - 禁用并清理 R2M 流通道
 */
int acomp_logger_example_deinit(void)
{
    int ret;

    /* 停止 logger 组件 */
    ret = acomp_logger_stop();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGW(TAG, "Failed to stop logger: %d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Logger stopped successfully");

    return ACOMP_ERR_OK;
}

/**
 * @brief 完整示例：初始化、运行、停止
 *
 * 这个示例展示了 logger 组件的完整生命周期
 */
int acomp_logger_example_full(void)
{
    int ret;

    LISA_LOGI(TAG, "=== ACOMP Logger Full Example ===");

    /* 初始化 */
    ret = acomp_logger_example_init();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    /*
     * 应用程序主循环
     * 在这期间，Remote 端的日志会自动显示
     */
    LISA_LOGI(TAG, "Logger is running...");
    LISA_LOGI(TAG, "You should now see Remote logs appearing");

    /* 模拟运行一段时间 */
    for (int i = 0; i < 10; i++) {
        LISA_LOGI(TAG, "Main loop iteration %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 停止 logger */
    LISA_LOGI(TAG, "Stopping logger...");
    ret = acomp_logger_example_deinit();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    LISA_LOGI(TAG, "=== Example completed ===");

    return ACOMP_ERR_OK;
}

/**
 * @brief 错误处理示例
 *
 * 展示如何处理 logger API 的错误返回值
 */
int acomp_logger_example_with_error_handling(void)
{
    int ret;

    /* 初始化 logger */
    ret = acomp_logger_init();
    switch (ret) {
        case ACOMP_ERR_OK:
            LISA_LOGI(TAG, "Logger initialized");
            break;
        case ACOMP_ERR_NO_MEM:
            LISA_LOGE(TAG, "Initialization failed: Out of memory");
            return ret;
        case ACOMP_ERR_INVALID_STATE:
            LISA_LOGE(TAG, "Initialization failed: Invalid state");
            return ret;
        case ACOMP_ERR_NOT_FOUND:
            LISA_LOGE(TAG, "Initialization failed: Logger device not found");
            return ret;
        default:
            LISA_LOGE(TAG, "Initialization failed: Unknown error %d", ret);
            return ret;
    }

    /* 启动 logger */
    ret = acomp_logger_start();
    switch (ret) {
        case ACOMP_ERR_OK:
            LISA_LOGI(TAG, "Logger started");
            break;
        case ACOMP_ERR_NO_MEM:
            LISA_LOGE(TAG, "Start failed: Out of memory");
            goto cleanup;
        case ACOMP_ERR_INVALID_STATE:
            LISA_LOGE(TAG, "Start failed: Invalid state (already started?)");
            goto cleanup;
        default:
            LISA_LOGE(TAG, "Start failed: Unknown error %d", ret);
            goto cleanup;
    }

    LISA_LOGI(TAG, "Logger running normally");

    /* 应用程序运行... */
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* 停止 logger */
    ret = acomp_logger_stop();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGW(TAG, "Stop returned error: %d (continuing anyway)", ret);
    }

    return ACOMP_ERR_OK;

cleanup:
    /* 清理资源 */
    acomp_logger_stop();
    return ret;
}

/**
 * @brief 自定义日志输出回调示例
 *
 * 展示如何使用自定义回调函数来控制 AP 日志的输出方式
 */

/* 自定义日志输出函数 - 输出到 printf */
static int custom_log_output_printf(const uint8_t *log, uint32_t len)
{
    printf("[AP] %.*s", len, log);
    return 0;
}

/* 自定义日志输出函数 - 带颜色高亮 */
static int custom_log_output_with_color(const uint8_t *log, uint32_t len)
{
    /* 使用 ANSI 颜色码：青色背景 + 黑色文字 */
    printf("\033[46m\033[30m[AP]\033[0m %.*s", len, log);
    return 0;
}

/* 自定义日志输出函数 - 写入文件 */
static int custom_log_output_to_file(const uint8_t *log, uint32_t len)
{
    /* 这里可以实现写入文件的逻辑 */
    /* 例如: fwrite(log, 1, len, log_file); */
    LISA_LOGI(TAG, "[AP->FILE] %.*s", len, log);
    return 0;
}

int acomp_logger_example_with_custom_output(void)
{
    int ret;

    LISA_LOGI(TAG, "=== Custom Output Callback Example ===");

    /* 初始化 logger */
    ret = acomp_logger_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Logger init failed: %d", ret);
        return ret;
    }

    /* 方式 1: 使用默认输出 */
    LISA_LOGI(TAG, "Using default output...");
    ret = acomp_logger_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Logger start failed: %d", ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 方式 2: 切换到自定义 printf 输出 */
    LISA_LOGI(TAG, "Switching to custom printf output...");
    acomp_logger_set_output_callback(custom_log_output_printf);

    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 方式 3: 切换到带颜色的输出 */
    LISA_LOGI(TAG, "Switching to colored output...");
    acomp_logger_set_output_callback(custom_log_output_with_color);

    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 方式 4: 切换到文件输出 */
    LISA_LOGI(TAG, "Switching to file output...");
    acomp_logger_set_output_callback(custom_log_output_to_file);

    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 恢复默认输出 */
    LISA_LOGI(TAG, "Restoring default output...");
    acomp_logger_set_output_callback(NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    /* 停止 logger */
    ret = acomp_logger_stop();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGW(TAG, "Logger stop failed: %d", ret);
    }

    LISA_LOGI(TAG, "=== Example completed ===");

    return ACOMP_ERR_OK;
}
