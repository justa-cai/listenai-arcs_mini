/*
 * aon_timer_nos_chk.c
 *
 *  Created on: Apr 6, 2022
 *      Author: USER
 */
#include "log_print.h"
#include "systick.h"
#include "dma.h"
#include "chip.h"
#include "cache.h"
#include "PSRAMManager.h"

#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 1000000){\
            break;\
        }\
    }\
    }while(0)

typedef int32_t (*function)(void);

static int32_t HAL_CACHE_Get_Cache_Info(void);
static int32_t HAL_CACHE_ICache_OnOff(void);
static int32_t HAL_CACHE_DCache_OnOff(void);
static int32_t HAL_CACHE_ICache_Invalidate_All(void);
static int32_t HAL_CACHE_ICache_Invalidate_Addr(void);
static int32_t HAL_CACHE_DCache_Invalidate_All(void);
static int32_t HAL_CACHE_DCache_Invalidate_Addr(void);
static int32_t HAL_CACHE_DCache_Flush_All(void);
static int32_t HAL_CACHE_DCache_Flush_Addr(void);
static int32_t HAL_CACHE_ICache_LockUnlock(void);
static int32_t HAL_CACHE_DCache_LockUnlock(void);

static function test_function_array[] = {
    HAL_CACHE_Get_Cache_Info,
//    HAL_CACHE_ICache_OnOff,
//    HAL_CACHE_DCache_OnOff,
//    HAL_CACHE_DCache_Invalidate_All,
//    HAL_CACHE_DCache_Invalidate_Addr,
//    HAL_CACHE_DCache_Flush_All,
//    HAL_CACHE_DCache_Flush_Addr,
//    HAL_CACHE_ICache_Invalidate_All,
//    HAL_CACHE_ICache_Invalidate_Addr,
//    HAL_CACHE_ICache_LockUnlock,
//    HAL_CACHE_DCache_LockUnlock,
};

//********************************************** DMA
static uint32_t volatile DMAEvent;

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    DMAEvent = event_info & 0xFF;
}

static int32_t dcache_memcpy(void* dst, void* src, uint32_t size){
    // size      :  size of sample

    // wsize = 0 :  byte
    //       = 1 :  half word
    //       = 2 :  word


    int32_t stat = 0;

    uint8_t ch = 0;
    // Select channel
    dma_channel_select(&ch, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);

    stat = dma_channel_configure (ch,
                (uint32_t) src,
                (uint32_t) dst,
                size,
                DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_BYTE) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_BYTE) |\
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


static int32_t HAL_CACHE_Get_Cache_Info(void)
{
	CacheInfo_Type dcache_info = {0};
	CacheInfo_Type icache_info = {0};

	CLOGD("[%s:%d]", __func__, __LINE__);

	GetICacheInfo(&icache_info);
	GetDCacheInfo(&dcache_info);

	CLOGD("ICACHE INFO---> {{ line size: %d, ways: %d, set per way: %d,"
			" size: %d}}", icache_info.linesize, icache_info.ways,
			icache_info.setperway, icache_info.size);

	CLOGD("DCACHE INFO---> {{ line size: %d, ways: %d, set per way: %d,"
			" size: %d}}", dcache_info.linesize, dcache_info.ways,
			dcache_info.setperway, dcache_info.size);

	return 0;
}


__attribute__((optimize("O0"))) static int32_t HAL_CACHE_ICache_OnOff(void)
{
    CLOGD("ICACHE On and Off verify");

#define ICACHE_ONOFF_LOOP              100
    for (uint32_t loop = ICACHE_ONOFF_LOOP; loop > 0; loop--){
        HAL_EnableICache();
        HAL_DisableICache();
    }

    HAL_DisableICache();

#undef ICACHE_ONOFF_LOOP

    return 0;
}


__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_OnOff(void)
{
    CLOGD("DCACHE On and Off verify");

#define DCACHE_ONOFF_LOOP              100
    for (uint32_t loop = DCACHE_ONOFF_LOOP; loop > 0; loop--){
        HAL_EnableDCache();
        HAL_DisableDCache();
    }

    HAL_DisableDCache();
#undef DCACHE_ONOFF_LOOP

    return 0;
}


