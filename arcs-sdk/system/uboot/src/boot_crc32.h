#ifndef __BOOT_CRC32_H__
#define __BOOT_CRC32_H__

#include <stdint.h>
#include <stddef.h>

/*
 * CRC32计算函数
 * 
 * 新芯片已有crc32()函数，这里提供crc32_calc()适配器
 * 保持与老项目接口兼容
 */

/* 底层crc32函数 (来自 hal/chip/arcs/driver/ota/crc32_sw.c) */
extern uint32_t crc32(uint32_t val, const uint8_t *buf, size_t len);

/*
 * CRC32计算 (兼容老项目接口)
 * @param data 数据指针
 * @param size 数据大小
 * @param last 上次计算结果 (首次为0)
 * @return CRC32值
 * 
 * 注意：这是一个真正的函数（非内联），因为多个文件需要链接它
 */
uint32_t crc32_calc(const void *data, uint32_t size, uint32_t last);

#endif /* __BOOT_CRC32_H__ */

