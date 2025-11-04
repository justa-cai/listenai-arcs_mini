#include "Driver_GPDMA.h"
#include "log_print.h"
#include "chip.h"
#include "systick.h"
#include "nmsis_core.h"

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

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

/* Exported constants --------------------------------------------------------*/
#define CSK_GPDMA_VALIDATION_WORD_LENGTH                  4
#define CSK_GPDMA_VALIDATION_HALFWORD_LENGTH              2
#define CSK_GPDMA_VALIDATION_BYTE_LENGTH                  1

/* Exported types ------------------------------------------------------------*/
static void* gpdma_src_buffer = NULL;
static void* gpdma_dst_buffer = NULL;
static void* gpdma_src1_buffer = NULL;
static void* gpdma_dst1_buffer = NULL;
static void* gpdma_src2_buffer = NULL;
static void* gpdma_dst2_buffer = NULL;

/* Fucntion decalre ------------------------------------------------------------*/
static void GPDMA_NormalMode_M2M_Word_Channel0_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel0_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel0_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel1_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel1_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel1_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel2_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel2_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel2_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel3_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel3_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel3_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel4_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel4_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel4_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel5_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel5_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel5_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel0_Succession_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel0_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel0_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel1_Succession_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel1_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel1_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel2_Succession_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel2_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel2_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel3_Succession_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel3_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel3_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel4_Succession_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel4_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel4_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Word_Channel5_Succession_Test(void);
static void GPDMA_NormalMode_M2M_HalfWord_Channel5_Succession_Test(void);
static void GPDMA_NormalMode_M2M_Byte_Channel5_Succession_Test(void);

static void GPDMA_PiPoMode_M2M_Word_Channel0_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel0_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel0_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel1_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel1_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel1_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel2_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel2_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel2_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel3_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel3_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel3_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel4_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel4_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel4_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel5_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel5_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel5_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel0_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel0_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel0_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel1_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel1_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel1_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel2_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel2_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel2_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel3_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel3_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel3_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel4_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel4_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel4_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Word_Channel5_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_HalfWord_Channel5_Reload_Test(void);
static void GPDMA_PiPoMode_M2M_Byte_Channel5_Reload_Test(void);

static void GPDMA_Gather_M2M_Word_Channel0_Test(void);
static void GPDMA_Gather_M2M_HalfWord_Channel0_Test(void);
static void GPDMA_Gather_M2M_Byte_Channel0_Test(void);
static void GPDMA_Gather_M2M_Word_Channel1_Test(void);
static void GPDMA_Gather_M2M_HalfWord_Channel1_Test(void);
static void GPDMA_Gather_M2M_Byte_Channel1_Test(void);
static void GPDMA_Gather_M2M_Word_Channel2_Test(void);
static void GPDMA_Gather_M2M_HalfWord_Channel2_Test(void);
static void GPDMA_Gather_M2M_Byte_Channel2_Test(void);
static void GPDMA_Gather_M2M_Word_Channel3_Test(void);
static void GPDMA_Gather_M2M_HalfWord_Channel3_Test(void);
static void GPDMA_Gather_M2M_Byte_Channel3_Test(void);
static void GPDMA_Gather_M2M_Word_Channel4_Test(void);
static void GPDMA_Gather_M2M_HalfWord_Channel4_Test(void);
static void GPDMA_Gather_M2M_Byte_Channel4_Test(void);
static void GPDMA_Gather_M2M_Word_Channel5_Test(void);
static void GPDMA_Gather_M2M_HalfWord_Channel5_Test(void);
static void GPDMA_Gather_M2M_Byte_Channel5_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel0_Test(void);
static void GPDMA_Scatter_M2M_HalfWord_Channel0_Test(void);
static void GPDMA_Scatter_M2M_Byte_Channel0_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel1_Test(void);
static void GPDMA_Scatter_M2M_HalfWord_Channel1_Test(void);
static void GPDMA_Scatter_M2M_Byte_Channel1_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel2_Test(void);
static void GPDMA_Scatter_M2M_HalfWord_Channel2_Test(void);
static void GPDMA_Scatter_M2M_Byte_Channel2_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel3_Test(void);
static void GPDMA_Scatter_M2M_HalfWord_Channel3_Test(void);
static void GPDMA_Scatter_M2M_Byte_Channel3_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel4_Test(void);
static void GPDMA_Scatter_M2M_HalfWord_Channel4_Test(void);
static void GPDMA_Scatter_M2M_Byte_Channel4_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel5_Test(void);
static void GPDMA_Scatter_M2M_HalfWord_Channel5_Test(void);
static void GPDMA_Scatter_M2M_Byte_Channel5_Test(void);
static void GPDMA_Gather_M2M_Word_Channel0_FullCoverage_Test(void);
static void GPDMA_Scatter_M2M_Word_Channel0_FullCoverage_Test(void);
static void GPDMA_GetCnt_Test(void);
static void GPDMA_Stop_Test(void);
static void GPDMA_StopRecume_Test(void);

