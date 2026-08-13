/*
 * timer_nos_chk.c
 *
 *      Author: USER
 */
#include "types.h"
#include "systick.h"
#include "venusa_ap.h"

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
#include "Driver_GPIO.h"

#include "tinyheap.h"

// PSRAM test case
// ensure disable DCACHE  *********************************************************************************

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

// Read and write back to back -> core
static void PSRAM_CheckData_WriteByRead_Word(void);
static void PSRAM_CheckData_WriteByRead_HWord(void);
static void PSRAM_CheckData_WriteByRead_Byte(void);
// Burst write and read -> core
static void PSRAM_CheckData_ListWR_Word(void);
static void PSRAM_CheckData_ListWR_Half_Word(void);
static void PSRAM_CheckData_ListWR_Byte(void);

static void PSRAM_MemoryCopyVerify(void);
static void PSRAM_MatrixVerify(void);
// Burst write and read -> dma
static void PSRAM_CheckData_DMA_ListWR_Burst16_Bytes(void);
static void PSRAM_CheckData_DMA_ListWR_Burst8_Bytes(void);
static void PSRAM_CheckData_DMA_ListWR_Burst4_Bytes(void);
static void PSRAM_CheckData_DMA_ListWR_Burst16_Half_Word(void);
static void PSRAM_CheckData_DMA_ListWR_Burst8_Half_Word(void);
static void PSRAM_CheckData_DMA_ListWR_Burst4_Half_Word(void);
static void PSRAM_CheckData_DMA_ListWR_Burst16_Word(void);
static void PSRAM_CheckData_DMA_ListWR_Burst8_Word(void);
static void PSRAM_CheckData_DMA_ListWR_Burst4_Word(void);

// Stress test
static void PSRAM_StressTest_Combination(void);

static function test_function_array[] = {
    // PSRAM_CheckData_WriteByRead_Word,
    // PSRAM_CheckData_WriteByRead_HWord,
    // PSRAM_CheckData_WriteByRead_Byte,
    // PSRAM_CheckData_ListWR_Word,
    // PSRAM_CheckData_ListWR_Half_Word,
    // PSRAM_CheckData_ListWR_Byte,

    // PSRAM_MemoryCopyVerify,
//    PSRAM_CheckData_DMA_ListWR_Burst16_Bytes,
//    PSRAM_CheckData_DMA_ListWR_Burst8_Bytes,
//    PSRAM_CheckData_DMA_ListWR_Burst4_Bytes,
//    PSRAM_CheckData_DMA_ListWR_Burst16_Half_Word,
//    PSRAM_CheckData_DMA_ListWR_Burst8_Half_Word,
//    PSRAM_CheckData_DMA_ListWR_Burst4_Half_Word,
//    PSRAM_CheckData_DMA_ListWR_Burst16_Word,
//    PSRAM_CheckData_DMA_ListWR_Burst8_Word,
//    PSRAM_CheckData_DMA_ListWR_Burst4_Word,

//    PSRAM_MatrixVerify,

	 PSRAM_StressTest_Combination,
};

static uint32_t error_times = 0;

static int32_t Psram_Inner_Memcmp(void* src, void* dst, uint32_t bsize){
    uint32_t i = 0;
    for (i = 0; i < bsize; i++){
        if (*((uint8_t*)src +i) != *((uint8_t*)dst + i)){
            CLOGD("[FAILED]: Src data: 0x%x \r\n"
                    "Dst data: 0x%x \r\n", *((uint8_t*)src+i), *((uint8_t*)dst+i));
            return -1;
        }
    }

    return 0;
}

#define CHECK_SIZE_0         (1024*1024*1)

static void PSRAM_CheckData_WriteByRead_Word(void){
    CLOGD("\r\n\r\n[PSRAM TEST] WRITE BY READ WITH WORD\r\n"
              "wait................");

    uint32_t i = 0;

    uint32_t* wr_address = (uint32_t*)PSRAM_BASE_ADDRESS;
    uint32_t* rd_address = (uint32_t*)PSRAM_BASE_ADDRESS;

    uint32_t wr_data = 0;
    uint32_t rd_data = 0;
    error_times = 0;

    uint32_t j = 0;

    for(i = 0; i < CHECK_SIZE_0; i++){
        // get random data
        wr_data = rand();

        // write random data to PSRAM
        (*wr_address) = wr_data;
        // read it back
        rd_data = (*rd_address);

        if(rd_data == wr_data){
            // PASS
        } else{
            error_times++;
            CLOGE("[Error]: [w]0x%x->0x%x; [r]0x%x->0x%x", wr_address, wr_data, rd_address, rd_data);
        }

        wr_address++;
        rd_address++;

    }

    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, CHECK_SIZE_0);
}

