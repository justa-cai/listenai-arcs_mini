#ifndef __WORDSEG_WRAPPER_H__
#define __WORDSEG_WRAPPER_H__

#include "FreeRTOS.h"
#include "log_print.h"
#include "wordseg.h"

// 分词位置
#define RES_WORDSEG_START_ADDR (0x6F61E4C00) // 29898984448

#define WS_HASHRES_FILE_PATH         "/firmware/engine/wordseg/hash.bin"  //分词资源 | 2.9MB
#define WS_DATARES_FILE_PATH         "/firmware/engine/wordseg/data.bin"  //分词资源 | 8.2MB

#define ivGridSize(n, m) ((unsigned int)(((unsigned int)(n) + ((m)-1)) & (~(m) + 1)))

#define LOGD(format, ...) CLOG(format, ##__VA_ARGS__)
#define LOGW(format, ...) CLOGW(format, ##__VA_ARGS__)
#define LOGE(format, ...) CLOGE(format, ##__VA_ARGS__)
typedef struct {
    void    *resident_buf;      //工作内存, PSRAM
    int     resident_buf_size;
    void    *tmp_buf;           //工作内存，PSRAM
    int     tmp_buf_size;

    void    *read_res_buf; // 用于读data.bin/hash.bin回调中裸读TF卡,需要IRAM
    int32_t read_res_buf_size;
    
    wordseg_result_t * result; // 分词结果
    int32_t     result_size;
    text_attr_t   attr[1];   
}ws_engine_t;

int wordseg_init_entry();
int wordseg_uninit_entry();
int wordseg_offline_test_entry();

extern void* os_mem_alloc_ext(unsigned int size, int type);
extern void *os_mem_calloc_ext(unsigned int num, unsigned int size, int32_t type);
extern void* os_mem_realloc_ext(void* old_ptr, unsigned int new_size, int type);
extern void os_mem_free_ext(void* ptr, int type);
extern void os_mem_free(void* ptr);
extern unsigned int os_ticks_get(void);

#endif
