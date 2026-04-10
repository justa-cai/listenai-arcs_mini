/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief listenai_code_relocate() 代码重定位演示
 *
 * 演示通过 CMake API 将代码/数据从 Flash 重定位到 SRAM 执行，
 * 无需修改 C 代码，只需在 CMakeLists.txt 中声明。
 *
 * 三种重定位方式：
 *   - FILES:    按源文件重定位（fast_math.c 整体搬入 SRAM）
 *   - SECTIONS: 按段名重定位（.text.irq_handler 搬入 SRAM）
 *   - LIBRARY:  按库重定位（整个第三方库搬入 PSRAM）
 */

#define LOG_TAG "relocate"
#include <lisa_log.h>

#include <stdio.h>
#include <stdint.h>

/*
 * 按段名重定位示例：
 * 将此函数放入 .text.irq_handler 段，
 * CMakeLists.txt 中配置了 listenai_code_relocate(SECTIONS .text.irq_handler LOCATION SRAM_TEXT)
 * 构建后此函数将在 SRAM 中执行，而非 Flash。
 */
__attribute__((section(".text.irq_handler")))
void simulated_irq_handler(void)
{
    /* SRAM 执行无 Flash 等待周期，中断响应更快 */
    static volatile uint32_t irq_count = 0;
    irq_count++;
}

/*
 * 普通函数：默认在 Flash 中执行（XIP）。
 * 与 simulated_irq_handler 对比运行地址的差异。
 */
void normal_function(void)
{
    LOGI("normal_function addr: %p (Flash/XIP)", normal_function);
}

int main(int argc, char **argv)
{
    LOGI("=== Code Relocate Demo ===");

    normal_function();
    LOGI("simulated_irq_handler addr: %p (SRAM)", simulated_irq_handler);

    /* 调用中断处理函数验证可执行 */
    simulated_irq_handler();
    LOGI("IRQ handler executed successfully from SRAM");

    /*
     * 验证方法：编译后查看 lst 文件确认函数地址
     *   - 默认 .text 地址范围: 0x3000xxxx（PSRAM）
     *   - SRAM 重定位后范围:   0x2001xxxx
     *
     *   grep simulated_irq build/arcs.lst
     */

    return 0;
}
