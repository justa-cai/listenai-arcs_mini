# Work Queue 组件

Work Queue 组件提供了一种轻量级的异步任务执行机制。它允许用户创建一个专用的工作线程（Work Queue），并将任务（函数及其参数）提交到该线程中串行执行。这对于将耗时操作从中断服务程序（ISR）或其他高优先级任务中卸载非常有用。

## 功能特性

- **异步执行**: 任务提交后立即返回，实际执行在专用线程中进行。
- **线程安全**: 基于 FreeRTOS Queue 实现，支持多线程并发提交。
- **灵活配置**: 支持自定义线程优先级、栈大小和队列深度。

## 配置选项

本组件默认不启用，需在 `prj.conf` 或 `Kconfig` 中进行配置：

```kconfig
CONFIG_WORK_QUEUE=y
```

## API 接口

### 数据结构

```c
typedef struct {
    void (*function)(void *param); // 工作函数指针
    void *parameter;               // 工作函数参数
} workqueue_item_t;

typedef struct {
    QueueHandle_t queue;     // 内部消息队列
    TaskHandle_t taskHandle; // 工作线程句柄
    // ...
} workqueue_t;
```

### 创建工作队列

```c
workqueue_t *workqueue_create(const char *name, int priority, uint16_t queueLength, uint16_t stackSize);
```

*   **name**: 工作线程名称。
*   **priority**: 工作线程优先级。
*   **queueLength**: 队列最大长度（可缓存的任务数量）。
*   **stackSize**: 工作线程栈大小。
*   **返回**: 成功返回 `workqueue_t` 指针，失败返回 `NULL`。

### 提交任务

```c
int workqueue_submit(workqueue_t *workQueue, void (*function)(void *), void *parameter);
```

*   **workQueue**: 工作队列句柄。
*   **function**: 要执行的任务函数。
*   **parameter**: 传递给任务函数的参数。
*   **返回**: `pdPASS` 表示提交成功，其他值表示失败（如队列已满）。
*   **说明**: 该函数为非阻塞调用（WaitTime=0）。如果队列已满，将直接返回失败。

## 使用示例

```c
#include "workqueue.h"
#include "lisa_log.h"

// 1. 定义工作任务函数
static void my_heavy_work(void *param)
{
    int value = (int)param;
    // Log message in English as per rule
    LISA_LOGI("Processing work item with value: %d", value);
    
    // 模拟耗时操作
    // vTaskDelay(100);
}

void app_main(void)
{
    // 2. 创建工作队列
    // 名称: "my_worker", 优先级: 5, 队列深度: 10, 栈大小: 2048
    workqueue_t *my_wq = workqueue_create("my_worker", 5, 10, 2048);

    if (my_wq == NULL) {
        LISA_LOGE("Failed to create work queue");
        return;
    }

    // 3. 提交任务
    for (int i = 0; i < 5; i++) {
        // 将整数 i 作为参数传递
        int ret = workqueue_submit(my_wq, my_heavy_work, (void *)i);
        
        if (ret != pdPASS) {
            LISA_LOGE("Failed to submit work item %d, queue might be full", i);
        } else {
            LISA_LOGI("Submitted work item %d", i);
        }
    }
}
```
