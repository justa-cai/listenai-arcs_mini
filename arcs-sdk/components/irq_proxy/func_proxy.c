#include <string.h>
#include "func_proxy.h"
#include "irq_proxy.h"
#include "log_print.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// #define LOG_DBG(format, ...) CLOGD(format, ##__VA_ARGS__)
// #define LOG_INF(format, ...) CLOGI(format, ##__VA_ARGS__)
// #define LOG_ERR(format, ...) CLOGE(format, ##__VA_ARGS__)
#define LOG_DBG(format, ...) 
#define LOG_INF(format, ...) 
#define LOG_ERR(format, ...) 

#define FUNC_PROXY_TASK_STACK_SIZE 2048
#define FUNC_PROXY_TASK_PRIORITY    configMAX_PRIORITIES - 1
#define FUNC_PROXY_QUEUE_LENGTH     5

#define MAX_FUNC_NAME_LEN 32
#define MAX_FUNC_NUM 16
#define MAX_DATA_SIZE 1024

// 共享内存结构体定义
struct func_proxy_shared_data {
    char func_name[MAX_FUNC_NAME_LEN];    // 函数名称
    uint8_t data_buf[MAX_DATA_SIZE];      // 数据缓冲区
    size_t in_size;                       // 输入数据大小
    size_t out_size;                      // 输出数据大小
    uint32_t call_flag;                   // 调用标志，0=空闲，1=请求调用，2=调用完成
    int32_t result;                       // 调用结果
    uint32_t reserve0;
    uint32_t reserve1;
};

// 共享内存基地址（与irq_proxy.c中定义相同）
// #define MEM_SHARED_BASE 0x2001F000

// // 获取共享内存中的函数代理数据结构
// #define func_proxy_shared ((volatile struct func_proxy_shared_data *)(MEM_SHARED_BASE + sizeof(uint32_t) + sizeof(uint32_t)))

struct func_proxy_shared_data share_data __attribute__((section(".ipc.func_proxy"))) = {0};
volatile struct func_proxy_shared_data * func_proxy_shared = &share_data;

// 函数注册表项
struct func_entry {
    char name[MAX_FUNC_NAME_LEN];
    func_proxy_handler_t handler;
};

// 函数注册表
static struct func_entry g_func_table[MAX_FUNC_NUM];
static int g_func_count = 0;

// AP执行完成信号量
static SemaphoreHandle_t g_exec_complete_sem;

// 查找函数
static func_proxy_handler_t find_func(const char *name)
{
    for (int i = 0; i < g_func_count; i++) {
        if (strcmp(g_func_table[i].name, name) == 0) {
            return g_func_table[i].handler;
        }
    }
    return NULL;
}

// AP核注册函数处理回调
int32_t func_proxy_register(const char *name, func_proxy_handler_t handler)
{
    if (g_func_count >= MAX_FUNC_NUM || strlen(name) >= MAX_FUNC_NAME_LEN) {
        return -1;
    }

    // 检查是否已经注册
    if (find_func(name) != NULL) {
        return -2;
    }

    strcpy(g_func_table[g_func_count].name, name);
    g_func_table[g_func_count].handler = handler;
    g_func_count++;

    return 0;
}

// CP核调用AP核函数
int32_t func_proxy_call(const char *name, func_proxy_data_t *in, func_proxy_data_t *out)
{
    LOG_INF("[PROXY] Call start: %s", name);
    if (strlen(name) >= MAX_FUNC_NAME_LEN) {
        LOG_ERR("[PROXY] Function name too long");
        return -1;
    }

    if (in && in->size > MAX_DATA_SIZE) {
        return -2;
    }

    // 等待之前的调用完成
    uint32_t wait_count = 0;
    while (func_proxy_shared->call_flag != 0) {
        wait_count++;
        if (wait_count % 1000000 == 0) {
            LOG_ERR("[PROXY] Waiting for previous call to complete, flag=%d", func_proxy_shared->call_flag);
        }
    }
    LOG_INF("[PROXY] Previous call completed");

    // 设置函数名和输入数据
    strcpy((char *)func_proxy_shared->func_name, name);
    LOG_INF("[PROXY] Function name set: %s", func_proxy_shared->func_name);

    if (in && in->data && in->size > 0) {
        memcpy((void *)func_proxy_shared->data_buf, in->data, in->size);
        func_proxy_shared->in_size = in->size;
        LOG_INF("[PROXY] Input data copied, size: %d", in->size);
    } else {
        func_proxy_shared->in_size = 0;
        LOG_INF("[PROXY] No input data");
    }

    // 设置期望的输出数据大小
    func_proxy_shared->out_size = out ? out->size : 0;

    // 设置调用标志
    func_proxy_shared->call_flag = 1;

    // 通知AP核执行函数
    LOG_INF("[PROXY] Triggering AP execution");
    irq_proxy_trigger(IRQ_PROXY_CHAN_FUNC_REQUEST);

    // 等待AP核执行完成
    LOG_INF("[PROXY] Waiting for AP execution");
    if (xSemaphoreTake(g_exec_complete_sem, portMAX_DELAY) != pdTRUE) {
        LOG_ERR("[PROXY] Failed to take completion semaphore");
        return -1;
    }
    LOG_INF("[PROXY] AP execution completed");

    // 获取输出数据
    if (out && out->data && func_proxy_shared->out_size > 0) {
        size_t copy_size = func_proxy_shared->out_size;
        if (copy_size > out->size) {
            copy_size = out->size;
        }
        memcpy(out->data, (const void *)func_proxy_shared->data_buf, copy_size);
        out->size = copy_size;
    }

    // 获取调用结果
    int32_t result = func_proxy_shared->result;

    // 清除标志
    func_proxy_shared->call_flag = 0;

    return result;
}