static void PSRAM_CheckData_WriteByRead_HWord(void){
    CLOGD("\r\n\r\n[PSRAM TEST] WRITE BY READ WITH HALF WORD\r\n"
              "wait................");

    uint32_t i = 0;

    uint16_t* wr_address = (uint16_t*)PSRAM_BASE_ADDRESS;
    uint16_t* rd_address = (uint16_t*)PSRAM_BASE_ADDRESS;

    uint16_t wr_data = 0;
    uint16_t rd_data = 0;
    error_times = 0;

    uint32_t j = 0;

    for(i = 0; i < CHECK_SIZE_0; i++){
        // get random data
        wr_data = (uint16_t)rand();

        // write random data to PSRAM
        (*wr_address) = wr_data;
        // read it back
        rd_data = (*rd_address);

        if(rd_data == wr_data){
            // PASS
        } else{
            error_times++;
            CLOGE("[Error]: [w]0x%x->0x%x; [r]0x%x->0x%x", wr_address, wr_data, rd_address, rd_data);
        }

        wr_address++;
        rd_address++;

    }

    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, CHECK_SIZE_0);
}


static void PSRAM_CheckData_WriteByRead_Byte(void){
    CLOGD("\r\n\r\n[PSRAM TEST] WRITE BY READ WITH BYTE\r\n"
              "wait................");

    uint32_t i = 0;

    uint8_t* wr_address = (uint8_t*)PSRAM_BASE_ADDRESS;
    uint8_t* rd_address = (uint8_t*)PSRAM_BASE_ADDRESS;

    uint8_t wr_data = 0;
    uint8_t rd_data = 0;
    error_times = 0;

    uint32_t j = 0;

    for(i = 0; i < CHECK_SIZE_0; i++){
        // get random data
        wr_data = (uint8_t)rand();

        // write random data to PSRAM
        (*wr_address) = wr_data;
        // read it back
        rd_data = (*rd_address);

        if(rd_data == wr_data){
            // PASS
        } else{
            error_times++;
            CLOGE("[Error]: [w]0x%x->0x%x; [r]0x%x->0x%x", wr_address, wr_data, rd_address, rd_data);
        }

        wr_address++;
        rd_address++;

    }

    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, CHECK_SIZE_0);
}

#define CHECK_SIZE         (1024*1024*1)
#define DATA_BASE          (0x12345678)

static void PSRAM_CheckData_ListWR_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK

// varient type
#define VARIENT_CLASS        uint32_t
#define VARIENT_CLASS_MASK   (0xFFFFFFFF)

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH WORD\r\n"
          "wait................");

    uint32_t i = 0;

    VARIENT_CLASS* wr_address = (VARIENT_CLASS*)PSRAM_BASE_ADDRESS;
    VARIENT_CLASS* rd_address = (VARIENT_CLASS*)PSRAM_BASE_ADDRESS;

    VARIENT_CLASS r_data = 0;

    error_times = 0;

    // write
    for (i = 0; i < CHECK_SIZE; i++){
        (*wr_address++) = ((DATA_BASE + i) & VARIENT_CLASS_MASK);
    }

    for (i = 0; i < CHECK_SIZE; i++){
        r_data = (*rd_address);
        if (r_data == ((DATA_BASE + i) & VARIENT_CLASS_MASK)){
            // PASS
        } else {
            error_times++;
            CLOGE("[Error]: [w]0x%x->0x%x; [r]0x%x->0x%x", wr_address, ((DATA_BASE + i) & VARIENT_CLASS_MASK), rd_address, r_data);
        }
        rd_address++;
    }

    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, CHECK_SIZE);
}

static void PSRAM_CheckData_ListWR_Half_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK

// varient type
#define VARIENT_CLASS        uint16_t
#define VARIENT_CLASS_MASK   (0xFFFF)

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH HALF WORD\r\n"
          "wait................");

    uint32_t i = 0;

    VARIENT_CLASS* wr_address = (VARIENT_CLASS*)PSRAM_BASE_ADDRESS;
    VARIENT_CLASS* rd_address = (VARIENT_CLASS*)PSRAM_BASE_ADDRESS;

    VARIENT_CLASS r_data = 0;

    error_times = 0;

    // write
    for (i = 0; i < CHECK_SIZE; i++){
        (*wr_address++) = ((DATA_BASE + i) & VARIENT_CLASS_MASK);
    }

    for (i = 0; i < CHECK_SIZE; i++){
        r_data = (*rd_address);
        if (r_data == ((DATA_BASE + i) & VARIENT_CLASS_MASK)){
            // PASS
        } else {
            error_times++;
            CLOGE("[Error]: [w]0x%x->0x%x; [r]0x%x->0x%x", wr_address, ((DATA_BASE + i) & VARIENT_CLASS_MASK), rd_address, r_data);
        }
        rd_address++;
    }

    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, CHECK_SIZE);
}

static void PSRAM_CheckData_ListWR_Byte(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK

// varient type
#define VARIENT_CLASS        uint8_t
#define VARIENT_CLASS_MASK   (0xFF)

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH BYTE\r\n"
          "wait................");

    uint32_t i = 0;

    VARIENT_CLASS* wr_address = (VARIENT_CLASS*)PSRAM_BASE_ADDRESS;
    VARIENT_CLASS* rd_address = (VARIENT_CLASS*)PSRAM_BASE_ADDRESS;

    VARIENT_CLASS r_data = 0;

    error_times = 0;

    // write
    for (i = 0; i < CHECK_SIZE; i++){
        (*wr_address++) = ((DATA_BASE + i) & VARIENT_CLASS_MASK);
    }

    for (i = 0; i < CHECK_SIZE; i++){
        r_data = (*rd_address);
        if (r_data == ((DATA_BASE + i) & VARIENT_CLASS_MASK)){
            // PASS
        } else {
            error_times++;
            CLOGE("[Error]: [w]0x%x->0x%x; [r]0x%x->0x%x", wr_address, ((DATA_BASE + i) & VARIENT_CLASS_MASK), rd_address, r_data);
        }
        rd_address++;
    }

    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, CHECK_SIZE);
}

#define PSRAM_MEMCPY_LOOP_TIMES        10000
#define PSRAM_MEMCPY_CNT_SIZE          100000

static void PSRAM_MemoryCopyVerify(void){
	uint32_t i = 0, j = 0;
	uint8_t golden_value = 0x0;
    uint8_t* psram_pointer = (uint8_t*)PSRAM_BASE_ADDRESS;
	for(i = 0; i < PSRAM_MEMCPY_LOOP_TIMES; i++){
		CLOGD("[MEMCPY] Loop count: %d", i);

		memset((uint8_t*)PSRAM_BASE_ADDRESS, golden_value, PSRAM_MEMCPY_CNT_SIZE);

        HAL_FlushInvalidateDCache_by_Addr((uint32_t *)PSRAM_BASE_ADDRESS, PSRAM_MEMCPY_CNT_SIZE);

        for (j = 0; j < PSRAM_MEMCPY_CNT_SIZE; j++){
            if (*(psram_pointer + j) != golden_value){
                CLOGE("[MEMCPY] Error in %d offset bytes", j);
                return;
            }
        }

        golden_value++;
	}
}

static uint32_t volatile DMAEvent;

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    DMAEvent = event_info & 0xFF;
}

static uint32_t DMA_SOUR_MODE = DMA_CH_CTLL_SRC_INC;
static uint32_t DMA_DST_MODE = DMA_CH_CTLL_DST_INC;
static uint32_t DMA_CACHE_ATTR = DMA_CACHE_SYNC_AUTO;