#define ICACHE_LOCK_ASSUME_SIZE_B    128
#define ICACHE_LOCK_ASSUME_SIZE_W    ICACHE_LOCK_ASSUME_SIZE_B/4
#define ICACHE_CODE_SIZE_B           0x20       // It is the size of code_in_cache_code0() in cache.dis

__attribute__((optimize("O0"))) __attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static void code_in_cache_code0(uint8_t loop){
    CLOGD("code0");
    CLOGD("ICACHE DMA Copy compelete, and code run success, %d", loop);
}

__attribute__((optimize("O0"))) __attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static void code_in_cache_code1(uint8_t loop){
    CLOGD("code1");
    CLOGD("ICACHE DMA Copy compelete, and code run success, %d", loop);
}

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_ICache_Invalidate_All(void){
    int32_t ret = 0;
    uint32_t i = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    HAL_EnableICache();

    void (*funcptr)(uint8_t) = code_in_cache_code0;

    CLOGD("ICACHE Lock verify function begin address: *0x%x=0x%x", funcptr, *(uint32_t*)funcptr);
    CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

    dma_initialize();

    for (i = 0; i < 5; i++){
        if (i == 1){
            ret = dcache_memcpy((uint32_t*)funcptr, (uint32_t*)code_in_cache_code1, ICACHE_CODE_SIZE_B);
            if(ret) {
                CLOGD("[%s:%d] dcache_memcpy error ret=%d", __func__, __LINE__, ret);
                return -1;
            }
        }
        if (i == 3){
            HAL_InvalidateICache();
        }

        CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

        code_in_cache_code0(i);
    }

    CLOGD("ICACHE Lock verify function begin address: *0x%x=0x%x", funcptr, *(uint32_t*)funcptr);
    CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

    HAL_DisableICache();

    return 0;
}

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_ICache_Invalidate_Addr(void){
    int32_t ret = 0;
    uint32_t i = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    HAL_EnableICache();

    void (*funcptr)(uint8_t) = code_in_cache_code0;

    CLOGD("ICACHE Lock verify function begin address: *0x%x=0x%x", funcptr, *(uint32_t*)funcptr);
    CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

    dma_initialize();

    for (i = 0; i < 5; i++){
        if (i == 1){
            ret = dcache_memcpy((uint32_t*)funcptr, (uint32_t*)code_in_cache_code1, ICACHE_CODE_SIZE_B);
            if(ret) {
                CLOGD("[%s:%d] dcache_memcpy error ret=%d", __func__, __LINE__, ret);
                return -1;
            }
        }
        if (i == 3){
            HAL_InvalidateICache_by_Addr((uint32_t*)funcptr, ICACHE_LOCK_ASSUME_SIZE_B);
        }

        CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

        code_in_cache_code0(i);
    }

    CLOGD("ICACHE Lock verify function begin address: *0x%x=0x%x", funcptr, *(uint32_t*)funcptr);
    CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

    HAL_DisableICache();

    return 0;
}


/* nop1=2Byte  nop100=200Byte nop1000=2000Byte nop10000=20000Byte */
#define nop100()   do { \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
        __ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop");__ASM volatile("nop"); \
    } while(0)

#define nop1000()   do { \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
        nop100(); \
    } while(0)

#define nop10000()   do { \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
        nop1000(); \
    } while(0)

