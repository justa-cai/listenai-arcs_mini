
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_heap_caps.h"
#include "plat_os.h"
#include "semphr.h"
#include "sysheap.h"
#define USE_PSRAM   1

void port_switch_isr(int higher_prio_task_woken)
{
    // if(higher_prio_task_woken == pdTRUE)
    // {
    //     portYIELD_FROM_ISR();
    // }
}

extern unsigned port_interruptNesting[];

int is_isr_context(void)
{
    return  xPortIsInsideInterrupt();
}

void os_thread_enter_critical(void)
{
    portENTER_CRITICAL();
}

void os_thread_exit_critical(void)
{
    //return;
    portEXIT_CRITICAL();
}

#define OS_CREATE_THREAD_LOG    1
#define OS_THREAD_USE_PSRAM     1

/*
 * 默认任务的栈使用psram,如果有需要使用内部ram的任务需求
 * 把任务的名字写在下面
 */
const char *iram_thread_buf[] = 
{
    "install",
    "low_ota",
    "low_sys",
    "nvs_ota",
    "init",
    "mod_wifi",
    "lcd_rotate",
    "trans_rx_task",
    "trans_tx_task",
};


/* 
 * 默认任务都是创建在Core0上的,如果有需要创建在Core1上的任务需求
 * 把任务的名字写在下面
 */
const char *core1_thread_buf[] =
{
    "mp3",
    "tts",
    "play",
    "talk",
    "mod_rec",
    "talk_nlp",
    "lcd_flush",
    "font_do_real",
    //"lcd_rotate",
    "sql_init",
    "post_00",
    "post_01",
    "post_02",
    "post_03",
    "post_04",
    "post_05",
    "post_06",
    "post_07",
    "post_08",
    "post_09",
    "get_0",
    "get_1",
    "get_2",
    "get_3",
    "get_4",
    "get_5",
    "get_6",
    "get_7",
    "get_8",
    "get_9",
    "tf_crc", //card recover crc
};

int os_thread_create(os_thread_t* thandle, const char* name, 
                    os_thread_entry(*entry)(os_thread_arg_t arg), void *arg, 
                    int32_t stack_size, int prio)
{
    int rev = 0;
    rev = xTaskCreate(entry, name, stack_size, arg, prio, thandle);

    rev = rev == pdPASS ? OS_EOK : rev;
    return rev;
}

int os_thread_delete(os_thread_t* thandle)
{
    if(thandle == NULL)
    {
        vTaskDelete(NULL);
    }
    else
    {
		os_thread_t thread = *thandle;
		*thandle = NULL;
        vTaskDelete(thread);
    }
    return OS_EOK;
}

/*
 * 现在sleep的单位为ms
 */
int os_thread_sleep(int ms)
{
    vTaskDelay(ms);
    return 0;
}

char* os_thread_get_name(os_thread_t handle)
{
    return pcTaskGetName(handle);
}

int os_semaphore_create_binary(os_semaphore_t *mhandle, const char* name)
{
    *mhandle = xSemaphoreCreateBinary();
    if(*mhandle == NULL)
    {
        printf("[os]sem_create failed!\n");
        return OS_EFAIL;
    }
    return OS_EOK;
}

int os_semaphore_create_counting(os_semaphore_t *mhandle, const char* name, unsigned long maxcount, unsigned long initcount)
{
    *mhandle = xSemaphoreCreateCounting(maxcount, initcount);
    if(*mhandle)
    {
        return OS_EOK;
    }
    else
    {
        return OS_EFAIL;
    }
}

#define SEMAPHORE_MAX   8
int os_semaphore_create(os_semaphore_t *mhandle, const char* name, int initval)
{
    return os_semaphore_create_counting(mhandle, name, SEMAPHORE_MAX, initval);
}

