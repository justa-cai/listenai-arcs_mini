/**
 * @file mock_os_ops.c
 * @brief WiFi管理器测试中使用的系统操作实现
 */
// 添加特性宏以支持pthread_mutex_timedlock

#include "mock_os_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define DEBUG_LOG 0

#define LOGD(fmt, args...) do { if (DEBUG_LOG) printf(fmt, ##args); } while (0)

// 全局变量，用于跟踪已创建的所有队列，便于终止时清理
#define MAX_QUEUES 32
static void *g_all_queues[MAX_QUEUES] = {NULL};
static pthread_mutex_t g_queues_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_shutdown_requested = 0;  // 全局终止请求标志

// 互斥锁操作实现
DEFINE_FAKE_VALUE_FUNC(void *, mock_mutex_create);
DEFINE_FAKE_VALUE_FUNC(int, mock_mutex_lock, void *, uint32_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_mutex_unlock, void *);
DEFINE_FAKE_VOID_FUNC(mock_mutex_delete, void *);

// 队列操作实现
DEFINE_FAKE_VALUE_FUNC(void *, mock_queue_create, uint32_t, const char *, size_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_queue_push, void *, const void *, size_t, uint32_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_queue_pop, void *, void *, size_t, uint32_t);
DEFINE_FAKE_VOID_FUNC(mock_queue_delete, void *);

// 线程操作实现
DEFINE_FAKE_VALUE_FUNC(void *, mock_thread_create, wifi_manager_os_thread_attr_t *, wifi_manager_os_thread_entry_t, void *);
DEFINE_FAKE_VOID_FUNC(mock_thread_delete, void *);

// 全局线程追踪
typedef struct linux_thread_t linux_thread_t;
#define MAX_THREADS 32
static linux_thread_t *g_all_threads[MAX_THREADS] = {NULL};
static pthread_mutex_t g_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

// 互斥锁Linux实现
static void *linux_mutex_create(void)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (mutex == NULL) {
        return NULL;
    }
    
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    
    return mutex;
}

