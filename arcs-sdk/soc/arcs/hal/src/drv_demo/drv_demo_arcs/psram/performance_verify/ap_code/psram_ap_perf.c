/*
 * timer_nos_chk.c
 *
 *      Author: USER
 */
#include "types.h"
#include "systick.h"
#include "arcs_ap.h"

#include "cache.h"

#include "log_print.h"
#include "IOMuxManager.h"
#include "PSRAMManager.h"
#include "ClockManager.h"
#include "systick.h"
#include "dma.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define PSRAM_LOOP_TEST                   0

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 100000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void PSRAM_PerformanceVerify_FlashMemory2PSRAM(void);
static void PSRAM_PerformanceVerify_Sram2PSRAM(void);
static void PSRAM_PerformanceVerify_RandomAccess_Sram2PSRAM(void);
static void PSRAM_PerformanceVerify_PSRAM2Sram(void);
static void PSRAM_PerformanceVerify_RandomAccess_PSRAM2Sram(void);
static void PSRAM_PerformanceVerify_Sram2PSRAM_DMA(void);
static void PSRAM_PerformanceVerify_PSRAM2Sram_DMA(void);
static void PSRAM_PerformanceVerify_ParallelMemcpyAndDMA(void);

static function test_function_array[] = {
	// PSRAM_PerformanceVerify_FlashMemory2PSRAM,
	PSRAM_PerformanceVerify_Sram2PSRAM,
    PSRAM_PerformanceVerify_RandomAccess_Sram2PSRAM,
	PSRAM_PerformanceVerify_PSRAM2Sram,
    PSRAM_PerformanceVerify_RandomAccess_PSRAM2Sram,
	PSRAM_PerformanceVerify_Sram2PSRAM_DMA,
	PSRAM_PerformanceVerify_PSRAM2Sram_DMA,
    PSRAM_PerformanceVerify_ParallelMemcpyAndDMA,
};

static uint32_t error_times = 0;

#define PERFORMANCE_VERIFY_PSRAMC_PREFETCH_DEEP         64

#define PERFORMANCE_VERIFY_MTIME_FRQ                    1000000
#define PERFORMANCE_VERIFY_FLASH_MEMORY                (uint32_t*)CMN_FLASH_REGION
#define PERFORMANCE_VERIFY_FLASH_SIZE                  (uint32_t)(1024*1024)
#define PERFORMANCE_VERIFY_PSRAM_MEMORY                (uint32_t*)CMN_PSRAM_REGION
#define PERFORMANCE_VERIFY_LOOP_COUNT                   128
#define PERFORMANCE_VERIFY_SRAM_MEMORY                 (uint32_t*)CP_DLM_RAM_REGION
#define PERFORMANCE_VERIFY_SRAM_SIZE                   (uint32_t)(CP_DLM_RAM_REGION_SIZE)
#define PERFORMANCE_VERIFY_SRAM_MEMORY_DMA             (uint32_t*)AP_DLM_RAM_REGION
#define PERFORMANCE_VERIFY_SRAM_SIZE_DMA               (uint32_t)(AP_DLM_RAM_REGION_SIZE)

static uint32_t volatile DMAEvent;

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    DMAEvent = event_info & 0xFF;
}

static int32_t psram_memcpy_performance(void* dst, void* src, uint32_t size){
    // size      :  size of sample

    // wsize = 0 :  byte
    //       = 1 :  half word
    //       = 2 :  word


    int32_t stat = 0;

    uint8_t ch = 0;
    // Select channel
    dma_channel_select(&ch, DMA_DrvEvent, 0, DMA_CACHE_SYNC_BOTH);

    stat = dma_channel_configure (ch,
                (uint32_t) src,
                (uint32_t) dst,
                size,
                DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) |\
              DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) |\
			  DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |\
              DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
              DMA_CH_CFGL_CH_PRIOR(1), // config_low
              DMA_CH_CFGH_FIFO_MODE, // config_high
              0, 0);

    if (stat == -1){
        return -2;
    }

    // Block
    while(1){
        // Complete
        if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
        // Error
        if(DMAEvent & DMA_EVENT_ERROR){
            return -3;
        }
    }

    return 0;
}

static int32_t psram_memcpy_performance_async(void* dst, void* src, uint32_t size){
    // size      :  size of sample

    // wsize = 0 :  byte
    //       = 1 :  half word
    //       = 2 :  word

    int32_t stat = 0;

    uint8_t ch = 0;
    // Select channel
    dma_channel_select(&ch, DMA_DrvEvent, 0, DMA_CACHE_SYNC_BOTH);

    stat = dma_channel_configure (ch,
                (uint32_t) src,
                (uint32_t) dst,
                size,
                DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) |\
              DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) |\
			  DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2M |\
              DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
              DMA_CH_CFGL_CH_PRIOR(1), // config_low
              DMA_CH_CFGH_FIFO_MODE, // config_high
              0, 0);

    if (stat == -1){
        return -2;
    }

    return 0;
}

static int32_t psram_memcpy_sync_func(void){
    // Block
    while(1){
        // Complete
        if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
        // Error
        if(DMAEvent & DMA_EVENT_ERROR){
            return -3;
        }
    }

    return 0;
}