int os_semaphore_get(os_semaphore_t *mhandle, TickType_t wait)
{
    int rev;
    signed portBASE_TYPE higher_prio_task_woken = pdFALSE;
    if(!mhandle || !(*mhandle))
    {
        return OS_EFAIL;
    }
    if(is_isr_context())
    {
        rev = xSemaphoreTakeFromISR(*mhandle, &higher_prio_task_woken);
        //port_switch_isr(higher_prio_task_woken);
    }
    else
    {
        rev = xSemaphoreTake(*mhandle, wait);
    }
    rev = (rev == pdTRUE) ? OS_EOK : OS_EFAIL;
    return rev;
}

int os_semaphore_put(os_semaphore_t *mhandle)
{
    int rev;
    signed portBASE_TYPE higher_prio_task_woken = pdFALSE;
    if(!mhandle || !(*mhandle))
    {
        return OS_EFAIL;
    }
    if(is_isr_context())
    {
        rev = xSemaphoreGiveFromISR(*mhandle, &higher_prio_task_woken);
        //port_switch_isr(higher_prio_task_woken);
    }
    else
    {
        rev = xSemaphoreGive(*mhandle);
    }
    rev = (rev == pdTRUE) ? OS_EOK : OS_EFAIL;
    return rev;
}

int os_semaphore_getcount(os_semaphore_t *mhandle)
{
    int count;
    count = uxQueueMessagesWaiting(*mhandle);
    return count;
}

int os_semaphore_delete(os_semaphore_t *mhandle)
{
    vSemaphoreDelete(*mhandle);
    return OS_EOK;
}

int os_mutex_create(os_mutex_t *mhandle, const char* name, int flags)
{
    if(flags == OS_MUTEX_NO_INHERIT)
    {
        *mhandle = NULL;
        //return OS_EFAIL; //现在NO_INHERIT
        printf("[os]mutex_create NO_INHERIT but use INHERIT\n");
    }
    
    *mhandle = xSemaphoreCreateMutex();
    if(*mhandle)
    {
        return OS_EOK;
    }
    else
    {
        return OS_EFAIL;
    }
}
int os_recursive_mutex_create(os_mutex_t *mhandle, const char *name, int flags)
{
    if(flags == OS_MUTEX_NO_INHERIT)
    {
        *mhandle = NULL;
        //return OS_EFAIL; //现在NO_INHERIT
        printf("[os]mutex_create NO_INHERIT but use INHERIT\n");
    }
    *mhandle = xSemaphoreCreateRecursiveMutex();
    if(*mhandle)
    {
        return OS_EOK;
    }
    else
    {
        return OS_EFAIL;
    }
}
int os_mutex_get(os_mutex_t *mhandle, TickType_t wait)
{
    int rev;
    rev = xSemaphoreTake(*mhandle, wait);
    rev = (rev == pdTRUE) ? OS_EOK : OS_EFAIL;
    return rev;
}

int os_mutex_put(os_mutex_t *mhandle)
{
    int rev;
    if(is_isr_context())
    {
        printf("[os]os_mutex_put in isr context)");
        while(1);
    }
    rev = xSemaphoreGive(*mhandle);
    rev = (rev == pdTRUE) ? OS_EOK : OS_EFAIL;
    return rev;
}

int os_mutex_delete(os_mutex_t *mhandle)
{
    vSemaphoreDelete(*mhandle);
    return OS_EOK;
}

int32_t os_queue_create(os_queue_t *handle, int32_t queue_num, int32_t item_size)
{
    *handle = xQueueCreate(queue_num, item_size);
    return *handle ? OS_EOK : -1;
}

int32_t os_queue_delete(os_queue_t *handle)
{
    vQueueDelete(*handle);
    return 0;
}

int32_t os_queue_send(os_queue_t *handle, void *item, int32_t wait)
{
    int32_t rev;
    if(is_isr_context())
    {
        rev = xQueueSendFromISR(*handle, item, NULL);
    }
    else
    {
        rev = xQueueSend(*handle, item, wait);
    }
    return rev == pdTRUE ? 0 : -1;
}

