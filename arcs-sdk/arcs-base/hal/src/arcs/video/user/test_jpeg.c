//#include "Driver_JEPG.h"
#include "Driver_GPDMA.h"
#include "log_print.h"
#include "chip.h"
#include "systick.h"
#include "ClockManager.h"
#include "PSRAMManager.h"

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_case.h"
#include "csk_driver.h"

#define TEST_JPEG_ENCODE_IN_GPDMA_CH               gp_dma_ch0
#define TEST_JPEG_ENCODE_OUT_GPDMA_CH              dma_2d_ch6
#define TEST_JPEG_DECODE_IN_GPDMA_CH               gp_dma_ch2
#define TEST_JPEG_DECODE_OUT_GPDMA_CH              gp_dma_ch3

/* encode */
extern const uint32_t jpeg_enc_htable_golden[384];
extern const uint8_t jpeg_enc_input_golden[24576];
extern const uint8_t jpeg_enc_output_golden[4002];
extern const uint32_t jpeg_enc_qtable_golden[128];
static uint8_t jpeg_enc_output_buffer[5000] = {0};

/* decode */
extern uint32_t jpeg_dec_base_table_golden[64];
extern uint32_t jpeg_dec_min_table_golden[16];
extern uint32_t jpeg_dec_symbol_table_golden[336];
extern uint32_t jpeg_dec_qtable_golden[256];
extern uint8_t jpeg_dec_input_golden[4002];
extern uint8_t jpeg_dec_output_golden[24576];
static uint8_t jpeg_dec_output_buffer[24576] = {0};

/* encoder api */
extern uint8_t enc_rgb888_128x128[49152];
extern uint8_t enc_rgb888_128x128_gray[16384];
extern uint8_t enc_yuv422_128x128[32768];

/* decoder api */
extern uint8_t dec_jpeg_128x128[3266];
extern uint8_t dec_jpeg_128x128_gray[3984];
static uint8_t codec_output_buffer[49152];

static int32_t test_jpeg_reset(void);
static int32_t test_jpeg_encode(void);
static int32_t test_jpeg_encode_psram(void);
static int32_t test_jpeg_decode(void);
static int32_t test_jpeg_decode_psram(void);
static void test_jpeg_decode_sw(void);
static int32_t test_jpeg_gpdma2d_image_to_8x8(void);
static int32_t test_jpeg_encoder_api(void);
static int32_t test_jpeg_decoder_api(void);

void test_jpeg(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //test_jpeg_gpdma2d_image_to_8x8();

    CHECK_FUNC_EXIT(test_jpeg_reset(), error);      // clk enable and reset     // pass

    //CHECK_FUNC_EXIT(test_jpeg_encoder_api(), error);

    //CHECK_FUNC_EXIT(test_jpeg_decoder_api(), error);

    while(1)
    {
        CHECK_FUNC_EXIT(test_jpeg_encode(), error);     // pass
        DELAY_MS(1000);
        CHECK_FUNC_EXIT(test_jpeg_encode_psram(), error);     // pass
        DELAY_MS(1000);
        CHECK_FUNC_EXIT(test_jpeg_decode(), error);     // pass
        DELAY_MS(1000);
        CHECK_FUNC_EXIT(test_jpeg_decode_psram(), error); 	// pass
        DELAY_MS(1000);
    }

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static int32_t test_jpeg_encode(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t i = 0;
    Jpeg_InitTypeDef jpeg_enc_cfg = {
            .mode = JPEG_MODE_ENCODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV411,
            .format_out = JPEG_DECODE_OUT_FORMAT_YUV422,
            .pixel_size = 128 * 128 * 3 / 2,
            .ecs_size = 0,
            .src_size = 128 * 128 * 3 / 2,
            .img_width = 128,
            .img_height = 128,
            .rst_enable = 1,
            .rst_num = 2,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .img_width_align = 128,
            .img_height_align = 128,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00},
    };

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);


    jpeg_init(&jpeg_enc_cfg);
    jpeg_send_huffman_table(jpeg_enc_htable_golden, sizeof(jpeg_enc_htable_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_enc_qtable_golden, sizeof(jpeg_enc_qtable_golden)/sizeof(uint32_t));
    jpeg_start();

    memset(jpeg_enc_output_buffer, 0, sizeof(jpeg_enc_output_buffer));
    jpeg_encode_gpdma_init(TEST_JPEG_ENCODE_IN_GPDMA_CH, TEST_JPEG_ENCODE_OUT_GPDMA_CH);
    jpeg_encode_gpdma_start(TEST_JPEG_ENCODE_IN_GPDMA_CH, (void*)jpeg_enc_input_golden, sizeof(jpeg_enc_input_golden)/sizeof(uint32_t), \
                           TEST_JPEG_ENCODE_OUT_GPDMA_CH, (void*)jpeg_enc_output_buffer, sizeof(jpeg_enc_output_buffer)/sizeof(uint32_t));

    /* wait done */
    timeout = 1000000; // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_gpdma_output_finish_cnt_get(), 0, timeout, error1);

    /* data check */
    ret = SUCCESS;
    for (i = 0; i < (sizeof(jpeg_enc_output_golden) - 2); i++)
    {
        if (jpeg_enc_output_buffer[i] != jpeg_enc_output_golden[i])
        {
            VIDEO_LOG("i=%d buf=0x%x golden=0x%x", i, jpeg_enc_output_buffer[i], jpeg_enc_output_golden[i]);
            ret = FAILURE;
        }
    }

error1:
    gpdma_reg_dump(TEST_JPEG_ENCODE_IN_GPDMA_CH);
    gpdma_reg_dump(TEST_JPEG_ENCODE_OUT_GPDMA_CH);
    jpeg_encode_gpdma_stop(TEST_JPEG_ENCODE_IN_GPDMA_CH, TEST_JPEG_ENCODE_OUT_GPDMA_CH);

    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    VIDEO_LOG("jpeg_enc_htable_golden: 0x%08x", jpeg_enc_htable_golden);
    VIDEO_LOG("jpeg_enc_input_golden:  0x%08x", jpeg_enc_input_golden);
    VIDEO_LOG("jpeg_enc_output_golden: 0x%08x", jpeg_enc_output_golden);
    VIDEO_LOG("jpeg_enc_qtable_golden: 0x%08x", jpeg_enc_qtable_golden);
    VIDEO_LOG("jpeg_enc_output_buffer: 0x%08x", jpeg_enc_output_buffer);
    VIDEO_LOG("size: 0x%x byte", sizeof(jpeg_enc_output_buffer));

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}

