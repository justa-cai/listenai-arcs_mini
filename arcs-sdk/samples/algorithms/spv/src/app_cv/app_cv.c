#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "cv/acomp_cv.h"
#include "ipc/acomp_ipc.h"
#include "lisa_mem.h"
#include "resmgr/resmgr.h"

#include "app_cv_image_input.h"

#define TAG "app_cv"
#include "lisa_log.h"

#define CV_RES_STITCH          0
#define CV_RES_CUTLINE         1
#define CV_RES_OCR             2
#define CV_RES_COUNT           3

#define CV_STATUS_TIMEOUT_MS   120000

#define RES_STORAGE_FLASH      0
#define RES_STORAGE_SD         1

static SemaphoreHandle_t g_cv_done_sem;
static volatile uint32_t g_cv_status;

static void cv_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    (void)priv;

    if ((event & CV_CB_EVENT_OCR_RESULT) && event_data != NULL && event_data_len > 0) {
        LISA_LOGI(TAG, "ocr result: %.*s", (int)event_data_len, (char *)event_data);
        xSemaphoreGive(g_cv_done_sem);
    }

    if ((event & CV_CB_EVENT_STATUS) && event_data != NULL && event_data_len >= sizeof(uint32_t)) {
        g_cv_status = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "scanpen vision status: %u", g_cv_status);
    }

    if ((event & CV_CB_EVENT_FRAME_DONE) && event_data != NULL && event_data_len >= sizeof(uint32_t)) {
        app_cv_image_on_frame_done(*(uint32_t *)event_data);
    }
}

static int app_cv_prepare_resource(void)
{
    int ret;
    bool cutline_from_flash = false;
    uint32_t size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * CV_RES_COUNT;
    acomp_ipc_prepare_t *prepare = lisa_mem_alloc(size);

    if (prepare == NULL) {
        return -1;
    }

    memset(prepare, 0, size);
    prepare->number = CV_RES_COUNT;

    prepare->item[CV_RES_STITCH].index = CV_RES_STITCH;

    prepare->item[CV_RES_CUTLINE].index = CV_RES_CUTLINE;
#ifdef CONFIG_ACOMP_RESMGR
    {
        uint32_t res_size = 0;
        void *addr = resmgr_get_item(RES_CV_CUTLINE, &res_size);

        if ((addr != NULL) && (res_size > 0)) {
            prepare->item[CV_RES_CUTLINE].attr.hdr.storage = RES_STORAGE_FLASH;
            prepare->item[CV_RES_CUTLINE].addr = (uint32_t)(uintptr_t)addr;
            prepare->item[CV_RES_CUTLINE].size = res_size;
            cutline_from_flash = true;
        }
    }
#endif

    if (!cutline_from_flash) {
        prepare->item[CV_RES_CUTLINE].attr.hdr.storage = RES_STORAGE_SD;
        prepare->item[CV_RES_CUTLINE].addr = CONFIG_ACOMP_CV_RES_CUTLINE_EMMC_ADDR;
        prepare->item[CV_RES_CUTLINE].size = CONFIG_ACOMP_CV_RES_CUTLINE_SIZE;
    }

    prepare->item[CV_RES_OCR].index = CV_RES_OCR;
    prepare->item[CV_RES_OCR].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[CV_RES_OCR].addr = CONFIG_ACOMP_CV_RES_OCR_EMMC_ADDR;
    prepare->item[CV_RES_OCR].size = CONFIG_ACOMP_CV_RES_OCR_SIZE;

    ret = acomp_cv_prepare(prepare);
    lisa_mem_free(prepare);

    return ret;
}

int app_cv_init(void)
{
    int ret;

    g_cv_done_sem = xSemaphoreCreateBinary();
    if (g_cv_done_sem == NULL) {
        LISA_LOGE(TAG, "create scanpen vision semaphore failed");
        return -1;
    }

#ifdef CONFIG_ACOMP_RESMGR
    ret = resmgr_init(CONFIG_ACOMP_RESMGR_FLASH_ADDR);
    if (ret != 0) {
        LISA_LOGW(TAG, "resmgr_init failed: %d", ret);
    }
#endif

    ret = acomp_cv_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_cv_init failed: %d", ret);
        return ret;
    }

    ret = acomp_cv_add_callback(CV_CB_EVENT_OCR_RESULT | CV_CB_EVENT_STATUS | CV_CB_EVENT_FRAME_DONE,
                                cv_event_handler, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_cv_add_callback failed: %d", ret);
        return ret;
    }

    ret = app_cv_prepare_resource();
    if (ret != 0) {
        LISA_LOGE(TAG, "app_cv_prepare_resource failed: %d", ret);
        return ret;
    }

    acomp_cv_set_scan_mode(0);
    acomp_cv_set_lr_mode(0);
    acomp_cv_set_boot_type(0);

    ret = app_cv_image_stream_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "app_cv_image_stream_init failed: %d", ret);
        return ret;
    }

    ret = acomp_cv_start();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_cv_start failed: %d", ret);
        return ret;
    }

    return 0;
}

int app_cv_send_test_image(void)
{
    int ret;

    xSemaphoreTake(g_cv_done_sem, 0);

    ret = app_cv_image_send_test_image();
    if (ret != 0) {
        acomp_cv_stop();
        acomp_cv_cleanup();
        return ret;
    }

    if (xSemaphoreTake(g_cv_done_sem, pdMS_TO_TICKS(CV_STATUS_TIMEOUT_MS)) != pdTRUE) {
        LISA_LOGW(TAG, "wait scanpen vision result timeout");
    }

    acomp_cv_stop();
    acomp_cv_cleanup();

    return 0;
}
