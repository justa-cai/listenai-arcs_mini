/**
 * @file mock_os_ops.h
 * @brief WiFi管理器测试中使用的系统操作接口
 */
#ifndef MOCK_OS_OPS_H
#define MOCK_OS_OPS_H

#include "wifi_manager/wifi_manager_os_ops.h"
#include "fff.h"

// 互斥锁操作
DECLARE_FAKE_VALUE_FUNC(void *, mock_mutex_create);
DECLARE_FAKE_VALUE_FUNC(int, mock_mutex_lock, void *, uint32_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_mutex_unlock, void *);
DECLARE_FAKE_VOID_FUNC(mock_mutex_delete, void *);

// 队列操作
DECLARE_FAKE_VALUE_FUNC(void *, mock_queue_create, uint32_t, const char *, size_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_queue_push, void *, const void *, size_t, uint32_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_queue_pop, void *, void *, size_t, uint32_t);
DECLARE_FAKE_VOID_FUNC(mock_queue_delete, void *);

// 线程操作
DECLARE_FAKE_VALUE_FUNC(void *, mock_thread_create, wifi_manager_os_thread_attr_t *, wifi_manager_os_thread_entry_t, void *);
DECLARE_FAKE_VOID_FUNC(mock_thread_delete, void *);

void mock_os_ops_init(void);
void mock_os_ops_reset(void);
wifi_manager_os_ops_t* mock_os_ops_get(void);

#endif /* MOCK_OS_OPS_H */ 