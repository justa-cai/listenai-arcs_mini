/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief C 代码段属性宏演示
 *
 * 演示在 C 代码中使用 SDK 提供的段属性宏，
 * 将函数和变量放入指定内存区域，无需修改链接脚本。
 *
 * 宏定义来源：
 *   - arcs_ap.h:  _FAST_TEXT / _FAST_DATA / _FAST_BSS
 *   - sysutils.h: __psram_data__ / __psram_bss__ / __noinit__ / __itcm_text__ 等
 */

#define LOG_TAG "section"
#include <lisa_log.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arcs_ap.h>
#include <sysutils.h>

/* -----------------------------------------------------------------------
 * _FAST_TEXT: 函数放入 SRAM .fast_text 段
 *
 * 适用场景：中断处理、音频回调等对延迟敏感的函数。
 * 从 SRAM 执行无需等待 Flash 读取周期。
 * ----------------------------------------------------------------------- */
_FAST_TEXT
static uint32_t fast_crc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* -----------------------------------------------------------------------
 * _FAST_DATA: 已初始化数据放入 SRAM .fast_data 段
 *
 * 适用场景：频繁查表的小型查找表。
 * ----------------------------------------------------------------------- */
_FAST_DATA
static const uint8_t lookup_table[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
};

/* -----------------------------------------------------------------------
 * __psram_bss__: 大体积零初始化数据放入 PSRAM
 *
 * 适用场景：图像帧缓冲、音频缓冲等大数组，避免占用宝贵的 SRAM。
 * ----------------------------------------------------------------------- */
__psram_bss__ static uint8_t frame_buffer[32 * 1024];

/* -----------------------------------------------------------------------
 * __noinit__: 不初始化段，复位后保留上次的值
 *
 * 适用场景：崩溃计数器、复位原因记录。
 * 注意：首次上电值为随机值，需在应用中做有效性判断。
 * ----------------------------------------------------------------------- */
__noinit__ static uint32_t boot_count;

/* -----------------------------------------------------------------------
 * __psram_data__: 大型已初始化数据放入 PSRAM
 *
 * 适用场景：大型字体表、常量配置表等。
 * ----------------------------------------------------------------------- */
__psram_data__ static const char welcome_msg[] = "Hello from PSRAM!";

int main(int argc, char **argv)
{
    LOGI("=== Section Attribute Demo ===");

    /* _FAST_TEXT: SRAM 快速函数 */
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc = fast_crc(test_data, sizeof(test_data));
    LOGI("_FAST_TEXT   fast_crc=%p, result=0x%08lX", fast_crc, (unsigned long)crc);

    /* _FAST_DATA: SRAM 快速数据 */
    LOGI("_FAST_DATA   lookup_table=%p, [0]=0x%02X", lookup_table, lookup_table[0]);

    /* __psram_bss__: PSRAM 大缓冲区 */
    memset(frame_buffer, 0x55, sizeof(frame_buffer));
    LOGI("__psram_bss__  frame_buffer=%p, size=%u", frame_buffer, (unsigned)sizeof(frame_buffer));

    /* __noinit__: 复位保留 */
    LOGI("__noinit__   boot_count=%lu (retained across soft reset)", (unsigned long)boot_count);
    boot_count++;

    /* __psram_data__: PSRAM 已初始化数据 */
    LOGI("__psram_data__ welcome_msg=%p, \"%s\"", welcome_msg, welcome_msg);

    /*
     * 验证方法：编译后查看 lst 文件确认各符号地址
     *   - SRAM 地址范围:       0x2001xxxx
     *   - PSRAM 数据地址范围:  0x2800xxxx
     *   - PSRAM 代码地址范围:  0x3000xxxx
     *
     *   grep -E "fast_crc|frame_buffer|boot_count" build/arcs.lst
     */

    return 0;
}