__attribute__((optimize("O0"))) __attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static void test_icache_nop10000(void){
    nop10000();
}

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_ICache_LockUnlock(void){
    int32_t ret = 0;
    uint32_t i = 0;
    void (*funcptr)(uint8_t) = code_in_cache_code0;
    uint8_t lock_enable = 1;

    CLOGD("[%s:%d]", __func__, __LINE__);

    //PSRAM_Initialize(NULL, NULL, 1);
    dma_initialize();

    HAL_EnableICache();
    HAL_EnableDCache();

    CLOGD("ICACHE Lock verify function begin address: *0x%x=0x%x", funcptr, *(uint32_t*)funcptr);
    CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

    code_in_cache_code0(1);

    if(lock_enable) {
        CLOGD("[%s:%d] LockICache", __func__, __LINE__);
        HAL_LockICache_by_Addr((uint32_t*)funcptr, 0x60);
    } else {
        CLOGD("[%s:%d] UnLockICache", __func__, __LINE__);
    }

    ret = dcache_memcpy((uint32_t*)funcptr, (uint32_t*)code_in_cache_code1, ICACHE_CODE_SIZE_B);
    if(ret) {
        CLOGD("[%s:%d] dcache_memcpy error ret=%d", __func__, __LINE__, ret);
        return -1;
    }

    //test_icache_nop10000();

    /*   LockICache: print "code1" */
    /* UnLockICache: print "code0" */
    code_in_cache_code0(2);

    CLOGD("ICACHE Lock verify function begin address: *0x%x=0x%x", funcptr, *(uint32_t*)funcptr);
    CLOGD("address: *0x%x=0x%x", funcptr + ICACHE_CODE_SIZE_B - 0x4, *(uint32_t*)(funcptr + ICACHE_CODE_SIZE_B - 0x4));

    HAL_DisableICache();

    return 0;
}