static int32_t psram_memcopy(uint8_t *ch, void* dst, void* src, uint32_t size, uint32_t bsize, uint32_t wsize){
    // size      :  size of sample

    // wsize = 0 :  byte
    //       = 1 :  half word
    //       = 2 :  word


    int32_t stat = 0;
    // Select channel
    dma_channel_select(ch, DMA_DrvEvent, 0, DMA_CACHE_ATTR);

    if (*ch == DMA_CHANNEL_ANY){
        // Channel error
        return -1;
    }

    stat = dma_channel_configure (*ch,
                (uint32_t) src,
                (uint32_t) dst,
                size,
                DMA_CH_CTLL_DST_WIDTH(wsize) | DMA_CH_CTLL_SRC_WIDTH(wsize) |\
              DMA_CH_CTLL_DST_BSIZE(bsize) | DMA_CH_CTLL_SRC_BSIZE(bsize) |\
              DMA_DST_MODE | DMA_SOUR_MODE | DMA_CH_CTLL_TTFC_M2M |\
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

#define PSRAM_DMA_CHANNEL       (0)
#define PSRAM_COPY_LOOP_NUMBER  (1024*8)
#define PSRAM_DMA_MEMCOPY_SIZE  (1024)  // bytes
#define PSRAM_BOTTOM_BOUNDARY   (PSRAM_BASE_ADDRESS)
#define PSRAM_TOP_BOUNDARY      (PSRAM_BOTTOM_BOUNDARY + PSRAM_MEM_SIZE)
#define PSRAM_REGION_MASK       (0x7FFFFF)

// width = bytes
#define PSRAM_MEMCPY_16_B(ch, dst, src, size)      psram_memcopy(ch, (uint8_t*)dst, (uint8_t*)src, size, DMA_BSIZE_16, DMA_WIDTH_BYTE)
#define PSRAM_MEMCPY_8_B(ch, dst, src, size)       psram_memcopy(ch, (uint8_t*)dst, (uint8_t*)src, size, DMA_BSIZE_8, DMA_WIDTH_BYTE)
#define PSRAM_MEMCPY_4_B(ch, dst, src, size)       psram_memcopy(ch, (uint8_t*)dst, (uint8_t*)src, size, DMA_BSIZE_4, DMA_WIDTH_BYTE)

// width = half word
#define PSRAM_MEMCPY_16_HW(ch, dst, src, size)     psram_memcopy(ch, (uint16_t*)dst, (uint16_t*)src, size, DMA_BSIZE_16, DMA_WIDTH_HALFWORD)
#define PSRAM_MEMCPY_8_HW(ch, dst, src, size)      psram_memcopy(ch, (uint16_t*)dst, (uint16_t*)src, size, DMA_BSIZE_8, DMA_WIDTH_HALFWORD)
#define PSRAM_MEMCPY_4_HW(ch, dst, src, size)      psram_memcopy(ch, (uint16_t*)dst, (uint16_t*)src, size, DMA_BSIZE_4, DMA_WIDTH_HALFWORD)

// width = word
#define PSRAM_MEMCPY_16_W(ch, dst, src, size)      psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_16, DMA_WIDTH_WORD)
#define PSRAM_MEMCPY_8_W(ch, dst, src, size)       psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_8, DMA_WIDTH_WORD)
#define PSRAM_MEMCPY_4_W(ch, dst, src, size)       psram_memcopy(ch, (uint32_t*)dst, (uint32_t*)src, size, DMA_BSIZE_4, DMA_WIDTH_WORD)

static void PSRAM_CheckData_DMA_ListWR_Burst16_Bytes(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint8_t
#define VARIENT_CLASS_MASK   (0xFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_16_B(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST16 BYTES\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst8_Bytes(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint8_t
#define VARIENT_CLASS_MASK   (0xFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_8_B(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST8 BYTES\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst4_Bytes(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint8_t
#define VARIENT_CLASS_MASK   (0xFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_4_B(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST4 BYTES\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst16_Half_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint16_t
#define VARIENT_CLASS_MASK   (0xFFFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_16_HW(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST16 HALF WORD\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst8_Half_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint16_t
#define VARIENT_CLASS_MASK   (0xFFFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_8_HW(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST8 HALF WORD\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst4_Half_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint16_t
#define VARIENT_CLASS_MASK   (0xFFFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_4_HW(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST4 HALF WORD\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst16_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint32_t
#define VARIENT_CLASS_MASK   (0xFFFFFFFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_16_W(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST16 WORD\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst8_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint32_t
#define VARIENT_CLASS_MASK   (0xFFFFFFFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_8_W(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST8 WORD\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

static void PSRAM_CheckData_DMA_ListWR_Burst4_Word(void){
#undef VARIENT_CLASS
#undef VARIENT_CLASS_MASK
#undef PSRAM_MEMCPY

// varient type
#define VARIENT_CLASS        uint32_t
#define VARIENT_CLASS_MASK   (0xFFFFFFFF)

// memcopy function
#define PSRAM_MEMCPY(ch, dst, src, size)     PSRAM_MEMCPY_4_W(ch, dst, src, size)

    VARIENT_CLASS* src_buf = NULL;
    VARIENT_CLASS* result_buf = NULL;
    VARIENT_CLASS* dst_buf = (VARIENT_CLASS*)PSRAM_BOTTOM_BOUNDARY;

    error_times = 0;

    src_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);
    result_buf = (VARIENT_CLASS*)malloc(PSRAM_DMA_MEMCOPY_SIZE);

    CLOGD("\r\n\r\n[PSRAM TEST] LIST WRITE AND READ WITH DMA IN BURST4 WORD\r\n"
              "wait................");

    // Initailze source buffer
    {
        uint32_t i = 0;
        for(i = 0; i < PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS); i++){
            src_buf[i] = ((0x12345678 + i) & VARIENT_CLASS_MASK);
        }
    }

    dma_initialize();

    uint8_t ch = PSRAM_DMA_CHANNEL;

    // src_buf                ===>  dst_buf (PSRAM REGION)
    // dst_buf (PSRAM REGION) ===>  result_buf

    {
        uint32_t i = 0;
        int32_t stat = 0;
        for (i = 0; i < PSRAM_COPY_LOOP_NUMBER; i++){

            // dst_buf = address(dst_buf) + sample=>((loop times * each size of bytes & PSRAM mask) / each sample bytes)
            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))),\
                    (VARIENT_CLASS*)src_buf, (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            stat = PSRAM_MEMCPY(&ch, (VARIENT_CLASS*)result_buf,\
                    (VARIENT_CLASS*)(dst_buf + (((i * PSRAM_DMA_MEMCOPY_SIZE) & PSRAM_REGION_MASK)/sizeof(VARIENT_CLASS))), (PSRAM_DMA_MEMCOPY_SIZE/sizeof(VARIENT_CLASS)));

            if((stat == -2) || (stat == -1)){
                CLOGD("[DMA ERROR] DMA CHANNEL INVALID OR DMA CONFIGURE ERROR, RETURN %i", stat);
                error_times++;
                continue;
            }

            if(stat == -3){
                error_times++;
                CLOGD("[DMA ERROR] TRIGGER EVENT ERROR EXCEPTION IN %d", i);
                CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, i);
                return;
            }

            // Compare data
            if (Psram_Inner_Memcmp(src_buf, result_buf, PSRAM_DMA_MEMCOPY_SIZE) == 0){
                // pass
            } else{
                error_times++;
                CLOGD("[ERROR] PSRAM DMA COPY TRIGGER EXCEPTION IN %d", i);
                return;
            }
        }
    }

    dma_uninitialize();
    free(src_buf);
    free(result_buf);

    CLOGD("ERROR SHOW BELOW:");
    CLOGD("RESULT ERROE TIMES: [%d/%d]", error_times, PSRAM_COPY_LOOP_NUMBER);
}

