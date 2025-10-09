/**
 * @file lisaui_dbus.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-03-02
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_DBUS_H__
#define __LISAUI_DBUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_type.h"

#ifdef FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#elif defined(__linux__)
#include <pthread.h>
#include <stdbool.h>
#endif


#define LISAUI_DBUS_MAX_QUEUE_SIZE 16
#define LISAUI_DBUS_TASK_STACK_SIZE 2048
#define LISAUI_DBUS_TASK_PRIORITY 5

/**
 * @brief dbus_handler_t 总线事件处理函数
 * 
 * @param data 事件数据
 * 
 * @warning 不可以在事件处理函数中调用 lisaui_dbus_publish，否则会导致死锁
 * 
 */
typedef void (*lisaui_dbus_handler_t)(void* data);

typedef struct _lisaui_dbus_node_t lisaui_dbus_node_t;

typedef struct {
    const char* event;
    void* data;
} lisaui_dbus_msg_t;

struct _lisaui_dbus_node_t {
    const char* event;
    lisaui_dbus_handler_t handler;
    lisaui_dbus_node_t* next;
};


typedef struct {
    lisaui_dbus_msg_t buffer[LISAUI_DBUS_MAX_QUEUE_SIZE];
    int head;
    int tail;
    int count;
#ifdef __POSIX__
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif
} lisaui_dbus_queue_t;

typedef struct _lisaui_dbus_t {
    lisaui_dbus_node_t* node;
#ifdef FREERTOS
    SemaphoreHandle_t mutex;
    QueueHandle_t event_queue;
    TaskHandle_t task_handle;
#else
#ifdef __POSIX__
    pthread_mutex_t mutex;
    pthread_t thread;
#endif

    lisaui_dbus_queue_t event_queue;
#endif
} lisaui_dbus_t;

lisaui_err_t lisaui_dbus_create(lisaui_dbus_t* bus);
lisaui_err_t lisaui_dbus_destroy(lisaui_dbus_t* bus);
lisaui_err_t lisaui_dbus_subscribe(lisaui_dbus_t* bus, const char* event, lisaui_dbus_handler_t handler);
lisaui_err_t lisaui_dbus_unsubscribe(lisaui_dbus_t* bus, const char* event, lisaui_dbus_handler_t handler);
lisaui_err_t lisaui_dbus_publish(lisaui_dbus_t* bus, const char* event, void* data);


#define LISAUI_DBUS_TEST (1)
void test_lisaui_dbus_example1(void);


#ifdef __cplusplus
}
#endif

#endif // __LISAUI_DBUS_H__
