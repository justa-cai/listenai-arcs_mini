/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_REGISTRY_H
#define APP_REGISTRY_H

#include <stdint.h>

/**
 * @brief 应用模块注册表
 *
 * 原理与 SDK 中 LISA_DEVICE_REGISTER / SYS_INIT 相同：
 *   - 各模块在自己的 .c 文件中使用 APP_MODULE_REGISTER() 宏注册
 *   - 链接器将所有注册条目收集到 .app_registry 段
 *   - SORT() 保证按名称排序
 *   - main() 遍历段内所有条目，依次调用 init 函数
 *
 * 优点：添加新模块只需在新文件中调用宏，无需修改已有代码。
 */

typedef struct {
    const char *name;
    int (*init)(void);
} app_module_entry_t;

/* 链接器导出的段起止符号 */
extern const app_module_entry_t __app_registry_start[];
extern const app_module_entry_t __app_registry_end[];

/**
 * @brief 注册一个应用模块
 *
 * @param mod_name  模块名称（C 标识符，不加引号）
 * @param init_fn   初始化函数指针 int (*)(void)
 *
 * 使用示例:
 *   APP_MODULE_REGISTER(led, led_init);
 */
#define APP_MODULE_REGISTER(mod_name, init_fn)                            \
    static const app_module_entry_t __app_module_##mod_name               \
        __attribute__((used, section(".app_registry." #mod_name))) = {    \
            .name = #mod_name,                                            \
            .init = (init_fn),                                            \
    }

#endif /* APP_REGISTRY_H */
