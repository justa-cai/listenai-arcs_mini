/**
 * @file mock_mem_ops.c
 * @brief WiFi管理器测试中使用的内存操作实现
 */
#include "mock_mem_ops.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

DEFINE_FAKE_VALUE_FUNC(void *, mock_malloc, size_t);
DEFINE_FAKE_VALUE_FUNC(void *, mock_calloc, size_t, size_t);
DEFINE_FAKE_VALUE_FUNC(void *, mock_align_malloc, size_t, size_t);
DEFINE_FAKE_VALUE_FUNC(void *, mock_nocache_malloc, size_t);
DEFINE_FAKE_VOID_FUNC(mock_free, void *);

static wifi_manager_mem_ops_t mem_ops = {
    .malloc = mock_malloc,
    .calloc = mock_calloc,
    .align_malloc = mock_align_malloc,
    .nocache_malloc = mock_nocache_malloc,
    .free = mock_free
};

void mock_mem_ops_reset(void)
{
    mock_mem_ops_init();
}

/**
 * @brief 测试用的内存分配函数
 */
static void* custom_mock_malloc(size_t size) {
    return malloc(size);
}

/**
 * @brief 测试用的内存清零分配函数
 */
static void* custom_mock_calloc(size_t nmemb, size_t size) {
    void *ptr = malloc(nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

/**
 * @brief 测试用的对齐内存分配函数
 */
static void* custom_mock_align_malloc(size_t align, size_t size) {
    // 简单实现，不考虑对齐
    return malloc(size);
}

/**
 * @brief 测试用的无缓存内存分配函数
 */
static void* custom_mock_nocache_malloc(size_t size) {
    return malloc(size);
}

/**
 * @brief 测试用的内存释放函数
 */
static void custom_mock_free(void* ptr) {
    free(ptr);
}

void mock_mem_ops_init(void)
{
    RESET_FAKE(mock_malloc);
    RESET_FAKE(mock_calloc);
    RESET_FAKE(mock_align_malloc);
    RESET_FAKE(mock_nocache_malloc);
    RESET_FAKE(mock_free);

    mock_malloc_fake.custom_fake = custom_mock_malloc;
    mock_calloc_fake.custom_fake = custom_mock_calloc;
    mock_align_malloc_fake.custom_fake = custom_mock_align_malloc;
    mock_nocache_malloc_fake.custom_fake = custom_mock_nocache_malloc;
    mock_free_fake.custom_fake = custom_mock_free;
}

wifi_manager_mem_ops_t* mock_mem_ops_get(void)
{
    return &mem_ops;
} 