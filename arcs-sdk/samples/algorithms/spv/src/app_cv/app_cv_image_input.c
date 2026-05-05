#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>

#include "cv/acomp_cv.h"
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "sysheap.h"

#define TAG "app_cv_in"
#include "lisa_log.h"

#define CV_STREAM_TX_CH_INDEX    0
#define CV_STREAM_TX_CH_CNAME    "stream.cv_image"
#define CV_STREAM_TX_BUF_SIZE    (5 * sizeof(uint32_t))

#define CV_STATUS_BEGIN          0
#define CV_STATUS_CONTINUE       1
#define CV_STATUS_END            2

#define CV_MOCK_IMAGE_WIDTH      96U
#define CV_MOCK_IMAGE_HEIGHT     240U
#define CV_MOCK_IMAGE_COUNT      100U
#define CV_MOCK_IMAGE_SIZE       (CV_MOCK_IMAGE_WIDTH * CV_MOCK_IMAGE_HEIGHT)
#define CV_MOCK_IMAGE_BASE_ADDR  0x9600000U

#define CV_SECTOR_SIZE           512U
#define CV_FRAME_DONE_TIMEOUT_MS 1000U

static uint8_t *g_cv_image_buf;
static lisa_device_t *g_sdmmc_dev;
static SemaphoreHandle_t g_cv_frame_done_sem;

static int cv_send_frame(uint32_t fb_addr, uint32_t fuid, uint16_t width, uint16_t height, uint32_t status)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    uint32_t *hdr;

    buffer = acomp_cv_stream_tx_buffer_alloc(CV_STREAM_TX_CH_INDEX, &len, &desc_idx);
    if ((buffer == NULL) || (len < CV_STREAM_TX_BUF_SIZE)) {
        return -1;
    }

    hdr = (uint32_t *)buffer;
    hdr[0] = status;
    hdr[1] = fuid;
    hdr[2] = width;
    hdr[3] = height;
    hdr[4] = fb_addr;

    return acomp_cv_stream_tx_buffer_submit(CV_STREAM_TX_CH_INDEX, buffer, CV_STREAM_TX_BUF_SIZE, desc_idx);
}

uint32_t app_cv_mock_image_count(void)
{
    return CV_MOCK_IMAGE_COUNT;
}

uint32_t app_cv_mock_image_width(void)
{
    return CV_MOCK_IMAGE_WIDTH;
}

uint32_t app_cv_mock_image_height(void)
{
    return CV_MOCK_IMAGE_HEIGHT;
}

uint32_t app_cv_mock_image_size(void)
{
    return CV_MOCK_IMAGE_SIZE;
}

uint32_t app_cv_mock_image_byte_addr(uint32_t index)
{
    return CV_MOCK_IMAGE_BASE_ADDR + (index * CV_MOCK_IMAGE_SIZE);
}

uint32_t app_cv_mock_frame_status(uint32_t index)
{
    if (index == 0U) {
        return CV_STATUS_BEGIN;
    }

    if (index == (CV_MOCK_IMAGE_COUNT - 1U)) {
        return CV_STATUS_END;
    }

    return CV_STATUS_CONTINUE;
}

static int app_cv_image_read_mock_image(uint32_t index)
{
    uint32_t byte_addr;
    uint32_t sector;
    uint32_t sector_count;
    int ret;

    if (index >= CV_MOCK_IMAGE_COUNT) {
        return -1;
    }

    if (g_sdmmc_dev == NULL) {
        g_sdmmc_dev = lisa_device_get("sdmmc0");
        if (g_sdmmc_dev == NULL) {
            LISA_LOGE(TAG, "get sdmmc0 failed");
            return -1;
        }
    }

    byte_addr = app_cv_mock_image_byte_addr(index);
    sector = byte_addr / CV_SECTOR_SIZE;
    sector_count = (CV_MOCK_IMAGE_SIZE + CV_SECTOR_SIZE - 1U) / CV_SECTOR_SIZE;

    ret = lisa_sdmmc_read(g_sdmmc_dev, g_cv_image_buf, sector, sector_count);
    if (ret != 0) {
        LISA_LOGE(TAG, "read mock image[%u] failed: %d", index, ret);
        return ret;
    }

    return 0;
}

int app_cv_image_stream_init(void)
{
    acomp_stream_chn_create_desc_t desc = {
        .cname = CV_STREAM_TX_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = CV_STREAM_TX_CH_INDEX,
        .buffer_size = CV_STREAM_TX_BUF_SIZE,
        .num_descs = 4,
        .kick_policy = 1,
    };

    g_cv_frame_done_sem = xSemaphoreCreateBinary();
    if (g_cv_frame_done_sem == NULL) {
        LISA_LOGE(TAG, "create frame_done semaphore failed");
        return -1;
    }

    g_cv_image_buf = psram_malloc(CV_MOCK_IMAGE_SIZE);
    if (g_cv_image_buf == NULL) {
        LISA_LOGE(TAG, "alloc scanpen vision image buffer failed");
        return -1;
    }

    return acomp_cv_stream_ch_enable(CV_STREAM_TX_CH_INDEX, &desc);
}

int app_cv_image_send_test_image(void)
{
    uint32_t index;

    LISA_LOGI(TAG, "send %u mock images from eMMC: base=0x%x, frame=%ux%u",
              CV_MOCK_IMAGE_COUNT, CV_MOCK_IMAGE_BASE_ADDR,
              CV_MOCK_IMAGE_WIDTH, CV_MOCK_IMAGE_HEIGHT);

    for (index = 0; index < CV_MOCK_IMAGE_COUNT; index++) {
        int ret;

        ret = app_cv_image_read_mock_image(index);
        if (ret != 0) {
            return ret;
        }

        ret = cv_send_frame((uint32_t)(uintptr_t)g_cv_image_buf,
                            index,
                            CV_MOCK_IMAGE_WIDTH,
                            CV_MOCK_IMAGE_HEIGHT,
                            app_cv_mock_frame_status(index));
        if (ret != 0) {
            LISA_LOGE(TAG, "send frame[%u] failed: %d", index, ret);
            return ret;
        }

        if (index != (CV_MOCK_IMAGE_COUNT - 1U)) {
            if (xSemaphoreTake(g_cv_frame_done_sem, pdMS_TO_TICKS(CV_FRAME_DONE_TIMEOUT_MS)) != pdTRUE) {
                LISA_LOGE(TAG, "wait frame_done timeout: %u", index);
                return -1;
            }
        }
    }

    LISA_LOGI(TAG, "all mock images submitted");
    return 0;
}

void app_cv_image_on_frame_done(uint32_t fb_addr)
{
    if ((g_cv_frame_done_sem != NULL) && (fb_addr == (uint32_t)(uintptr_t)g_cv_image_buf)) {
        xSemaphoreGive(g_cv_frame_done_sem);
    }
}