#define PSRAM_SPEEDTEST_MEMCPY_LOOP_CNT           (100)
#define SPEEDTEST_MEM_SRAM_START_ADDRESS          0x80000
#define SPEEDTEST_MEM_START_ADDRESS               PSRAM_BASE_ADDRESS
#define SPEEDTEST_MEM_END_ADDRESS                 0x30010000
#define SPEEDTEST_MEM_1M_SIZEGAP                  0x100000

static void PSRAM_SpeedTest_OnlyMemcpy_Word(void){
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    uint32_t systime_size = PSRAM_SPEEDTEST_MEMCPY_LOOP_CNT * (((uint32_t)SPEEDTEST_MEM_END_ADDRESS - (uint32_t)SPEEDTEST_MEM_START_ADDRESS) / SPEEDTEST_MEM_1M_SIZEGAP);
    uint8_t times = 0;

    CLOGD("\r\n\r\n[PSRAM SPEED TEST] AP MEMCPY WRITE WITH WORD BETWEEN 0x%x --- 0x%x\r\n"
                      "wait................",SPEEDTEST_MEM_START_ADDRESS,  SPEEDTEST_MEM_END_ADDRESS);

    SysTick_Open(48000);
    systime_1 = SysTick_Time();
    for(times = 0; times < PSRAM_SPEEDTEST_MEMCPY_LOOP_CNT; times++){
        memcpy((uint8_t*)SPEEDTEST_MEM_START_ADDRESS, (uint8_t*)SPEEDTEST_MEM_SRAM_START_ADDRESS, (SPEEDTEST_MEM_END_ADDRESS - SPEEDTEST_MEM_START_ADDRESS));
    }
    systime_2 = SysTick_Time();

    SysTick_Close();

    systime_diff = systime_2 - systime_1;
    systime_diff = systime_diff / systime_size;

    CLOGD("[PSRAM SPEED TEST] AP MEMCPY WITH WORD SPEED TEST REPORT: %ld CLOCK/M \r\n\r\n", systime_diff);
}

