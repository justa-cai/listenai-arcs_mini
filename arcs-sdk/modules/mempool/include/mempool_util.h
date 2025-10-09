#ifndef _LISTENAI_MEMORYPOOL_UTIL_H_
#define _LISTENAI_MEMORYPOOL_UTIL_H_

#include <stdint.h>

/**
 * @brief 内存池初始化
 * @note 只可初始化一次, 多次初始化无效
 * @param pool_size 内存池大小, 最大为其2倍
 * @return 0 初始化成功
 * @return -1 初始化失败
 * @return 1 已经初始化
 */
int mempool_init(uint32_t pool_size);

/**
 * @brief 内存池逆初始化
 * @note 初始化多少次就要调用多少次逆初始化, 全部调用后才会真正逆初始化
 */
void mempool_uninit();

/**
 * @brief 从内存池申请内存
 * @param size 待申请内存大小
 * @return 申请的内存地址
 */
void *mempool_malloc(uint32_t size);

/**
 * @brief 释放从内存池申请的内存
 * @param addr 内存地址
 */
void mempool_free(void *addr);

/**
 * @brief 内存池使用情况
 * @return 内存池使用大小
 */
unsigned long long mempool_usage();

#endif