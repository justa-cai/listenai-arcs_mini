#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "Driver_DMA2D.h"
#include "nmsis_core.h"

typedef void (*function)(void);

/* Exported constants --------------------------------------------------------*/
#define CSK_DMA2D_VALIDATION_WORD_LENGTH 4

/* Exported types ------------------------------------------------------------*/
static void *dma2d_src_buffer = NULL;
static void *dma2d_dst_buffer = NULL;
static void *dma2d_src1_buffer = NULL;
static void *dma2d_dst1_buffer = NULL;

/* Fucntion decalre ------------------------------------------------------------*/
static void DMA2D_NormalMode_M2M_Word_Channel6_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel7_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel8_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel9_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel6_Succession_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel7_Succession_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel8_Succession_Test(void);
static void DMA2D_NormalMode_M2M_Word_Channel9_Succession_Test(void);
static void DMA2D_PiPoMode_M2M_Word_Channel6_Test(void);
static void DMA2D_PiPoMode_M2M_Word_Channel7_Test(void);
static void DMA2D_PiPoMode_M2M_Word_Channel8_Test(void);
static void DMA2D_PiPoMode_M2M_Word_Channel9_Test(void);
static void DMA2D_Image_YUV444_to_YUV422_Test(void);
static void DMA2D_Image_YUV422_to_RGB888_Test(void);
static void DMA2D_Image_YUV422_to_BGR888_Test(void);
static void DMA2D_Image_YUV422_to_RGBA8888_Test(void);
static void DMA2D_Image_YUV422_to_BGRA8888_Test(void);
static void DMA2D_Image_YUV444_to_RGB888_Test(void);
static void DMA2D_Image_YUV444_to_BGR888_Test(void);
static void DMA2D_Image_YUV444_to_RGBA8888_Test(void);
static void DMA2D_Image_YUV422_to_Y8_Test(void);
static void DMA2D_Image_BGR888_to_Y8_Test(void);
static void DMA2D_Image_RGB888_to_Y8_Test(void);


static void DMA2D_Image_YUV422_Crop_Test(void);
static void DMA2D_Image_RGB888_Crop_Test(void);
static void DMA2D_Image_BGR888_Crop_Test(void);
static void DMA2D_Image_ARGB8888_Crop_Test(void);
static void DMA2D_Image_ABGR8888_Crop_Test(void);


static void DMA2D_Image_RGB888_Zoom_Scale_1_to_2(void);
static void DMA2D_Image_RGB888_Zoom_Scale_1_to_3(void);
static void DMA2D_Image_RGB888_Zoom_Scale_1_to_4(void);
static void DMA2D_Image_ARGB888_Zoom_Scale_1_to_2(void);
static void DMA2D_Image_ARGB888_Zoom_Scale_1_to_3(void);
static void DMA2D_Image_ARGB888_Zoom_Scale_1_to_4(void);
static void DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_2(void);
static void DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_3(void);
static void DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_4(void);
static void DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_2(void);
static void DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_3(void);
static void DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_4(void);

static void DMA2D_Image_YUV422_Rotate_CW_90(void);
static void DMA2D_Image_YUV422_Rotate_CCW_90(void);
static void DMA2D_Image_YUV422_Rotate_CW_90_to_RGB(void);
static void DMA2D_Image_YUV422_Rotate_CCW_90_to_RGB(void);

static function test_function_array[] = {
//	DMA2D_NormalMode_M2M_Word_Channel6_Test,
//	DMA2D_NormalMode_M2M_Word_Channel7_Test,
//	DMA2D_NormalMode_M2M_Word_Channel8_Test,
//	DMA2D_NormalMode_M2M_Word_Channel9_Test,
//	DMA2D_NormalMode_M2M_Word_Channel6_Succession_Test,
//	DMA2D_NormalMode_M2M_Word_Channel7_Succession_Test,
//	DMA2D_NormalMode_M2M_Word_Channel8_Succession_Test,
//	DMA2D_NormalMode_M2M_Word_Channel9_Succession_Test,
//	DMA2D_PiPoMode_M2M_Word_Channel6_Test,
//	DMA2D_PiPoMode_M2M_Word_Channel7_Test,
//	DMA2D_PiPoMode_M2M_Word_Channel8_Test,
//	DMA2D_PiPoMode_M2M_Word_Channel9_Test,
//	DMA2D_Image_YUV444_to_YUV422_Test,
//	DMA2D_Image_YUV422_to_RGB888_Test,
//	DMA2D_Image_YUV422_to_BGR888_Test,
//	DMA2D_Image_YUV422_to_RGBA8888_Test,
//	DMA2D_Image_YUV422_to_BGRA8888_Test,
//	DMA2D_Image_YUV444_to_RGB888_Test,
//	DMA2D_Image_YUV444_to_BGR888_Test,
//	DMA2D_Image_YUV444_to_RGBA8888_Test,

//	DMA2D_Image_YUV422_to_Y8_Test,
//	DMA2D_Image_BGR888_to_Y8_Test,
//	DMA2D_Image_RGB888_to_Y8_Test,

//	DMA2D_Image_YUV422_Crop_Test,
//	DMA2D_Image_RGB888_Crop_Test,
//	DMA2D_Image_BGR888_Crop_Test,
//	DMA2D_Image_ARGB8888_Crop_Test,
//	DMA2D_Image_ABGR8888_Crop_Test,

//	DMA2D_Image_RGB888_Zoom_Scale_1_to_2,
//	DMA2D_Image_RGB888_Zoom_Scale_1_to_3,
//	DMA2D_Image_RGB888_Zoom_Scale_1_to_4,
//	DMA2D_Image_ARGB888_Zoom_Scale_1_to_2,
//	DMA2D_Image_ARGB888_Zoom_Scale_1_to_3,
//	DMA2D_Image_ARGB888_Zoom_Scale_1_to_4,
//	DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_2,
//	DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_3,
//	DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_4,
//	DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_2,
//	DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_3,
//	DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_4,

//	DMA2D_Image_YUV422_Rotate_CW_90,
//	DMA2D_Image_YUV422_Rotate_CCW_90,
//	DMA2D_Image_YUV422_Rotate_CW_90_to_RGB,
	DMA2D_Image_YUV422_Rotate_CCW_90_to_RGB,

};

/* Fucntion implement ------------------------------------------------------------*/
static volatile uint32_t DMA2D_Event = 0;

static void dma2d_normal_test_callback(uint32_t event, void *workspace)
{
    if (event & CSK_DMA2D_EVENT_TRANSFER_DONE)
    {
        CLOGD("DMA2D Normal mode trigger");
    }

    DMA2D_Event = CSK_DMA2D_EVENT_TRANSFER_DONE;
}

#define CSK_DMA2D_IMG_IN_LENGTH 100