// 函数代理任务句柄
static TaskHandle_t func_proxy_task_handle;

// 函数代理任务
static void func_proxy_task(void *arg)
{
    while (1) {
        // 等待通知
        uint32_t notification;
        if (xTaskNotifyWait(0, UINT32_MAX, &notification, portMAX_DELAY) == pdTRUE) {
            LOG_INF("[PROXY] Notified handler task");
            // 检查调用标志
            if (func_proxy_shared->call_flag != 1) {
                continue;
            }

            // 查找函数
            func_proxy_handler_t handler = find_func((const char *)func_proxy_shared->func_name);
            if (!handler) {
                LOG_ERR("[PROXY-AP] Handler not found for: %s", func_proxy_shared->func_name);
                func_proxy_shared->result = -1;
                func_proxy_shared->out_size = 0;
            } else {
                // 准备输入输出数据
                func_proxy_data_t in = {
                    .data = func_proxy_shared->in_size > 0 ? (void *)func_proxy_shared->data_buf : NULL,
                    .size = func_proxy_shared->in_size
                };
                func_proxy_data_t out = {
                    .data = (void *)func_proxy_shared->data_buf,
                    .size = func_proxy_shared->out_size
                };
                LOG_INF("[PROXY] Calling handler: %s", func_proxy_shared->func_name);
                // 执行函数
                func_proxy_shared->result = handler(&in, &out);
                func_proxy_shared->out_size = out.size;
            }
            LOG_INF("[PROXY] before IRQ_PROXY_CHAN_FUNC_COMPLETE");
            // 设置完成标志
            func_proxy_shared->call_flag = 2;
            // 通知CP核完成
            irq_proxy_trigger(IRQ_PROXY_CHAN_FUNC_COMPLETE);
            LOG_INF("[PROXY] IRQ_PROXY_CHAN_FUNC_COMPLETE");
        }
    }
}

// AP核函数调用中断处理
static int32_t func_proxy_complete_handler(void)
{
    // 释放执行完成信号量
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(g_exec_complete_sem, &need_yield);
    portYIELD_FROM_ISR(need_yield);
    return 0;
}

static int32_t func_proxy_handler(void)
{
    // 发送通知给处理任务
    BaseType_t need_yield = pdFALSE;
    xTaskNotifyFromISR(func_proxy_task_handle, 1, eSetBits, &need_yield);
    portYIELD_FROM_ISR(need_yield);
    return 0;
}

void func_proxy_init(void)
{
    // 创建执行完成信号量
    g_exec_complete_sem = xSemaphoreCreateBinary();
    if (g_exec_complete_sem == NULL) {
        LOG_ERR("[PROXY] Failed to create completion semaphore");
        return;
    }

    LOG_INF("[PROXY] Initializing function proxy");
    
    // 初始化函数表
    memset(g_func_table, 0, sizeof(g_func_table));
    g_func_count = 0;

    // 初始化共享内存
    volatile struct func_proxy_shared_data *shared = func_proxy_shared;
    memset((void *)shared, 0, sizeof(struct func_proxy_shared_data));
    LOG_INF("[PROXY] Shared memory initialized at %p", (void *)shared);

    // 注册AP执行完成中断处理函数
    irq_proxy_register_callback(IRQ_PROXY_CHAN_FUNC_COMPLETE, func_proxy_complete_handler);

    // 创建函数代理任务
    BaseType_t ret = xTaskCreate(func_proxy_task,
                                "func_proxy",
                                FUNC_PROXY_TASK_STACK_SIZE,
                                NULL,
                                FUNC_PROXY_TASK_PRIORITY,
                                &func_proxy_task_handle);
    if (ret != pdPASS) {
        LOG_ERR("[PROXY] Failed to create function proxy task");
        return;
    }
    LOG_INF("[PROXY] Function proxy task created");

    // 注册函数调用处理回调
    irq_proxy_register_callback(IRQ_PROXY_CHAN_FUNC_REQUEST, func_proxy_handler);
    LOG_INF("[PROXY] Function proxy initialized");
}