static int32_t psram_memcpy_performance(void* dst, void* src, uint32_t size){
    // size      :  size of sample

    // wsize = 0 :  byte
    //       = 1 :  half word
    //       = 2 :  word


    int32_t stat = 0;

    uint8_t ch = 0;
    // Select channel
    dma_channel_select((uint8_t*)&ch, DMA_DrvEvent, 0, DMA_CACHE_SYNC_BOTH);

    stat = dma_channel_configure (ch,
                (uint32_t) src,
                (uint32_t) dst,
                size,
                DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) |\
              DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) |\
              DMA_DST_MODE | DMA_SOUR_MODE | DMA_CH_CTLL_TTFC_M2M |\
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

#define PERFORMANCE_VERIFY_MTIME_FRQ                    1000000
#define PERFORMANCE_VERIFY_FLASH_MEMORY                0
#define PERFORMANCE_VERIFY_FLASH_SIZE                  (uint32_t)(1024*1024)
#define PERFORMANCE_VERIFY_PSRAM_MEMORY                0
#define PERFORMANCE_VERIFY_LOOP_COUNT                   128
#define PERFORMANCE_VERIFY_SRAM_MEMORY                 0
#define PERFORMANCE_VERIFY_SRAM_SIZE                   0
#define PERFORMANCE_VERIFY_SRAM_MEMORY_DMA             0
#define PERFORMANCE_VERIFY_SRAM_SIZE_DMA               0


static void PSRAM_PerformanceVerify_FlashMemory2PSRAM(){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM FLASH TO PSRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_FLASH_MEMORY,  PERFORMANCE_VERIFY_PSRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Value();

    for (uint8_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
    	memcpy(PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_FLASH_MEMORY, PERFORMANCE_VERIFY_FLASH_SIZE);
    }

    systime_2 = SysTick_Value();
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

    systime_1 = SysTick_Value();

    for (uint8_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
    	memcpy(PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE);
    }

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] SRAM to PSRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE) / t_time);
        CLOGD("[Performance] SRAM to PSRAM speed -> %d", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_PSRAM2Sram(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM PSRAM TO SRAM 0x%x --- 0x%x\r\n"
                      "wait................",PERFORMANCE_VERIFY_PSRAM_MEMORY , PERFORMANCE_VERIFY_SRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Value();

    for (uint8_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
    	memcpy(PERFORMANCE_VERIFY_SRAM_MEMORY, PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE);
    }

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] PSRAM to SRAM %d + %d ----> %d", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE) / t_time);
        CLOGD("[Performance] PSRAM to SRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

static void PSRAM_PerformanceVerify_Sram2PSRAM_DMA(void){
    CLOGD("\r\n\r\n[PSRAM PERFORMACE VERIFY] MEMCPY FROM SRAM TO PSRAM WITH DMA 0x%x --- 0x%x Bytes\r\n"
                      "wait................",PERFORMANCE_VERIFY_SRAM_MEMORY_DMA,  PERFORMANCE_VERIFY_PSRAM_MEMORY);

    SysTick_Open(SYSTICK_MAX_INT);

    uint64_t systime_1 = 0;
    uint64_t systime_2 = 0;
    uint64_t systime_diff = 0;

    systime_1 = SysTick_Value();

    for (uint8_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
        psram_memcpy_performance(PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_MEMORY_DMA, PERFORMANCE_VERIFY_SRAM_SIZE_DMA/4);
    }

    systime_2 = SysTick_Value();
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

    systime_1 = SysTick_Value();

    for (uint8_t i = 0; i < PERFORMANCE_VERIFY_LOOP_COUNT; i++){
        psram_memcpy_performance(PERFORMANCE_VERIFY_SRAM_MEMORY_DMA, PERFORMANCE_VERIFY_PSRAM_MEMORY, PERFORMANCE_VERIFY_SRAM_SIZE_DMA/4);
    }

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    SysTick_Close();
    {
        CLOGD("[Performance] SRAM to PSRAM %d + %d ----> %d Bytes", (uint32_t)((systime_diff >> 32) & 0xFFFFFFFF), (uint32_t)(systime_diff & 0xFFFFFFFF), (PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA));
        double t_time = (double)systime_diff / (double)PERFORMANCE_VERIFY_MTIME_FRQ;
        uint64_t t_speed = (uint64_t)((double)(PERFORMANCE_VERIFY_LOOP_COUNT * PERFORMANCE_VERIFY_SRAM_SIZE_DMA) / t_time);
        CLOGD("[Performance] SRAM to PSRAM speed -> %d B/s", (uint32_t)t_speed);
    }
}

#define MAX_N 100   // The max matrix
#define MIN_N 10   // The min matrix

static tinyheap th_handler;

// Generate random matrix
static void generate_random_matrix(double **matrix, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
            matrix[i][j] = (double)(rand() % 100) / 10.0;  // Generate the random data with double type from 0 - 9
        }
    }
}