static function test_function_array[] = {
    GPDMA_NormalMode_M2M_Word_Channel0_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel0_Test,
    GPDMA_NormalMode_M2M_Byte_Channel0_Test,
    GPDMA_NormalMode_M2M_Word_Channel1_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel1_Test,
    GPDMA_NormalMode_M2M_Byte_Channel1_Test,
    GPDMA_NormalMode_M2M_Word_Channel2_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel2_Test,
    GPDMA_NormalMode_M2M_Byte_Channel2_Test,
    GPDMA_NormalMode_M2M_Word_Channel3_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel3_Test,
    GPDMA_NormalMode_M2M_Byte_Channel3_Test,
    GPDMA_NormalMode_M2M_Word_Channel4_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel4_Test,
    GPDMA_NormalMode_M2M_Byte_Channel4_Test,
    GPDMA_NormalMode_M2M_Word_Channel5_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel5_Test,
    GPDMA_NormalMode_M2M_Byte_Channel5_Test,
    GPDMA_NormalMode_M2M_Word_Channel0_Succession_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel0_Succession_Test,
    GPDMA_NormalMode_M2M_Byte_Channel0_Succession_Test,
    GPDMA_NormalMode_M2M_Word_Channel1_Succession_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel1_Succession_Test,
    GPDMA_NormalMode_M2M_Byte_Channel1_Succession_Test,
    GPDMA_NormalMode_M2M_Word_Channel2_Succession_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel2_Succession_Test,
    GPDMA_NormalMode_M2M_Byte_Channel2_Succession_Test,
    GPDMA_NormalMode_M2M_Word_Channel3_Succession_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel3_Succession_Test,
    GPDMA_NormalMode_M2M_Byte_Channel3_Succession_Test,
    GPDMA_NormalMode_M2M_Word_Channel4_Succession_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel4_Succession_Test,
    GPDMA_NormalMode_M2M_Byte_Channel4_Succession_Test,
    GPDMA_NormalMode_M2M_Word_Channel5_Succession_Test,
    GPDMA_NormalMode_M2M_HalfWord_Channel5_Succession_Test,
    GPDMA_NormalMode_M2M_Byte_Channel5_Succession_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel0_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel0_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel0_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel1_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel1_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel1_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel2_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel2_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel2_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel3_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel3_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel3_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel4_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel4_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel4_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel5_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel5_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel5_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel0_Reload_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel0_Reload_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel0_Reload_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel1_Reload_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel1_Reload_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel1_Reload_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel2_Reload_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel2_Reload_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel2_Reload_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel3_Reload_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel3_Reload_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel3_Reload_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel4_Reload_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel4_Reload_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel4_Reload_Test,
//    GPDMA_PiPoMode_M2M_Word_Channel5_Reload_Test,
//    GPDMA_PiPoMode_M2M_HalfWord_Channel5_Reload_Test,
//    GPDMA_PiPoMode_M2M_Byte_Channel5_Reload_Test,
//    GPDMA_Gather_M2M_Word_Channel0_Test,
//    GPDMA_Gather_M2M_HalfWord_Channel0_Test,
//    GPDMA_Gather_M2M_Byte_Channel0_Test,
//    GPDMA_Gather_M2M_Word_Channel1_Test,
//    GPDMA_Gather_M2M_HalfWord_Channel1_Test,
//    GPDMA_Gather_M2M_Byte_Channel1_Test,
//    GPDMA_Gather_M2M_Word_Channel2_Test,
//    GPDMA_Gather_M2M_HalfWord_Channel2_Test,
//    GPDMA_Gather_M2M_Byte_Channel2_Test,
//    GPDMA_Gather_M2M_Word_Channel3_Test,
//    GPDMA_Gather_M2M_HalfWord_Channel3_Test,
//    GPDMA_Gather_M2M_Byte_Channel3_Test,
//    GPDMA_Gather_M2M_Word_Channel4_Test,
//    GPDMA_Gather_M2M_HalfWord_Channel4_Test,
//    GPDMA_Gather_M2M_Byte_Channel4_Test,
//    GPDMA_Gather_M2M_Word_Channel5_Test,
//    GPDMA_Gather_M2M_HalfWord_Channel5_Test,
//    GPDMA_Gather_M2M_Byte_Channel5_Test,
//    GPDMA_Scatter_M2M_Word_Channel0_Test,
//    GPDMA_Scatter_M2M_HalfWord_Channel0_Test,
//    GPDMA_Scatter_M2M_Byte_Channel0_Test,
//    GPDMA_Scatter_M2M_Word_Channel1_Test,
//    GPDMA_Scatter_M2M_HalfWord_Channel1_Test,
//    GPDMA_Scatter_M2M_Byte_Channel1_Test,
//    GPDMA_Scatter_M2M_Word_Channel2_Test,
//    GPDMA_Scatter_M2M_HalfWord_Channel2_Test,
//    GPDMA_Scatter_M2M_Byte_Channel2_Test,
//    GPDMA_Scatter_M2M_Word_Channel3_Test,
//    GPDMA_Scatter_M2M_HalfWord_Channel3_Test,
//    GPDMA_Scatter_M2M_Byte_Channel3_Test,
//    GPDMA_Scatter_M2M_Word_Channel4_Test,
//    GPDMA_Scatter_M2M_HalfWord_Channel4_Test,
//    GPDMA_Scatter_M2M_Byte_Channel4_Test,
//    GPDMA_Scatter_M2M_Word_Channel5_Test,
//    GPDMA_Scatter_M2M_HalfWord_Channel5_Test,
//    GPDMA_Scatter_M2M_Byte_Channel5_Test,
//    GPDMA_Gather_M2M_Word_Channel0_FullCoverage_Test,
//    GPDMA_Scatter_M2M_Word_Channel0_FullCoverage_Test,
//    GPDMA_GetCnt_Test,
//    GPDMA_Stop_Test,
//    GPDMA_StopRecume_Test,
};