static int32_t test_jpeg_encode_psram(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t i = 0;
    uint32_t *jpeg_enc_htable_golden_p = (uint32_t*)0x28010000;
    uint32_t *jpeg_enc_qtable_golden_p = (uint32_t*)0x28020000;
    uint8_t *jpeg_enc_input_golden_p = (uint8_t*)0x28030000;
    uint8_t *jpeg_enc_output_golden_p = (uint8_t*)0x28040000;
    uint8_t *jpeg_enc_output_buffer_p = (uint8_t*)0x28050000;
    Jpeg_InitTypeDef jpeg_enc_cfg = {
            .mode = JPEG_MODE_ENCODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV411,
            .format_out = JPEG_DECODE_OUT_FORMAT_YUV422,
            .pixel_size = 128 * 128 * 3 / 2,
            .ecs_size = 0,
            .src_size = 128 * 128 * 3 / 2,
            .img_width = 128,
            .img_height = 128,
            .rst_enable = 1,
            .rst_num = 2,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .img_width_align = 128,
            .img_height_align = 128,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00},
    };

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    memcpy(jpeg_enc_htable_golden_p, jpeg_enc_htable_golden, sizeof(jpeg_enc_htable_golden));
    for (i = 0; i < sizeof(jpeg_enc_htable_golden)/sizeof(jpeg_enc_htable_golden[0]); i++)
    {
        if (jpeg_enc_htable_golden_p[i] != jpeg_enc_htable_golden[i])
        {
            VIDEO_LOG("htable:i=%d buf=0x%x golden=0x%x", i, jpeg_enc_htable_golden_p[i], jpeg_enc_htable_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_enc_qtable_golden_p, jpeg_enc_qtable_golden, sizeof(jpeg_enc_qtable_golden));
    for (i = 0; i < sizeof(jpeg_enc_qtable_golden)/sizeof(jpeg_enc_qtable_golden[0]); i++)
    {
        if (jpeg_enc_qtable_golden_p[i] != jpeg_enc_qtable_golden[i])
        {
            VIDEO_LOG("qtable:i=%d buf=0x%x golden=0x%x", i, jpeg_enc_qtable_golden_p[i], jpeg_enc_qtable_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_enc_input_golden_p, jpeg_enc_input_golden, sizeof(jpeg_enc_input_golden));
    for (i = 0; i < sizeof(jpeg_enc_input_golden); i++)
    {
        if (jpeg_enc_input_golden_p[i] != jpeg_enc_input_golden[i])
        {
            VIDEO_LOG("inputgoden:i=%d buf=0x%x golden=0x%x", i, jpeg_enc_input_golden_p[i], jpeg_enc_input_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_enc_output_golden_p, jpeg_enc_output_golden, sizeof(jpeg_enc_output_golden));
    for (i = 0; i < sizeof(jpeg_enc_output_golden); i++)
    {
        if (jpeg_enc_output_golden_p[i] != jpeg_enc_output_golden[i])
        {
            VIDEO_LOG("outputgoden:i=%d buf=0x%x golden=0x%x", i, jpeg_enc_output_golden_p[i], jpeg_enc_output_golden[i]);
            ret = FAILURE;
        }
    }

    jpeg_init(&jpeg_enc_cfg);
    jpeg_send_huffman_table(jpeg_enc_htable_golden_p, sizeof(jpeg_enc_htable_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_enc_qtable_golden_p, sizeof(jpeg_enc_qtable_golden)/sizeof(uint32_t));
    jpeg_start();

    memset(jpeg_enc_output_buffer_p, 0, sizeof(jpeg_enc_output_buffer));
    jpeg_encode_gpdma_init(TEST_JPEG_ENCODE_IN_GPDMA_CH, TEST_JPEG_ENCODE_OUT_GPDMA_CH);
    jpeg_encode_gpdma_start(TEST_JPEG_ENCODE_IN_GPDMA_CH, (void*)jpeg_enc_input_golden_p, sizeof(jpeg_enc_input_golden)/sizeof(uint32_t), \
                           TEST_JPEG_ENCODE_OUT_GPDMA_CH, (void*)jpeg_enc_output_buffer_p, sizeof(jpeg_enc_output_buffer)/sizeof(uint32_t));

    /* wait done */
    timeout = 1000000; // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_gpdma_output_finish_cnt_get(), 0, timeout, error1);

    /* data check */
    ret = SUCCESS;
    for (i = 0; i < (sizeof(jpeg_enc_output_golden) - 2); i++)
    {
        if (jpeg_enc_output_buffer_p[i] != jpeg_enc_output_golden_p[i])
        {
            VIDEO_LOG("i=%d buf=0x%x golden=0x%x", i, jpeg_enc_output_buffer_p[i], jpeg_enc_output_golden_p[i]);
            ret = FAILURE;
        }
    }


error1:
    gpdma_reg_dump(TEST_JPEG_ENCODE_IN_GPDMA_CH);
    gpdma_reg_dump(TEST_JPEG_ENCODE_OUT_GPDMA_CH);
    jpeg_encode_gpdma_stop(TEST_JPEG_ENCODE_IN_GPDMA_CH, TEST_JPEG_ENCODE_OUT_GPDMA_CH);

    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    VIDEO_LOG("jpeg_enc_htable_golden: 0x%08x, 0x%08x", jpeg_enc_htable_golden, jpeg_enc_htable_golden_p);
    VIDEO_LOG("jpeg_enc_input_golden:  0x%08x, 0x%08x", jpeg_enc_input_golden, jpeg_enc_input_golden_p);
    VIDEO_LOG("jpeg_enc_output_golden: 0x%08x, 0x%08x", jpeg_enc_output_golden, jpeg_enc_output_golden_p);
    VIDEO_LOG("jpeg_enc_qtable_golden: 0x%08x, 0x%08x", jpeg_enc_qtable_golden, jpeg_enc_qtable_golden_p);
    VIDEO_LOG("jpeg_enc_output_buffer: 0x%08x, 0x%08x", jpeg_enc_output_buffer, jpeg_enc_output_buffer_p);
    VIDEO_LOG("size: 0x%x byte", sizeof(jpeg_enc_output_buffer));

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_jpeg_decode(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t i = 0;

    Jpeg_InitTypeDef jpeg_dec_cfg = {
            .mode = JPEG_MODE_DECODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV411,
            .format_out = JPEG_DECODE_OUT_FORMAT_YUV422,
            .pixel_size = 128 * 128 * 3 / 2,
            .ecs_size = 4002,
            .src_size = 4002,
            .img_width = 128,
            .img_height = 128,
            .rst_enable = 1,
            .rst_num = 2,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .img_width_align = 128,
            .img_height_align = 128,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00},
    };

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    jpeg_init(&jpeg_dec_cfg);
    jpeg_send_base_table(jpeg_dec_base_table_golden, sizeof(jpeg_dec_base_table_golden)/sizeof(uint32_t));
    jpeg_send_min_table(jpeg_dec_min_table_golden, sizeof(jpeg_dec_min_table_golden)/sizeof(uint32_t));
    jpeg_send_symbol_table(jpeg_dec_symbol_table_golden, sizeof(jpeg_dec_symbol_table_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_dec_qtable_golden, sizeof(jpeg_dec_qtable_golden)/sizeof(uint32_t) / 2);
    jpeg_start();

    memset(jpeg_dec_output_buffer, 0, sizeof(jpeg_dec_output_buffer));
    jpeg_decode_gpdma_init(TEST_JPEG_DECODE_IN_GPDMA_CH, TEST_JPEG_DECODE_OUT_GPDMA_CH);
    jpeg_decode_gpdma_start(TEST_JPEG_DECODE_IN_GPDMA_CH, (void*)jpeg_dec_input_golden, sizeof(jpeg_dec_input_golden)/sizeof(uint32_t) + 1, \
            TEST_JPEG_DECODE_OUT_GPDMA_CH, (void*)jpeg_dec_output_buffer, sizeof(jpeg_dec_output_buffer)/sizeof(uint32_t));

    /* wait done */
    timeout = 1000000;  // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_gpdma_output_finish_cnt_get(), 0, timeout, error0);

    /* data check */
    ret = SUCCESS;
    for (i = 0; i < (sizeof(jpeg_dec_output_golden)); i++)
    {
        if (jpeg_dec_output_buffer[i] != jpeg_dec_output_golden[i])
        {
            VIDEO_LOG("i=%d buf=0x%x golden=0x%x", i, jpeg_dec_output_buffer[i], jpeg_dec_output_golden[i]);
            ret = FAILURE;
        }
    }

error0:
    gpdma_reg_dump(TEST_JPEG_DECODE_IN_GPDMA_CH);
    gpdma_reg_dump(TEST_JPEG_DECODE_OUT_GPDMA_CH);
    jpeg_decode_gpdma_stop(TEST_JPEG_DECODE_IN_GPDMA_CH, TEST_JPEG_DECODE_OUT_GPDMA_CH);

    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    VIDEO_LOG("jpeg_dec_base_table_golden:   0x%08x", jpeg_dec_base_table_golden);
    VIDEO_LOG("jpeg_dec_min_table_golden:    0x%08x", jpeg_dec_min_table_golden);
    VIDEO_LOG("jpeg_dec_symbol_table_golden: 0x%08x", jpeg_dec_symbol_table_golden);
    VIDEO_LOG("jpeg_dec_qtable_golden:       0x%08x", jpeg_dec_qtable_golden);
    VIDEO_LOG("jpeg_dec_input_golden:        0x%08x", jpeg_dec_input_golden);
    VIDEO_LOG("jpeg_dec_output_golden:       0x%08x", jpeg_dec_output_golden);
    VIDEO_LOG("jpeg_dec_output_buffer:       0x%08x", jpeg_dec_output_buffer);
    VIDEO_LOG("size: 0x%x byte", sizeof(jpeg_dec_output_buffer));

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}

static int32_t test_jpeg_decode_psram(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t i = 0;
    uint32_t *jpeg_dec_base_table_golden_p = (uint32_t*)0x28010000;
    uint32_t *jpeg_dec_min_table_golden_p = (uint32_t*)0x28020000;
    uint32_t *jpeg_dec_symbol_table_golden_p = (uint32_t*)0x28030000;
    uint32_t *jpeg_dec_qtable_golden_p = (uint32_t*)0x28040000;
    uint8_t *jpeg_dec_input_golden_p = (uint8_t*)0x28050000;
    uint8_t *jpeg_dec_output_golden_p = (uint8_t*)0x28060000;
    uint8_t *jpeg_dec_output_buffer_p = (uint8_t*)0x28070000;

    Jpeg_InitTypeDef jpeg_dec_cfg = {
            .mode = JPEG_MODE_DECODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV411,
            .format_out = JPEG_DECODE_OUT_FORMAT_YUV422,
            .pixel_size = 128 * 128 * 3 / 2,
            .ecs_size = 4002,
            .src_size = 4002,
            .img_width = 128,
            .img_height = 128,
            .rst_enable = 1,
            .rst_num = 2,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .img_width_align = 128,
            .img_height_align = 128,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00},
    };

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    memcpy(jpeg_dec_base_table_golden_p, jpeg_dec_base_table_golden, sizeof(jpeg_dec_base_table_golden));
    for (i = 0; i < sizeof(jpeg_dec_base_table_golden)/sizeof(jpeg_dec_base_table_golden[0]); i++)
    {
        if (jpeg_dec_base_table_golden_p[i] != jpeg_dec_base_table_golden[i])
        {
            VIDEO_LOG("base_table:i=%d buf=0x%x golden=0x%x", i, jpeg_dec_base_table_golden_p[i], jpeg_dec_base_table_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_dec_min_table_golden_p, jpeg_dec_min_table_golden, sizeof(jpeg_dec_min_table_golden));
    for (i = 0; i < sizeof(jpeg_dec_min_table_golden)/sizeof(jpeg_dec_min_table_golden[0]); i++)
    {
        if (jpeg_dec_min_table_golden_p[i] != jpeg_dec_min_table_golden[i])
        {
            VIDEO_LOG("min_table:i=%d buf=0x%x golden=0x%x", i, jpeg_dec_min_table_golden_p[i], jpeg_dec_min_table_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_dec_symbol_table_golden_p, jpeg_dec_symbol_table_golden, sizeof(jpeg_dec_symbol_table_golden));
    for (i = 0; i < sizeof(jpeg_dec_symbol_table_golden)/sizeof(jpeg_dec_symbol_table_golden[0]); i++)
    {
        if (jpeg_dec_symbol_table_golden_p[i] != jpeg_dec_symbol_table_golden[i])
        {
            VIDEO_LOG("symbol_table:i=%d buf=0x%x golden=0x%x", i, jpeg_dec_symbol_table_golden_p[i], jpeg_dec_symbol_table_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_dec_qtable_golden_p, jpeg_dec_qtable_golden, sizeof(jpeg_dec_qtable_golden));
    for (i = 0; i < sizeof(jpeg_dec_qtable_golden)/sizeof(jpeg_dec_qtable_golden[0]); i++)
    {
        if (jpeg_dec_qtable_golden_p[i] != jpeg_dec_qtable_golden[i])
        {
            VIDEO_LOG("qtable:i=%d buf=0x%x golden=0x%x", i, jpeg_dec_qtable_golden_p[i], jpeg_dec_qtable_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_dec_input_golden_p, jpeg_dec_input_golden, sizeof(jpeg_dec_input_golden));
    for (i = 0; i < sizeof(jpeg_dec_input_golden); i++)
    {
        if (jpeg_dec_input_golden_p[i] != jpeg_dec_input_golden[i])
        {
            VIDEO_LOG("inputgolden:i=%d buf=0x%x golden=0x%x", i, jpeg_dec_input_golden_p[i], jpeg_dec_input_golden[i]);
            ret = FAILURE;
        }
    }

    memcpy(jpeg_dec_output_golden_p, jpeg_dec_output_golden, sizeof(jpeg_dec_output_golden));
    for (i = 0; i < sizeof(jpeg_dec_output_golden); i++)
    {
        if (jpeg_dec_output_golden_p[i] != jpeg_dec_output_golden[i])
        {
            VIDEO_LOG("outputgolden:i=%d buf=0x%x golden=0x%x", i, jpeg_dec_input_golden_p[i], jpeg_dec_output_golden[i]);
            ret = FAILURE;
        }
    }

    jpeg_init(&jpeg_dec_cfg);
    jpeg_send_base_table(jpeg_dec_base_table_golden_p, sizeof(jpeg_dec_base_table_golden)/sizeof(uint32_t));
    jpeg_send_min_table(jpeg_dec_min_table_golden_p, sizeof(jpeg_dec_min_table_golden)/sizeof(uint32_t));
    jpeg_send_symbol_table(jpeg_dec_symbol_table_golden_p, sizeof(jpeg_dec_symbol_table_golden)/sizeof(uint32_t));
    jpeg_send_quantization_table(jpeg_dec_qtable_golden_p, sizeof(jpeg_dec_qtable_golden)/sizeof(uint32_t) / 2);
    jpeg_start();

    memset(jpeg_dec_output_buffer_p, 0, sizeof(jpeg_dec_output_buffer));
    jpeg_decode_gpdma_init(TEST_JPEG_DECODE_IN_GPDMA_CH, TEST_JPEG_DECODE_OUT_GPDMA_CH);
    jpeg_decode_gpdma_start(TEST_JPEG_DECODE_IN_GPDMA_CH, (void*)jpeg_dec_input_golden_p, sizeof(jpeg_dec_input_golden)/sizeof(uint32_t) + 1, \
            TEST_JPEG_DECODE_OUT_GPDMA_CH, (void*)jpeg_dec_output_buffer_p, sizeof(jpeg_dec_output_buffer)/sizeof(uint32_t));

    /* wait done */
    timeout = 1000000;  // us
    CHECK_EQ_TIMEOUT_EXIT(jpeg_gpdma_output_finish_cnt_get(), 0, timeout, error0);

    /* data check */
    ret = SUCCESS;
    for (i = 0; i < (sizeof(jpeg_dec_output_golden)); i++)
    {
        if (jpeg_dec_output_buffer_p[i] != jpeg_dec_output_golden_p[i])
        {
            VIDEO_LOG("i=%d buf=0x%x golden=0x%x", i, jpeg_dec_output_buffer_p[i], jpeg_dec_output_golden_p[i]);
            ret = FAILURE;
        }
    }

error0:
    gpdma_reg_dump(TEST_JPEG_DECODE_IN_GPDMA_CH);
    gpdma_reg_dump(TEST_JPEG_DECODE_OUT_GPDMA_CH);
    jpeg_decode_gpdma_stop(TEST_JPEG_DECODE_IN_GPDMA_CH, TEST_JPEG_DECODE_OUT_GPDMA_CH);

    jpeg_reg_dump();
    jpeg_stop();
    jpeg_deinit();

    VIDEO_LOG("jpeg_dec_base_table_golden:   0x%08x 0x%08x", jpeg_dec_base_table_golden, jpeg_dec_base_table_golden_p);
    VIDEO_LOG("jpeg_dec_min_table_golden:    0x%08x 0x%08x", jpeg_dec_min_table_golden, jpeg_dec_min_table_golden_p);
    VIDEO_LOG("jpeg_dec_symbol_table_golden: 0x%08x 0x%08x", jpeg_dec_symbol_table_golden, jpeg_dec_symbol_table_golden_p);
    VIDEO_LOG("jpeg_dec_qtable_golden:       0x%08x 0x%08x", jpeg_dec_qtable_golden, jpeg_dec_qtable_golden_p);
    VIDEO_LOG("jpeg_dec_input_golden:        0x%08x 0x%08x", jpeg_dec_input_golden, jpeg_dec_input_golden_p);
    VIDEO_LOG("jpeg_dec_output_golden:       0x%08x 0x%08x", jpeg_dec_output_golden, jpeg_dec_output_golden_p);
    VIDEO_LOG("jpeg_dec_output_buffer:       0x%08x 0x%08x", jpeg_dec_output_buffer, jpeg_dec_output_buffer_p);
    VIDEO_LOG("size: 0x%x byte", sizeof(jpeg_dec_output_buffer));

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}

/* test jpeg : clk enable and reset */
static int32_t test_jpeg_reset(void)
{
    int32_t ret = FAILURE;

    Jpeg_InitTypeDef jpeg_enc_cfg = {
            .mode = JPEG_MODE_ENCODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV411,
            .format_out = JPEG_DECODE_OUT_FORMAT_YUV422,
            .pixel_size = 128 * 128 * 3 / 2,
            .ecs_size = 0,
            .src_size = 128 * 128 * 3 / 2,
            .img_width = 128,
            .img_height = 128,
            .rst_enable = 1,
            .rst_num = 2,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .img_width_align = 128,
            .img_height_align = 128,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00},
    };

    Jpeg_InitTypeDef jpeg_dec_cfg = {
            .mode = JPEG_MODE_DECODE,
            .format_in = JPEG_DECODE_IN_FORMAT_YUV411,
            .format_out = JPEG_DECODE_OUT_FORMAT_YUV422,
            .pixel_size = 128 * 128 * 3 / 2,
            .ecs_size = 4002,
            .src_size = 4002,
            .img_width = 128,
            .img_height = 128,
            .rst_enable = 1,
            .rst_num = 2,
            .sampling_h = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .sampling_v = 2*CSK_JPEG_BLOCK_BASIC_PIXEL_NUM,
            .img_width_align = 128,
            .img_height_align = 128,
            .qt_index = {0x00,0x01,0x01,0x00},
            .ht_index = {0x00,0x11,0x11,0x00},
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    jpeg_reset();
    jpeg_init(&jpeg_enc_cfg);
    jpeg_start();
    jpeg_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_JPEG->REG_CONTRL.all,                  0x00000006, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_PIXEL_DMA_START.all,         0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ECS_DMA_START.all,           0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_PIXEL_DMA_TRANSFER_SIZE.all, 0x00006000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ECS_DMA_TRANSFER_SIZE.all,   0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_SOURCE_DATA_LENGTH.all,      0x00006000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_RESULT_DATA_LENGTH.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_DEC_DUMMY.all,               0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_INT_ST_CLR.all,              0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_JPEG->REG_INT_MASK.all,                0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_SCALING_CTRL.all,            0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ENC_PDMA_START.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ENC_PIC_SIZE.all,            0x00803080, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_DEC_PIC_SIZE.all,            0x00803080, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_RELOAD.all,                  0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_IFCTRL.all,                  0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR0.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR1.all,                    0x00000006, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR2.all,                    0x0000003f, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR3.all,                    0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR4.all,                    0x00000030, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR5.all,                    0x00000007, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR6.all,                    0x00000007, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR7.all,                    0x00000000, error);

    jpeg_reset();
    jpeg_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_JPEG->REG_CONTRL.all,                  0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_PIXEL_DMA_START.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ECS_DMA_START.all,           0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_PIXEL_DMA_TRANSFER_SIZE.all, 0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ECS_DMA_TRANSFER_SIZE.all,   0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_SOURCE_DATA_LENGTH.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_RESULT_DATA_LENGTH.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_DEC_DUMMY.all,               0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_INT_ST_CLR.all,              0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_INT_MASK.all,                0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_SCALING_CTRL.all,            0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ENC_PDMA_START.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ENC_PIC_SIZE.all,            0x00003000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_DEC_PIC_SIZE.all,            0x00003000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_RELOAD.all,                  0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_IFCTRL.all,                  0x00000002, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR0.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR1.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR2.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR3.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR4.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR5.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR6.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR7.all,                    0x00000000, error);

    jpeg_reset();
    jpeg_init(&jpeg_dec_cfg);
    jpeg_start();
    jpeg_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_JPEG->REG_CONTRL.all,                  0x00000007, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_PIXEL_DMA_START.all,         0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ECS_DMA_START.all,           0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_PIXEL_DMA_TRANSFER_SIZE.all, 0x00006000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ECS_DMA_TRANSFER_SIZE.all,   0x00000fa2, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_SOURCE_DATA_LENGTH.all,      0x00000fa2, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_RESULT_DATA_LENGTH.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_DEC_DUMMY.all,               0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_INT_ST_CLR.all,              0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_JPEG->REG_INT_MASK.all,                0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_SCALING_CTRL.all,            0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ENC_PDMA_START.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_ENC_PIC_SIZE.all,            0x00803080, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_DEC_PIC_SIZE.all,            0x00803080, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_RELOAD.all,                  0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_IFCTRL.all,                  0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR0.all,                    0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR1.all,                    0x0000000e, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR2.all,                    0x0000003f, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR3.all,                    0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR4.all,                    0x00000030, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR5.all,                    0x00000007, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR6.all,                    0x00000007, error);
    CHECK_RET_EQ_EXIT(IP_JPEG->REG_JCR7.all,                    0x00000000, error);

    ret = SUCCESS;

error:
    jpeg_reset();

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static volatile uint32_t test_jpeg_input_gpdma_finish_cnt = 0;
static volatile uint32_t test_jpeg_output_gpdma_finish_cnt = 0;

static void jpeg_input_gpdma_callback(uint32_t event, void* workspace)
{
    VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_jpeg_input_gpdma_finish_cnt++;
}

static void jpeg_output_gpdma_callback(uint32_t event, void* workspace)
{
    VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_jpeg_output_gpdma_finish_cnt++;
}

#define TEST_JPEG_IMAGE_WIDTH   64
#define TEST_JPEG_IMAGE_HEIGHT  64

static int32_t test_jpeg_gpdma2d_image_to_8x8(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t i = 0;
    uint8_t *image_buf = NULL;
    uint8_t *image_buf_tmp0 = NULL;
    uint8_t *image_buf_tmp1 = NULL;
    uint8_t *image_8x8_buf = NULL;
    uint32_t image_size_byte = 0;
    uint16_t image_width = TEST_JPEG_IMAGE_WIDTH;
    uint16_t image_height = TEST_JPEG_IMAGE_HEIGHT;

    csk_gpdma_init_t gpdma_cfg = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_8spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = hs_none,
    };

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc YUV422 */
    image_size_byte = image_width * image_height * 2;
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());
    image_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_buf);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);

    image_8x8_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_8x8_buf);
    VIDEO_LOG("image_8x8_buf=0x%08x size=0x%x byte", image_8x8_buf, image_size_byte);

    image_buf_tmp0 = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_buf_tmp0);
    VIDEO_LOG("image_buf_tmp0=0x%08x size=0x%x byte", image_buf_tmp0, image_size_byte);

    image_buf_tmp1 = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL(image_buf_tmp1);
    VIDEO_LOG("image_buf_tmp1=0x%08x size=0x%x byte", image_buf_tmp1, image_size_byte);

    rgb565_colorbar_create((uint16_t *)image_buf, image_width, image_height, image_height/5);
    rgb565_grid_create((uint16_t *)image_buf, image_width, image_height, image_height);
    rgb565_to_yuv422yuyv((uint16_t *)image_buf, image_buf, image_width * image_height);

#if 0
    GPDMA_Initialize();
    GPDMA_Config(&gpdma_cfg, jpeg_input_gpdma_callback, NULL);

    test_jpeg_input_gpdma_finish_cnt = 0;
    GPDMA_Start_Normal(gpdma_cfg.dma_ch, (uint32_t*)image_buf, (uint32_t*)image_8x8_buf, image_size_byte / sizeof(uint32_t));

    /* wait done */
    timeout = 1000000;
    CHECK_EQ_TIMEOUT_EXIT(test_jpeg_input_gpdma_finish_cnt, 0, timeout, error0);
#else
    /* GPDMA-2D config CH6: yuv pack -> yuv planar */
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_CH_HS_SEL_CH6 = dvp_hs_num5; // handsharking
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_FLOW_CTRL_CH6 = 0;           // 0:dma  1:pre
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_TFR_MODE_CH6 = 2;            // 0:p2m  1:m2p  2:mem
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_SRC_INC_CH6 = 0;             // src address  0:add read  1:fix read
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_DST_INC_CH6 = 0;             // dst address  0:add read  1:fix read
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_SRC_BURST_LEN_CH6 = 1;       // src  0:1words  1:2words  2:4words  3:8words
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_DST_BURST_LEN_CH6 = 1;       // dst  0:1words  1:2words  2:4words  3:8words
    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_TRANS_SRC_BASE_UNIT_CH6 = 2;
    IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH6 = 2;

    IP_GPDMA->REG_DMA_SRC_ADDR0_CH6.all = (uint32_t)image_buf;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH6.all = (uint32_t)image_buf_tmp0;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH6.all = (uint32_t)image_buf_tmp1;
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH6.all = image_size_byte / sizeof(uint32_t);
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all = image_size_byte / sizeof(uint32_t);
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.bit.CFG_IMAGE_WIDTH_IN_CH6 = image_width;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.bit.CFG_IMAGE_HEIGHT_IN_CH6 = image_height;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.bit.CFG_IMAGE_WIDTH_OUT_CH6 = image_width;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.bit.CFG_IMAGE_HEIGHT_OUT_CH6 = image_height;

    IP_GPDMA->REG_DMA_IMAGE_FORMAT.bit.CFG_YUV_INPUT_FORMAT_CH6 = 0;     // 0:yuv422
    IP_GPDMA->REG_DMA_IMAGE_FORMAT.bit.CFG_YUV422_FORMAT_CH6 = 0;        // 0:YUYV  1:UYVY  2:YVYU  3:VYUY

    IP_GPDMA->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = (1 << 0);
    IP_GPDMA->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = (1 << 0);
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_BYPASS_CH6 = 1;

    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_WNUM_CH6 = 1;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_HNUM_CH6 = 1;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6 = 3;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = 2;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM1_CH6 = 1;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM2_CH6 = 1;

    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_CH_EN_CH6 = 1;

    /* GPDMA-2D config CH7: yuv planar -> 8x8 block (8x8 Y, 8x8 Y, 8x8 U, 8x8 V) */
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_CH_HS_SEL_CH7 = jpg_p_hs_num7; // handsharking
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_FLOW_CTRL_CH7 = 0;           // 0:dma  1:pre
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_TFR_MODE_CH7 = 2;            // 0:p2m  1:m2p  2:mem
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_SRC_INC_CH7 = 0;             // src address  0:add read  1:fix read
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_DST_INC_CH7 = 0;             // dst address  0:add read  1:fix read
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_SRC_BURST_LEN_CH7 = 3;       // src  0:1words  1:2words  2:4words  3:8words
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_DST_BURST_LEN_CH7 = 3;       // dst  0:1words  1:2words  2:4words  3:8words
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_TRANS_SRC_BASE_UNIT_CH7 = 2;
    IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH7 = 2;

    IP_GPDMA->REG_DMA_SRC_ADDR0_CH7.all = (uint32_t)image_buf_tmp0;
    IP_GPDMA->REG_DMA_SRC_ADDR1_CH7.all = (uint32_t)image_buf_tmp1;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH7.all = (uint32_t)image_8x8_buf;
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH7.all = image_width * 8 * 2 / sizeof(uint32_t);
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.all = image_width * 8 * 2 / sizeof(uint32_t);
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.bit.CFG_IMAGE_WIDTH_IN_CH7 = image_width;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.bit.CFG_IMAGE_HEIGHT_IN_CH7 = image_height;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_WIDTH_OUT_CH7 = image_width;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_HEIGHT_OUT_CH7 = image_height;

    IP_GPDMA->REG_DMA_IMAGE_FORMAT.bit.CFG_YUV_INPUT_FORMAT_CH7 = 0;     // 0:yuv422
    IP_GPDMA->REG_DMA_IMAGE_FORMAT.bit.CFG_YUV422_FORMAT_CH7 = 0;        // 0:YUYV  1:UYVY  2:YVYU  3:VYUY

    IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = (1 << 1);
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_BYPASS_CH7 = 1;

    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7 = 2;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7 = 8;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7 = 3;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = 2;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM1_CH7 = 1;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM2_CH7 = 1;

    IP_GPDMA->REG_DMA_CH_TRIGGER_CTRL.bit.CFG_CH_TRIGGERED_EN = (1 << 1);
    IP_GPDMA->REG_DMA_CH7_CTRL.bit.CFG_CH_EN_CH7 = 1;

    IP_GPDMA->REG_DMA_CH6_CTRL.bit.CFG_CH_START_CH6 = 1;
#endif

    DELAY_MS(1000);
    for(i = 0; i < 64; i++)
    {
        VIDEO_LOG("image_buf[%d]=0x%02x ", i, image_buf[i]);
    }
    for(i = 0; i < 64; i++)
    {
        VIDEO_LOG("image_buf_tmp0[%d]=0x%02x ", i, image_buf_tmp0[i]);
    }
    for(i = 0; i < 64; i++)
    {
        VIDEO_LOG("image_buf_tmp1[%d]=0x%02x ", i, image_buf_tmp1[i]);
    }
    for(i = 0; i < 64; i++)
    {
        VIDEO_LOG("image_8x8_buf[%d]=0x%02x ", i, image_8x8_buf[i]);
    }

    ret = SUCCESS;

error0:
    //gpdma_reg_dump(gpdma_cfg.dma_ch);
    gpdma_reg_dump(6);
    gpdma_reg_dump(7);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
    VIDEO_LOG("image_buf_tmp0=0x%08x size=0x%x byte", image_buf_tmp0, image_size_byte);
    VIDEO_LOG("image_buf_tmp1=0x%08x size=0x%x byte", image_buf_tmp1, image_size_byte);
    VIDEO_LOG("image_8x8_buf=0x%08x size=0x%x byte", image_8x8_buf, image_size_byte);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


#if 1
#include "jpeg_dec.h"
#include "img_converters.h"

extern char dec_jpeg_96x96_golden_tab[];
extern char dec_bgr565_96x96_golden_tab[];

static void test_jpeg_decode_sw(void)       // test pass
{
    uint32_t i = 0;
    char *jpeg = dec_jpeg_96x96_golden_tab;
    uint32_t jpeg_size = 5332;   // sizeof(dec_jpeg_96x96_golden_tab)
    uint16_t rgb565[96 * 96] = {0};
    char *bgr565;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    if(0 != jpeg_to_rgb565(jpeg, jpeg_size, rgb565))
    {
        VIDEO_LOG("[%s:%d] jpeg decode error", __func__, __LINE__);
        return;
    }

    rgb565_to_bgr565(rgb565, sizeof(rgb565) / sizeof(uint16_t));
    bgr565 = (char *)rgb565;

    for (i = 0; i < sizeof(rgb565); i++)
    {
        if (bgr565[i] != dec_bgr565_96x96_golden_tab[i])
        {
            VIDEO_LOG("i=%d bgr565=0x%x golden=0x%x", i, bgr565[i], dec_bgr565_96x96_golden_tab[i]);
        }
    }

    VIDEO_LOG("jpeg: 0x%08x", jpeg);
    VIDEO_LOG("jpeg_size: 0x%x byte", jpeg_size);
    VIDEO_LOG("bgr565: 0x%08x", bgr565);
    VIDEO_LOG("bgr565_size: 0x%x byte", sizeof(rgb565));

    /* J-Link cmmmander : savebin D:\src\picture\dec_rgb565_96x96_out.bin 0x2003b7d8 0x4800 */
}
#endif

/***************** head check ****************************************************************/

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

/*****************Macro********************/
#define jpeg_malloc(size)                   malloc(size)
#define jpeg_free(addr)                     free(addr)


/*****************Mark********************/
#define JEPG_START_MARK_VALUE                    0xFF

/*****************Type********************/
#define JEPG_TYPE_START_OF_IMAGE                 0xD8                 // SOI (Start of Image)
#define JEPG_TYPE_APPLICATION_SPECIFIC           0xE0                 // APPn (Application Specific)
#define JEPG_TYPE_DEFINE_QUANTIZATION_TABLE      0xDB                 // DQT (Define Quantization Table)
#define JEPG_TYPE_START_OF_FRAME                 0xC0                 // SOF0 (Start of Frame, Baseline DCT)
#define JEPG_TYPE_START_OF_SCAN                  0xDA                 // SOS (Start of Scan)
#define JEPG_TYPE_DEFINE_HUFFMAN_TABLE           0xC4                 // DHT (Define Huffman Table)
#define JEPG_TYPE_END_OF_IMAGE                   0xD9                 // EOI (End of Image)
#define JEPG_TYPE_COMMENT                        0xFE                 // COM (Comment)

//********************************Segment Start
/// The base segment structure
typedef struct _segment_data {
    uint8_t mark;
    uint8_t type;
    uint16_t len;
    uint8_t data[0];
} segment_data_t;

/// Application specific segment structure
typedef struct _segment_app0 {
    uint8_t exif[5];
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t unit;
    uint16_t x_density;
    uint16_t y_density;
    uint8_t x_thumbnail;
    uint8_t y_thumbnail;
    uint8_t thumbnail_data[0];
} segment_app0_t;

/// Comment segment structure
typedef struct _segment_com {
    uint8_t com[0];
} segment_com_t;

/// Quantization segment structure
typedef struct _segment_dqt {
    uint8_t qt_info;
    uint8_t qt[0];
} segment_dqt_t;

/// Start of frame segment structure
typedef struct _segment_sof0_component {
    uint8_t component_id;
    uint8_t sample_coe;
    uint8_t qt_id;
} segment_sof0_component_t;

#pragma pack(push, 1)
typedef struct _segment_sof0 {
    uint8_t sample;
    uint16_t height;
    uint16_t width;
    uint8_t component_count;
    uint8_t component[0];
} segment_sof0_t;
#pragma pack(push, 1)

/// Huffman table segment structure
typedef struct _segment_dht {
    uint8_t info;
    uint8_t ht_size[16];
    uint8_t ht_value[0];
} segment_dht_t;

/// Start of scan segment structure
typedef struct _segment_sos_component {
    uint8_t component_id;
    uint8_t dht_id;
} segment_sos_component_t;

typedef struct _segment_sos {
    uint8_t component_count;
    uint8_t component[0];
} segment_sos_t;
//********************************Segment End

typedef struct _f_handler_type {
    uint8_t* address;
    uint32_t len;
    uint32_t cur_len;
} f_handler_type_t;

static f_handler_type_t f_handler = {0};

/*
static f_handler_type_t* f_open(f_handler_type_t* handler, uint8_t* address, uint32_t len){
    handler->address = address;
    handler->len = len;
    handler->cur_len = 0;
}
*/
static uint8_t f_getc(f_handler_type_t* handler){
    if (handler->cur_len >= handler->len) {
        // Error EOF
        assert(0);
    }

    uint8_t* p_addr = handler->address + handler->cur_len;
    handler->cur_len++;

    return *p_addr;
}

static uint8_t* f_address(f_handler_type_t* handler){
    uint8_t* p_addr = handler->address + handler->cur_len;
    return p_addr;
}

static uint32_t f_tell(f_handler_type_t* handler){
    return handler->cur_len;
}

static void f_seek(f_handler_type_t* handler, uint32_t len){
    handler->cur_len = len;
    if (handler->cur_len >= handler->len) {
        // Error EOF
        assert(0);
    }
}

#define JEPG_SEGMENT_LEN_VALUE(byte0, byte1)                  (((byte0 << 8) | byte1) - 2) // Except length segment self
#define JEPG_ENDIAN_REVERSE_UINT16(value)                     (uint16_t)((((uint16_t)value & 0x00FF) << 8) | (((uint16_t)value & 0xFF00) >> 8))

static uint8_t jpeg_qt[4][64] = {0};
static uint8_t jpeg_dht[4][272] = {0};

static int32_t jpeg_decode_segment_abstract(f_handler_type_t* handler){
    uint8_t temp_char = 0;
    int32_t ret = 0;
    uint16_t len = 0;

    while(1){
        temp_char = f_getc(handler);
        if (temp_char != JEPG_START_MARK_VALUE){
            continue;
        }

        // segment flag
        temp_char = f_getc(handler);

        switch (temp_char){
        case JEPG_TYPE_END_OF_IMAGE:
            VIDEO_LOG("[JEPG] End of image");
            break;
        case JEPG_TYPE_START_OF_IMAGE:
            VIDEO_LOG("[JEPG] Start of image");
            break;
        case JEPG_TYPE_APPLICATION_SPECIFIC:
        {
            uint8_t byte0 = f_getc(handler);
            uint8_t byte1 = f_getc(handler);
            len = JEPG_SEGMENT_LEN_VALUE(byte0, byte1);
            VIDEO_LOG("[JEPG] Locate <APP> table, length: %d", len);

            segment_app0_t* app = (segment_app0_t*)f_address(handler);

            VIDEO_LOG("[APP]EXIF: %s", app->exif);
            VIDEO_LOG("[APP]MAJOR: %d", app->major_version);
            VIDEO_LOG("[APP]MINOR: %d", app->minor_version);
            VIDEO_LOG("[APP]X Density: %d", app->x_density);
            VIDEO_LOG("[APP]Y Density: %d", app->y_density);

            // Jump app0 segment
            f_seek(handler, f_tell(handler) + len);
        }
            break;
        case JEPG_TYPE_DEFINE_QUANTIZATION_TABLE:
        {
            uint8_t byte0 = f_getc(handler);
            uint8_t byte1 = f_getc(handler);
            len = JEPG_SEGMENT_LEN_VALUE(byte0, byte1);

            VIDEO_LOG("[JEPG] Locate <QT> table, length: %d", len);

            uint8_t num = len / (65);
            for(; num > 0; num--){
                segment_dqt_t* qt = (segment_dqt_t*)f_address(handler);

                memcpy(jpeg_qt[(qt->qt_info) & 0xf], (uint8_t*)qt->qt, 64);
            }

            // Jump qt segment
            f_seek(handler, f_tell(handler) + len);
        }
            break;
        case JEPG_TYPE_START_OF_FRAME:
        {
            uint8_t byte0 = f_getc(handler);
            uint8_t byte1 = f_getc(handler);
            len = JEPG_SEGMENT_LEN_VALUE(byte0, byte1);

            VIDEO_LOG("[JEPG] Locate <SOF> table, length: %d", len);

            segment_sof0_t* sof = (segment_sof0_t*)f_address(handler);

            VIDEO_LOG("[SOF]SAMPLE: %d", sof->sample);
            VIDEO_LOG("[SOF]WIDTH: %d", JEPG_ENDIAN_REVERSE_UINT16(sof->width));
            VIDEO_LOG("[SOF]HEIGHT: %d", JEPG_ENDIAN_REVERSE_UINT16(sof->height));
            VIDEO_LOG("[SOF]CNT: %d", sof->component_count);
            uint8_t i = 0;

            f_seek(handler, f_tell(handler) + sizeof(segment_sof0_t));

            // TODO Need implement later
            for (i = 0; i < sof->component_count; i++){
                segment_sof0_component_t* component = (segment_sof0_component_t*)f_address(handler);

                VIDEO_LOG("[SOF][COMPONENT] ID: %d", component->component_id);
                VIDEO_LOG("[SOF][COMPONENT] VERTICAL COE: %d", (component->sample_coe & 0xF));
                VIDEO_LOG("[SOF][COMPONENT] HORIZONTAL COE: %d", ((component->sample_coe >> 4) & 0xF));
                VIDEO_LOG("[SOF][COMPONENT] QT ID: %d", component->qt_id);

                f_seek(handler, f_tell(handler) + sizeof(segment_sof0_component_t));
            }
        }
            break;
        case JEPG_TYPE_START_OF_SCAN:
        {
            uint8_t byte0 = f_getc(handler);
            uint8_t byte1 = f_getc(handler);
            len = JEPG_SEGMENT_LEN_VALUE(byte0, byte1);

            VIDEO_LOG("[JEPG] Locate <SOS> table, length: %d", len);

            segment_sos_t* sos = (segment_sos_t*)f_address(handler);

            f_seek(handler, f_tell(handler) + sizeof(segment_sos_t));
            uint8_t i = 0;

            for (i = 0; i < sos->component_count; i++){
                segment_sos_component_t* component = (segment_sos_component_t*)f_address(handler);
                VIDEO_LOG("[SOS][COMPONENT] ID: %d", component->component_id);
                VIDEO_LOG("[SOS][COMPONENT] AC ID: %d", component->dht_id & 0xF);
                VIDEO_LOG("[SOS][COMPONENT] DC ID: %d", (component->dht_id >> 4) & 0xF);

                f_seek(handler, f_tell(handler) + sizeof(segment_sos_component_t));
            }

            // Jump sos segment
            f_seek(handler, f_tell(handler) + 3); // The end of 3 bytes

            // Segment scan end
            ret = 1;
        }
            break;
        case JEPG_TYPE_DEFINE_HUFFMAN_TABLE:
        {
            uint8_t byte0 = f_getc(handler);
            uint8_t byte1 = f_getc(handler);
            len = JEPG_SEGMENT_LEN_VALUE(byte0, byte1);

            VIDEO_LOG("[JEPG] Locate <HT> table, length: %d", len);

            segment_dht_t* dht = (segment_dht_t*)f_address(handler);

            uint8_t hid = ((dht->info >> 4) ? 2 : 0) | ((dht->info & 0xf) ? 1 : 0); // A?D | ID

            memcpy(jpeg_dht[hid], dht->ht_size, len - 1);

            // Jump huffman segment
            f_seek(handler, f_tell(handler) + len);
        }
            break;
        case JEPG_TYPE_COMMENT:
        {
            uint8_t byte0 = f_getc(handler);
            uint8_t byte1 = f_getc(handler);
            len = JEPG_SEGMENT_LEN_VALUE(byte0, byte1);

            VIDEO_LOG("[JEPG] Locate <COM> table, length: %d", len);

            segment_com_t* com = (segment_com_t*)f_address(handler);

            VIDEO_LOG("[COM] Comment: %s", com->com);

            // Jump huffman segment
            f_seek(handler, f_tell(handler) + len);
        }
            break;
        default:
            VIDEO_LOG("JEPG Decode un-support 0x%x segment flag", temp_char);
            while(1);
        }

        if (ret == 1){
            break;
        }
    }

    // Data decode

    return ret;
}

//static void Jepg_Decode_BaseValidation(void){
//    f_open(&f_handler, CMN_RAM1_REGION, 4421);
//
//    jpeg_decode_segment_abstract(&f_handler);
//}

static int32_t test_jpeg_encoder_api(void)
{
    int32_t ret = SUCCESS;
    uint32_t out_size = sizeof(codec_output_buffer);
    Jpeg_EncoderCfg enc_cfg;
    memset(&enc_cfg, 0, sizeof(enc_cfg));

    /* rgb888 */
    enc_cfg.width = 128;
    enc_cfg.height = 128;
    enc_cfg.q_factor = 50;
    enc_cfg.input_format = JPEG_PIXEL_FORMAT_RGB888;
    if (SUCCESS == (ret = jpeg_encoder(enc_rgb888_128x128, codec_output_buffer, &out_size, enc_cfg)))
        VIDEO_LOG("[%s:%d] test encode rgb888 SUCCESS", __FUNCTION__, __LINE__);
    else
        VIDEO_LOG("[%s:%d] test encode rgb888 FAILED", __FUNCTION__, __LINE__);

    /* y8 */
    enc_cfg.input_format = JPEG_PIXEL_FORMAT_GRAY;
    if (SUCCESS == (ret = jpeg_encoder(enc_rgb888_128x128_gray, codec_output_buffer, &out_size, enc_cfg)))
        VIDEO_LOG("[%s:%d] test encode gray8 SUCCESS", __FUNCTION__, __LINE__);
    else
        VIDEO_LOG("[%s:%d] test encode gray8 FAILED", __FUNCTION__, __LINE__);

    /* yuv422 */
    enc_cfg.input_format = JPEG_PIXEL_FORMAT_YUV422;
    if (SUCCESS == (ret = jpeg_encoder(enc_yuv422_128x128, codec_output_buffer, &out_size, enc_cfg)))
        VIDEO_LOG("[%s:%d] test encode yuv422 packed SUCCESS", __FUNCTION__, __LINE__);
    else
        VIDEO_LOG("[%s:%d] test encode yuv422 packed FAILED", __FUNCTION__, __LINE__);

    /* yuv422 */
    enc_cfg.input_format = JPEG_PIXEL_FORMAT_YUV422;
    if (SUCCESS == (ret = jpeg_encoder_ext(enc_yuv422_128x128, codec_output_buffer, &out_size, enc_cfg)))
        VIDEO_LOG("[%s:%d] test encode_ext yuv422 packed SUCCESS", __FUNCTION__, __LINE__);
    else
        VIDEO_LOG("[%s:%d] test encode_ext yuv422 packed FAILED", __FUNCTION__, __LINE__);

    return ret;
}

static int32_t test_jpeg_decoder_api(void)
{
    int32_t ret = SUCCESS;
    uint16_t width = 0;
    uint16_t height = 0;
    Jpeg_DecoderCfg dec_cfg;
    memset(&dec_cfg, 0, sizeof(dec_cfg));

    dec_cfg.output_format = JPEG_PIXEL_FORMAT_RGB888;
    if (SUCCESS == (ret = jpeg_decoder(dec_jpeg_128x128, sizeof(dec_jpeg_128x128), codec_output_buffer, &width, &height, dec_cfg)))
        VIDEO_LOG("[%s:%d] test decode rgb888 SUCCESS", __FUNCTION__, __LINE__);
    else
        VIDEO_LOG("[%s:%d] test decode rgb888 FAILED", __FUNCTION__, __LINE__);

    dec_cfg.output_format = JPEG_PIXEL_FORMAT_GRAY;
    if (SUCCESS == (ret = jpeg_decoder(dec_jpeg_128x128_gray, sizeof(dec_jpeg_128x128_gray), codec_output_buffer, &width, &height, dec_cfg)))
        VIDEO_LOG("[%s:%d] test decode gray8 SUCCESS", __FUNCTION__, __LINE__);
    else
        VIDEO_LOG("[%s:%d] test decode gray8 FAILED", __FUNCTION__, __LINE__);

    return ret;
}


