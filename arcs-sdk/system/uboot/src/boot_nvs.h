#ifndef __BOOT_NVS_H__
#define __BOOT_NVS_H__

#include <stdint.h>
#include <stddef.h>

#define NVS_SECTOR_SIZE     (0x1000)    /* 4KB */
#define NVS_FLASH_BASE      (0x30000000)

/*
 * 初始化NVS设备
 */
int boot_nvs_init(void);

/*
 * 读取数据
 * @param dst 目标地址
 * @param src 源地址 (flash地址)
 * @param len 长度
 */
void boot_nvs_read(void *dst, const void *src, size_t len);

/*
 * 擦除 (按sector对齐)
 * @param addr flash地址
 * @param size 大小
 * @return 0成功，其他失败
 */
int boot_nvs_erase(uint32_t addr, size_t size);

/*
 * 写入 (不带擦除，需先擦除)
 * @param addr flash地址
 * @param data 数据
 * @param size 大小
 * @return 0成功，其他失败
 */
int boot_nvs_write(uint32_t addr, const void *data, size_t size);

/*
 * 擦除并写入
 * @param addr flash地址
 * @param data 数据
 * @param size 大小
 * @return 0成功，其他失败
 */
int boot_nvs_erase_write(uint32_t addr, const void *data, size_t size);

#endif /* __BOOT_NVS_H__ */

