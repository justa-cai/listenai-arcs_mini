/*
 * CRC32计算函数实现
 * 
 * 提供crc32_calc()函数，兼容老项目接口
 */

#include "boot_crc32.h"

extern uint32_t crc32_sw(uint32_t val, const uint8_t *buf, size_t len);

uint32_t crc32(uint32_t val, const uint8_t *buf, size_t len)
{
    return crc32_sw(val, buf, len);
}

/*
 * CRC32计算 (兼容老项目接口)
 * @param data 数据指针
 * @param size 数据大小
 * @param last 上次计算结果 (首次为0)
 * @return CRC32值
 */
uint32_t crc32_calc(const void *data, uint32_t size, uint32_t last)
{
    return crc32(last, (const uint8_t *)data, size);
}
