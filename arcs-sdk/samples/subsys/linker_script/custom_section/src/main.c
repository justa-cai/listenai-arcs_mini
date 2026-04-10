/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 自定义段注入 + 散加载演示
 *
 * 演示如何通过 listenai_add_linker_section() 注入自定义段，
 * 并通过 listenai_add_linker_scatter() 注册启动时的数据复制/清零。
 *
 * 流程：
 *   1. .ld 文件定义 .app_config 段（>RAM AT>ROM）
 *   2. CMake 注册段到 ROM slot + 注册 SCATLOAD
 *   3. C 代码中将数据放入该段
 *   4. 启动代码自动将数据从 ROM 复制到 RAM
 *   5. main() 中通过链接器符号遍历段内容
 */

#define LOG_TAG "section"
#include <lisa_log.h>

#include <stdio.h>
#include <stdint.h>

/* 配置项结构 */
typedef struct {
    const char *key;
    int32_t     value;
} app_config_entry_t;

/* 链接器导出的段起止符号 */
extern const app_config_entry_t __app_config_start[];
extern const app_config_entry_t __app_config_end[];

/* 将配置项放入 .app_config 段 */
#define APP_CONFIG_DEFINE(cfg_key, cfg_value)                              \
    static const app_config_entry_t __app_cfg_##cfg_key                   \
        __attribute__((used, section(".app_config." #cfg_key))) = {       \
            .key   = #cfg_key,                                            \
            .value = (cfg_value),                                         \
    }

/* 定义几个配置项 */
APP_CONFIG_DEFINE(baud_rate,   115200);
APP_CONFIG_DEFINE(retry_count, 3);
APP_CONFIG_DEFINE(timeout_ms,  5000);

int main(int argc, char **argv)
{
    LOGI("=== Custom Section Demo ===");

    /* 遍历 .app_config 段中的所有配置项 */
    const app_config_entry_t *entry;
    int count = 0;

    for (entry = __app_config_start; entry < __app_config_end; entry++) {
        LOGI("  config[%d]: %s = %ld", count++, entry->key, (long)entry->value);
    }

    LOGI("Total config entries: %d", count);

    /*
     * 验证方法：
     *   grep app_config build/linker.ld  # 确认最终链接脚本包含段定义和散加载条目
     */

    return 0;
}
