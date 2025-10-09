/**
 * @file mock_mem_ops.c
 * @brief MAC管理器测试中使用的内存操作实现
 */
#include "mock_mem_ops.h"
#include <stdlib.h>
#include <stdio.h>

DEFINE_FAKE_VALUE_FUNC(void *, mock_malloc, size_t);
DEFINE_FAKE_VOID_FUNC(mock_free, void *);

static mac_manager_mem_ops_t mem_ops = {
    .malloc = mock_malloc,
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
 * @brief 测试用的内存释放函数
 */
static void custom_mock_free(void* ptr) {
    free(ptr);
}


void mock_mem_ops_init(void)
{
    RESET_FAKE(mock_malloc);
    RESET_FAKE(mock_free);

    mem_ops.malloc = mock_malloc;
    mem_ops.free = mock_free;

    mock_malloc_fake.custom_fake = custom_mock_malloc;
    mock_free_fake.custom_fake = custom_mock_free;
}



mac_manager_mem_ops_t* mock_mem_ops_get(void)
{
    return &mem_ops;
}