// shuffle the rows
static void shuffle_rows(double **matrix, double ** ref_matrix, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        uint32_t r1 = rand() % N;  // Choose random row
        uint32_t r2 = rand() % N;
        
        if (r1 != r2) {
            // Exchange the rows
            for (uint32_t j = 0; j < N; j++) {
                double temp = matrix[r1][j];                
                matrix[r1][j] = matrix[r2][j];
                matrix[r2][j] = temp;

                temp = ref_matrix[r1][j];
                ref_matrix[r1][j] = ref_matrix[r2][j];
                ref_matrix[r2][j] = temp;
            }
        }
    }
}

// shuffle the columns
static void shuffle_columns(double **matrix, double ** ref_matrix, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        uint32_t c1 = rand() % N;  // Choose random column
        uint32_t c2 = rand() % N;
        
        if (c1 != c2) {
            // Exchange the columns
            for (uint32_t j = 0; j < N; j++) {
                double temp = matrix[j][c1];
                matrix[j][c1] = matrix[j][c2];
                matrix[j][c2] = temp;

                temp = ref_matrix[j][c1];
                ref_matrix[j][c1] = ref_matrix[j][c2];
                ref_matrix[j][c2] = temp;
            }
        }
    }
}

// Print matrix
static void print_matrix(double **matrix, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
        	uint64_t value = (uint64_t)matrix[i][j];
            CLOGD("high: %d , low: %d", (value & 0xFFFFFFFF), (value >> 32) & 0xFFFFFFFF);
        }
    }
}

// Multiply matrices
static void multiply_matrices(double **A, double **B, double **result, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
            result[i][j] = 0.0;
            for (uint32_t k = 0; k < N; k++) {
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Compare the two matrices
uint32_t compare_matrices(double **A, double **B, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
            if (A[i][j] != B[i][j]){
            	uint64_t value_A = A[i][j];
            	uint64_t value_B = B[i][j];
            	CLOGD("Matrix in [%d,%d] value error", i, j);
                return 0;
            }
        }
    }
    return 1; // Equal
}

double** allocate_matrix(uint32_t N) {
    double **matrix = (double **)malloc(N * sizeof(double *));
    for (uint32_t i = 0; i < N; i++) {
        matrix[i] = (double *)malloc(N * sizeof(double));
    }
    return matrix;
}

double** allocate_matrix_in_region(uint32_t N){
    double **matrix = (double **)th_malloc(&th_handler, N * sizeof(double *));
    for (uint32_t i = 0; i < N; i++) {
        matrix[i] = (double *)th_malloc(&th_handler, N * sizeof(double));
    }
    return matrix;
}

// Free matrix
void free_matrix(double **matrix, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// Free matrix in region
void free_matrix_in_region(double **matrix, uint32_t N){
    for (uint32_t i = 0; i < N; i++) {
        th_free(&th_handler, matrix[i]);
    }
    th_free(&th_handler, matrix);
}

static void PSRAM_MatrixVerify(void){
    srand(0);  // Set seed

    th_init(&th_handler, (void*)(0x28000000), 0x80000);

    while(1){
        // Initialize matrix length
        int N = rand() % (MAX_N - MIN_N + 1) + MIN_N;
        CLOGD("Initial Matrix size: %d x %d\n", N, N);

        double **A = allocate_matrix_in_region(N);
        double **B = allocate_matrix_in_region(N);
        double **result = allocate_matrix_in_region(N);

        double **A_reference = allocate_matrix(N);
        double **B_reference = allocate_matrix(N);
        double **result_reference = allocate_matrix(N);

        generate_random_matrix(A_reference, N);
        generate_random_matrix(B_reference, N);

        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                A[i][j] = A_reference[i][j];
                B[i][j] = B_reference[i][j];
            }
        }

        shuffle_rows(A, A_reference, N);
        shuffle_columns(A, A_reference, N);
        shuffle_rows(B, B_reference, N);
        shuffle_columns(B, B_reference, N);

        multiply_matrices(A_reference, B_reference, result_reference, N);
        multiply_matrices(A, B, result, N);

        if (compare_matrices(result, result_reference, N)) {
            CLOGD("\nMatrix multiply (after shuffling) is correct!\n");
        } else {
            CLOGD("\nMatrix multiply (after shuffling) is incorrect!\n");
            while(1);
        }

//        print_matrix(result, N);

        free_matrix_in_region(A, N);
        free_matrix_in_region(B, N);
        free_matrix_in_region(result, N);
        free_matrix(A_reference, N);
        free_matrix(B_reference, N);
        free_matrix(result_reference, N);
    }    
}