#define HAL_DCACHE_VERIFY_COUNT                      4
__attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) uint8_t* dcache_verify_src = (uint8_t*)0x20080000;
__attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) uint8_t* dcache_verify_dst = (uint8_t*)0x20090000;

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_Invalidate_All(void)
{
    // Source init
    uint32_t i = 0;
    uint8_t err_cnt = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    dma_initialize();

    HAL_EnableDCache();
    HAL_DisableICache();

    // Dst all in cache
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        *(dcache_verify_dst + i) = i * 2 + 1;
    }

    // Src all in cache
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        *(dcache_verify_src + i) = i;
    }

    // DMA copy in memory
    dcache_memcpy(dcache_verify_dst, dcache_verify_src, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);


    CLOGD("[%s:%d]", __func__, __LINE__);
    err_cnt = 0;
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (*(dcache_verify_dst + i) == *(dcache_verify_src + i)){
            err_cnt++;
        }
    }
    if (err_cnt){
        CLOGD("[DCACH] Invalidate error -> DMA Stage");
        HAL_DisableDCache();
        return err_cnt;
    }


    // Invalidate all dcache, then dst and src is accordance
    HAL_InvalidateDCache();

    CLOGD("[%s:%d]", __func__, __LINE__);
    err_cnt = 0;
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (*(dcache_verify_dst + i) != *(dcache_verify_src + i)){
            err_cnt++;
        }
    }
    if (err_cnt){
        CLOGD("[DCACH] Invalidate error -> Invalidate Stage");
        HAL_DisableDCache();
        return err_cnt;
    } else {
        CLOGD("[DCACH] Invalidate pass");
    }


    HAL_DisableDCache();

    return 0;
}

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_Flush_All(void){
    // Source init
    uint32_t i = 0;
    uint8_t err_cnt = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    dma_initialize();

    HAL_EnableDCache();
    HAL_DisableICache();

    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        dcache_verify_dst[i] = i * 2 + 1; // in dcache
    }

    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        dcache_verify_src[i] = i; // in dcache
    }

    dcache_memcpy(dcache_verify_dst, dcache_verify_src, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT); // new dest data not in dcache


    CLOGD("[%s:%d]", __func__, __LINE__);
    err_cnt = 0;
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (dcache_verify_dst[i] == dcache_verify_src[i]){
            err_cnt++;
        }
    }
    if (err_cnt){
        CLOGD("[DCACH] Flush error -> DMA Stage");
        HAL_DisableDCache();
        return err_cnt;
    }


    HAL_FlushDCache(); // Write back

    CLOGD("[%s:%d]", __func__, __LINE__);
    err_cnt = 0;
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (dcache_verify_dst[i] == dcache_verify_src[i]){
            err_cnt++;
        }
    }
    if (err_cnt){
        CLOGD("[DCACH] Flush error -> Flush Stage");
        HAL_DisableDCache();
        return err_cnt;
    } else {
        CLOGD("[DCACH] Flush pass");
    }


    // Invalidate all dcache, then dst and src is accordance
    HAL_InvalidateDCache();

    CLOGD("[%s:%d]", __func__, __LINE__);
    err_cnt = 0;
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (*(dcache_verify_dst + i) == *(dcache_verify_src + i)){
            err_cnt++;
        }
    }
    if (err_cnt){
        CLOGD("[DCACH] Invalidate error -> Invalidate Stage");
        HAL_DisableDCache();
        return err_cnt;
    } else {
        CLOGD("[DCACH] Invalidate pass");
    }


    HAL_DisableDCache();

    return 0;
}

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_Invalidate_Addr(void){
    // Source init
    uint32_t i = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    dma_initialize();

    HAL_EnableDCache();
    HAL_DisableICache();

    uint8_t value = 7;
    uint8_t value_r = 0;
    uint8_t * ptr = (uint8_t *)dcache_verify_src;
    uint8_t * ref_ptr = (uint8_t*)dcache_verify_dst;
    uint8_t * misaligned_prt_31B = (uint8_t*)((uint32_t)ptr + 31);
    uint8_t * misaligned_prt_1B = (uint8_t*)((uint32_t)ptr + 1);

    for(i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        ref_ptr[i] = i;
    }

    *misaligned_prt_31B = 0x1;
    // If not use invalidate, the value_r = 1, otherwise = 7
    HAL_InvalidateDCache_by_Addr((uint32_t*)misaligned_prt_31B, 1);

    dcache_memcpy(misaligned_prt_31B, &value, 1);

    value_r = *misaligned_prt_31B;
    if (value_r == 7){
        CLOGD("[DCACHE][Invalidate][ADDR] Pass -> BYTE");
    } else {
        CLOGD("[DCACHE][Invalidate][ADDR] Error -> BYTE");
        HAL_DisableDCache();
        return -1;
    }

    uint16_t s_value = 0x5a6b;
    uint16_t s_value_r = 0;

    *misaligned_prt_31B = 0x2;
    *(misaligned_prt_31B + 1) = 0x3;
    // If not use invalidate, the s_value_r = 0x302, otherwise = 0x5a6b
    HAL_InvalidateDCache_by_Addr((uint32_t*)misaligned_prt_31B, 2);
    dcache_memcpy(misaligned_prt_31B, &s_value, 2);

    s_value_r = *((uint16_t*)misaligned_prt_31B);

    if (s_value_r == 0x5a6b){
        CLOGD("[DCACHE][Invalidate][ADDR] Pass -> SHORT");
    } else {
        CLOGD("[DCACHE][Invalidate][ADDR] Error -> SHORT");
        HAL_DisableDCache();
        return -2;
    }
    
    // Take ptr all data in cache line, and set value to 0xAA
    memset(ptr, 0xAA, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);
    // cover 1 cache line
    HAL_InvalidateDCache_by_Addr((uint32_t*)misaligned_prt_1B, 31);
    dcache_memcpy(ptr, ref_ptr, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (ptr[i] != ref_ptr[i]){
            if (i < HAL_DCACHE_CFG_LINE_SIZE){
                CLOGD("[DCACHE][Invalidate][ADDR] Error -> COVER LINE(1)");
                HAL_DisableDCache();
                return -3;
            }
        }
    }
    
    CLOGD("[DCACHE][Invalidate][ADDR] Pass -> COVER LINE(2)");

    // Take ptr all data in cache line
    memcpy(ptr, ref_ptr, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);
    // misaligned_prt_31B align 31 bytes, so when dma copy 2 bytes data will cover 2 cache line
    HAL_InvalidateDCache_by_Addr((uint32_t*)misaligned_prt_31B, 2);

    // Write allocate
    for(i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (ptr[i] != ref_ptr[i]){
            if (i >= HAL_DCACHE_CFG_LINE_SIZE * 2){
                CLOGD("[DCACHE][Invalidate][ADDR] Error -> COVER LINE");
                HAL_DisableDCache();
                return -4;
            }
        }
    }

    CLOGD("[DCACHE][Invalidate][ADDR] Pass -> COVER LINE");

    HAL_DisableDCache();

    return 0;
}

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_Flush_Addr(void){
    // Source init
    uint32_t i = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    dma_initialize();

    HAL_EnableDCache();
    HAL_DisableICache();

    uint8_t value = 7;
    uint8_t value_r = 0;
    uint8_t * ptr = (uint8_t *)dcache_verify_src;
    uint8_t * ref_ptr = (uint8_t*)dcache_verify_dst;
    uint8_t * misaligned_prt_31B = (uint8_t*)((uint32_t)ptr + 31);
    uint8_t * misaligned_prt_1B = (uint8_t*)((uint32_t)ptr + 1);

    for(i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        ref_ptr[i] = i;
    }

    *misaligned_prt_31B = 0x1;    

    dcache_memcpy(misaligned_prt_31B, &value, 1);
    // If not use flush, the value_r = 1, otherwise = 7
    HAL_FlushDCache_by_Addr((uint32_t*)misaligned_prt_31B, 1);

    value_r = *misaligned_prt_31B;
    if (value_r == 1){
        CLOGD("[DCACHE][Flush][ADDR] Pass -> BYTE");
    } else {
        CLOGD("[DCACHE][Flush][ADDR] Error -> BYTE");
        HAL_DisableDCache();
        return -1;
    }


    uint16_t s_value = 0x5a6b;
    uint16_t s_value_r = 0;

    *misaligned_prt_31B = 0x2;
    *(misaligned_prt_31B + 1) = 0x3;
    dcache_memcpy(misaligned_prt_31B, &s_value, 2);

    // If not use flush, the s_value_r = 0x302, otherwise = 0x5a6b
    HAL_FlushDCache_by_Addr((uint32_t*)misaligned_prt_31B, 2);

    s_value_r = *((uint16_t*)misaligned_prt_31B);

    if (s_value_r == 0x302){
        CLOGD("[DCACHE][Flush][ADDR] Pass -> SHORT");
    } else {
        CLOGD("[DCACHE][Flush][ADDR] Error -> SHORT");
        HAL_DisableDCache();
        return -2;
    }
    

    // Take ptr all data in cache line, and set value to 0xAA
    memset(ptr, 0xAA, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);
    HAL_FlushDCache_by_Addr((uint32_t *)ptr, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);

    // Set all cache to 0xBB, and memory is all 0xAA
    memset(ptr, 0xBB, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);
    // cover 1 cache line
    HAL_FlushDCache_by_Addr((uint32_t*)misaligned_prt_1B, 31);

    // Invalidate all cache line to memory
    HAL_InvalidateDCache_by_Addr((uint32_t*)ref_ptr, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);

    // DMA copy ptr to ref, and first cacheline is 0xBB
    dcache_memcpy(ref_ptr, ptr, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);

//    for(i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
//        CLOGD("%d ptr[%d]=0x%x  ref_ptr[%d]=0x%x", __LINE__, i, ptr[i], i, ref_ptr[i]);
//    }
    // ptr = 0xBB and ref = 0xBB(first cacheline) and 0xBB(3 cacheline)
    for (i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (ptr[i] != ref_ptr[i]){
            if (i < HAL_DCACHE_CFG_LINE_SIZE){
                CLOGD("[DCACHE][Invalidate][ADDR] Error -> COVER LINE(1)");
                HAL_DisableDCache();
                return -3;
            }
        }
    }
    
    CLOGD("[DCACHE][Invalidate][ADDR] Pass -> COVER LINE(2)");


    // Take ptr all data in cache line
    memcpy(ptr, ref_ptr, HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT);
    // misaligned_prt_31B align 31 bytes, so when dma copy 2 bytes data will cover 2 cache line
    HAL_FlushDCache_by_Addr((uint32_t*)misaligned_prt_31B, 2);

    // Write allocate
    for(i = 0; i < HAL_DCACHE_CFG_LINE_SIZE * HAL_DCACHE_VERIFY_COUNT; i++){
        if (ptr[i] != ref_ptr[i]){
            if (i >= HAL_DCACHE_CFG_LINE_SIZE * 2){
                CLOGD("[DCACHE][Invalidate][ADDR] Error -> COVER LINE");
                HAL_DisableDCache();
                return -4;
            }
        }
    }

    CLOGD("[DCACHE][Invalidate][ADDR] Pass -> COVER LINE");

    HAL_DisableDCache();

    return 0;
}