static void DMA2D_NormalMode_M2M_Word_Channel6_Test()
{
    CLOGD("[DMA2D][Channel6] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch6,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    DMA2D_Start_Normal(dma_2d_ch6, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

    while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
        ;
    DMA2D_Event = 0;

    int32_t ret = 0;
    ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
    if (ret != 0) {
        CLOGE("[DMA2D][NORMAL][M2M][CH6][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
    }else {
        CLOGD("[DMA2D][NORMAL][M2M][CH6][Word] source: 0x%x -> destination: 0x%x transfer success!!!", dma2d_src_buffer, dma2d_dst_buffer);
    }
    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

static void DMA2D_NormalMode_M2M_Word_Channel7_Test(void)
{
    CLOGD("[DMA2D][Channel7] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch7,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    DMA2D_Start_Normal(dma_2d_ch7, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

    while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
        ;
    DMA2D_Event = 0;

    int32_t ret = 0;
    ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
    if (ret != 0) {
        CLOGE("[DMA2D][NORMAL][M2M][CH7][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
    } else {
        CLOGD("[DMA2D][NORMAL][M2M][CH7][Word] source: 0x%x -> destination: 0x%x transfer success!!!", dma2d_src_buffer, dma2d_dst_buffer);
    }
    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

static void DMA2D_NormalMode_M2M_Word_Channel8_Test(void)
{
    CLOGD("[DMA2D][Channel8] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch8,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    DMA2D_Start_Normal(dma_2d_ch8, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

    while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
        ;
    DMA2D_Event = 0;

    int32_t ret = 0;
    ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
    if (ret != 0){
        CLOGE("[DMA2D][NORMAL][M2M][CH8][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
    } else {
        CLOGD("[DMA2D][NORMAL][M2M][CH8][Word] source: 0x%x -> destination: 0x%x transfer success!!!", dma2d_src_buffer, dma2d_dst_buffer);
    }
    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

static void DMA2D_NormalMode_M2M_Word_Channel9_Test(void)
{
    CLOGD("[DMA2D][Channel9] Memory to memory with Word base validation, using normal mode and one block transfer, only trigger one interrupt!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    DMA2D_Start_Normal(dma_2d_ch9, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

    while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
        ;
    DMA2D_Event = 0;

    int32_t ret = 0;
    ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
    if (ret != 0) {
        CLOGE("[DMA2D][NORMAL][M2M][CH9][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
    } else {
        CLOGD("[DMA2D][NORMAL][M2M][CH9][Word] source: 0x%x -> destination: 0x%x transfer success!!!", dma2d_src_buffer, dma2d_dst_buffer);
    }
    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

#define DMA2D_SUCCESSION_LOOP 50

static void DMA2D_NormalMode_M2M_Word_Channel6_Succession_Test(void)
{
    CLOGD("[DMA2D][Channel6] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch6,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    uint8_t cnt = 0;

    uint8_t test_time = 0;

    for (cnt = 0; cnt < DMA2D_SUCCESSION_LOOP; cnt++)
    {
        DMA2D_Start_Normal(dma_2d_ch6, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

        while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
            ;
        DMA2D_Event = 0;

        int32_t ret = 0;
        ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
        if (ret != 0) {
            CLOGE("[DMA2D][NORMAL][M2M][CH6][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
        } else {
            test_time++;
            CLOGD("Success Test_time = %d\n", test_time);
        }
    }

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

static void DMA2D_NormalMode_M2M_Word_Channel7_Succession_Test(void)
{
    CLOGD("[DMA2D][Channel7] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for(i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch7,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    uint8_t cnt = 0;

    uint8_t test_time = 0;

    for (cnt = 0; cnt < DMA2D_SUCCESSION_LOOP; cnt++)
    {
        DMA2D_Start_Normal(dma_2d_ch7, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

        while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
            ;
        DMA2D_Event = 0;

        int32_t ret = 0;
        ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
        if (ret != 0) {
            CLOGE("[DMA2D][NORMAL][M2M][CH7][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
        } else {
            test_time++;
            CLOGD("Success Test_time = %d\n", test_time);
        }
    }

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

static void DMA2D_NormalMode_M2M_Word_Channel8_Succession_Test(void)
{
    CLOGD("[DMA2D][Channel8] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch8,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    uint8_t cnt = 0;

    uint8_t test_time = 0;

    for (cnt = 0; cnt < DMA2D_SUCCESSION_LOOP; cnt++)
    {
        DMA2D_Start_Normal(dma_2d_ch8, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

        while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
            ;
        DMA2D_Event = 0;

        int32_t ret = 0;
        ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
        if (ret != 0) {
            CLOGE("[DMA2D][NORMAL][M2M][CH8][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
        } else {
            test_time++;
            CLOGD("Success Test_time = %d\n", test_time);
        }
    }

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

static void DMA2D_NormalMode_M2M_Word_Channel9_Succession_Test(void)
{
    CLOGD("[DMA2D][Channel9] Memory to memory with Word succession validation, using normal mode and one block transfer, it will trigger a mount of interrupt depending on the macro 'GPDMA_SUCCESSION_LOOP'!!!, %s", __FUNCTION__);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    if (dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_IMG_IN_LENGTH);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_IMG_IN_LENGTH; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_normal_test_callback, NULL);

    uint8_t cnt = 0;

    uint8_t test_time = 0;

    for (cnt = 0; cnt < DMA2D_SUCCESSION_LOOP; cnt++)
    {
        DMA2D_Start_Normal(dma_2d_ch9, (void *)dma2d_src_buffer, (void *)dma2d_dst_buffer, CSK_DMA2D_IMG_IN_LENGTH);

        while (!(DMA2D_Event & CSK_DMA2D_EVENT_TRANSFER_DONE))
            ;
        DMA2D_Event = 0;

        int32_t ret = 0;
        ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_IMG_IN_LENGTH * CSK_DMA2D_VALIDATION_WORD_LENGTH);
        if (ret != 0) {
            CLOGE("[DMA2D][NORMAL][M2M][CH9][Word] source: 0x%x -> destination: 0x%x transfer error", dma2d_src_buffer, dma2d_dst_buffer);
        } else {
            test_time++;
            CLOGD("Success Test_time = %d\n", test_time);
        }
    }

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);
}

#define CSK_DMA2D_PIPO_IMG_IN_LEN 					0x1000
#define CSK_DMA2D_PIPO_IMG_OUT_LEN 					0x1000
#define CSK_DMA2D_PIPO_VALIDATION_LOOP_CNT 			2

static volatile uint8_t g_flag = 0;
static uint32_t pipo_workspace = 0;

static void dma2d_pipo_test_callback(uint32_t event, void *workspace)
{
    uint32_t sample_len = 0;
    uint8_t ch = 0;
    sample_len = ((uint32_t) * (uint32_t *)workspace & 0xF);
    ch = (((uint32_t) * (uint32_t *)workspace >> 16) & 0xF);

    if (event & CSK_DMA2D_EVENT_TRANSFER_DONE)
    {
        // PIPO0
        if (event & CSK_DMA2D_EVENT_PIPO0_DONE)
        {
            int32_t ret = 0;
            ret = memcmp(dma2d_dst_buffer, dma2d_src_buffer, CSK_DMA2D_PIPO_IMG_IN_LEN * sample_len);
            if (ret != 0) {
                CLOGE("[DMA2D][PIPO0][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer error", ch, dma2d_src_buffer, dma2d_dst_buffer);
            } else {
                CLOGD("[DMA2D][PIPO0][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer success!!!", ch, dma2d_src_buffer, dma2d_dst_buffer);
            }
            if (g_flag != 0) {
                g_flag--;
            } else {
                DMA2D_Stop(ch);
            }
        }
        // PIPO1
        else if (event & CSK_DMA2D_EVENT_PIPO1_DONE)
        {
            int32_t ret = 0;
            ret = memcmp(dma2d_dst1_buffer, dma2d_src1_buffer, CSK_DMA2D_PIPO_IMG_IN_LEN * sample_len);
            if (ret != 0) {
                CLOGE("[DMA2D][PIPO1][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer error", ch, dma2d_src1_buffer, dma2d_dst1_buffer);
            } else {
                CLOGD("[DMA2D][PIPO1][M2M][ch%d] source: 0x%x -> destination: 0x%x transfer success!!!", ch, dma2d_src1_buffer, dma2d_dst1_buffer);
            }
            if (g_flag != 0) {
                g_flag--;
            } else {
                DMA2D_Stop(ch);
            }
        }
        // Error
        else
        {
            // Error Handle
        }
    }
}

static void DMA2D_PiPoMode_M2M_Word_Channel6_Test(void)
{
    CLOGD("[DMA2D][Channel6] Memory to memory with Word ping pong mode validation, the source and destination are both PING PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_DMA2D_PIPO_VALIDATION_LOOP_CNT;

    // set workspace
    pipo_workspace = (dma_2d_ch6 << 16) | (4);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    // Malloc buffer
    dma2d_src1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    if(dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL || dma2d_src1_buffer == NULL || dma2d_dst1_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_src1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_PIPO_IMG_IN_LEN; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
        *((uint32_t *)dma2d_src1_buffer + i) = i * (i + 1);
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch6,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_pipo,
        .dst_mode = address_mode_pipo,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();
    DMA2D_Config(&dma2d_para, dma2d_pipo_test_callback, &pipo_workspace);
    DMA2D_Start_PiPo(dma_2d_ch6, (void *)dma2d_src_buffer, (void *)dma2d_src1_buffer, (void *)dma2d_dst_buffer, (void *)dma2d_dst1_buffer, CSK_DMA2D_PIPO_IMG_IN_LEN, CSK_DMA2D_PIPO_IMG_OUT_LEN);

    while (g_flag)
        ;
    uint32_t cnt_len = 0;
    DMA2D_GetCnt(dma_2d_ch6, &cnt_len);
    CLOGD("[DMA2D][PIPO][M2M][Channel6] Get count: %d", cnt_len);

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);

    free(dma2d_src1_buffer);
    free(dma2d_dst1_buffer);
}

static void DMA2D_PiPoMode_M2M_Word_Channel7_Test(void)
{
    CLOGD("[DMA2D][Channel7] Memory to memory with Word ping pong mode validation, the source and destination are both PING PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_DMA2D_PIPO_VALIDATION_LOOP_CNT;

    // set workspace
    pipo_workspace = (dma_2d_ch7 << 16) | (4);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    // Malloc buffer
    dma2d_src1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    if(dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL || dma2d_src1_buffer == NULL || dma2d_dst1_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_src1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_PIPO_IMG_IN_LEN; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
        *((uint32_t *)dma2d_src1_buffer + i) = i * (i + 1);
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch7,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_pipo,
        .dst_mode = address_mode_pipo,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();
    DMA2D_Config(&dma2d_para, dma2d_pipo_test_callback, &pipo_workspace);
    DMA2D_Start_PiPo(dma_2d_ch7, (void *)dma2d_src_buffer, (void *)dma2d_src1_buffer, (void *)dma2d_dst_buffer, (void *)dma2d_dst1_buffer, CSK_DMA2D_PIPO_IMG_IN_LEN, CSK_DMA2D_PIPO_IMG_OUT_LEN);

    while (g_flag)
        ;
    uint32_t cnt_len = 0;
    DMA2D_GetCnt(dma_2d_ch7, &cnt_len);
    CLOGD("[DMA2D][PIPO][M2M][Channel7] Get count: %d", cnt_len);

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);

    free(dma2d_src1_buffer);
    free(dma2d_dst1_buffer);
}

static void DMA2D_PiPoMode_M2M_Word_Channel8_Test(void)
{
    CLOGD("[DMA2D][Channel8] Memory to memory with Word ping pong mode validation, the source and destination are both PING PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_DMA2D_PIPO_VALIDATION_LOOP_CNT;

    // set workspace
    pipo_workspace = (dma_2d_ch8 << 16) | (4);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    // Malloc buffer
    dma2d_src1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    if(dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL || dma2d_src1_buffer == NULL || dma2d_dst1_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_src1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_PIPO_IMG_IN_LEN; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
        *((uint32_t *)dma2d_src1_buffer + i) = i * (i + 1);
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch8,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_pipo,
        .dst_mode = address_mode_pipo,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();
    DMA2D_Config(&dma2d_para, dma2d_pipo_test_callback, &pipo_workspace);
    DMA2D_Start_PiPo(dma_2d_ch8, (void *)dma2d_src_buffer, (void *)dma2d_src1_buffer, (void *)dma2d_dst_buffer, (void *)dma2d_dst1_buffer, CSK_DMA2D_PIPO_IMG_IN_LEN, CSK_DMA2D_PIPO_IMG_OUT_LEN);

    while (g_flag)
        ;
    uint32_t cnt_len = 0;
    DMA2D_GetCnt(dma_2d_ch8, &cnt_len);
    CLOGD("[DMA2D][PIPO][M2M][Channel8] Get count: %d", cnt_len);

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);

    free(dma2d_src1_buffer);
    free(dma2d_dst1_buffer);
}

static void DMA2D_PiPoMode_M2M_Word_Channel9_Test(void)
{
    CLOGD("[DMA2D][Channel9] Memory to memory with Word ping pong mode validation, the source and destination are both PING PONG mode!!!, %s", __FUNCTION__);

    g_flag = CSK_DMA2D_PIPO_VALIDATION_LOOP_CNT;

    // set workspace
    pipo_workspace = (dma_2d_ch9 << 16) | (4);

    // Malloc buffer
    dma2d_src_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    // Malloc buffer
    dma2d_src1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    dma2d_dst1_buffer = malloc(sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    if(dma2d_src_buffer == NULL || dma2d_dst_buffer == NULL || dma2d_src1_buffer == NULL || dma2d_dst1_buffer == NULL) {
        CLOGE("Failed to allocate memory");
    }

    // Set value 0
    memset(dma2d_src_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_src1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);
    memset(dma2d_dst1_buffer, 0, sizeof(uint32_t) * CSK_DMA2D_PIPO_IMG_IN_LEN);

    uint32_t i = 0;
    for (i = 0; i < CSK_DMA2D_PIPO_IMG_IN_LEN; i++) {
        *((uint32_t *)dma2d_src_buffer + i) = i * i;
        *((uint32_t *)dma2d_src1_buffer + i) = i * (i + 1);
    }

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_pipo,
        .dst_mode = address_mode_pipo,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    DMA2D_Initialize();
    DMA2D_Config(&dma2d_para, dma2d_pipo_test_callback, &pipo_workspace);
    DMA2D_Start_PiPo(dma_2d_ch9, (void *)dma2d_src_buffer, (void *)dma2d_src1_buffer, (void *)dma2d_dst_buffer, (void *)dma2d_dst1_buffer, CSK_DMA2D_PIPO_IMG_IN_LEN, CSK_DMA2D_PIPO_IMG_OUT_LEN);

    while (g_flag)
        ;
    uint32_t cnt_len = 0;
    DMA2D_GetCnt(dma_2d_ch9, &cnt_len);
    CLOGD("[DMA2D][PIPO][M2M][Channel9] Get count: %d", cnt_len);

    // Free
    free(dma2d_src_buffer);
    free(dma2d_dst_buffer);

    free(dma2d_src1_buffer);
    free(dma2d_dst1_buffer);
}

#define DMA2D_IMAGE_WIDTH_SIZE 			64
#define DMA2D_IMAGE_HEIGHT_SIZE			64

#define DMA2D_IMAGE_START_COL			9
#define DMA2D_IMAGE_START_ROW			9
#define DMA2D_IMAGE_END_COL				48
#define DMA2D_IMAGE_END_ROW				48

static volatile uint32_t dma2d_image_event = 0;

static volatile uint32_t dma2d_image_event_res0 = 0;
static volatile uint32_t dma2d_image_event_res1 = 0;

__USED static uint32_t *golden_yuv444_format_image = NULL;
__USED static uint32_t *golden_yuv422_format_image = NULL;
__USED static uint32_t* golden_yuv420_format_image = NULL;
__USED static uint32_t* golden_argb_format_image = NULL;
__USED static uint32_t* golden_abgr_format_image = NULL;
__USED static uint32_t* golden_xrgb_format_image = NULL;
__USED static uint32_t* golden_y8_format_image = NULL;
__USED static uint32_t* golden_xbgr_format_image = NULL;

__USED static uint32_t* golden_yuv422_crop_image = NULL;
__USED static uint32_t* golden_xrgb_crop_image = NULL;
__USED static uint32_t* golden_xbgr_crop_image = NULL;
__USED static uint32_t* golden_argb_crop_image = NULL;
__USED static uint32_t* golden_abgr_crop_image = NULL;

__USED static uint32_t* golden_xrgb_zoom_scale_image = NULL;
__USED static uint32_t* golden_xbgr_zoom_scale_image = NULL;
__USED static uint32_t* golden_argb_zoom_scale_image = NULL;
__USED static uint32_t* golden_abgr_zoom_scale_image = NULL;

__USED static uint32_t* golden_xrgb_crop_zoom_scale_image = NULL;
__USED static uint32_t* golden_argb_crop_zoom_scale_image = NULL;

__USED static uint32_t* golden_yuv422_planar_image_address0 = NULL;
__USED static uint32_t* golden_yuv422_planar_image_address1 = NULL;
__USED static uint32_t* golden_yuv422_rotate_cw_90 = NULL;
__USED static uint32_t* golden_yuv422_rotate_ccw_90 = NULL;
__USED static uint32_t* golden_yuv422_rotate_cw_90_to_rgb = NULL;
__USED static uint32_t* golden_yuv422_rotate_ccw_90_to_rgb = NULL;

#define IMAGE_YUV444_COMPONENT(y, u, v)       ( ((0x00) << 24) | (v << 16) | (u << 8) | y )
#define IMAGE_YUV422_COMPONENT(y, u, v)       ( ((v) << 24) | (y << 16) | (u << 8) | y )

#define clip(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define YUV444ToBGR(Y, Cb, Cr)  ((clip((256 * Y + 444 * (Cb - 128)) >> 8)) | (clip((256 * Y - 183 * (Cr - 128) - 88 * (Cb - 128)) >> 8) << 8) | (clip((256 * Y + 359 * (Cr - 128)) >> 8) << 16))
#define YUV444ToRGB(Y, Cb, Cr)  ((clip((256 * Y + 359 * (Cr - 128)) >> 8)) | (clip((256 * Y - 183 * (Cr - 128) - 88 * (Cb - 128)) >> 8) << 8) | (clip((256 * Y + 444 * (Cb - 128)) >> 8) << 16))

#define YUV422ToBGR(Y, Cb, Cr)  ((clip((256 * Y + 444 * (Cb - 128)) >> 8)) | (clip((256 * Y - 183 * (Cr - 128) - 88 * (Cb - 128)) >> 8) << 8) | (clip((256 * Y + 359 * (Cr - 128)) >> 8) << 16))
#define YUV422ToRGB(Y, Cb, Cr)  ((clip((256 * Y + 359 * (Cr - 128)) >> 8)) | (clip((256 * Y - 183 * (Cr - 128) - 88 * (Cb - 128)) >> 8) << 8) | (clip((256 * Y + 444 * (Cb - 128)) >> 8) << 16))

#define RGBToY8(R, G, B)	((77 * (R) + 150 * (G) + 29 * (B)) / 256)
#define BGRToY8(R, G, B)	((77 * (R) + 150 * (G) + 29 * (B)) / 256)

#define DEF_Y 0xAA
#define DEF_U 0x88
#define DEF_V 0x33

#define DEF_R 0xFF
#define DEF_G 0x55
#define DEF_B 0x11

extern uint8_t buf_yuv422[];
extern uint8_t buf_yuv444[];
extern uint8_t buf_rgb888[];
extern uint8_t buf_bgr888[];
extern uint8_t buf_abgr8888[];
extern uint8_t buf_argb8888[];

__USED static uint8_t *buf_yuv444_padded = NULL;

void insert_zero_padding(const uint8_t *src, uint8_t *dst, size_t src_size) {
    size_t i, j;
    for (i = 0, j = 0; i < src_size; i += 3, j += 4) {
        dst[j] = src[i];
        dst[j + 1] = src[i + 1];
        dst[j + 2] = src[i + 2];
        dst[j + 3] = 0x00;
    }
}

static void dma2d_image_test_callback(uint32_t event, void* workspace) {
    dma2d_image_event = event;
}

static void DMA2D_Image_YUV444_to_YUV422_Test(void) {
	CLOGD("[DMA2D][Image] YUV444 Transfer to YUV422 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE,  __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch6,
		.burst_len = dma2d_burst_len_1spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv444,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv444_yuv422,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
	};


	// Malloc Buffer
	golden_yuv444_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);
	golden_yuv422_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2);

    if(golden_yuv444_format_image == NULL || golden_yuv422_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv444_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);
	memset(golden_yuv422_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2);


	// insert 0x00 at the most 8bit
//	insert_zero_padding((uint8_t*)buf_yuv444, (uint8_t*)golden_yuv444_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4);

	// Generate golden image with YUV444 format
    uint16_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
        golden_yuv444_format_image[i] = IMAGE_YUV444_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch6, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch6, golden_yuv444_format_image, golden_yuv422_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);

    while(!dma2d_image_event)
    	;

    for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2; i++) {
        if ((golden_yuv422_format_image[i]) != (IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V))) {
        	CLOGD("[DMA2D][FORMAT] Index->%d   YUV444->YUV422 compare error: 0x%08x != 0x%08x", i, IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V), golden_yuv422_format_image[i]);
        }
    }

    free(golden_yuv444_format_image);
    free(golden_yuv422_format_image);

    CLOGD("YUV444 Transfer to YUV422 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV422_to_RGB888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Transfer to RGB88 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch6,
		.burst_len = dma2d_burst_len_1spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv422,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv422_xrgb,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_xrgb_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    if(golden_yuv422_format_image == NULL || golden_xrgb_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	memset(golden_xrgb_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    // Generate golden image with YUV422 format
    uint16_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2; i++) {
        golden_yuv422_format_image[i] = IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch6, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch6, golden_yuv422_format_image, golden_xrgb_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2);

    while(!dma2d_image_event);

    int ret = 0;
    volatile uint32_t rgb = YUV422ToRGB(DEF_Y, DEF_U, DEF_V);
    CLOG("rgb_value = 0x%08x", rgb);

    volatile uint32_t brgb = (rgb << 24) | rgb;
    volatile uint32_t gbrg = (rgb << 16) | (rgb >> 8);
    volatile uint32_t rgbr = (rgb << 8) | (rgb >> 16);

    for (uint32_t i = 0; i < (DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4); i++){
        // brgb
        if (i % 3 == 0) {
            if (golden_xrgb_format_image[i] != brgb) {
                ret = -1;
            }
        }
        // gbrg
        else if (i % 3 == 1) {
            if (golden_xrgb_format_image[i] != gbrg) {
                ret = -2;
            }
        }
        // rgbr
        else if (i % 3 == 2) {
            if (golden_xrgb_format_image[i] != rgbr) {
                ret = -3;
            }
        }

        if (ret == -1) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV422->XRGB compare error: 0x%x != 0x%x", i, brgb, golden_xrgb_format_image[i]);
        } else if (ret == -2) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV422->XRGB compare error: 0x%x != 0x%x", i, gbrg, golden_xrgb_format_image[i]);
        } else if (ret == -3) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV422->XRGB compare error: 0x%x != 0x%x", i, rgbr, golden_xrgb_format_image[i]);
        }
        ret = 0;
    }

    free(golden_yuv422_format_image);
    free(golden_xrgb_format_image);

    CLOGD("YUV422 Transfer to RGB888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV422_to_BGR888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Transfer to BGR888 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch7,
		.burst_len = dma2d_burst_len_1spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv422,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv422_xbgr,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		.img_rgb888_format = csk_image_bgr888_format,
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2);
	golden_xbgr_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    if(golden_yuv422_format_image == NULL || golden_xbgr_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2);
	memset(golden_xbgr_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 /4);

    // Generate golden image with YUV422 format
    uint16_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2; i++){
        golden_yuv422_format_image[i] = IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch7, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch7, golden_yuv422_format_image, golden_xbgr_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2);

    while(!dma2d_image_event);

    int ret = 0;
    volatile uint32_t bgr = YUV422ToBGR(DEF_Y, DEF_U, DEF_V);
    CLOG("bgr_value = 0x%08x", bgr);

    volatile uint32_t brgb = (bgr << 24) | bgr;
    volatile uint32_t gbrg = (bgr << 16) | (bgr >> 8);
    volatile uint32_t rgbr = (bgr << 8) | (bgr >> 16);

    for (uint32_t i = 0; i < (DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4); i++){
        // brgb
        if (i % 3 == 0) {
            if (golden_xbgr_format_image[i] != brgb) {
                ret = -1;
            }
        }
        // gbrg
        else if (i % 3 == 1) {
            if (golden_xbgr_format_image[i] != gbrg) {
                ret = -2;
            }
        }
        // rgbr
        else if (i % 3 == 2) {
            if (golden_xbgr_format_image[i] != rgbr) {
                ret = -3;
            }
        }

        if (ret == -1) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV422->XRGB compare error: 0x%x != 0x%x", i, brgb, golden_xbgr_format_image[i]);
        } else if (ret == -2) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV422->XRGB compare error: 0x%x != 0x%x", i, gbrg, golden_xbgr_format_image[i]);
        } else if (ret == -3) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV422->XRGB compare error: 0x%x != 0x%x", i, rgbr, golden_xbgr_format_image[i]);
        }
        ret = 0;
    }
    free(golden_yuv422_format_image);
    free(golden_xbgr_format_image);

    CLOGD("YUV422 Transfer to BGR888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV422_to_RGBA8888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Transfer to RGBA8888 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch8,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv422,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv422_argb,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_argb_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    if(golden_yuv422_format_image == NULL || golden_argb_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	memset(golden_argb_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    // Generate golden image with YUV422 format
    uint16_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2; i++){
        golden_yuv422_format_image[i] = IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch8, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch8, golden_yuv422_format_image, golden_argb_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

    while(!dma2d_image_event);

    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4; i++){
        if (golden_argb_format_image[i] != (YUV422ToRGB(DEF_Y, DEF_U, DEF_V) | (0xFF << 24))){
            CLOGD("[DMA2D][FORMAT] Index->%d   YUV422->RGBA compare error: 0x%x != 0x%x", i, (YUV422ToRGB(DEF_Y, DEF_U, DEF_V) | (0xFF << 24)), golden_argb_format_image[i]);
        }
    }

    free(golden_yuv422_format_image);
    free(golden_argb_format_image);

    CLOGD("YUV422 Transfer to RGBA888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV422_to_BGRA8888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Transfer to BGRA888 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv422,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv422_abgr,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		.img_rgb888_format = csk_image_bgr888_format,
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_abgr_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 3);

    if(golden_yuv422_format_image == NULL || golden_abgr_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	memset(golden_abgr_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 3);

    // Generate golden image with YUV422 format
    for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2; i++){
        golden_yuv422_format_image[i] = IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch9, golden_yuv422_format_image, golden_abgr_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

    while(!dma2d_image_event);

    for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4; i++){
        if (golden_abgr_format_image[i] != (YUV422ToBGR(DEF_Y, DEF_U, DEF_V) | (0xFF << 24))){
            CLOGD("[DMA2D][FORMAT] Index->%d   YUV422->BGRA compare error: 0x%x != 0x%x", i, (YUV422ToBGR(DEF_Y, DEF_U, DEF_V) | (0xFF << 24)), golden_abgr_format_image[i]);
        }
    }

    free(golden_yuv422_format_image);
    free(golden_abgr_format_image);

    CLOGD("YUV422 Transfer to BGRA888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV444_to_RGB888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV444 Transfer to RGB888 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.sample_unit = dma2d_sample_unit_word,
		.tfr_mode = tfr_mode_m2m,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv444,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv444_xrgb,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_yuv444_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
	golden_xrgb_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    if(golden_yuv444_format_image == NULL || golden_xrgb_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv444_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
	memset(golden_xrgb_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

   // Generate golden image with YUV444 format
	for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++){
		golden_yuv444_format_image[i] = IMAGE_YUV444_COMPONENT(DEF_Y, DEF_U, DEF_V);
	}

	// insert 0x00 at the most 8bit
//	insert_zero_padding((uint8_t*)buf_yuv444, (uint8_t*)golden_yuv444_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4);

	DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, golden_yuv444_format_image, golden_xrgb_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);

	while(!dma2d_image_event);

    int ret = 0;
    volatile uint32_t rgb = YUV444ToRGB(DEF_Y, DEF_U, DEF_V);
    CLOG("rgb_value = 0x%08x", rgb);

    volatile uint32_t brgb = (rgb << 24) | rgb;
    volatile uint32_t gbrg = (rgb << 16) | (rgb >> 8);
    volatile uint32_t rgbr = (rgb << 8) | (rgb >> 16);

    for (uint32_t i = 0; i < (DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4); i++) {
        // brgb
        if (i % 3 == 0) {
            if (golden_xrgb_format_image[i] != brgb) {
                ret = -1;
            }
        }
        // gbrg
        else if (i % 3 == 1) {
            if (golden_xrgb_format_image[i] != gbrg) {
                ret = -2;
            }
        }
        // rgbr
        else if (i % 3 == 2) {
            if (golden_xrgb_format_image[i] != rgbr) {
                ret = -3;
            }
        }

        if (ret == -1) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV444->XRGB compare error: 0x%x != 0x%x", i, brgb, golden_xrgb_format_image[i]);
        } else if (ret == -2) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV444->XRGB compare error: 0x%x != 0x%x", i, gbrg, golden_xrgb_format_image[i]);
        } else if (ret == -3) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV444->XRGB compare error: 0x%x != 0x%x", i, rgbr, golden_xrgb_format_image[i]);
        }
        ret = 0;
    }

    free(golden_yuv444_format_image);
    free(golden_xrgb_format_image);

    CLOGD("YUV444 Transfer to RGB888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV444_to_BGR888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV444 Transfer to BGR888 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv444,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv444_xbgr,
		.img_rgb888_format = csk_image_bgr888_format,
	};

	// Malloc Buffer
	golden_yuv444_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
	golden_xbgr_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    if(golden_yuv444_format_image == NULL || golden_xbgr_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv444_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
	memset(golden_xbgr_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

   // Generate golden image with YUV444 format
	for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
		golden_yuv444_format_image[i] = IMAGE_YUV444_COMPONENT(DEF_Y, DEF_U, DEF_V);
	}

	DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, golden_yuv444_format_image, golden_xbgr_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);

	while(!dma2d_image_event);

    int ret = 0;
    volatile uint32_t bgr = YUV444ToBGR(DEF_Y, DEF_U, DEF_V);
    CLOG("bgr_value = 0x%08x", bgr);

    volatile uint32_t brgb = (bgr << 24) | bgr;
    volatile uint32_t gbrg = (bgr << 16) | (bgr >> 8);
    volatile uint32_t rgbr = (bgr << 8) | (bgr >> 16);

    for (uint32_t i = 0; i < (DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4); i++){
        // brgb
        if (i % 3 == 0) {
            if (golden_xbgr_format_image[i] != brgb) {
                ret = -1;
            }
        }
        // gbrg
        else if (i % 3 == 1) {
            if (golden_xbgr_format_image[i] != gbrg) {
                ret = -2;
            }
        }
        // rgbr
        else if (i % 3 == 2) {
            if (golden_xbgr_format_image[i] != rgbr) {
                ret = -3;
            }
        }

        if (ret == -1) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV444->XBGR compare error: 0x%x != 0x%x", i, brgb, golden_xbgr_format_image[i]);
        } else if (ret == -2) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV444->XBGR compare error: 0x%x != 0x%x", i, gbrg, golden_xbgr_format_image[i]);
        } else if (ret == -3) {
            CLOGD("[DMA2D][FORMAT] Index->%d  YUV444->XBGR compare error: 0x%x != 0x%x", i, rgbr, golden_xbgr_format_image[i]);
        }
        ret = 0;
    }
    free(golden_yuv444_format_image);
    free(golden_xbgr_format_image);

    CLOGD("YUV444 Transfer to BGR888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV444_to_RGBA8888_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV444 Transfer to RGBA8888 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch8,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv444,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv444_argb,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_yuv444_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
	golden_argb_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    if(golden_yuv444_format_image == NULL || golden_argb_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv444_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
	memset(golden_argb_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    // Generate golden image with YUV444 format
    uint32_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++){
        golden_yuv444_format_image[i] = IMAGE_YUV444_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch8, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch8, golden_yuv444_format_image, golden_argb_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);

    while(!dma2d_image_event);

    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
        if (golden_argb_format_image[i] != (YUV444ToRGB(DEF_Y, DEF_U, DEF_V) | (0xFF << 24))){
            CLOGD("[DMA2D][FORMAT] Index->%d   YUV444->RGBA compare error: 0x%x != 0x%x", i, (YUV444ToRGB(DEF_Y, DEF_U, DEF_V) | (0xFF << 24)), golden_argb_format_image[i]);
        }
    }

    free(golden_yuv444_format_image);
    free(golden_argb_format_image);

    CLOGD("YUV444 Transfer to RGBA888 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_YUV422_to_Y8_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Transfer to Y8 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);

	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch8,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv422,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_yuv422_y8,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_y8_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 4);

    if(golden_yuv422_format_image == NULL || golden_y8_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	memset(golden_y8_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 4);

    // Generate golden image with YUV422 format
    uint32_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4; i++){
    	golden_yuv422_format_image[i] = IMAGE_YUV422_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch8, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch8, golden_yuv422_format_image, golden_y8_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

    while(!dma2d_image_event);

    for(uint32_t  i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 2; i++ ) {
    	uint32_t y0 = (golden_yuv422_format_image[i] & 0xFF);
    	uint32_t y1 = (golden_yuv422_format_image[i] >> 16) & 0xFF;

        if (y0 != ((uint8_t *)golden_y8_format_image)[i * 2]) {
            CLOGD("[DMA2D][FORMAT] Index->%d   YUV422->Y8 compare error: 0x%x != 0x%x", i * 2, y0, golden_y8_format_image[i * 2]);
        }
        if (y1 != ((uint8_t *)golden_y8_format_image)[i * 2 + 1]) {
            CLOGD("[DMA2D][FORMAT] Index->%d   YUV422->Y8 compare error: 0x%x != 0x%x", i * 2 + 1, y1, golden_y8_format_image[i * 2 + 1]);
        }
    }

    free(golden_yuv422_format_image);
    free(golden_y8_format_image);

    CLOGD("YUV422 Transfer to Y8 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_BGR888_to_Y8_Test(void) {
	CLOGD("[DMA2D][IMAGE] BGR888 Transfer to Y8 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch8,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xbgr,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_xbgr_y8,
		.img_rgb888_format = csk_image_bgr888_format,
	};

	// Malloc Buffer
	golden_xbgr_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);
	golden_y8_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 4);

	memset(golden_xbgr_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);
	memset(golden_y8_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 4);

	uint8_t y_from_bgr = BGRToY8(DEF_R, DEF_G, DEF_B);
	CLOGD("y_from_bgr = 0x%x",y_from_bgr);

    // Generate golden image with xbgr format
    uint32_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4; i++){
    	uint8_t r = DEF_R;
    	uint8_t g = DEF_G;
    	uint8_t b = DEF_B;

    	uint32_t pixel = 0;
    	switch (i % 3) {
    		case 0:
    			// B R G B
    			pixel = (b << 24) | (r << 16) | (g << 8) | b;
    			break;
    		case 1: // G B R G
    			pixel = (g << 24) | (b << 16) | (r << 8) | g;
    			break;
    		case 2: //R G B R
    			pixel = (r << 24) | (g << 16) | (b << 8) | r;
    			break;
    	}
    	golden_xbgr_format_image[i] = pixel;
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch8, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch8, golden_xbgr_format_image, golden_y8_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    while(!dma2d_image_event);

    uint8_t bgr_to_y8 = BGRToY8(DEF_R, DEF_G, DEF_B);
    CLOGD("y_from_bgr = 0x%x",y_from_bgr);
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
        if (((uint8_t *)golden_y8_format_image)[i] != bgr_to_y8) {
            CLOGD("[DMA2D][FORMAT] Index->%d   BGR888->Y8 compare error: 0x%x != 0x%x", i, BGRToY8(DEF_R, DEF_G, DEF_B), ((uint8_t *)golden_y8_format_image)[i]);
        }
    }

    free(golden_xbgr_format_image);
    free(golden_y8_format_image);

    CLOGD("BGR888 Transfer to Y8 with DMA2D function Success!\r\n");
}

static void DMA2D_Image_RGB888_to_Y8_Test(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 Transfer to Y8 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch8,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_output_fromat_transfer = csk_image_format_transfer_xrgb_y8,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);
	golden_y8_format_image = malloc(sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 4);

    if(golden_xrgb_format_image == NULL || golden_y8_format_image == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_xrgb_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);
	memset(golden_y8_format_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE / 4);

	uint8_t y_from_rgb = RGBToY8(DEF_R, DEF_G, DEF_B);
	CLOGD("y_from_rgb = 0x%x",y_from_rgb);

    // Generate golden image with RGB888 format
    uint32_t i = 0;
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4; i++){
    	uint8_t r = DEF_R;
    	uint8_t g = DEF_G;
    	uint8_t b = DEF_B;

    	uint32_t pixel = 0;
    	switch (i % 3) {
    		case 0:
    			// R B G R
    			pixel = (r << 24) | (b << 16) | (g << 8) | r;
    			break;
    		case 1: // G R B G
    			pixel = (g << 24) | (r << 16) | (b << 8) | g;
    			break;
    		case 2: //B G R B
    			pixel = (b << 24) | (g << 16) | (r << 8) | b;
    			break;
    	}
    	golden_xrgb_format_image[i] = pixel;
    }

	DMA2D_Initialize();

    DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

    DMA2D_Image_Config_Extend(dma_2d_ch8, &dma2d_img_cfg);

    DMA2D_Start_Normal(dma_2d_ch8, golden_xrgb_format_image, golden_y8_format_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    while(!dma2d_image_event);

    uint8_t rgb_to_y8 = RGBToY8(DEF_R, DEF_G, DEF_B);
    CLOGD("y_from_rgb = 0x%x",rgb_to_y8);
    for (i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
        if (((uint8_t *)golden_y8_format_image)[i] != rgb_to_y8) {
            CLOGD("[DMA2D][FORMAT] Index->%d   RGB888->Y8 compare error: 0x%x != 0x%x", i, BGRToY8(DEF_R, DEF_G, DEF_B), ((uint8_t *)golden_y8_format_image)[i]);
        }
    }

    free(golden_xrgb_format_image);
    free(golden_y8_format_image);

    CLOGD("RGB888 Transfer to Y8 with DMA2D function Success!\r\n");

}

static void DMA2D_Image_YUV422_Crop_Test(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Crop with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_yuv422,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
		.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
	};

	// Malloc Buffer
	golden_yuv422_crop_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	memset(golden_yuv422_crop_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_yuv422, golden_yuv422_crop_image, DMA2D_IMAGE_END_COL * DMA2D_IMAGE_END_ROW / 2);

	while(!dma2d_image_event);

	free(golden_yuv422_crop_image);
}

static void DMA2D_Image_RGB888_Crop_Test(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 Crop with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,

		.img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_crop_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_crop_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_crop_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_crop_image);
}

static void DMA2D_Image_BGR888_Crop_Test(void) {
	CLOGD("[DMA2D][IMAGE] BGR888 Crop with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xbgr,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,

		.img_output_fromat_transfer = csk_image_format_transfer_xbgr_crop,
		.img_rgb888_format = csk_image_bgr888_format,
	};

	golden_xbgr_crop_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xbgr_crop_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_bgr888, golden_xbgr_crop_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xbgr_crop_image);
}

static void DMA2D_Image_ARGB8888_Crop_Test(void) {
	CLOGD("[DMA2D][IMAGE] ARGB888 Crop with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,

		.img_output_fromat_transfer = csk_image_format_transfer_argb_crop,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	golden_argb_crop_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_argb_crop_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_argb8888, golden_argb_crop_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_argb_crop_image);
}

static void DMA2D_Image_ABGR8888_Crop_Test(void) {
	CLOGD("[DMA2D][IMAGE] ABGR888 Crop with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_abgr,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,

		.img_output_fromat_transfer = csk_image_fromat_transfer_abgr_crop,
		.img_rgb888_format = csk_image_bgr888_format,
	};

	// Malloc Buffer
	golden_abgr_crop_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_abgr_crop_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_abgr8888,
						golden_abgr_crop_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_abgr_crop_image);
}

static void DMA2D_Image_RGB888_Zoom_Scale_1_to_2(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 Zoom Scale 1 to 2 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.img_zoom_scale = csk_image_zoom_scale_1_to_2,

		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_zoom_scale_image);

}

static void DMA2D_Image_RGB888_Zoom_Scale_1_to_3(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 Zoom Scale 1 to 3 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.img_zoom_scale = csk_image_zoom_scale_1_to_3,

		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_zoom_scale_image);
}

static void DMA2D_Image_RGB888_Zoom_Scale_1_to_4(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 Zoom Scale 1 to 4 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.img_zoom_scale = csk_image_zoom_scale_1_to_4,

		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_zoom_scale_image);

}

static void DMA2D_Image_ARGB888_Zoom_Scale_1_to_2(void) {
	CLOGD("[DMA2D][IMAGE] ARGB8888 Zoom Scale 1 to 2 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.img_zoom_scale = csk_image_zoom_scale_1_to_2,

		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_argb_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_argb_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_argb8888, golden_argb_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_argb_zoom_scale_image);

}

static void DMA2D_Image_ARGB888_Zoom_Scale_1_to_3(void) {
	CLOGD("[DMA2D][IMAGE] ARGB8888 Zoom Scale 1 to 3 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch8,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,

		.img_zoom_scale = csk_image_zoom_scale_1_to_3,
		.img_rgb888_format = csk_image_rgb888_format,
	};
}

static void DMA2D_Image_ARGB888_Zoom_Scale_1_to_4(void) {
	CLOGD("[DMA2D][IMAGE] ARGB8888 Zoom Scale 1 to 4 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.img_zoom_scale = csk_image_zoom_scale_1_to_4,

		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_argb_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_argb_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_argb8888, golden_argb_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_argb_zoom_scale_image);
}

static void DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_2(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 the image size[%d, %d] crop to [%d, %d], then zoom scale 1 to 2 with DMA2D function,  %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, 48, 48, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_zoom_scale = csk_image_zoom_scale_1_to_2,

		.img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop_and_scale,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_crop_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_crop_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_crop_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_crop_zoom_scale_image);
}

static void DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_3(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 the image size[%d, %d] crop to [%d, %d], then zoom scale 1 to 3 with DMA2D function,  %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, 48, 48, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_zoom_scale = csk_image_zoom_scale_1_to_3,

		.img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop_and_scale,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_crop_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_crop_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_crop_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_crop_zoom_scale_image);
}

