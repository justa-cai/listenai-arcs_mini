#ifndef __LISA_OS_THREAD__
#define __LISA_OS_THREAD__

#include <lisa_err.h>
#include <FreeRTOS.h>

typedef void *lisa_threadhandle_t;

/**
 * @brief 	内部线程体参数
 * @param	fn		外部创建线程传入的线程体函数
 * @param	arg		外部创建线程传入的线程参数
 */
typedef struct LisaThreadArg {
	void (*fn)(void *);
	void *arg;
} LisaThreadArg;

typedef struct {
	lisa_threadhandle_t handle;
} lisa_thread_t;

/**
 * @brief 	Lisa Task
 */
typedef struct {
	// 系统线程句, TCB
	StaticTask_t ltask;
	// 是否为用户内存栈
	uint32_t user_stack;
	// 系统线程栈内存
	StackType_t *stack;
	// 标记
	uint32_t magic;
	// Lisa Porting 线程句柄
	lisa_thread_t *lisa_thread;
} LisaStaticTask_t;

typedef struct {
	uint8_t *name;
	uint32_t stack_size;
	uint32_t priority;
} lisa_thread_attr_t;

/**
 * @brief create
 * @param  attr             
 * @param  entry            
 * @param  arg              
 * @return lisa_thread_t* 
 */
lisa_thread_t *lisa_thread_create(const lisa_thread_attr_t *attr, void (*entry)(void *), void *arg);

lisa_thread_t *lisa_thread_create_bymem(const lisa_thread_attr_t *attr, void *task_mem, void (*entry)(void *), void *arg);

/**
 * @brief set priority
 * @param thread 
 * @param priority 
 * @return lisa_err_t 
 */
lisa_err_t lisa_thread_set_priority(lisa_thread_t *thread, uint8_t priority);

/**
 * @brief delete
 * @param  thread           
 * @return lisa_err_t 
 */
lisa_err_t lisa_thread_delete(lisa_thread_t *thread);

/**
 * @brief delay
 * @param  ticks            秒
 * @return lisa_err_t 
 */
lisa_err_t lisa_thread_delay(uint32_t ticks);

/**
 * @brief mdelay
 * @param  ms               毫秒
 * @return lisa_err_t 
 */
lisa_err_t lisa_thread_mdelay(uint32_t ms);

/**
 * @brief Release time slice
 * @return lisa_err_t
 */
lisa_err_t lisa_thread_yield(void);

/**
 * @brief Get current thread name
 * @return Current thread name
 */
char * lisa_thread_cur_thread_name(void);

#endif  //__LISA_OS_THREAD__
