#ifndef _WORKQUEUE_H_
#define _WORKQUEUE_H_

#include "FreeRTOS.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// 工作项结构体
typedef struct {
    void (*function)(void *param); // 工作函数指针
    void *parameter;               // 工作函数参数
    TickType_t exec_ticks;
} workqueue_item_t;

// 工作队列句柄
typedef struct {
    QueueHandle_t queue;     // 存储工作项的队列
    TaskHandle_t taskHandle; // 工作队列任务句柄

} workqueue_t;

workqueue_t *workqueue_create(const char *name, int priority, uint16_t queueLength, uint16_t stackSize);
int workqueue_submit(workqueue_t *workQueue, void (*function)(void *), void *parameter,TickType_t delay_ticks);

#ifdef __cplusplus
}
#endif

#endif