static void DMA2D_Image_RGB888_Crop_and_Zoom_Scale_1_to_4(void) {
	CLOGD("[DMA2D][IMAGE] RGB888 the image size[%d, %d] crop to [%d, %d], then zoom scale 1 to 4 with DMA2D function,  %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, 48, 48, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_xrgb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_zoom_scale = csk_image_zoom_scale_1_to_4,

		.img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop_and_scale,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_xrgb_crop_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	memset(golden_xrgb_crop_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_rgb888, golden_xrgb_crop_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	while(!dma2d_image_event);

    free(golden_xrgb_crop_zoom_scale_image);
}

static void DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_2(void) {
	CLOGD("[DMA2D][IMAGE] ARGB888 the image size[%d, %d] crop to [%d, %d], then zoom scale 1 to 2 with DMA2D function,  %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, 48, 48, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_zoom_scale = csk_image_zoom_scale_1_to_2,

		.img_output_fromat_transfer = csk_image_format_transfer_argb_crop_and_scale,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_argb_crop_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_argb_crop_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_argb8888, golden_argb_crop_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_argb_crop_zoom_scale_image);
}

static void DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_3(void) {
	CLOGD("[DMA2D][IMAGE] ARGB888 the image size[%d, %d] crop to [%d, %d], then zoom scale 1 to 3 with DMA2D function,  %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, 48, 48, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_zoom_scale = csk_image_zoom_scale_1_to_3,

		.img_output_fromat_transfer = csk_image_format_transfer_argb_crop_and_scale,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_argb_crop_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_argb_crop_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_argb8888, golden_argb_crop_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_argb_crop_zoom_scale_image);
}

static void DMA2D_Image_ARGB888_Crop_and_Zoom_Scale_1_to_4(void) {
	CLOGD("[DMA2D][IMAGE] ARGB888 the image size[%d, %d] crop to [%d, %d], then zoom scale 1 to 4 with DMA2D function,  %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, 48, 48, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma2d_init_t dma2d_para = {
		.dma_ch = dma_2d_ch9,
		.burst_len = dma2d_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2m,
		.sample_unit = dma2d_sample_unit_word,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_increase,
		.prio_lvl = prio_mode_vhigh,
		.rd_done_ack = read_done_ack_enable,
		.handshake = hs_none,
	};

	csk_dma_2d_image_cfg_t dma2d_img_cfg = {
		.img_input_format = csk_image_format_argb,
		.img_height = DMA2D_IMAGE_WIDTH_SIZE,
		.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
		.start_col = DMA2D_IMAGE_START_COL,
		.start_row = DMA2D_IMAGE_START_ROW,
		.end_col = DMA2D_IMAGE_END_COL,
		.end_row = DMA2D_IMAGE_END_ROW,
		.img_zoom_scale = csk_image_zoom_scale_1_to_4,

		.img_output_fromat_transfer = csk_image_format_transfer_argb_crop_and_scale,
		.img_rgb888_format = csk_image_rgb888_format,
	};

	// Malloc Buffer
	golden_argb_crop_zoom_scale_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	memset(golden_argb_crop_zoom_scale_image, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

    DMA2D_Initialize();

	DMA2D_Config(&dma2d_para, dma2d_image_test_callback, NULL);

	DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);

	DMA2D_Start_Normal(dma_2d_ch9, buf_argb8888, golden_argb_crop_zoom_scale_image, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);

	while(!dma2d_image_event);

    free(golden_argb_crop_zoom_scale_image);
}

static void dma2d_image_test_callback_res0(uint32_t event, void* workspace) {
    dma2d_image_event_res0 = event;
    CLOG("Channel_0 one trigger!");
}

static void dma2d_image_test_callback_res1(uint32_t event, void* workspace) {
    dma2d_image_event_res1 = event;
    CLOG("Channel_1 one trigger!");
}

static void DMA2D_Image_YUV422_Rotate_CW_90(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Rotate CW 90 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_1= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch6,
			.burst_len = dma2d_burst_len_1spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_output_fromat_transfer = csk_image_format_transfer_yuv422_cw_90,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
			.image_yuv422_rotate_mode = csk_image_yuv422_clockwise_rotate,
		}
	};

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_2= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch7,
			.burst_len = dma2d_burst_len_8spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		}
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_yuv422_rotate_cw_90 = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

    if(golden_yuv422_format_image == NULL || golden_yuv422_rotate_cw_90 == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_rotate_cw_90, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	DMA2D_Initialize();

	DMA2D_Rotate_Config(&dam2d_rotate_para_1, &dam2d_rotate_para_2, dma2d_image_test_callback_res0, dma2d_image_test_callback_res1, NULL, NULL);

	DMA2D_Image_Rotate_Config_Extend(&dam2d_rotate_para_1, &dam2d_rotate_para_2);

	DMA2D_Start_Rotate(&dam2d_rotate_para_1, &dam2d_rotate_para_2, buf_yuv422, golden_yuv422_rotate_cw_90, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	while (!(dma2d_image_event_res0 && dma2d_image_event_res1));

	free(golden_yuv422_rotate_cw_90);
}

static void DMA2D_Image_YUV422_Rotate_CCW_90(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Rotate CCW 90 with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_1= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch8,
			.burst_len = dma2d_burst_len_1spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_output_fromat_transfer = csk_image_format_transfer_yuv422_ccw_90,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
			.image_yuv422_rotate_mode = csk_image_yuv422_counterclockwise_rotate,
		}
	};

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_2= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch9,
			.burst_len = dma2d_burst_len_8spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		}
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_yuv422_rotate_ccw_90 = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

    if(golden_yuv422_format_image == NULL || golden_yuv422_rotate_ccw_90 == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_rotate_ccw_90, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	DMA2D_Initialize();

	DMA2D_Rotate_Config(&dam2d_rotate_para_1, &dam2d_rotate_para_2, dma2d_image_test_callback_res0, dma2d_image_test_callback_res1, NULL, NULL);

	DMA2D_Image_Rotate_Config_Extend(&dam2d_rotate_para_1, &dam2d_rotate_para_2);

	DMA2D_Start_Rotate(&dam2d_rotate_para_1, &dam2d_rotate_para_2, buf_yuv422, golden_yuv422_rotate_ccw_90, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	while (!(dma2d_image_event_res0 && dma2d_image_event_res1));

	free(golden_yuv422_rotate_ccw_90);
}

static void DMA2D_Image_YUV422_Rotate_CW_90_to_RGB(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Rotate CW 90 to RGB with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_1= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch6,
			.burst_len = dma2d_burst_len_1spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_output_fromat_transfer = csk_image_fromat_transfer_yuv422_cw90_to_rgb,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
			.image_yuv422_rotate_mode = csk_image_yuv422_clockwise_rotate,
		}
	};

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_2= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch7,
			.burst_len = dma2d_burst_len_8spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		}
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_yuv422_rotate_cw_90_to_rgb = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    if(golden_yuv422_format_image == NULL || golden_yuv422_rotate_cw_90_to_rgb == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_rotate_cw_90_to_rgb, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	DMA2D_Initialize();

	DMA2D_Rotate_Config(&dam2d_rotate_para_1, &dam2d_rotate_para_2, dma2d_image_test_callback_res0, dma2d_image_test_callback_res1, NULL, NULL);

	DMA2D_Image_Rotate_Config_Extend(&dam2d_rotate_para_1, &dam2d_rotate_para_2);

	DMA2D_Start_Rotate(&dam2d_rotate_para_1, &dam2d_rotate_para_2, buf_yuv422, golden_yuv422_rotate_cw_90_to_rgb, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	while (!(dma2d_image_event_res0 && dma2d_image_event_res1));

	free(golden_yuv422_rotate_cw_90_to_rgb);
}

static void DMA2D_Image_YUV422_Rotate_CCW_90_to_RGB(void) {
	CLOGD("[DMA2D][IMAGE] YUV422 Rotate CCW 90 to RGB with DMA2D function, the image size[%d, %d], %s", DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
	dma2d_image_event = 0;

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_1= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch6,
			.burst_len = dma2d_burst_len_1spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_output_fromat_transfer = csk_image_fromat_transfer_yuv422_ccw90_to_rgb,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
			.image_yuv422_rotate_mode = csk_image_yuv422_counterclockwise_rotate,
		}
	};

	csk_dma_2d_rotate_cfg_t dam2d_rotate_para_2= {
		.dma2d_init = {
			.dma_ch = dma_2d_ch7,
			.burst_len = dma2d_burst_len_8spl,
			.src_mode = address_mode_normal,
			.dst_mode = address_mode_normal,
			.tfr_mode = tfr_mode_m2m,
			.sample_unit = dma2d_sample_unit_word,
			.src_inc_mode = inc_mode_increase,
			.dst_inc_mode = inc_mode_increase,
			.prio_lvl = prio_mode_vhigh,
			.rd_done_ack = read_done_ack_enable,
			.handshake = hs_none,
		},

		.dma2d_img_cfg = {
			.img_input_format = csk_image_format_yuv422,
			.img_height = DMA2D_IMAGE_WIDTH_SIZE,
			.img_width = DMA2D_IMAGE_HEIGHT_SIZE,
			.img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
		}
	};

	// Malloc Buffer
	golden_yuv422_format_image = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);
	golden_yuv422_rotate_ccw_90_to_rgb = malloc(sizeof(int32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

    if(golden_yuv422_format_image == NULL || golden_yuv422_rotate_ccw_90_to_rgb == NULL) {
        CLOGE("Failed to allocate memory");
    }

	memset(golden_yuv422_rotate_ccw_90_to_rgb, 0, sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);

	DMA2D_Initialize();

	DMA2D_Rotate_Config(&dam2d_rotate_para_1, &dam2d_rotate_para_2, dma2d_image_test_callback_res0, dma2d_image_test_callback_res1, NULL, NULL);

	DMA2D_Image_Rotate_Config_Extend(&dam2d_rotate_para_1, &dam2d_rotate_para_2);

	DMA2D_Start_Rotate(&dam2d_rotate_para_1, &dam2d_rotate_para_2, buf_yuv422, golden_yuv422_rotate_ccw_90_to_rgb, DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 2 / 4);

	while (!(dma2d_image_event_res0 && dma2d_image_event_res1));

	free(golden_yuv422_rotate_ccw_90_to_rgb);
}

int main(void)
{
    uint32_t times;

    // Enable global interrupt
    enable_GINT();

    // Disable D-Cache
    DisableDCache();
    __RWMB();
    __FENCE_I();

    logInit(0, 115200);

    CLOGD("DMA2D VALIDATION\r\n");

    for (times = 0; times < sizeof(test_function_array) / sizeof(test_function_array[0]); times++)
    {
        test_function_array[times]();
    }

    while (1)
        ;
}
