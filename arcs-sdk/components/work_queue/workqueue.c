#include "workqueue.h"

#define TAG "workqueue"
#include "lisa_log.h"
// 工作队列任务函数
static void workqueue_task(void *pvParameters)
{
    workqueue_t *workQueue = (workqueue_t *)pvParameters;
    workqueue_item_t workItem;

    for (;;) {
        // 等待队列中的工作项
        if (xQueueReceive(workQueue->queue, &workItem, portMAX_DELAY) == pdPASS) {
            // 执行工作项
            if (workItem.function != NULL) {
                workItem.function(workItem.parameter);
            }
        }
    }
}

// 创建工作队列
workqueue_t *workqueue_create(const char *name, int priority, uint16_t queueLength, uint16_t stackSize)
{
    workqueue_t *workQueue = (workqueue_t *)pvPortMalloc(sizeof(workqueue_t));
    if (workQueue == NULL) {
        return NULL;
    }

    // 创建队列
    workQueue->queue = xQueueCreate(queueLength, sizeof(workqueue_item_t));
    if (workQueue->queue == NULL) {
        vPortFree(workQueue);
        return NULL;
    }

    // 创建任务
    if (xTaskCreate(workqueue_task, name, stackSize, workQueue, priority, &workQueue->taskHandle) != pdPASS) {
        vQueueDelete(workQueue->queue);
        vPortFree(workQueue);
        return NULL;
    }

    return workQueue;
}


// 创建工作队列
workqueue_t *workqueue_create_static(const char *name, int priority, uint16_t queueLength, void* stack,uint16_t stackSize)
{
    workqueue_t *workQueue = (workqueue_t *)pvPortMalloc(sizeof(workqueue_t));
    if (workQueue == NULL) {
        return NULL;
    }

    // 创建队列
    workQueue->queue = xQueueCreate(queueLength, sizeof(workqueue_item_t));
    if (workQueue->queue == NULL) {
        vPortFree(workQueue);
        return NULL;
    }

    // 创建任务
    workQueue->taskHandle = xTaskCreateStatic(workqueue_task, name, stackSize, workQueue, priority,stack, &workQueue->statictask);
    if (workQueue->taskHandle == NULL) {
        vQueueDelete(workQueue->queue);
        vPortFree(workQueue);
        return NULL;
    }

    return workQueue;
}

// 提交工作到工作队列
int workqueue_submit(workqueue_t *workQueue, void (*function)(void *), void *parameter)
{
    if (workQueue == NULL || function == NULL) {
        return -1;
    }

    workqueue_item_t workItem;
    workItem.function = function;
    workItem.parameter = parameter;

    return xQueueSend(workQueue->queue, &workItem, 0);
}