static int linux_mutex_lock(void *mutex, uint32_t timeout)
{
    pthread_mutex_t *pmutex = (pthread_mutex_t *)mutex;
    
    // 如果请求了关闭，快速失败
    if (g_shutdown_requested) {
        return -ECANCELED;
    }
    
    if (timeout == 0xFFFFFFFFU) { // LISA_WAIT_FOREVER
        LOGD("func: %s, line: %d, lock mutex: %p\n", __FUNCTION__, __LINE__, pmutex);
        return pthread_mutex_lock(pmutex);
    } else if (timeout == 0) { // LISA_NO_WAIT
        LOGD("func: %s, line: %d, lock mutex: %p\n", __FUNCTION__, __LINE__, pmutex);
        return pthread_mutex_trylock(pmutex);
    } else {
        // 有超时的锁定实现 - 使用替代方案
        // 一些Linux系统可能不支持pthread_mutex_timedlock，采用轮询方式实现
        struct timespec start_time, current_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        uint64_t timeout_ns = (uint64_t)timeout * 1000000; // 毫秒转纳秒
        
        while (1) {
            // 检查是否请求了关闭
            if (g_shutdown_requested) {
                return -ECANCELED;
            }
            
            // 尝试获取锁
            int ret = pthread_mutex_trylock(pmutex);
            if (ret == 0) {
                return 0; // 成功获取锁
            }
            
            if (ret != EBUSY) {
                return ret; // 其他错误
            }
            
            // 检查是否超时
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            uint64_t elapsed_ns = 
                (current_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                (current_time.tv_nsec - start_time.tv_nsec);
                
            if (elapsed_ns >= timeout_ns) {
                return ETIMEDOUT;
            }
            
            // 短暂睡眠避免CPU占用过高
            struct timespec sleep_time = {0, 1000000}; // 1ms
            nanosleep(&sleep_time, NULL);
        }
    }
}

static int linux_mutex_unlock(void *mutex)
{
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

static void linux_mutex_delete(void *mutex)
{
    pthread_mutex_t *pmutex = (pthread_mutex_t *)mutex;
    pthread_mutex_destroy(pmutex);
    free(pmutex);
}

// 简单队列实现
typedef struct queue_node {
    void *data;
    size_t size;
    struct queue_node *next;
} queue_node_t;

typedef struct {
    queue_node_t *head;
    queue_node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint32_t count;
    uint32_t max_count;
    size_t item_size;
    char name[32];
    int id;                // 队列ID
    volatile int valid;    // 队列是否有效
} linux_queue_t;

// 添加队列到全局列表
static void add_queue_to_list(void *queue)
{
    pthread_mutex_lock(&g_queues_mutex);
    for (int i = 0; i < MAX_QUEUES; i++) {
        if (g_all_queues[i] == NULL) {
            g_all_queues[i] = queue;
            ((linux_queue_t*)queue)->id = i;
            break;
        }
    }
    pthread_mutex_unlock(&g_queues_mutex);
}

// 从全局列表中移除队列
static void remove_queue_from_list(void *queue)
{
    pthread_mutex_lock(&g_queues_mutex);
    for (int i = 0; i < MAX_QUEUES; i++) {
        if (g_all_queues[i] == queue) {
            g_all_queues[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&g_queues_mutex);
}

static void *linux_queue_create(uint32_t queue_length, const char *name, size_t item_size)
{
    linux_queue_t *queue = (linux_queue_t *)malloc(sizeof(linux_queue_t));
    if (queue == NULL) {
        return NULL;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->max_count = queue_length;
    queue->item_size = item_size;
    queue->valid = 1;  // 标记为有效
    
    if (name != NULL) {
        strncpy(queue->name, name, sizeof(queue->name) - 1);
        queue->name[sizeof(queue->name) - 1] = '\0';
    } else {
        queue->name[0] = '\0';
    }
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue);
        return NULL;
    }
    
    if (pthread_cond_init(&queue->cond, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    
    // 添加到全局队列列表
    add_queue_to_list(queue);
    
    mock_queue_create_fake.return_val = queue;
    return queue;
}

static int linux_queue_push(void *q, const void *item, size_t item_size, uint32_t timeout)
{
    linux_queue_t *queue = (linux_queue_t *)q;
    int ret = 0;

    LOGD("func: %s, line: %d, lock queue: %p\n", __FUNCTION__, __LINE__, queue);
    
    // 检查退出标志
    if (g_shutdown_requested || !queue->valid) {
        return -ECANCELED;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    // 重新检查队列是否有效
    if (!queue->valid) {
        pthread_mutex_unlock(&queue->mutex);
        return -ECANCELED;
    }
    
    if (queue->count >= queue->max_count) {
        if (timeout == 0) { // LISA_NO_WAIT
            pthread_mutex_unlock(&queue->mutex);
            return -EAGAIN;
        } else {
            // 等待直到有空间或超时
            struct timespec ts;
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            
            // 如果是无限等待，设置一个较短的超时时间，以便定期检查退出标志
            if (timeout == 0xFFFFFFFFU) {
                timeout = 100; // 100毫秒
            }
            
            ts.tv_sec = now.tv_sec + (timeout / 1000);
            ts.tv_nsec = now.tv_nsec + ((timeout % 1000) * 1000000);
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            
            while (queue->count >= queue->max_count) {
                ret = pthread_cond_timedwait(&queue->cond, &queue->mutex, &ts);
                
                // 检查退出标志
                if (g_shutdown_requested || !queue->valid) {
                    pthread_mutex_unlock(&queue->mutex);
                    return -ECANCELED;
                }
                
                if (ret != 0) {
                    pthread_mutex_unlock(&queue->mutex);
                    return -ETIMEDOUT;
                }
            }
        }
    }
    
    queue_node_t *node = (queue_node_t *)malloc(sizeof(queue_node_t));
    if (node == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return -ENOMEM;
    }
    
    node->data = malloc(item_size);
    if (node->data == NULL) {
        free(node);
        pthread_mutex_unlock(&queue->mutex);
        return -ENOMEM;
    }
    
    memcpy(node->data, item, item_size);
    node->size = item_size;
    node->next = NULL;
    
    if (queue->tail == NULL) {
        queue->head = queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
    
    queue->count++;
    
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

static int linux_queue_pop(void *q, void *item, size_t item_size, uint32_t timeout)
{
    linux_queue_t *queue = (linux_queue_t *)q;
    int ret = 0;
    
    LOGD("func: %s, line: %d, lock queue: %p, timeout: %d\n", __FUNCTION__, __LINE__, queue, timeout);
    
    // 检查退出标志
    if (g_shutdown_requested || !queue->valid) {
        return -ECANCELED;
    }
    
    pthread_mutex_lock(&queue->mutex);
    LOGD("func: %s, line: %d, finally lock queue: %p\n", __FUNCTION__, __LINE__, queue);
    
    // 重新检查队列是否有效
    if (!queue->valid) {
        pthread_mutex_unlock(&queue->mutex);
        return -ECANCELED;
    }
    
    if (queue->count == 0) {
        if (timeout == 0) { // LISA_NO_WAIT
            pthread_mutex_unlock(&queue->mutex);
            LOGD("func: %s, line: %d, unlock queue: %p\n", __FUNCTION__, __LINE__, queue);
            return -EAGAIN;
        } else {
            LOGD("func: %s, line: %d, wait queue timeout: %d\n", __FUNCTION__, __LINE__, timeout);
            
            // 定义一个较短的等待周期，以便定期检查退出标志
            uint32_t wait_interval = 100; // 100毫秒
            uint32_t remaining_timeout = timeout;
            
            // 主等待循环
            while (queue->count == 0) {
                // 检查退出标志
                if (g_shutdown_requested || !queue->valid) {
                    pthread_mutex_unlock(&queue->mutex);
                    return -ECANCELED;
                }
                
                // 计算本次等待的超时时间
                uint32_t current_timeout = (timeout == 0xFFFFFFFFU) ? 
                                          wait_interval : 
                                          (remaining_timeout < wait_interval ? remaining_timeout : wait_interval);
                
                // 设置超时时间
                struct timespec ts;
                struct timespec now;
                clock_gettime(CLOCK_REALTIME, &now);
                ts.tv_sec = now.tv_sec + (current_timeout / 1000);
                ts.tv_nsec = now.tv_nsec + ((current_timeout % 1000) * 1000000);
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000;
                }
                
                // 等待
                ret = pthread_cond_timedwait(&queue->cond, &queue->mutex, &ts);
                
                // 再次检查退出标志
                if (g_shutdown_requested || !queue->valid) {
                    pthread_mutex_unlock(&queue->mutex);
                    return -ECANCELED;
                }
                
                // 如果有数据了，跳出循环
                if (queue->count > 0) {
                    break;
                }
                
                // 检查超时
                if (ret != 0 && timeout != 0xFFFFFFFFU) {
                    // 更新剩余超时时间
                    if (remaining_timeout <= wait_interval) {
                        // 已经超时
                        pthread_mutex_unlock(&queue->mutex);
                        LOGD("func: %s, line: %d, unlock queue: %p\n", __FUNCTION__, __LINE__, queue);
                        return -ETIMEDOUT;
                    }
                    
                    remaining_timeout -= wait_interval;
                }
            }
        }
    }
    
    queue_node_t *node = queue->head;

    LOGD("func: %s, line: %d, node: %p\n", __FUNCTION__, __LINE__, node);
    
    if (node == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return -EAGAIN;
    }
    
    LOGD("func: %s, line: %d, copy_size: %d\n", __FUNCTION__, __LINE__, node->size);
    size_t copy_size = (item_size < node->size) ? item_size : node->size;
    memcpy(item, node->data, copy_size);
    
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    
    LOGD("func: %s, line: %d, count: %d\n", __FUNCTION__, __LINE__, queue->count);
    queue->count--;
    
    free(node->data);
    free(node);
    
    LOGD("func: %s, line: %d, unlock queue: %p\n", __FUNCTION__, __LINE__, queue);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

// 安全清理队列内存，不获取互斥锁
static void linux_queue_cleanup_unsafe(linux_queue_t *queue)
{
    queue_node_t *node, *next;
    
    // 标记队列为无效
    queue->valid = 0;
    
    // 清理队列中的所有节点
    for (node = queue->head; node != NULL; node = next) {
        next = node->next;
        free(node->data);
        free(node);
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    
    // 唤醒所有等待的线程
    pthread_cond_broadcast(&queue->cond);
}

static void linux_queue_delete(void *q)
{
    linux_queue_t *queue = (linux_queue_t *)q;
    
    if (!queue) return;

    LOGD("func: %s, line: %d, lock queue: %p\n", __FUNCTION__, __LINE__, queue);
    
    // 从全局队列列表中移除
    remove_queue_from_list(queue);
    
    // 标记队列为无效
    queue->valid = 0;
    
    // 尝试获取锁，但不阻塞
    if (pthread_mutex_trylock(&queue->mutex) == 0) {
        // 成功获得锁，清理队列
        linux_queue_cleanup_unsafe(queue);
        pthread_mutex_unlock(&queue->mutex);
    } else {
        // 无法获得锁，发送广播信号让等待的线程退出
        pthread_cond_broadcast(&queue->cond);
    }

    LOGD("func: %s, line: %d\n", __FUNCTION__, __LINE__);
    
    // 延迟销毁互斥锁和条件变量
    usleep(10000); // 等待10毫秒，让所有线程有时间退出
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    
    free(queue);
}

// 线程实现
struct linux_thread_t {
    pthread_t thread;
    wifi_manager_os_thread_entry_t entry;
    void *arg;
    volatile int running;
    pthread_mutex_t mutex;  // 保护running标志的互斥锁
    int id; // 线程ID
};

// 添加线程到全局列表
static void add_thread_to_list(linux_thread_t *thread)
{
    pthread_mutex_lock(&g_threads_mutex);
    for (int i = 0; i < MAX_THREADS; i++) {
        if (g_all_threads[i] == NULL) {
            g_all_threads[i] = thread;
            thread->id = i;
            break;
        }
    }
    pthread_mutex_unlock(&g_threads_mutex);
}

// 从全局列表中移除线程
static void remove_thread_from_list(linux_thread_t *thread)
{
    pthread_mutex_lock(&g_threads_mutex);
    for (int i = 0; i < MAX_THREADS; i++) {
        if (g_all_threads[i] == thread) {
            g_all_threads[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&g_threads_mutex);
}

static void *thread_wrapper(void *arg)
{
    linux_thread_t *thread = (linux_thread_t *)arg;
    
    // 设置取消类型为异步取消，允许在任何点取消线程
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    // 检查全局终止标志
    if (g_shutdown_requested) {
        pthread_mutex_lock(&thread->mutex);
        thread->running = 0;
        pthread_mutex_unlock(&thread->mutex);
        return NULL;
    }
    
    // 执行用户函数
    thread->entry(thread->arg);
    
    // 标记线程已完成
    pthread_mutex_lock(&thread->mutex);
    thread->running = 0;
    pthread_mutex_unlock(&thread->mutex);
    
    return NULL;
}

static void *linux_thread_create(wifi_manager_os_thread_attr_t *attr, wifi_manager_os_thread_entry_t entry, void *arg)
{
    linux_thread_t *thread = (linux_thread_t *)malloc(sizeof(linux_thread_t));
    if (thread == NULL) {
        return NULL;
    }
    
    thread->entry = entry;
    thread->arg = arg;
    thread->running = 1;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&thread->mutex, NULL) != 0) {
        free(thread);
        return NULL;
    }
    
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    
    if (attr != NULL) {
        if (attr->stack_size > 0) {
            pthread_attr_setstacksize(&pthread_attr, attr->stack_size);
        }
        
        // Linux优先级设置较复杂，这里简化处理
    }
    
    // 添加到全局线程列表
    add_thread_to_list(thread);
    
    int ret = pthread_create(&thread->thread, &pthread_attr, thread_wrapper, thread);
    pthread_attr_destroy(&pthread_attr);
    
    if (ret != 0) {
        pthread_mutex_destroy(&thread->mutex);
        remove_thread_from_list(thread);
        free(thread);
        return NULL;
    }
    
    return thread;
}

static void linux_thread_delete(void *t)
{
    linux_thread_t *thread = (linux_thread_t *)t;
    
    if (thread == NULL) {
        return;
    }
    
    // 从全局线程列表中移除
    remove_thread_from_list(thread);
    
    // 检查线程是否仍在运行
    pthread_mutex_lock(&thread->mutex);
    int running = thread->running;
    pthread_mutex_unlock(&thread->mutex);
    
    if (running) {
        // 先尝试发送取消信号
        pthread_cancel(thread->thread);
        
        // 等待线程完成，但设置超时避免永久阻塞
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1; // 1秒超时
        
        int ret = pthread_timedjoin_np(thread->thread, NULL, &ts);
        if (ret != 0) {
            // 如果超时，发送SIGUSR1信号强制终止线程
            pthread_kill(thread->thread, SIGUSR1);
            
            // 再次尝试等待，但不阻塞
            pthread_tryjoin_np(thread->thread, NULL);
        }
    }
    
    // 清理资源
    pthread_mutex_destroy(&thread->mutex);
    free(thread);
}

// 清理所有资源的函数
static void cleanup_all_resources(void)
{
    g_shutdown_requested = 1;
    
    // 关闭所有队列
    pthread_mutex_lock(&g_queues_mutex);
    for (int i = 0; i < MAX_QUEUES; i++) {
        linux_queue_t *queue = (linux_queue_t*)g_all_queues[i];
        if (queue) {
            // 标记队列为无效
            queue->valid = 0;
            
            // 广播唤醒所有等待的线程
            pthread_cond_broadcast(&queue->cond);
            
            // 不要在这里销毁队列，让每个队列的delete函数处理
            g_all_queues[i] = NULL;
        }
    }
    pthread_mutex_unlock(&g_queues_mutex);
    
    // 让队列线程有时间响应退出
    usleep(100000); // 100毫秒
    
    // 取消所有线程
    pthread_mutex_lock(&g_threads_mutex);
    for (int i = 0; i < MAX_THREADS; i++) {
        linux_thread_t *thread = g_all_threads[i];
        if (thread) {
            if (thread->running) {
                pthread_cancel(thread->thread);
                // 不要在这里等待线程，让每个线程的delete函数处理
            }
            g_all_threads[i] = NULL;
        }
    }
    pthread_mutex_unlock(&g_threads_mutex);
}

static wifi_manager_os_ops_t os_ops = {
    // 互斥锁操作
    .mutex_create = mock_mutex_create,
    .mutex_lock = mock_mutex_lock,
    .mutex_unlock = mock_mutex_unlock,
    .mutex_delete = mock_mutex_delete,
    
    // 队列操作
    .queue_create = mock_queue_create,
    .queue_push = mock_queue_push,
    .queue_pop = mock_queue_pop,
    .queue_delete = mock_queue_delete,
    
    // 线程操作
    .thread_create = mock_thread_create,
    .thread_delete = mock_thread_delete
};

void mock_os_ops_init(void)
{
    // 初始化全局变量
    g_shutdown_requested = 0;
    
    // 重置所有模拟函数
    RESET_FAKE(mock_mutex_create);
    RESET_FAKE(mock_mutex_lock);
    RESET_FAKE(mock_mutex_unlock);
    RESET_FAKE(mock_mutex_delete);
    
    RESET_FAKE(mock_queue_create);
    RESET_FAKE(mock_queue_push);
    RESET_FAKE(mock_queue_pop);
    RESET_FAKE(mock_queue_delete);
    
    RESET_FAKE(mock_thread_create);
    RESET_FAKE(mock_thread_delete);
    
    // 配置使用Linux实现
    mock_mutex_create_fake.custom_fake = linux_mutex_create;
    mock_mutex_lock_fake.custom_fake = linux_mutex_lock;
    mock_mutex_unlock_fake.custom_fake = linux_mutex_unlock;
    mock_mutex_delete_fake.custom_fake = linux_mutex_delete;
    
    mock_queue_create_fake.custom_fake = linux_queue_create;
    mock_queue_push_fake.custom_fake = linux_queue_push;
    mock_queue_pop_fake.custom_fake = linux_queue_pop;
    mock_queue_delete_fake.custom_fake = linux_queue_delete;
    
    mock_thread_create_fake.custom_fake = linux_thread_create;
    mock_thread_delete_fake.custom_fake = linux_thread_delete;
}

void mock_os_ops_reset(void)
{
    // 清理所有资源
    cleanup_all_resources();
    
    // 重新初始化
    mock_os_ops_init();
}

wifi_manager_os_ops_t* mock_os_ops_get(void)
{
    return &os_ops;
} 