/* Fucntion implement ------------------------------------------------------------*/
static volatile uint32_t GPDMA_Event = 0;

static void gpdma_normal_test_callback(uint32_t event, void* workspace){
    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE){
        CLOGD("GPDMA Normal mode trigger");
    }

    GPDMA_Event = CSK_GPDMA_EVENT_TRANSFER_DONE;
}

#define CSK_GPDMA_VALIDATION_LENGTH          100

static void GPDMA_NormalMode_M2M_Word_Channel0_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel0_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with HalfWord base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel0_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with Byte base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel1_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel1_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with HalfWord base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel1_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with Byte base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel2_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel2_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with HalfWord base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel2_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with Byte base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel3_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel3_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with HalfWord base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel3_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with Byte base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel4_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel4_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with HalfWord base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel4_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with Byte base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel5_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel5_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with HalfWord base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel5_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with Byte base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}

#define GPDMA_SUCCESSION_LOOP          50

static void GPDMA_NormalMode_M2M_Word_Channel0_Succession_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel0_Succession_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with HalfWord succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel0_Succession_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with Byte succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel1_Succession_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel1_Succession_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with HalfWord succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel1_Succession_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with Byte succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel2_Succession_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel2_Succession_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with HalfWord succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel2_Succession_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with Byte succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel3_Succession_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel3_Succession_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with HalfWord succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel3_Succession_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with Byte succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel4_Succession_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel4_Succession_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with HalfWord succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel4_Succession_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with Byte succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Word_Channel5_Succession_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_HalfWord_Channel5_Succession_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with HalfWord succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_HALFWORD_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}
static void GPDMA_NormalMode_M2M_Byte_Channel5_Succession_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with Byte succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    uint8_t cnt = 0;

    for(cnt = 0; cnt < GPDMA_SUCCESSION_LOOP; cnt++){
        GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

        while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
        GPDMA_Event = 0;

        int32_t ret = 0;
        ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_BYTE_LENGTH);
        if (ret != 0){
            CLOGE("[GPDMA][NORMAL][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
        } else {
            CLOGD("[GPDMA][NORMAL][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}

#define CSK_GPDMA_PIPO_VALIDATION_LENGTH               0x1000
#define CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT             100

static volatile uint8_t g_flag = 0;

static void gpdma_pipo_test_callback(uint32_t event, void* workspace){
    uint32_t sample_len = 0;
    uint8_t ch = 0;
    sample_len = ((uint32_t)*(uint32_t*)workspace & 0xF);
    ch = (((uint32_t)*(uint32_t*)workspace >> 16) & 0xF);

//    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE){
        // PIPO0
        if (event & CSK_GPDMA_EVENT_PIPO0_DONE){
            int32_t ret = 0;
            ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH * sample_len);
            if (ret != 0){
                CLOGE("[GPDMA][PIPO0][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer error", ch, gpdma_src_buffer, gpdma_dst_buffer);
            } else {
                CLOGD("[GPDMA][PIPO0][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer success!!!", ch, gpdma_src_buffer, gpdma_dst_buffer);
            }
            if (g_flag != 0){
                g_flag--;
            } else {
                GPDMA_Stop(ch);
            }
        }
        // PIPO1
        else if (event & CSK_GPDMA_EVENT_PIPO1_DONE){
            int32_t ret = 0;
            ret = memcmp(gpdma_dst1_buffer, gpdma_src1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH * sample_len);
            if (ret != 0){
                CLOGE("[GPDMA][PIPO1][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer error", ch, gpdma_src1_buffer, gpdma_dst1_buffer);
            } else {
                CLOGD("[GPDMA][PIPO1][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer success!!!", ch, gpdma_src1_buffer, gpdma_dst1_buffer);
            }
            if (g_flag != 0){
                g_flag--;
            } else {
                GPDMA_Stop(ch);
            }
        }
        else {
            // Error
        }
//    }
}

static uint32_t pipo_workspace = 0;

static void GPDMA_PiPoMode_M2M_Word_Channel0_Test(void){
    CLOGD("[GPDMA][Channel0] Memory to memory with Word ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (0 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch0, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][0] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_HalfWord_Channel0_Test(void){
    CLOGD("[GPDMA][Channel0] Memory to memory with HalfWord ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (0 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch0, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][0] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Byte_Channel0_Test(void){
    CLOGD("[GPDMA][Channel0] Memory to memory with Byte ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (0 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch0, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][0] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Word_Channel1_Test(void){
    CLOGD("[GPDMA][Channel1] Memory to memory with Word ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (1 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch1, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][1] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel1_Test(void){
    CLOGD("[GPDMA][Channel1] Memory to memory with HalfWord ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (1 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch1, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][1] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Byte_Channel1_Test(void){
    CLOGD("[GPDMA][Channel1] Memory to memory with Byte ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (1 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch1, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][1] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Word_Channel2_Test(void){
    CLOGD("[GPDMA][Channel2] Memory to memory with Word ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (2 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch2, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][2] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_HalfWord_Channel2_Test(void){
    CLOGD("[GPDMA][Channel2] Memory to memory with HalfWord ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (2 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch2, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][2] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Byte_Channel2_Test(void){
    CLOGD("[GPDMA][Channel2] Memory to memory with Byte ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (2 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch2, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][2] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Word_Channel3_Test(void){
    CLOGD("[GPDMA][Channel3] Memory to memory with Word ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (3 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch3, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][3] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_HalfWord_Channel3_Test(void){
    CLOGD("[GPDMA][Channel3] Memory to memory with HalfWord ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (3 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch3, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][3] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Byte_Channel3_Test(void){
    CLOGD("[GPDMA][Channel3] Memory to memory with Byte ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (3 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch3, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][3] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Word_Channel4_Test(void){
    CLOGD("[GPDMA][Channel4] Memory to memory with Word ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (4 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch4, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][4] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_HalfWord_Channel4_Test(void){
    CLOGD("[GPDMA][Channel4] Memory to memory with HalfWord ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (4 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch4, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][4] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Byte_Channel4_Test(void){
    CLOGD("[GPDMA][Channel4] Memory to memory with Byte ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (4 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch4, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][4] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Word_Channel5_Test(void){
    CLOGD("[GPDMA][Channel5] Memory to memory with Word ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (5 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][5] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_HalfWord_Channel5_Test(void){
    CLOGD("[GPDMA][Channel5] Memory to memory with HalfWord ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (5 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][5] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}
static void GPDMA_PiPoMode_M2M_Byte_Channel5_Test(void){
    CLOGD("[GPDMA][Channel5] Memory to memory with Byte ping pong mode validation, the source and destination are both PION PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_VALIDATION_LOOP_CNT;

    // Malloc buffer
    pipo_workspace = (5 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_test_callback, &pipo_workspace);

    GPDMA_Start_PiPo(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_src1_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst1_buffer, CSK_GPDMA_PIPO_VALIDATION_LENGTH);

    while(g_flag);

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][PIPO][M2M][5] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);
}

#define CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH               512
#define CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT             100

static uint32_t pipo_reload_workspace = 0;
static uint8_t cur_buffer_0, cur_buffer_1;

static void gpdma_pipo_reload_test_callback(uint32_t event, void* workspace){
    uint32_t sample_len = 0;
    uint8_t ch = 0;
    sample_len = ((uint32_t)*(uint32_t*)workspace & 0xF);
    ch = (((uint32_t)*(uint32_t*)workspace >> 16) & 0xF);

    void* src_buffer, *dst_buffer;
    void* reload_src_buffer, *reload_dst_buffer;
    reload_src_buffer = NULL;
    reload_dst_buffer = NULL;

//    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE){

        // PIPO0 trigger
        if (event & CSK_GPDMA_EVENT_PIPO0_DONE){
            int32_t ret = 0;

            // Switch the source and destination buffer for compare and reload
            switch(cur_buffer_0){
            case 0:
                src_buffer = gpdma_src_buffer;
                dst_buffer = gpdma_dst_buffer;

                reload_src_buffer = gpdma_src1_buffer;
                reload_dst_buffer = gpdma_dst1_buffer;

                cur_buffer_0 = 1;
                break;
            case 1:
                src_buffer = gpdma_src1_buffer;
                dst_buffer = gpdma_dst1_buffer;

                reload_src_buffer = gpdma_src2_buffer;
                reload_dst_buffer = gpdma_dst2_buffer;

                cur_buffer_0 = 2;
                break;
            case 2:
                src_buffer = gpdma_src2_buffer;
                dst_buffer = gpdma_dst2_buffer;

                reload_src_buffer = gpdma_src_buffer;
                reload_dst_buffer = gpdma_dst_buffer;

                cur_buffer_0 = 0;
                break;
            }

            GPDMA_PiPo_Reload(ch, reload_src_buffer, reload_dst_buffer);

            ret = memcmp(dst_buffer, src_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH * sample_len);
            if (ret != 0){
                CLOGE("[GPDMA][PIPO0][RELOAD][M2M][%d] source: 0x%x -> destination: 0x%x transfer error", ch, src_buffer, dst_buffer);
            } else {
                CLOGD("[GPDMA][PIPO0][RELOAD][M2M][%d] source: 0x%x -> destination: 0x%x transfer success!!!", ch, src_buffer, dst_buffer);
            }

            if (g_flag != 0){
                g_flag--;
            } else {
                GPDMA_Stop(ch);
            }
        }
        // PIPO1
        else if (event & CSK_GPDMA_EVENT_PIPO1_DONE){
            int32_t ret = 0;

            // Switch the source and destination buffer for compare and reload
            switch(cur_buffer_1){
            case 0:
                src_buffer = gpdma_src_buffer;
                dst_buffer = gpdma_dst_buffer;

                reload_src_buffer = gpdma_src1_buffer;
                reload_dst_buffer = gpdma_dst1_buffer;

                cur_buffer_1 = 1;
                break;
            case 1:
                src_buffer = gpdma_src1_buffer;
                dst_buffer = gpdma_dst1_buffer;

                reload_src_buffer = gpdma_src2_buffer;
                reload_dst_buffer = gpdma_dst2_buffer;

                cur_buffer_1 = 2;
                break;
            case 2:
                src_buffer = gpdma_src2_buffer;
                dst_buffer = gpdma_dst2_buffer;

                reload_src_buffer = gpdma_src_buffer;
                reload_dst_buffer = gpdma_dst_buffer;

                cur_buffer_1 = 0;
                break;
            }

            GPDMA_PiPo_Reload(ch, reload_src_buffer, reload_dst_buffer);

            ret = memcmp(dst_buffer, src_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH * sample_len);
            if (ret != 0){
                CLOGE("[GPDMA][PIPO1][RELOAD][M2M][%d] source: 0x%x -> destination: 0x%x transfer error", ch, src_buffer, dst_buffer);
            } else {
                CLOGD("[GPDMA][PIPO1][RELOAD][M2M][%d] source: 0x%x -> destination: 0x%x transfer success!!!", ch, src_buffer, dst_buffer);
            }

            if (g_flag != 0){
                g_flag--;
            } else {
                GPDMA_Stop(ch);
            }
        }
        else {
            // Error
        }
//    }
}

static void GPDMA_PiPoMode_M2M_Word_Channel0_Reload_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with Word ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (0 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint32_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel0_Reload_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with HalfWord ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (0 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint16_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Byte_Channel0_Reload_Test(){
    CLOGD("[GPDMA][Channel0] Memory to memory with Byte ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (0 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint8_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Word_Channel1_Reload_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with Word ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (1 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint32_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel1_Reload_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with HalfWord ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (1 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint16_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Byte_Channel1_Reload_Test(){
    CLOGD("[GPDMA][Channel1] Memory to memory with Byte ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (1 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint8_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch1,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Word_Channel2_Reload_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with Word ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (2 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint32_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel2_Reload_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with HalfWord ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (2 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint16_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Byte_Channel2_Reload_Test(){
    CLOGD("[GPDMA][Channel2] Memory to memory with Byte ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (2 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint8_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch2,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Word_Channel3_Reload_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with Word ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (3 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint32_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel3_Reload_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with HalfWord ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (3 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint16_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Byte_Channel3_Reload_Test(){
    CLOGD("[GPDMA][Channel3] Memory to memory with Byte ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (3 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint8_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch3,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Word_Channel4_Reload_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with Word ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (4 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint32_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel4_Reload_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with HalfWord ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (4 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint16_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Byte_Channel4_Reload_Test(){
    CLOGD("[GPDMA][Channel4] Memory to memory with Byte ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (4 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint8_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch4,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Word_Channel5_Reload_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with Word ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (5 << 16) | (4);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
        *((uint32_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint32_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_HalfWord_Channel5_Reload_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with HalfWord ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (5 << 16) | (2);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
        *((uint16_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint16_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_halfword,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

static void GPDMA_PiPoMode_M2M_Byte_Channel5_Reload_Test(){
    CLOGD("[GPDMA][Channel5] Memory to memory with Byte ping pong reload mode validation, the source and destination are both PIPO mode, will change in each interrupt!!!, %s", __FUNCTION__);

    g_flag = CSK_GPDMA_PIPO_RELOAD_VALIDATION_LOOP_CNT;

        // Malloc buffer
    pipo_reload_workspace = (5 << 16) | (1);
    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst1_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    gpdma_src2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);
    gpdma_dst2_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    cur_buffer_0 = 0;
    cur_buffer_1 = 2;

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
        *((uint8_t*)gpdma_src1_buffer + i) = i*(i + 1);
        *((uint8_t*)gpdma_src2_buffer + i) = i*(i + 2);
    }

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_byte,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_pipo_reload_test_callback, &pipo_reload_workspace);

    GPDMA_Start_PiPo(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_src2_buffer, (void*)gpdma_dst_buffer, (void*)gpdma_dst2_buffer, CSK_GPDMA_PIPO_RELOAD_VALIDATION_LENGTH);

    while(g_flag);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);

    free(gpdma_src1_buffer);
    free(gpdma_dst1_buffer);

    free(gpdma_src2_buffer);
    free(gpdma_dst2_buffer);
}

#define CSK_GPDMA_GATHER_VALIDATION_LENGTH                0x1000
#define CSK_GPDMA_SCATTER_VALIDATION_LENGTH               0x1000

static volatile uint32_t GPDMA_Scatter_Event = 0;
static volatile uint32_t GPDMA_Gather_Event = 0;

static void* gpdma_ref_buffer = NULL;

static void gpdma_scatter_test_callback(uint32_t event, void* workspace){
    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE){
        CLOGD("GPDMA Scatter trigger");
    }

    GPDMA_Scatter_Event = CSK_GPDMA_EVENT_TRANSFER_DONE;
}

static void gpdma_gather_test_callback(uint32_t event, void* workspace){
    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE){
        CLOGD("GPDMA Gather trigger");
    }

    GPDMA_Gather_Event = CSK_GPDMA_EVENT_TRANSFER_DONE;
}

static uint32_t __gpdma_gather_golden_data(uint8_t unit, uint16_t interval, uint8_t count){
    uint32_t res_length = 0;
    uint32_t i, j;
    for(i = 0, j = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i += interval){
        if (i + count >= CSK_GPDMA_GATHER_VALIDATION_LENGTH){
            break;
        }

        memcpy(((uint8_t*)gpdma_ref_buffer + j * unit), ((uint8_t*)gpdma_src_buffer + i * unit), unit * count);

        // Increase count
        i += count;
        j += count;
        res_length += count;
        if (res_length >= CSK_GPDMA_GATHER_VALIDATION_LENGTH){
            res_length -= count;
            break;
        }
    }

    return res_length;
}

static uint32_t __gpdma_scatter_golden_data(uint8_t unit, uint16_t interval, uint8_t count){
    uint32_t res_length = 0;
    uint32_t i, j;
    for(i = 0, j = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; j += interval){
        if (j + count >= CSK_GPDMA_SCATTER_VALIDATION_LENGTH){
            break;
        }

        memcpy(((uint8_t*)gpdma_ref_buffer + j * unit), ((uint8_t*)gpdma_src_buffer + i * unit), unit * count);

        i += count;
        j += count;
        res_length += count;
        if (j >= CSK_GPDMA_SCATTER_VALIDATION_LENGTH){
            res_length -= count;
            break;
        }
    }

    return res_length;
}

__attribute__((used)) static int __gpdma_memcmp(uint32_t length){
    uint32_t i = 0;
    for (i = 0; i < length; i++){
        if (*((uint32_t*)gpdma_dst_buffer + i) != *((uint32_t*)gpdma_ref_buffer + i)){
            return -1;
        }
    }

    return 0;
}

static void GPDMA_Gather_M2M_Word_Channel0_FullCoverage_Test(){
    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .sample_unit = gpdma_sample_unit_word,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
            .gather_en = 1,
            .gather_counter = 1,
            .gather_interval = 0,
    };

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    for (gpdma_para.gather_interval = 0; gpdma_para.gather_interval <= GPDMA_SCATTER_GATHER_INTERVAL_MAX; gpdma_para.gather_interval += 1){
        for (gpdma_para.gather_counter = 1; gpdma_para.gather_counter < GPDMA_SCATTER_GATHER_COUNTER_MAX; gpdma_para.gather_counter += 1){
            memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

            ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

            CLOGD("[GPDMA][GATHER][M2M][CH0][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

            GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

            GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

            while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
            GPDMA_Gather_Event = 0;

            ret = __gpdma_memcmp(ref_length);
            if (ret != 0){
                CLOGE("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
            } else {
                CLOGD("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
            }
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}

static void GPDMA_Gather_M2M_Word_Channel0_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_HalfWord_Channel0_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Byte_Channel0_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH0][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Word_Channel1_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch1,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_HalfWord_Channel1_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch1,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);
    CLOGD("ref_length = %d", ref_length);

    CLOGD("[GPDMA][GATHER][M2M][CH1][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Byte_Channel1_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch1,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH1][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Word_Channel2_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch2,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_HalfWord_Channel2_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch2,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Byte_Channel2_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch2,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH2][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Word_Channel3_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch3,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_HalfWord_Channel3_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch3,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Byte_Channel3_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch3,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }
    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH3][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Word_Channel4_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch4,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_HalfWord_Channel4_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch4,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Byte_Channel4_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch4,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH4][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Word_Channel5_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch5,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(4, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][Word] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_HalfWord_Channel5_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch5,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(2, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][HalfWord] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Gather_M2M_Byte_Channel5_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch5,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .gather_en = 1,
        .gather_counter = 1,
        .gather_interval = 0,
    };

    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_GATHER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.gather_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.gather_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    ref_length = __gpdma_gather_golden_data(1, gpdma_para.gather_interval, gpdma_para.gather_counter);

    CLOGD("[GPDMA][GATHER][M2M][CH5][Byte] Gather counter: 0x%x; Gather interval: 0x%x, length: 0x%x", gpdma_para.gather_counter, gpdma_para.gather_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_gather_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Gather_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Gather_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][GATHER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][GATHER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}

static void GPDMA_Scatter_M2M_Word_Channel0_FullCoverage_Test(){
    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .sample_unit = gpdma_sample_unit_word,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
            .scatter_counter = 1,
            .scatter_en = 1,
            .scatter_interval = 0,
    };

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    for (gpdma_para.scatter_interval = 0; gpdma_para.scatter_interval <= GPDMA_SCATTER_GATHER_INTERVAL_MAX; gpdma_para.scatter_interval += 1){
        for (gpdma_para.scatter_counter = 1; gpdma_para.scatter_counter < GPDMA_SCATTER_GATHER_COUNTER_MAX; gpdma_para.scatter_counter += 1){
            memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
            memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

            ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

            CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

            GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

            GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

            while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
            GPDMA_Scatter_Event = 0;

            ret = __gpdma_memcmp(ref_length);
            if (ret != 0){
                CLOGE("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
            } else {
                CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
            }
        }
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}

static void GPDMA_Scatter_M2M_Word_Channel0_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_HalfWord_Channel0_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Byte_Channel0_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH0][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH0][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Word_Channel1_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch1,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_HalfWord_Channel1_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch1,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Byte_Channel1_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch1,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH1][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch1, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH1][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Word_Channel2_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch2,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_HalfWord_Channel2_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch2,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Byte_Channel2_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch2,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH2][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch2, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH2][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Word_Channel3_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch3,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_HalfWord_Channel3_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch3,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Byte_Channel3_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch3,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH3][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch3, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH3][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Word_Channel4_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch4,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_HalfWord_Channel4_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch4,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Byte_Channel4_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch4,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH4][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch4, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH4][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Word_Channel5_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch5,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_word,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint32_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(4, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][Word] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][Word] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_HalfWord_Channel5_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch5,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_halfword,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint16_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint16_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(2, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][HalfWord] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][HalfWord] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}
static void GPDMA_Scatter_M2M_Byte_Channel5_Test(){
    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch5,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .sample_unit = gpdma_sample_unit_byte,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
        .scatter_counter = 1,
        .scatter_en = 1,
        .scatter_interval = 0,
    };

    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    volatile uint32_t ref_length = 0;
    volatile int32_t ret = 0;

    GPDMA_Initialize();

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    gpdma_ref_buffer = malloc(sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);

    if (gpdma_src_buffer == NULL || gpdma_dst_buffer == NULL || gpdma_ref_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(gpdma_src_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_GATHER_VALIDATION_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_SCATTER_VALIDATION_LENGTH; i++){
        *((uint8_t*)gpdma_src_buffer + i) = i*i;
    }

    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Reconfigure
    gpdma_para.scatter_counter = GPDMA_SCATTER_GATHER_COUNTER_MIN + rand() % (GPDMA_SCATTER_GATHER_COUNTER_MAX - GPDMA_SCATTER_GATHER_COUNTER_MIN + 1);
    gpdma_para.scatter_interval = rand() % GPDMA_SCATTER_GATHER_INTERVAL_MAX;

    memset(gpdma_dst_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    memset(gpdma_ref_buffer, 0, sizeof(uint8_t) * CSK_GPDMA_SCATTER_VALIDATION_LENGTH);
    ref_length = __gpdma_scatter_golden_data(1, gpdma_para.scatter_interval, gpdma_para.scatter_counter);

    CLOGD("[GPDMA][SCATTER][M2M][CH5][Byte] Scatter counter: 0x%x; Scatter interval: 0x%x, length: 0x%x", gpdma_para.scatter_counter, gpdma_para.scatter_interval, ref_length);

    GPDMA_Config(&gpdma_para, gpdma_scatter_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, ref_length);

    while(!(GPDMA_Scatter_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Scatter_Event = 0;

    ret = __gpdma_memcmp(ref_length);
    if (ret != 0){
        CLOGE("[GPDMA][SCATTER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer error\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][SCATTER][M2M][CH5][Byte] source: 0x%x -> destination: 0x%x transfer success!!!\r\n\r\n\r\n", gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    free(gpdma_ref_buffer);
}

static void GPDMA_GetCnt_Test(){
    CLOGD("[GPDMA]Memory to memory with normal mode, Using GPDMA_GetCnt function to get transfer count!!!, %s", __FUNCTION__);

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t*) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t*) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++){
        *((uint32_t*)gpdma_src_buffer + i) = i*i;
    }

    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = gpdma_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void*)gpdma_src_buffer, (void*)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[GPDMA][NORMAL][M2M] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer, gpdma_dst_buffer);
    } else {
        CLOGD("[GPDMA][NORMAL][M2M] source: 0x%x -> destination: 0x%x transfer success!!!", gpdma_src_buffer, gpdma_dst_buffer);
    }

    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch0, &cnt_len);
    CLOGD("[GPDMA][NORMAL][M2M] Get count: %d", cnt_len);

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
}

#define CSK_GPDMA_VERY_LARGE_VALIDATION_LENGTH        0x123456

static void GPDMA_Stop_Test(){
    CLOGD("[GPDMA]Memory to memory with normal mode, Using GPDMA_Stop function to stop current transfer!!!, %s", __FUNCTION__);

    uint32_t gpdma_fix_src_buffer = 0x55aa55aa;
    uint32_t gpdma_fix_dst_buffer;

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_fix,
            .sample_unit = gpdma_sample_unit_word,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)&gpdma_fix_src_buffer, (void*)&gpdma_fix_dst_buffer, CSK_GPDMA_VERY_LARGE_VALIDATION_LENGTH);

    // Wait for 1 ms
    SysTick_Delay_Ms(1);

    GPDMA_Stop(gp_dma_ch5);

    // Get count
    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][NORMAL][M2M] Get count: %d", cnt_len);

    // Restart
    GPDMA_Start_Normal(gp_dma_ch5, (void*)&gpdma_fix_src_buffer, (void*)&gpdma_fix_dst_buffer, CSK_GPDMA_VERY_LARGE_VALIDATION_LENGTH);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][NORMAL][M2M] Get count: %d", cnt_len);

    GPDMA_Uninitialize();
}

static void GPDMA_StopRecume_Test(){
    CLOGD("[GPDMA]Memory to memory with normal mode, Using GPDMA_Stop function and resume current transfer!!!, %s", __FUNCTION__);

    uint32_t gpdma_fix_src_buffer = 0x55aa55aa;
    uint32_t gpdma_fix_dst_buffer;

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch5,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_fix,
            .sample_unit = gpdma_sample_unit_word,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch5, (void*)&gpdma_fix_src_buffer, (void*)&gpdma_fix_dst_buffer, CSK_GPDMA_VERY_LARGE_VALIDATION_LENGTH);

    // Wait for 1 ms
    SysTick_Delay_Ms(1);

    GPDMA_Stop(gp_dma_ch5);

    // Get count
    uint32_t cnt_len = 0;
    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][NORMAL][M2M] Get count: %d", cnt_len);

    // Resume
    GPDMA_Resume(gp_dma_ch5);

    while(!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPDMA_Event = 0;

    GPDMA_GetCnt(gp_dma_ch5, &cnt_len);
    CLOGD("[GPDMA][NORMAL][M2M] Get count: %d", cnt_len);

    GPDMA_Uninitialize();
}

int main(){
    uint32_t times;

    // Enable global interrupt
    enable_GINT();

    // Disable D-Cache
    DisableDCache();
    __RWMB();
    __FENCE_I();

    logInit(0, 115200);

    CLOGD("GPDMA VALIDATION");
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }

    while(1);
}