static void PSRAM_PerformanceVerify_FlashMemory2PSRAM(){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM FLASH TO PSRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_FLASH_MEMORY,  PERFORMANCE_VERIFY_PSRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
    	memcpy(PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_FLASH_MEMORY, PERFORMANCE_VERIFY_FLASH_SIZE);
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] Flash to PSRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_FLASH_SIZE);
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        double t_speed = (double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_FLASH_SIZE) / t_time;
        CLOGD("[Performance] Flash to PSRAM speed -> %d", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_Sram2PSRAM(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM SRAM TO PSRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_SRAM_MEMORY,  PERFORMANCE_VERIFY_PSRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
    	memcpy(PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE);
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] SRAM to PSRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE) / t_time);
        CLOGD("[Performance] SRAM to PSRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_RandomAccess_Sram2PSRAM(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] RANDOM ACCESS FROM SRAM TO PSRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_SRAM_MEMORY,  PERFORMANCE_VERIFY_PSRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
        for (uint32_t j = 0; j < PERFORMANCE_VERIFY_SRAM_SIZE; j++){
            *(PERFORMANCE_VERIFY_PSRAM_MEMORY + j * PERFORMANCE_VERIFY_PSRAMC_PREFETCH_DEEP) = *(PERFORMANCE_VERIFY_SRAM_MEMORY + j);
        }
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance][RANDOM] SRAM to PSRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE) / t_time);
        CLOGD("[Performance][RANDOM] SRAM to PSRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_PSRAM2Sram(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM PSRAM TO SRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_PSRAM_MEMORY , PERFORMANCE_VERIFY_SRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
    	memcpy(PERFORMANCE_VERIFY_SRAM_MEMORY, PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE);
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] PSRAM to SRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE) / t_time);
        CLOGD("[Performance] PSRAM to SRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_RandomAccess_PSRAM2Sram(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] RANDOM ACCESS FROM PSRAM TO SRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_PSRAM_MEMORY,  PERFORMANCE_VERIFY_SRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
        for (uint32_t j = 0; j < PERFORMANCE_VERIFY_SRAM_SIZE; j++){
            *(PERFORMANCE_VERIFY_SRAM_MEMORY + j) = *(PERFORMANCE_VERIFY_PSRAM_MEMORY + j * PERFORMANCE_VERIFY_PSRAMC_PREFETCH_DEEP);
        }
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance][RANDOM] PSRAM to SRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE) / t_time);
        CLOGD("[Performance][RANDOM] PSRAM to SRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_Sram2PSRAM_DMA(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM SRAM TO PSRAM WITH DMA 0x%x --- 0x%x Bytes\r\n"
                      "wait................",PERFORMANCE_VERIFY_SRAM_MEMORY_DMA,  PERFORMANCE_VERIFY_PSRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
        psram_memcpy_performance(PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_MEMORY_DMA, PERFORMANCE_VERIFY_SRAM_SIZE_DMA/4);
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] SRAM to PSRAM %d + %d ----> %d Bytes", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA) / t_time);
        CLOGD("[Performance] SRAM to PSRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_PSRAM2Sram_DMA(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM PSRAM TO SRAM WITH DMA 0x%x --- 0x%x Bytes\r\n"
                      "wait................",PERFORMANCE_VERIFY_PSRAM_MEMORY,  PERFORMANCE_VERIFY_SRAM_MEMORY_DMA);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
        psram_memcpy_performance(PERFORMANCE_VERIFY_SRAM_MEMORY_DMA, PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE_DMA/4);
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] SRAM to PSRAM %d + %d ----> %d Bytes", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA) / t_time);
        CLOGD("[Performance] SRAM to PSRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_ParallelMemcpyAndDMA(void) {
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] PARALLEL MEMCPY TO PSRAM AND DMA FROM PSRAM\r\n"
                      "wait................");

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    uint32_t j = 0;

    systime_1 = SysTick_Time();

    for (uint16_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++) {
        // Start DMA from PSRAM to SRAM
        psram_memcpy_performance_async(PERFORMANCE_VERIFY_SRAM_MEMORY_DMA, PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE_DMA / 4);

        while(1){
            *(PERFORMANCE_VERIFY_PSRAM_MEMORY + j++ * PERFORMANCE_VERIFY_PSRAMC_PREFETCH_DEEP) = 0x5a5a5a5a;

            // Complete
            if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
                DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
                break;
            }
            // Error
            if(DMAEvent & DMA_EVENT_ERROR){
                error_times++;
                CLOGD("[ERROR] Parallel Memcpy to PSRAM and DMA from PSRAM error");

                while (1);
            }
        }
    }

    systime_2 = SysTick_Time();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] Parallel Write to PSRAM and DMA from PSRAM %d + %d ----> %d Bytes", 
              (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), 
              (uint32_t)(systime_diff & 0xFFFFFFFF), 
              (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA) / t_time);
        CLOGD("[Performance] Parallel Memcpy to PSRAM and DMA from PSRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

int main(){
    uint32_t times;
    uint32_t error = 0;
    logInit(0, 115200);
    CLOGD("[PSRAM TEST] BEGIN...");
    dma_initialize();
    PSRAM_Initialize(NULL, NULL, 1);
    CLOGD("[TEST] PSRAM CONFIG COMPLETE");

    HAL_DisableDCache();

    /* PSRAM Initialize Complete */
    srand(0);

    do {
        for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
            test_function_array[times]();
            if(error_times != 0){
                error++;
            }
            CLOGD("[TEST] TOTAL ERROR TIMES: %d", error);
        }
    } while(PSRAM_LOOP_TEST);

    CLOGD("[TEST] CLOSE");
    while(1);
}