extern int test_stuck_address(unsigned long volatile *bufa, size_t count);
extern int test_random_value(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_xor_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_sub_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_mul_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_div_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_or_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_and_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_seqinc_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_solidbits_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_checkerboard_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_blockseq_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_walkbits0_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_walkbits1_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_bitspread_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_bitflip_comparison(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_8bit_wide_random(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);
extern int test_16bit_wide_random(unsigned long volatile *bufa, unsigned long volatile *bufb, size_t count);

typedef struct _test{
    char *name;
    int (*fp)();
}test;

static test psram_stress_test[] = {
    { "Random Value", test_random_value },
    { "Compare XOR", test_xor_comparison },
    { "Compare SUB", test_sub_comparison },
    { "Compare MUL", test_mul_comparison },
    { "Compare DIV",test_div_comparison },
    { "Compare OR", test_or_comparison },
    { "Compare AND", test_and_comparison },
    { "Sequential Increment", test_seqinc_comparison },
    { "Solid Bits", test_solidbits_comparison },
    { "Block Sequential", test_blockseq_comparison },
    { "Checkerboard", test_checkerboard_comparison },
    { "Bit Spread", test_bitspread_comparison },
    { "Bit Flip", test_bitflip_comparison },
    { "Walking Ones", test_walkbits1_comparison },
    { "Walking Zeroes", test_walkbits0_comparison },
    { "8-bit Writes", test_8bit_wide_random },
    { "16-bit Writes", test_16bit_wide_random },
};

#define PSRAM_ALIGNED_ADDRESS          (PSRAM_BASE_ADDRESS)
#define PSRAM_BUFFER_SIZE              4*1024*1024   // 8MB
#define TEST_LOOPS                     (10000)

static void PSRAM_StressTest_Combination(void){
    ul loop, i;
    size_t bufsize, halflen, count;
    ulv *bufa, *bufb;

    uint32_t error = 0;

    halflen = PSRAM_BUFFER_SIZE / 2;
    count = halflen / sizeof(ul);
    bufa = (ulv *) PSRAM_ALIGNED_ADDRESS;
    bufb = (ulv *) ((size_t) PSRAM_ALIGNED_ADDRESS + halflen);

    for(loop = 0; loop < TEST_LOOPS; loop++){

        CLOGD("Loop %lu", loop);

        CLOGD("  %-20s: ", "Stuck Address");
        if (!test_stuck_address((ulv *)PSRAM_ALIGNED_ADDRESS, PSRAM_BUFFER_SIZE / sizeof(ul))) {
            CLOGD("ok");
        } else {
            CLOGD("error");
            error++;
        }

        for(i = 0; i < sizeof(psram_stress_test)/sizeof(psram_stress_test[0]); i++){
            CLOGD("  %-20s: ", psram_stress_test[i].name);

            if (!psram_stress_test[i].fp(bufa, bufb, count)) {
                CLOGD("ok");
            } else {
                CLOGD("error");
                error++;
            }
            CLOGD("Error time: %d", error);
        }
    }
}

int main(){
    uint32_t times;
    uint32_t error = 0;
    logInit(0, 115200);
    CLOGD("[PSRAM TEST] BEGIN...");

    DisableICache();
    DisableDCache();

    __RWMB();
    __FENCE_I();

    dma_initialize();
    PSRAM_Initialize(NULL, NULL, 1);
    CLOGD("[TEST] PSRAM CONFIG COMPLETE");

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