#if 1
/*
 *  #define HAL_ICACHE_RAM_SIZE                         (32*1024)
 *  #define HAL_DCACHE_RAM_SIZE                         (16*1024)
 *  DBG:ICACHE INFO---> {{ line size: 32, ways: 2, set per way: 256, size: 16384}}
 *  DBG:DCACHE INFO---> {{ line size: 32, ways: 2, set per way: 128, size: 8192}}
 *
 */
#define HAL_DCACHE_TABLE0_SIZE_B        (128)
#define HAL_DCACHE_TABLE1_SIZE_B        (8*1024)    // must more 8192

#if 1
__attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static uint8_t table0[HAL_DCACHE_TABLE0_SIZE_B]={0};
__attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static uint8_t table1[HAL_DCACHE_TABLE1_SIZE_B]={0};
#else
static uint8_t *table0 = (uint8_t *)0x28000000;
static uint8_t *table1 = (uint8_t *)0x28010000;
#endif

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_LockUnlock(void){
    int32_t ret = 0;
    uint32_t i = 0;
    uint8_t run_times = 10;
    uint8_t lock_enable = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    //PSRAM_Initialize(NULL, NULL, 1);
    dma_initialize();

    HAL_EnableDCache();
    HAL_DisableICache();

    while(run_times--)
    {
        if(run_times & 0x1) {
            lock_enable = 1;
        } else {
            lock_enable = 0;
        }

        for(i = 0; i < HAL_DCACHE_TABLE1_SIZE_B; i++){
            table1[i] = (i + 10) & 0xFF;
        }
        HAL_FlushDCache_by_Addr((uint32_t*)table1, HAL_DCACHE_TABLE1_SIZE_B);

        for(i = 0; i < HAL_DCACHE_TABLE0_SIZE_B; i++){
            table0[i] = i & 0xFF;
        }
        HAL_FlushDCache_by_Addr((uint32_t*)table0, HAL_DCACHE_TABLE0_SIZE_B);

        if(lock_enable) {
            CLOGD("[%s:%d] LockDCache", __func__, __LINE__);
            HAL_LockDCache_by_Addr((uint32_t*)table0, HAL_DCACHE_TABLE0_SIZE_B);
        } else {
            CLOGD("[%s:%d] UnLockDCache", __func__, __LINE__);
        }

        ret = dcache_memcpy((uint32_t*)table0, (uint32_t*)table1, HAL_DCACHE_TABLE0_SIZE_B);
        if(ret) {
            CLOGD("[%s:%d] dcache_memcpy error ret=%d", __func__, __LINE__, ret);
            return -1;
        }

        for(i = 0; i < HAL_DCACHE_TABLE1_SIZE_B; i += 2) {
            table1[i] += 10;
        }
        for(i = 1; i < HAL_DCACHE_TABLE1_SIZE_B; i += 2) {
            table1[i] += 10;
        }

//        for(i = 0; i < 100; i++){
//            CLOGD("table0[%d]=%d", i, table0[i]);
//        }

        if(lock_enable) {
            for(i = 0; i < HAL_DCACHE_TABLE0_SIZE_B; i++)
            {
                if(table0[i] != (i & 0xFF)) {
                    CLOGD("[%s:%d] test failed", __func__, __LINE__);
                    CLOGD("table0[%d]=%d", i, table0[i]);
                    return -1;
                }
            }
        } else {
            for(i = 0; i < HAL_DCACHE_TABLE0_SIZE_B; i++)
            {
                if(table0[i] != ((i + 10) & 0xFF)) {
                    CLOGD("[%s:%d] test failed", __func__, __LINE__);
                    CLOGD("table0[%d]=%d", i, table0[i]);
                    return -1;
                }
            }
        }

        if(lock_enable) {
            HAL_UnLockDCache_by_Addr((uint32_t*)table0, HAL_DCACHE_TABLE0_SIZE_B);
        }
    }

    CLOGD("[%s:%d] test success", __func__, __LINE__);
    return 0;
}

