#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "chip.h"
#include "log_print.h"
#include "systick.h"
#include "cache.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DMA2D.h"
#include "Driver_Blender.h"

#include "test_case.h"
#include "csk_driver.h"
#include "csk_dma2d_wrap.h"

#define TEST_LVGL_DMA2D_WHILE1_ENABLE           1
#define TEST_LVGL_DMA2D_IMAGE_STOP_ENABLE       0
#define TEST_LVGL_DMA2D_WIDTH_SCAN_ENABLE       0

static int32_t test_lv_rgb565_copy(void);    // src step, dts step
static int32_t test_lv_rgb565_fill(void);
static int32_t test_lv_rgb565_fill_mask(void);
static int32_t test_lv_rgb565_blend(void);
static int32_t test_lv_rgb565_blend_mask(void);

static int32_t test_lv_rgb565_blend_mask_reg(void);

void test_lvgl_dma2d_port3(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //test_lv_rgb565_blend_mask_reg();

    while(1)
    {
        CHECK_FUNC_EXIT(test_lv_rgb565_copy(), error);
        CHECK_FUNC_EXIT(test_lv_rgb565_fill(), error);
        CHECK_FUNC_EXIT(test_lv_rgb565_fill_mask(), error);
        CHECK_FUNC_EXIT(test_lv_rgb565_blend(), error);
        CHECK_FUNC_EXIT(test_lv_rgb565_blend_mask(), error);
    }

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static int32_t test_lv_rgb565_copy(void)
{
    int32_t ret = FAILURE;
    uint8_t *src_buf = NULL;
    uint8_t *dts_buf = NULL;
    uint32_t src_offset = 0;
    uint32_t dts_offset = 0;
    uint32_t src_size_byte = 0;
    uint32_t dts_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    uint16_t src_img_width = 320;
    uint16_t src_img_height = 86;
    uint16_t src_start_x = 60;
    uint16_t src_start_y = 20;
    uint16_t dts_img_width = 320;
    uint16_t dts_img_height = 128;
    uint16_t dts_start_x = 16;
    uint16_t dts_start_y = 32;
    uint16_t copy_width = 100;
    uint16_t copy_height = 50;

    src_size_byte = src_img_width * src_img_height * RGB565_PIXEL_BYTE;
    dts_size_byte = dts_img_width * dts_img_height * RGB565_PIXEL_BYTE;
    src_offset = (src_img_width * src_start_y + src_start_x) * RGB565_PIXEL_BYTE;
    dts_offset = (dts_img_width * dts_start_y + dts_start_x) * RGB565_PIXEL_BYTE;
    VIDEO_LOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
    VIDEO_LOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);

    /* malloc */
    src_buf = tiny_malloc(src_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
    VIDEO_LOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

    dts_buf = tiny_malloc(dts_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
    VIDEO_LOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

    rgb565_colorbar_create((uint16_t *)src_buf, src_img_width, src_img_height, 10);
    rgb565_color_fill((uint16_t *)dts_buf, RGB565_YELLOW, dts_img_width, dts_img_height);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = rgb565_dma2d_init();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    do {
        ret = lv_gpu_dma2d_copy(dts_buf + dts_offset, dts_img_width, src_buf + src_offset, src_img_width, copy_width, copy_height);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        DELAY_MS(1);
    } while(TEST_LVGL_DMA2D_WHILE1_ENABLE);
    while(TEST_LVGL_DMA2D_IMAGE_STOP_ENABLE);

#if TEST_LVGL_DMA2D_WIDTH_SCAN_ENABLE
    for(src_img_width = 2; src_img_width <= dts_img_width; src_img_width += 2)
    {
        for(copy_width = 2; copy_width <= src_img_width; copy_width += 2)
        {
            if(copy_width > src_img_width)
                break;

            ret = lv_gpu_dma2d_copy(dts_buf, dts_img_width, src_buf, src_img_width, copy_width, copy_height);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        }
    }
#endif

    ret = SUCCESS;

error:
    tiny_free(src_buf);
    tiny_free(dts_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_lv_rgb565_fill(void)
{
    int32_t ret = FAILURE;
    uint8_t *img_buf = NULL;
    uint32_t img_offset = 0;
    uint32_t img_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    uint16_t img_width = 320;
    uint16_t img_height = 86;
    uint16_t start_x = 60;
    uint16_t start_y = 20;
    uint16_t color_width = 100;
    uint16_t color_height = 50;
    uint32_t color = RGB565_RED;      // 0xF800

    img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
    img_offset = (img_width * start_y + start_x) * RGB565_PIXEL_BYTE;
    VIDEO_LOG("[%s:%d] img_size_byte=0x%x", __func__, __LINE__, img_size_byte);

    /* malloc */
    img_buf = tiny_malloc(img_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
    VIDEO_LOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

    rgb565_colorbar_create((uint16_t *)img_buf, img_width, img_height, 10);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

#if 0
    uint32_t cycle_start = 0;
    uint32_t cycle_end = 0;
    cycle_start = __RV_CSR_READ(CSR_MCYCLE);
    HAL_FlushDCache_by_Addr((uint32_t*)img_buf, img_size_byte);
    //HAL_FlushDCache();
    cycle_end = __RV_CSR_READ(CSR_MCYCLE);
    VIDEO_LOG("[%s:%d] cycle=%d", __func__, __LINE__, cycle_end - cycle_start);
#endif

    ret = rgb565_dma2d_init();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    do {
        ret = lv_gpu_dma2d_fill(img_buf + img_offset, img_width, color, color_width, color_height);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        DELAY_MS(1);
    } while(TEST_LVGL_DMA2D_WHILE1_ENABLE);
    while(TEST_LVGL_DMA2D_IMAGE_STOP_ENABLE);

#if TEST_LVGL_DMA2D_WIDTH_SCAN_ENABLE
    for(color_width = 2; color_width <= img_width; color_width += 2)
    {
        ret = lv_gpu_dma2d_fill(img_buf, img_width, color, color_width, color_height);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
        //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    }
#endif

    ret = SUCCESS;

error:
    tiny_free(img_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_lv_rgb565_fill_mask(void)
{
    int32_t ret = FAILURE;
    uint8_t *img_buf = NULL;
    uint8_t *mask_buf = NULL;
    uint32_t img_offset = 0;
    uint32_t img_size_byte = 0;
    uint32_t mask_size_byte = 0;
    uint8_t mask_value = 0;
    uint16_t x = 0;
    uint16_t y = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
#if 1
    uint16_t img_width = 320;
    uint16_t img_height = 86;
    uint16_t start_x = 60;
    uint16_t start_y = 20;
    uint16_t mask_width = 100;
    uint16_t color_width = 100;
    uint16_t color_height = 50;
    uint32_t color = RGB565_RED;      // 0xF800
    uint8_t opa = 0x80;
#else
    uint16_t img_width = 320;
    uint16_t img_height = 86;
    uint16_t start_x = 0;
    uint16_t start_y = 0;
    uint16_t mask_width = 320;
    uint16_t color_width = 320;
    uint16_t color_height = 50;
    uint32_t color = RGB565_RED;      // 0xF800
    uint8_t opa = 0x80;
#endif

    img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
    img_offset = (img_width * start_y + start_x) * RGB565_PIXEL_BYTE;
    mask_size_byte = mask_width * color_height;
    VIDEO_LOG("[%s:%d] img_size_byte=0x%x", __func__, __LINE__, img_size_byte);
    VIDEO_LOG("[%s:%d] mask_size_byte=0x%x", __func__, __LINE__, mask_size_byte);

    /* malloc */
    img_buf = tiny_malloc(img_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
    VIDEO_LOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

    mask_buf = tiny_malloc(mask_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(mask_buf, error);
    VIDEO_LOG("mask_buf=0x%08x size=0x%x byte", mask_buf, mask_size_byte);

    for(y = 0; y < color_height; y++)
    {
        mask_value = y << 4;
        memset(mask_buf + (mask_width * y), mask_value, mask_width);
    }
    //rgb565_colorbar_create((uint16_t *)img_buf, img_width, img_height, 40);
    rgb565_color_fill((uint16_t *)img_buf, RGB565_YELLOW, img_width, img_height);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = rgb565_dma2d_init();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    do {
        ret = lv_gpu_dma2d_fill_mask(img_buf + img_offset, img_width, color, mask_buf, mask_width, opa, color_width, color_height);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        DELAY_MS(1);
    } while(TEST_LVGL_DMA2D_WHILE1_ENABLE);
    while(TEST_LVGL_DMA2D_IMAGE_STOP_ENABLE);

#if TEST_LVGL_DMA2D_WIDTH_SCAN_ENABLE
    tiny_free(mask_buf);
    color_height = 33;
    mask_size_byte = img_width * img_height;
    mask_buf = tiny_malloc(mask_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(mask_buf, error);
    VIDEO_LOG("mask_buf=0x%08x size=0x%x byte", mask_buf, mask_size_byte);
    for(y = 0; y < img_height; y++)
    {
        for(x = 0; x < img_width; x++)
        {
            *(uint8_t *)mask_buf = x & 0xFF;
        }
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    for(mask_width = 2; mask_width <= img_width; mask_width += 2)
    {
        for(color_width = 2; color_width <= img_width; color_width += 2)
        {
            if(color_width > mask_width)
                break;

            ret = lv_gpu_dma2d_fill_mask(img_buf, img_width, color, mask_buf, mask_width, opa, color_width, color_height);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        }
    }
#endif

    ret = SUCCESS;

error:
    tiny_free(img_buf);
    tiny_free(mask_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_lv_rgb565_blend(void)
{
    int32_t ret = FAILURE;
    uint8_t *img_buf = NULL;
    uint8_t *map_buf = NULL;
    uint32_t img_offset = 0;
    uint32_t map_offset = 0;
    uint32_t img_size_byte = 0;
    uint32_t map_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    uint16_t img_width = 320;
    uint16_t img_height = 86;
    uint16_t img_start_x = 60;
    uint16_t img_start_y = 20;
    uint16_t map_width = 320;
    uint16_t map_height = 128;
    uint16_t map_start_x = 16;
    uint16_t map_start_y = 32;
    uint16_t copy_width = 100;
    uint16_t copy_height = 50;
    uint8_t opa = 0x80;

    img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
    map_size_byte = map_width * map_height * RGB565_PIXEL_BYTE;
    img_offset = (img_width * img_start_y + img_start_x) * RGB565_PIXEL_BYTE;
    map_offset = (map_width * map_start_y + map_start_x) * RGB565_PIXEL_BYTE;
    VIDEO_LOG("[%s:%d] img_size_byte=0x%x", __func__, __LINE__, img_size_byte);
    VIDEO_LOG("[%s:%d] map_size_byte=0x%x", __func__, __LINE__, map_size_byte);

    /* malloc */
    img_buf = tiny_malloc(img_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
    VIDEO_LOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

    map_buf = tiny_malloc(map_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(map_buf, error);
    VIDEO_LOG("map_buf=0x%08x size=0x%x byte", map_buf, map_size_byte);

    rgb565_color_fill((uint16_t *)img_buf, RGB565_YELLOW, img_width, img_height);
    rgb565_colorbar_create((uint16_t *)map_buf, map_width, map_height, 10);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = rgb565_dma2d_init();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    do {
        ret = lv_gpu_dma2d_blend(img_buf + img_offset, img_width, map_buf + map_offset, opa, map_width, copy_width, copy_height);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        DELAY_MS(1);
    } while(TEST_LVGL_DMA2D_WHILE1_ENABLE);
    while(TEST_LVGL_DMA2D_IMAGE_STOP_ENABLE);

#if TEST_LVGL_DMA2D_WIDTH_SCAN_ENABLE
    for(map_width = 2; map_width <= img_width; map_width += 2)
    {
        for(copy_width = 2; copy_width <= img_width; copy_width += 2)
        {
            if(copy_width > map_width)
                break;

            ret = lv_gpu_dma2d_blend(img_buf, img_width, map_buf, opa, map_width, copy_width, copy_height);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        }
    }
#endif

    ret = SUCCESS;

error:
    tiny_free(img_buf);
    tiny_free(map_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_lv_rgb565_blend_mask(void)
{
    int32_t ret = FAILURE;
    uint8_t *img_buf = NULL;
    uint8_t *map_buf = NULL;
    uint8_t *mask_buf = NULL;
    uint32_t img_offset = 0;
    uint32_t map_offset = 0;
    uint32_t img_size_byte = 0;
    uint32_t map_size_byte = 0;
    uint32_t mask_size_byte = 0;
    uint16_t x = 0;
    uint16_t y = 0;

#if 1
    /* RGB565  src:320x240 -> dts:100x100 */
#if 1
    uint16_t img_width = 320;
    uint16_t img_height = 86;
    uint16_t img_start_x = 60;
    uint16_t img_start_y = 20;
    uint16_t map_width = 320;
    uint16_t map_height = 86;
    uint16_t map_start_x = 60;
    uint16_t map_start_y = 20;
    uint16_t mask_width = 100;
    uint16_t copy_width = 100;
    uint16_t copy_height = 50;
    uint8_t opa = 0x80;
#else
    uint16_t img_width = 320;
    uint16_t img_height = 86;
    uint16_t img_start_x = 0;
    uint16_t img_start_y = 0;
    uint16_t map_width = 320;
    uint16_t map_height = 86;
    uint16_t map_start_x = 0;
    uint16_t map_start_y = 0;
    uint16_t mask_width = 320;
    uint16_t copy_width = 320;
    uint16_t copy_height = 50;
    uint8_t opa = 0x80;
#endif

    img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
    map_size_byte = map_width * map_height * RGB565_PIXEL_BYTE;
    mask_size_byte = mask_width * copy_height;
    img_offset = (img_width * img_start_y + img_start_x) * RGB565_PIXEL_BYTE;
    map_offset = (map_width * map_start_y + map_start_x) * RGB565_PIXEL_BYTE;

    /* malloc */
    img_buf = tiny_malloc(img_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
    VIDEO_LOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

    map_buf = tiny_malloc(map_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(map_buf, error);
    VIDEO_LOG("map_buf=0x%08x size=0x%x byte", map_buf, map_size_byte);

    mask_buf = tiny_malloc(mask_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(mask_buf, error);
    VIDEO_LOG("mask_buf=0x%08x size=0x%x byte", mask_buf, mask_size_byte);
#else
    uint16_t img_width = 640;
    uint16_t img_height = 172;
    uint16_t img_start_x = 0;
    uint16_t img_start_y = 0;
    uint16_t map_width = 68;
    uint16_t map_height = 8;
    uint16_t map_start_x = 0;
    uint16_t map_start_y = 0;
    uint16_t mask_width = 68;
    uint16_t copy_width = 68;
    uint16_t copy_height = 8;
    uint8_t opa = 0x80;

#if 0   // SRAM  0x20060000~0x200AFFFF  64x5
    img_buf = (uint8_t *)0x20060000;        // 215KB
    map_buf = (uint8_t *)0x200A0000;
    mask_buf = (uint8_t *)0x200A8000;
#endif

#if 0   // PSRAM
    img_buf = (uint8_t *)0x28000000;        // 215KB
    map_buf = (uint8_t *)0x28040000;
    mask_buf = (uint8_t *)0x28048000;
#endif

    uint32_t total_start = 0;
    uint32_t total_end = 0;

    map_size_byte = map_width * map_height * RGB565_PIXEL_BYTE;
    mask_size_byte = mask_width * copy_height;

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    memcpy(0x28000000, 0x20060000, map_size_byte);
    memcpy(0x28040000, 0x200A0000, map_size_byte);
    memcpy(0x28048000, 0x200A8000, mask_size_byte);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
    VIDEO_LOG("memcpy=%dcycle  =%dns", total_end - total_start, (total_end - total_start) * 10 / 3);

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    memcpy(0x20060000, 0x28000000, map_size_byte);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
    VIDEO_LOG("memcpy=%dcycle  =%dns", total_end - total_start, (total_end - total_start) * 10 / 3);
#endif

    //rgb565_colorbar_create((uint16_t *)img_buf, img_width, img_height, 20);
    rgb565_color_fill((uint16_t *)img_buf, RGB565_WHITE, img_width, img_height);
    rgb565_color_fill((uint16_t *)map_buf, RGB565_YELLOW, map_width, map_height);
    for(y = 0; y < copy_height; y++)
    {
        memset(mask_buf + (mask_width * y), y << 4, mask_width);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = rgb565_dma2d_init();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    do {
        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        ret = lv_gpu_dma2d_blend_mask(img_buf + img_offset, img_width, map_buf + map_offset, map_width, mask_buf, mask_width, opa, copy_width, copy_height);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
        DELAY_MS(1);
    } while(TEST_LVGL_DMA2D_WHILE1_ENABLE);
    while(TEST_LVGL_DMA2D_IMAGE_STOP_ENABLE);

#if TEST_LVGL_DMA2D_WIDTH_SCAN_ENABLE
    tiny_free(mask_buf);
    copy_height = 33;
    mask_size_byte = img_width * img_height;
    mask_buf = tiny_malloc(mask_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(mask_buf, error);
    VIDEO_LOG("mask_buf=0x%08x size=0x%x byte", mask_buf, mask_size_byte);
    for(y = 0; y < img_height; y++)
    {
        for(x = 0; x < img_width; x++)
        {
            *(uint8_t *)mask_buf = x & 0xFF;
        }
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    for(map_width = 2; map_width <= img_width; map_width += 2)
    {
        for(mask_width = 2; mask_width <= img_width; mask_width += 2)
        {
            for(copy_width = 2; copy_width <= img_width; copy_width += 2)
            {
                if(copy_width > map_width)
                    break;

                if(copy_width > mask_width)
                    break;

                ret = lv_gpu_dma2d_blend_mask(img_buf, img_width, map_buf, map_width, mask_buf, mask_width, opa, copy_width, copy_height);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
                //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
            }
        }
    }
#endif

    ret = SUCCESS;

error:
    tiny_free(img_buf);
    tiny_free(map_buf);
    tiny_free(mask_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }

    return ret;
}



#define GP_DMAC     IP_GPDMA
#define D2B         IP_D2BLENDER

static int32_t test_lv_rgb565_blend_mask_reg(void)
{
    int32_t ret = FAILURE;
    uint8_t *img_buf = NULL;
    uint8_t *map_buf = NULL;
    uint8_t *mask_buf = NULL;
    uint32_t img_size_byte = 64*64*2;
    uint32_t map_size_byte = 64*64*2;
    uint32_t mask_size_byte = 16*16*1;

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    disable_IRQ(IRQ_DMAC_GP_IMG_VECTOR);

    /* malloc */
    img_buf = tiny_malloc(img_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
    VIDEO_LOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

    map_buf = tiny_malloc(map_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(map_buf, error);
    VIDEO_LOG("map_buf=0x%08x size=0x%x byte", map_buf, map_size_byte);

    mask_buf = tiny_malloc(mask_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(mask_buf, error);
    VIDEO_LOG("mask_buf=0x%08x size=0x%x byte", mask_buf, mask_size_byte);

    GP_DMAC->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN                         = 0x1;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_CH_HS_SEL_CH9                             = 0x1;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_SRC_BURST_LEN_CH9                         = 0x3; //0: 1, 1: 2, 2: 4, 3: 8
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_DST_BURST_LEN_CH9                         = 0x3;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_TFR_MODE_CH9                              = 0x0; //0: p2m, 1: m2p, 2: m2m
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_SRC_INC_CH9                               = 0x1; //0: inc, 1: fix
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_DST_INC_CH9                               = 0x0;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9                      = 0x1; //TUDO
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_AHB_LOCK_CH9                              = 0x1;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_DST_PIPO_CH9                              = 0x0;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_DMA_AUTO_TFR_CH9                          = 0x0;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_TRANS_SRC_BASE_UNIT_CH9                   = 0x2; //2:word
    GP_DMAC->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH9        = 0x2; //2:word
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_BLOCK_FINISH_MASK_CH9                     = 0x0;
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_FLOW_CTRL_CH9                             = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9         = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_CH9_BLENDER_STEP_SEL       = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9       = 0x1;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9          = 0x8;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9          = 0x10;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9       = 0x1c;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9        = 0x4;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9      = 0x10;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9       = 0x1;
    GP_DMAC->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9            = 0x1;

    GP_DMAC->REG_DMA_BLOCK_LEN_CH9.bit.CFG_BLOCK_LEN_CH9                        = 0x80;
    GP_DMAC->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.bit.CFG_IMAGE_OUT_BLOCK_LEN_CH9    = 0x80;
    GP_DMAC->REG_DMA_SRC_ADDR0_CH9.bit.CFG_SRC_ADDR0_CH9                        = VIEDO_BASE + 0x9800;
    GP_DMAC->REG_DMA_DST_ADDR0_CH9.bit.CFG_DST_ADDR0_CH9                        = (uint32_t)img_buf;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9      = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9     = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9        = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9       = 0x10;  //16*16

    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_CH_EN_CH9                                 = 0x1;

    //Configure DMA.CH8  d2blender fore fifo
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_CH_HS_SEL_CH8                             = 0x4;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_SRC_BURST_LEN_CH8                         = 0x3;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_DST_BURST_LEN_CH8                         = 0x3;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_TFR_MODE_CH8                              = 0x1;//m2p
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_SRC_INC_CH8                               = 0x0;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_DST_INC_CH8                               = 0x1;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_READ_DONE_ACK_EN_CH8                      = 0x1;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_AHB_LOCK_CH8                              = 0x0;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_DMA_AUTO_TFR_CH8                          = 0x0;

    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_BYPASS_CH8         = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_CH8_BLENDER_STEP_SEL       = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_SW_CTRL_CH8       = 0x1;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_WNUM_CH8          = 0x8;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_HNUM_CH8          = 0x10;
   // GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_L0_CH8       = 0x1c;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8        = 0x1c;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8      = 0x1;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8       = 0x1;

    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_TRANS_SRC_BASE_UNIT_CH8                   = 0x2;
    GP_DMAC->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH8        = 0x2;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_BLOCK_FINISH_MASK_CH8                     = 0x0;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_FLOW_CTRL_CH8                             = 0x0;

    GP_DMAC->REG_DMA_BLOCK_LEN_CH8.bit.CFG_BLOCK_LEN_CH8                        = 0x80;
    GP_DMAC->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.bit.CFG_IMAGE_OUT_BLOCK_LEN_CH8    = 0x80;
    GP_DMAC->REG_DMA_SRC_ADDR0_CH8.bit.CFG_SRC_ADDR0_CH8                        = (uint32_t)map_buf;
    GP_DMAC->REG_DMA_DST_ADDR0_CH8.bit.CFG_DST_ADDR0_CH8                        = VIEDO_BASE + 0x8000;//pbuf
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.bit.CFG_IMAGE_WIDTH_OUT_CH8      = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.bit.CFG_IMAGE_HEIGHT_OUT_CH8     = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH8.bit.CFG_IMAGE_WIDTH_IN_CH8        = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH8.bit.CFG_IMAGE_HEIGHT_IN_CH8       = 0x10;

    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_CH_EN_CH8                                 = 0x1;


    //Configure DMA.CH7  d2blender back fifo
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_CH_HS_SEL_CH7                             = 0x3;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_SRC_BURST_LEN_CH7                         = 0x3;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_DST_BURST_LEN_CH7                         = 0x3;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_TFR_MODE_CH7                              = 0x1;//m2p
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_SRC_INC_CH7                               = 0x0;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_DST_INC_CH7                               = 0x1;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_READ_DONE_ACK_EN_CH7                      = 0x1;

    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_TRANS_SRC_BASE_UNIT_CH7                   = 0x2;
    GP_DMAC->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH7        = 0x2;

    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_BLOCK_FINISH_MASK_CH7                     = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_BYPASS_CH7         = 0x0;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_CH7_BLENDER_STEP_SEL       = 0x0;

    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_SW_CTRL_CH7       = 0x1;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7          = 0x8;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7          = 0x10;
   // GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_L0_CH7       = 0x1c;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7        = 0x1c;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7      = 0x1;
    GP_DMAC->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7       = 0x1;

    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_FLOW_CTRL_CH7                             = 0x0;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_BLOCK_FINISH_MASK_CH7                     = 0x0;
    GP_DMAC->REG_DMA_BLOCK_LEN_CH7.bit.CFG_BLOCK_LEN_CH7                        = 0x80;
    GP_DMAC->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.bit.CFG_IMAGE_OUT_BLOCK_LEN_CH7    = 0x80;
    GP_DMAC->REG_DMA_SRC_ADDR0_CH7.bit.CFG_SRC_ADDR0_CH7                        = (uint32_t)img_buf;
    GP_DMAC->REG_DMA_DST_ADDR0_CH7.bit.CFG_DST_ADDR0_CH7                        = VIEDO_BASE + 0x8800;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_WIDTH_OUT_CH7      = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_HEIGHT_OUT_CH7     = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.bit.CFG_IMAGE_WIDTH_IN_CH7        = 0x10;
    GP_DMAC->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.bit.CFG_IMAGE_HEIGHT_IN_CH7       = 0x10;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_CH_EN_CH7                                 = 0x1;




    //Configure DMA.CH6  d2blender mask
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_CH_HS_SEL_CH6                             = 0x2;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_SRC_BURST_LEN_CH6                         = 0x2;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_DST_BURST_LEN_CH6                         = 0x2;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_TFR_MODE_CH6                              = 0x1;//m2p
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_SRC_INC_CH6                               = 0x0;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_DST_INC_CH6                               = 0x1;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_READ_DONE_ACK_EN_CH6                      = 0x0;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_FLOW_CTRL_CH6                             = 0x0;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_BLOCK_FINISH_MASK_CH6                     = 0x0;
    GP_DMAC->REG_DMA_BLOCK_LEN_CH6.bit.CFG_BLOCK_LEN_CH6                        = 0x40;
    GP_DMAC->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.bit.CFG_IMAGE_OUT_BLOCK_LEN_CH6    = 0x40;
    GP_DMAC->REG_DMA_SRC_ADDR0_CH6.bit.CFG_SRC_ADDR0_CH6                        = (uint32_t)mask_buf;
    GP_DMAC->REG_DMA_DST_ADDR0_CH6.bit.CFG_DST_ADDR0_CH6                        = VIEDO_BASE + 0x9000;


    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_TRANS_SRC_BASE_UNIT_CH6                   = 0x2;
    GP_DMAC->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH6        = 0x2;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_BLOCK_FINISH_MASK_CH6                     = 0x0;

    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_CH_EN_CH6                                 = 0x1;

////////////////////////////////////////////////////
//d2blender
////////////////////////////////////////////////////
    //IMAGE_PROC->REG_D2BLENDER_EN.bit.D2BLENDER_EN          = 0x1;
    D2B->REG_BLENDER_CTRL.bit.BLENDER_MODE = 0x1;
    D2B->REG_BLENDER_CTRL.bit.BACK_COLOR_MODE = 0x0;
    D2B->REG_BLENDER_CTRL.bit.FORE_COLOR_MODE = 0x2;
    D2B->REG_BLENDER_CTRL.bit.ALPHA_MODE = 0x1;
    D2B->REG_ALPHA.bit.ALPHA = 0x88;
    D2B->REG_COLOR.bit.COLOR_R = 0x1;
    D2B->REG_COLOR.bit.COLOR_G = 0x1;
    D2B->REG_COLOR.bit.COLOR_B = 0x1;
    D2B->REG_FIFO_BURST_THD.bit.D2FORE_BURST_THD = 0x8;
    D2B->REG_FIFO_BURST_THD.bit.D2BACK_BURST_THD = 0x8;
    D2B->REG_FIFO_BURST_THD.bit.D2MASK_BURST_THD = 0x4;
    D2B->REG_FIFO_BURST_THD.bit.D2OUT_BURST_THD = 0x8;

    GP_DMAC->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x1;
   //Enable DMA
    GP_DMAC->REG_DMA_CH9_CTRL.bit.CFG_CH_START_CH9         = 0x1;
    GP_DMAC->REG_DMA_CH8_CTRL.bit.CFG_CH_START_CH8         = 0x1;
    GP_DMAC->REG_DMA_CH7_CTRL.bit.CFG_CH_START_CH7         = 0x1;
    GP_DMAC->REG_DMA_CH6_CTRL.bit.CFG_CH_START_CH6         = 0x1;

    VIDEO_LOG("[%s:%d] state=0x%x", __func__, __LINE__, IP_DMA2D->REG_DMA_IMAGE_INT_STATUS.all);

    D2B->REG_D2BLENDER_FORE_SIZE.bit.D2BLENDER_FORE_SIZE = 0x80;
    D2B->REG_D2BLENDER_BACK_SIZE.bit.D2BLENDER_BACK_SIZE = 0x80;
    D2B->REG_D2BLENDER_MASK_SIZE.bit.D2BLENDER_MASK_SIZE = 0x40;
    D2B->REG_BLENDER_EN.bit.BLENDER_EN = 0x1;

    lvgl_gpdma_reg_dump();
    lvgl_blender_reg_dump();

    while(1)
    {
        VIDEO_LOG("[%s:%d] state=0x%x", __func__, __LINE__, IP_DMA2D->REG_DMA_IMAGE_INT_STATUS.all);
        DELAY_MS(100);
    }

    ret = SUCCESS;

error:
    tiny_free(img_buf);
    tiny_free(map_buf);
    tiny_free(mask_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;

}
