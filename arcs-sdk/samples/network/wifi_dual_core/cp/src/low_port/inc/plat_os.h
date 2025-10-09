
#ifndef __plat_os_h__
#define __plat_os_h__

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "sysheap.h"

#define OS_TICK_FREQ    1000
#define OS_NO_WAIT      0
#define OS_WAIT_FOREVER 0xFFFFFFFF

#define OS_EFAIL       -1
#define OS_EOK          0

#define OS_PRIO_0       (tskIDLE_PRIORITY + 8)
#define OS_PRIO_1       (tskIDLE_PRIORITY + 7)
#define OS_PRIO_2       (tskIDLE_PRIORITY + 6)
#define OS_PRIO_3       (tskIDLE_PRIORITY + 5)
#define OS_PRIO_4       (tskIDLE_PRIORITY + 4)
#define OS_PRIO_5       (tskIDLE_PRIORITY + 3)
#define OS_PRIO_6       (tskIDLE_PRIORITY + 2)
#define OS_PRIO_7       (tskIDLE_PRIORITY + 1)
#define OS_PRIO_MIN	    OS_PRIO_7

typedef TaskHandle_t         os_thread_t;
typedef SemaphoreHandle_t    os_semaphore_t;
typedef SemaphoreHandle_t    os_mutex_t;
typedef QueueSetHandle_t     os_queue_t;

typedef void* os_thread_arg_t;
typedef void os_thread_entry;
#define os_thread_out() return

/*
 * 系统进入临界段
 */
void os_thread_enter_critical(void);

/*
 * 退出临界段
 */
void os_thread_exit_critical(void);


/*
 * 创建一个任务
 *
 * thandle[ou]:
 *  创建成功时保存任务的句柄或者id,可以被其他任务接口操作，比如os_thread_delete
 * name[in]:
 *  任务名字
 * main_fun[in]:
 *  任务函数体
 * arg[in]:
 *  传给任务的参数
 * stack[in]:
 *  用来确定任务的栈大小
 * prio[in]:
 *  任务优先级
 * retval:
 *  OS_EOK:ok
 *  -1:err
 */
int os_thread_create(os_thread_t* thandle, const char* name, 
                    os_thread_entry(*entry)(os_thread_arg_t arg), void *arg, 
                    int32_t stack_size, int prio);

/*
 * 删除一个任务
 * thandle[in]:
 *  要删除的任务句柄或者id,NULL表示删除当前任务
 * retval:
 *  OS_EOK:ok
 *  -1:err
 */
int os_thread_delete(os_thread_t* thandle);

/*
 * 任务延时一段时间
 * ticks[in]
 *  任务延时的tick,目前应用里面认为系统的tick是1KHz的，要求平台系统tick为1KHz
 * retval:
 *  0:
 */
int os_thread_sleep(int ticks);

/*
 * 延时us,非必须函数，可以不实现
 * us:延时的微秒数
 */
void os_thread_sleep_us(uint32_t us);

/*
 * 获取任务的名字，非必须函数，可以直接返回NULL
 */
char* os_thread_get_name(os_thread_t handle);

/*
 * 创建一个信号量
 * mhandle[ou]:
 *  创建成功后保存信号量的句柄
 * name[in]:
 * initval:
 *   初始值
 *  信号量的名字
 * retval:
 *  0:ok
 * -1:failed
 */
int os_semaphore_create(os_semaphore_t *mhandle, const char* name, int initval);
int os_semaphore_create_binary(os_semaphore_t *mhandle, const char* name);
int os_semaphore_get(os_semaphore_t *mhandle, TickType_t wait);
int os_semaphore_put(os_semaphore_t *mhandle);
int os_semaphore_getcount(os_semaphore_t *mhandle);
int os_semaphore_delete(os_semaphore_t *mhandle);
int os_semaphore_set_null(os_semaphore_t *mhandle);
int os_semaphore_is_null(os_semaphore_t *mhandle);

#define OS_MUTEX_NO_INHERIT 0
#define OS_MUTEX_INHERIT    1
/*
 * 创建一个互斥体
 * mhandle[ou]:
 *  创建好的互斥体赋值给这个指针
 * name[in]:
 *  互斥体的名字
 * flags[in]:
 *  是否使用优先级继承，目前固定会传 OS_MUTEX_INHERIT
 * retval:
 *  0:ok
 * -1:failed
 */
int os_mutex_create(os_mutex_t *mhandle, const char* name, int flags);
int os_recursive_mutex_create(os_mutex_t *mhandle, const char *name, int flags);
int os_mutex_get(os_mutex_t *mhandle, TickType_t wait);
int os_mutex_put(os_mutex_t *mhandle);
int os_mutex_delete(os_mutex_t *mhandle);

/*
 * 创建消息队列
 * qhandle:
 *  创建好的消除队列赋值给这个指针
 * queue_num:
 *  消息队列最多能容纳的消息数量
 * item_size:
 *  消息的大小，注意send,receive,的消息要固定是这个大小
 * retval:
 *  0:ok
 * -1:failed
 */
int32_t os_queue_create(os_queue_t *handle, int32_t queue_num, int32_t item_size);
int32_t os_queue_delete(os_queue_t *handle);
int32_t os_queue_send(os_queue_t *handle, void *item, int32_t wait);
int32_t os_queue_receive(os_queue_t *handle, void *item, int32_t wait);

void* os_mem_alloc(unsigned int size);
void* os_mem_calloc(unsigned int num, unsigned int size);
void* os_mem_realloc(void* old_ptr, unsigned int new_size);
void os_mem_free(void* ptr);

#define OS_MEM_IRAM    0   //芯片内部ram
#define OS_MEM_ERAM    1   //芯片外部ram

void* os_mem_alloc_ext(unsigned int size, int type);
void *os_mem_calloc_ext(unsigned int num, unsigned int size, int32_t type);
void* os_mem_realloc_ext(void* old_ptr, unsigned int new_size, int type);
void os_mem_free_ext(void* ptr, int type);

/*
 * 以下几个读写锁是使用上述函数封装而成，移植的时候可以不用再重新实现
 */
typedef int(*cb_fun)(void* plock, unsigned int wait_time);
typedef struct __os_rw_lock
{
    os_mutex_t reader_mutex;
    os_semaphore_t rw_lock;
    cb_fun reader_cb;
    unsigned int reader_count;
}os_rw_lock_t;

int os_rwlock_create_with_cb(os_rw_lock_t *lock, const char *mutex_name, const char *lock_name, cb_fun r_fn);
int os_rwlock_create(os_rw_lock_t *lock, const char *mutex_name, const char *lock_name);
int os_rwlock_delete(os_rw_lock_t *lock);
int os_rwlock_write_lock(os_rw_lock_t *lock, unsigned int wait_time);
int os_rwlock_write_unlock(os_rw_lock_t *lock);
int os_rwlock_read_lock(os_rw_lock_t *lock, unsigned int wait_time);
int os_rwlock_read_unlock(os_rw_lock_t *lock);

/*
 * 获取系统的tick
 */
unsigned int os_ticks_get(void);

/*
 * 获取微妙时间，非必须时间
 */
uint64_t os_us_get(void);


/*
 * 以下是系统调试时要用到的，是非必须的，定义一个空函数即可
 */

/*
 * 显示系统剩余的堆空间
 * parm:
 *  和堆空间一起打印出来的信息
 */
void os_heap_free_show(char* parm);

/*
 * 显示当前任务栈最小剩余量
 */
void os_thread_stack_free_show(void);

/*
 * 显示所有任务的状态
 */
void os_thread_list_show(void);

#endif