#else

#define HAL_DCACHE_TABLE0_SIZE_B        (32*1024)
#define HAL_DCACHE_TABLE1_SIZE_B        (64*1024)
#define ICACHE_LOCK_CODE_SIZE_B         0x50
#if 0
__attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static uint8_t table0[HAL_DCACHE_TABLE0_SIZE_B]={0};
__attribute__((aligned(HAL_DCACHE_CFG_LINE_SIZE))) static uint8_t table1[HAL_DCACHE_TABLE1_SIZE_B]={0};
#else
static uint8_t *table0 = (uint8_t *)0x28000000;
static uint8_t *table1 = (uint8_t *)0x28020000;
#endif

__attribute__((optimize("O0"))) static int32_t HAL_CACHE_DCache_LockUnlock(void){
    int32_t ret = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t times = 10;
    uint64_t start_mtime = 0;
    uint32_t delta_mtime = 0;

    CLOGD("[%s:%d]", __func__, __LINE__);

    PSRAM_Initialize(NULL, NULL, 1);

    HAL_EnableDCache();
    HAL_DisableICache();

    while(times--)
    {
        for(i = 0; i < HAL_DCACHE_TABLE0_SIZE_B; i++){
            table0[i] = i;
        }
        HAL_FlushDCache_by_Addr((uint32_t*)table0, HAL_DCACHE_TABLE0_SIZE_B);

        for(i = 0; i < HAL_DCACHE_TABLE1_SIZE_B; i++){
            table1[i] = i;
        }
        HAL_FlushDCache_by_Addr((uint32_t*)table1, HAL_DCACHE_TABLE1_SIZE_B);

        if(times & 0x1) {
            CLOGD("[%s:%d] LockDCache", __func__, __LINE__);
            HAL_LockDCache_by_Addr((uint32_t*)table1, HAL_DCACHE_TABLE1_SIZE_B);
        } else {
            CLOGD("[%s:%d] UnLockDCache", __func__, __LINE__);
            HAL_UnLockDCache_by_Addr((uint32_t*)table1, HAL_DCACHE_TABLE1_SIZE_B);
        }

        start_mtime = SysTimer_GetLoadValue();
        for(j = 0; j < 100; j++){
            for(i = 0; i < HAL_DCACHE_TABLE0_SIZE_B; i++){
                table0[i] = i;
            }
        }
        delta_mtime = SysTimer_GetLoadValue() - start_mtime;
        CLOGD("delta_mtime=%ld", delta_mtime);
    }

    CLOGD("[%s:%d]", __func__, __LINE__);
    return 0;
}
#endif


int main()
{
    int32_t ret[sizeof(test_function_array) / sizeof(test_function_array[0])] = {0};
    uint32_t times = 0;

    logInit(0, 115200);

    CLOGD("[%s:%d]", __func__, __LINE__);
    CLOGD("Cache validation");

    // Init PSRAM
    //PSRAM_Initialize(0, 0, 1);

    CLOGD("MSTATUS MPP : 0x%x", __RV_EXTRACT_FIELD(__RV_CSR_READ(CSR_MSTATUS), MSTATUS_MPP));

    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        ret[times] = test_function_array[times]();
    }

    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        if(ret[times]) {
            CLOGD("test case%d failed ret=%d", times, ret[times]);
        } else {
            CLOGD("test case%d success", times);
        }
    }

    CLOGD("[%s:%d]", __func__, __LINE__);
    while(1);
}