int32_t os_queue_receive(os_queue_t *handle, void *item, int32_t wait)
{
    int32_t rev;
    if(is_isr_context())
    {
        rev = xQueueReceiveFromISR(*handle, item, NULL);
    }
    else
    {
        rev = xQueueReceive(*handle, item, wait);
    }
    return rev == pdTRUE ? 0 : -1;
}

#define OS_MEM_LOG      0 //mem_alloc和mem_free的时候是否打印log

void* os_mem_alloc(unsigned int size)
{
#if USE_PSRAM
    void *ptr = os_mem_alloc_ext(size, OS_MEM_ERAM);
#else
     void* ptr = inram_malloc(4,size);
#endif
    if(ptr)
    {
        memset(ptr, 0, size);
    }
#if OS_MEM_LOG == 1
    printf("[os]m a:0x%x [%s][%d]\n", (uint32_t)ptr, pcTaskGetTaskName(NULL)?:"",size);
#endif
    // ESP_LOGI("os_mem_alloc", "malloc:%p, size:%d, called:0x%08x", ptr, size, (intptr_t)__builtin_return_address(0) - 2);
    return ptr;
}

/*
 * 申请一段内存并且清零
 */
void* os_mem_calloc(unsigned int num, unsigned int size)
{
#if USE_PSRAM
    void *ptr = os_mem_calloc_ext(num, size, OS_MEM_ERAM);
#else
    void* ptr = inram_calloc(4,num,size);
#endif
    if(ptr)
    {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

void* os_mem_realloc(void* old_ptr, unsigned int new_size)
{
#if USE_PSRAM
    void *ptr = os_mem_realloc_ext(old_ptr, new_size, OS_MEM_ERAM);
#else
    void *ptr = inram_realloc(old_ptr, new_size);
#endif
    return ptr;
}

void os_mem_free(void* ptr)
{
#if OS_MEM_LOG == 1
    printf("[os]m f:0x%x [%s]\n", (uint32_t)ptr, pcTaskGetTaskName(NULL)?:"");
#endif
#if USE_PSRAM
    exram_free(ptr);
#else
    inram_free(ptr);
#endif
}

void* os_mem_alloc_ext(unsigned int size, int type)
{
    switch(type)
    {
        case OS_MEM_IRAM:
            return inram_malloc(4, size);
        case OS_MEM_ERAM:
            return exram_malloc(32, size);
        default:
            return NULL;
    }
    return NULL;
}

void *os_mem_calloc_ext(unsigned int num, unsigned int size, int32_t type)
{
    void *ptr = os_mem_alloc_ext(num * size, type);
    if(ptr)
    {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

void* os_mem_realloc_ext(void* old_ptr, unsigned int new_size, int type)
{
    switch(type)
    {
        case OS_MEM_IRAM:
            return inram_realloc(old_ptr, new_size);
        case OS_MEM_ERAM:
            return exram_realloc(old_ptr, new_size);
        default:
            return NULL;
    }
    return NULL;
}

void os_mem_free_ext(void* ptr, int type)
{
    heap_caps_free(ptr);
}

unsigned int os_ticks_get(void)
{
    return xTaskGetTickCount();
}

uint64_t os_us_get(void)
{
    int64_t time_us;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    return time_us;
}

/*********************test**********************/

#if 0

#define TEST_SEM    0
#define TEST_MUX    1

os_semaphore_t sem;
os_mutex_t  mutex;

void entry1(void* arg)
{
    char* parm;
    int rev, cnt = 0;
    parm = (char*)arg;
    printf("This is Entry1 parm:%s\n", parm?:"");
    while(1)
    {
#if TEST_SEM == 1
        rev = os_semaphore_get(&sem, OS_WAIT_FOREVER);
        printf("entry1 get sem rev:%d\n", rev);
#endif

#if TEST_MUX == 1
        rev = os_mutex_get(&mutex, 0);
        printf("entry1 get mutex rev:%d\n", rev);
#endif
        printf("entry1 cnt:%d\n", cnt++);
    }
    printf("entry1 over\n");
    os_thread_delete(NULL);
}

void entry2(void* arg)
{
    char* parm;
    int cnt = 0;
    parm = (char*)arg;
    printf("This is Entry2\n");
    while(cnt < 20)
    {
        printf("entry2:%s------>cnt:%d\n", parm?:"", cnt++);
        os_thread_sleep(1000);
#if TEST_SEM == 1
        os_semaphore_put(&sem);
#endif
#if TEST_MUX == 1
        os_mutex_put(&mutex);
#endif
    }
    printf("entry2 over\n");
    os_thread_delete(NULL); 
}

void plat_os_test(void)
{
    int rev;
    os_thread_t handle1, handle2;
    os_thread_stack_define(stack1, 1024);
    os_thread_stack_define(stack2, 1024);
    printf("this is main of test.c\n");

    rev = os_semaphore_create_counting(&sem, "testsem", 10, 1);
    printf("os_semaphore create rev:%d\n", rev);
    rev = os_semaphore_put(&sem);
    printf("os_semaphore create rev:%d\n", rev);

    rev = os_mutex_create(&mutex, "testmutex", OS_MUTEX_NO_INHERIT);
    printf("os_mutex_create rev:%d\n", rev);
     
    rev = os_thread_create(&handle1, NULL, entry1, "parm1", &stack1, 0);
    printf("create entry1 rev:%d handle:0x%x\n", rev, (unsigned int)handle1);

    rev = os_thread_create(&handle2, NULL, entry2, "parm2", &stack2, 0);
    printf("create entry2 rev:%d handle:0x%x\n", rev, (unsigned int)handle2);

    os_thread_sleep(10000);
    printf("delete thread1\n");
    os_thread_delete(&handle1);
    os_thread_sleep(10000);
    printf("delete thread2\n");
    os_thread_delete(&handle2);

    printf("plat_test over\n");
}

#endif

#if 0
void os_thread_stack_free_show(void)
{
    char* name;
    uint32_t free_word;
    name = pcTaskGetName(NULL);
    free_word = INCLUDE_uxTaskGetStackHighWaterMark(NULL); 
    printf("[os]------------------------------->[%s]stack min free:%d\n", name, free_word);
}
#endif

char* list_buf = NULL;

#if 1

void os_thread_list_show(void)
{
    if(list_buf == NULL)
    {
        list_buf = os_mem_alloc(4 * 1024 + 128);
    }
    vTaskList(list_buf);
    printf("%s\n", list_buf?:"");
}

#else

typedef struct
{
    char name[16];
    char status[2];
    int32_t prio;
    int32_t size;
    int32_t num;
} task_list_s;
task_list_s task_list[20] = {0};

void os_thread_list_show(void)
{
    if(list_buf == NULL)
    {
        list_buf = os_mem_alloc(4 * 1024 + 128);
    }
    vTaskList(list_buf);
    {
      char *p;
      int i;
      int flag = 0;
      p = strtok(list_buf, "\r\n");
      while (p != NULL)
      {
          sscanf(p, "%s%s%d%d%d", task_list[0].name?:"", task_list[0].status?:"", &task_list[0].prio, &task_list[0].size, &task_list[0].num);
          for (i=1; i<20; i++)
          {
              if (strlen(task_list[i].name) == 0)
              {
                  memcpy(&task_list[i], &task_list[0], sizeof(task_list_s));
                  flag = 1;
                  break;
              }
              else if (strcmp(task_list[i].name, task_list[0].name) == 0)
              {
                  if (task_list[i].size > task_list[0].size)
                  {
                      task_list[i].size = task_list[0].size;
                      flag = 1;
                  }
                  break;
            }
          }
          p = strtok(NULL, "\r\n");
      }
      if (flag)
      {
          for (i=1; i<20; i++)
          {
              if (strlen(task_list[i].name) == 0)
              {
                break;
              }
              printf("%-14s\t%s\t\%d\t%d\t%d\r\n", task_list[i].name, task_list[i].status?:"", task_list[i].prio, task_list[i].size, task_list[i].num);
          }
      }
    }
}
#endif