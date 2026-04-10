/**
 * @file sysheap.h
 * @brief ARCS 系统堆管理接口
 *
 * 基于 ESP heap_caps 框架的系统堆管理组件,为 ARCS 平台提供多种内存堆的分配和管理接口。
 * 支持内部 SRAM、外部 PSRAM (缓存/非缓存) 等多种内存类型的统一管理。
 *
 * @note 本组件在系统启动时自动初始化,应用程序通常不需要直接调用 sysheap_init()
 */

#ifndef __SYSHEAP_H__
#define __SYSHEAP_H__

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sysheap_init 初始化接口
 * @{
 */

/**
 * @brief 初始化系统堆管理
 *
 * 该函数初始化系统的各种堆区域,包括:
 * - 内部 SRAM 堆 (CONFIG_HEAP)
 * - PSRAM 堆 (CONFIG_PSRAM_HEAP)
 * - PSRAM 非缓存堆 (CONFIG_PSRAM_NOCACHE_HEAP)
 *
 * @note 该函数由系统启动流程自动调用,应用程序无需手动调用
 * @note 必须在使用任何堆分配函数之前调用
 */
void sysheap_init(void);

/** @} */

/**
 * @defgroup sysheap_inram 内部 SRAM 堆接口
 * @brief 内部 SRAM 内存分配接口,内存位于芯片内部
 * @{
 */

/**
 * @brief 分配对齐的内部 SRAM 内存
 *
 * @param align 内存对齐字节数,必须是 2 的幂次方 (如 4, 8, 16 等)
 * @param size 要分配的内存大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 *
 * @note 分配的内存位于内部 SRAM,速度快但容量有限
 * @note 使用 inram_free() 释放
 */
void *inram_malloc(size_t align, size_t size);

/**
 * @brief 分配并清零对齐的内部 SRAM 内存
 *
 * @param align 内存对齐字节数,必须是 2 的幂次方
 * @param num 元素个数
 * @param size 每个元素的大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 *
 * @note 分配的内存会被初始化为 0
 * @note 总分配大小为 num * size 字节
 */
void *inram_calloc(size_t align, size_t num, size_t size);

/**
 * @brief 重新分配内部 SRAM 内存
 *
 * @param ptr 原内存指针,可以为 NULL
 * @param size 新的内存大小 (字节)
 * @return void* 成功返回新的内存指针,失败返回 NULL
 *
 * @note 如果 ptr 为 NULL,等同于 inram_malloc(4, size)
 * @note 原内存内容会被保留 (在 min(old_size, size) 范围内)
 */
void *inram_realloc(void *ptr, size_t size);

/**
 * @brief 释放内部 SRAM 内存
 *
 * @param ptr 要释放的内存指针,可以为 NULL
 */
void inram_free(void *ptr);

/** @} */

/**
 * @defgroup sysheap_exram 外部 RAM 堆接口 (兼容接口)
 * @brief 外部 RAM 内存分配接口,内部实现同 psram_xxx 接口
 * @{
 */

/**
 * @brief 分配对齐的外部 RAM 内存
 *
 * @param align 内存对齐字节数,必须是 2 的幂次方
 * @param size 要分配的内存大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 *
 * @note 该接口内部调用 PSRAM 堆,等同于 psram_malloc_align()
 * @note 需要启用 CONFIG_PSRAM_HEAP 配置
 */
void *exram_malloc(size_t align, size_t size);

/**
 * @brief 分配并清零对齐的外部 RAM 内存
 *
 * @param align 内存对齐字节数,必须是 2 的幂次方
 * @param num 元素个数
 * @param size 每个元素的大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 */
void *exram_calloc(size_t align, size_t num, size_t size);

/**
 * @brief 重新分配外部 RAM 内存
 *
 * @param ptr 原内存指针,可以为 NULL
 * @param size 新的内存大小 (字节)
 * @return void* 成功返回新的内存指针,失败返回 NULL
 */
void *exram_realloc(void *ptr, size_t size);

/**
 * @brief 释放外部 RAM 内存
 *
 * @param ptr 要释放的内存指针,可以为 NULL
 */
void exram_free(void *ptr);

/** @} */

/**
 * @defgroup sysheap_psram PSRAM 堆接口
 * @brief PSRAM (外部 SPIRAM) 内存分配接口
 * @note 需要启用 CONFIG_PSRAM_HEAP 配置
 * @{
 */

/**
 * @brief 分配 PSRAM 内存 (4 字节对齐)
 *
 * @param size 要分配的内存大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 *
 * @note 分配的内存位于外部 PSRAM,容量大但速度较内部 SRAM 慢
 * @note 默认使用 4 字节对齐
 * @note 使用 psram_free() 释放
 */
void *psram_malloc(size_t size);

/**
 * @brief 分配指定对齐的 PSRAM 内存
 *
 * @param align 内存对齐字节数,必须是 2 的幂次方 (如 4, 8, 16, 32 等)
 * @param size 要分配的内存大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 *
 * @note 用于需要特定对齐要求的场景,如 DMA 传输 (通常需要 32 字节对齐)
 */
void *psram_malloc_align(size_t align, size_t size);

/**
 * @brief 分配并清零 PSRAM 内存 (4 字节对齐)
 *
 * @param num 元素个数
 * @param size 每个元素的大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 *
 * @note 分配的内存会被初始化为 0
 * @note 总分配大小为 num * size 字节
 */
void *psram_calloc(size_t num, size_t size);

/**
 * @brief 分配并清零指定对齐的 PSRAM 内存
 *
 * @param align 内存对齐字节数,必须是 2 的幂次方
 * @param num 元素个数
 * @param size 每个元素的大小 (字节)
 * @return void* 成功返回分配的内存指针,失败返回 NULL
 */
void *psram_calloc_align(size_t align, size_t num, size_t size);

/**
 * @brief 重新分配 PSRAM 内存
 *
 * @param ptr 原内存指针,可以为 NULL
 * @param size 新的内存大小 (字节)
 * @return void* 成功返回新的内存指针,失败返回 NULL
 *
 * @note 如果 ptr 为 NULL,等同于 psram_malloc(size)
 * @note 原内存内容会被保留 (在 min(old_size, size) 范围内)
 */
void *psram_realloc(void *ptr, size_t size);

/**
 * @brief 释放 PSRAM 内存
 *
 * @param ptr 要释放的内存指针,可以为 NULL
 *
 * @note 该函数也可用于释放通过 exram_xxx 接口分配的内存
 */
void psram_free(void *ptr);

/** @} */

/**
 * @defgroup sysheap_debug 调试接口
 * @{
 */

/**
 * @brief 打印堆内存使用摘要信息
 *
 * 打印所有已注册堆区域的统计信息,包括:
 * - 堆起始/结束地址
 * - 已分配/空闲块数量
 * - 最大空闲块大小
 * - 已分配/空闲字节数
 * - 最小空闲字节数 (历史最低值)
 *
 * @note 输出格式为表格形式,通过 printf 打印到标准输出
 * @note 用于调试内存使用情况和诊断内存泄漏
 *
 * @par 示例输出:
 * @code
 *    [Start]      [End]   [Alloc/BK]    [Free/BK]   [Total/BK] [MaxFree/BK]    [Alloc/B]     [Free/B]  [MinFree/B]
 * 0x20055000 0x2005a000           10            5           15         2048        12288         8192         4096
 * @endcode
 */
void heap_summary_info(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __SYSHEAP_H__ */
