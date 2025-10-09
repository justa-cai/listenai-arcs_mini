/**
 * @file mock_mem_ops.h
 * @brief MAC管理器测试中使用的内存操作接口
 */
#ifndef MOCK_MEM_OPS_H
#define MOCK_MEM_OPS_H

#include <stdlib.h>
#include "mac_manager.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(void *, mock_malloc, size_t);
DECLARE_FAKE_VOID_FUNC(mock_free, void *);


void mock_mem_ops_init(void);

void mock_mem_ops_reset(void);

mac_manager_mem_ops_t* mock_mem_ops_get(void);

#endif /* MOCK_MEM_OPS_H */
