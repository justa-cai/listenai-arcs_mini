/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 编译时注册表模式演示
 *
 * 演示基于自定义链接器段的"编译时注册、运行时遍历"模式。
 * 这与 SDK 中 LISA_DEVICE_REGISTER / SYS_INIT 的实现原理相同。
 *
 * 核心机制：
 *   1. 各模块在自己的 .c 文件中用 APP_MODULE_REGISTER() 宏注册
 *   2. 链接器 SORT() 将 .app_registry.* 段按名称排序收集
 *   3. main() 通过 __app_registry_start / __app_registry_end 遍历所有条目
 *
 * 添加新模块只需：新建 .c 文件 + APP_MODULE_REGISTER() + 加入 CMakeLists.txt，
 * 无需修改 main.c 或任何已有代码。
 */

#define LOG_TAG "registry"
#include <lisa_log.h>

#include <stdio.h>
#include "app_registry.h"

int main(int argc, char **argv)
{
    LOGI("=== App Registry Demo ===");

    /* 遍历链接器段中的所有注册模块 */
    const app_module_entry_t *entry;
    int count = 0;

    for (entry = __app_registry_start; entry < __app_registry_end; entry++) {
        LOGI("Init module[%d]: %s", count, entry->name);
        int ret = entry->init();
        if (ret != 0) {
            LOGE("  module %s init failed: %d", entry->name, ret);
        }
        count++;
    }

    LOGI("All %d modules initialized", count);

    return 0;
}
