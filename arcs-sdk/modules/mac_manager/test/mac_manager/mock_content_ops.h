/**
 * @file mock_content_ops.h
 * @brief MAC管理器测试中使用的内容操作接口
 */
#ifndef MOCK_CONTENT_OPS_H
#define MOCK_CONTENT_OPS_H

#include "mac_manager.h"
#include "fff.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

DECLARE_FAKE_VALUE_FUNC(int, mock_mac_set, const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_mac_get, uint8_t* , size_t*);
DECLARE_FAKE_VALUE_FUNC(int, mock_mac_del);
DECLARE_FAKE_VALUE_FUNC(int, mock_mac_random, uint8_t*, size_t *);

void mock_content_ops_init(void);

void mock_content_ops_reset(void);


/**
 * @brief 获取预先配置好的内容操作结构体
 * @return 内容操作结构体
 */
mac_manager_content_ops_t* mock_content_get_ops(void);

#endif /* MOCK_CONTENT_OPS_H */
