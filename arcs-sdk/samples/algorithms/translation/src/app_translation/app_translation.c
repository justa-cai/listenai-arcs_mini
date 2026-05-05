#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "translation/acomp_translation.h"
#include "resmgr/resmgr.h"

#define TAG "app_trans"
#include "lisa_log.h"

#define TRANS_TIMEOUT_MS        10000
#define TRANS_RESULT_BUF_SIZE   1024

#define TRANS_TYPE_CN2EN        1
#define TRANS_TYPE_EN2CN        2
#define TRANS_STATUS_BEGIN      (1UL << 0)
#define TRANS_STATUS_END        (1UL << 2)

static SemaphoreHandle_t g_trans_done_sem;
static char g_trans_result[TRANS_RESULT_BUF_SIZE];
static uint32_t g_trans_status;
static bool g_trans_seen_begin;
static bool g_trans_seen_end;
static bool g_trans_has_result;

static void translation_try_finish(void)
{
    if (g_trans_done_sem == NULL) {
        return;
    }

    if (g_trans_seen_begin && g_trans_seen_end && g_trans_has_result) {
        xSemaphoreGive(g_trans_done_sem);
    }
}

static void translation_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    (void)priv;

    if ((event & TRANS_CB_EVENT_RESULT) && event_data != NULL && event_data_len > 0) {
        uint32_t copy_len = event_data_len;

        if (!g_trans_seen_begin) {
            return;
        }

        if (copy_len >= sizeof(g_trans_result)) {
            copy_len = sizeof(g_trans_result) - 1;
        }

        memcpy(g_trans_result, event_data, copy_len);
        g_trans_result[copy_len] = '\0';

        LISA_LOGI(TAG, "translation result: %s", g_trans_result);

        if (g_trans_result[0] != '\0') {
            g_trans_has_result = true;
        }

        translation_try_finish();
    }

    if ((event & TRANS_CB_EVENT_STATUS) && event_data != NULL && event_data_len >= sizeof(uint32_t)) {
        g_trans_status = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "translation status: %u", g_trans_status);

        if (g_trans_status == TRANS_STATUS_BEGIN) {
            g_trans_seen_begin = true;
            g_trans_seen_end = false;
            g_trans_has_result = false;
            memset(g_trans_result, 0, sizeof(g_trans_result));
        } else if (g_trans_status == TRANS_STATUS_END) {
            g_trans_seen_end = true;
        }

        translation_try_finish();
    }
}

static int translation_run_once(int type, const char *text, const char *tag)
{
    int ret;

    memset(g_trans_result, 0, sizeof(g_trans_result));
    g_trans_status = 0;
    g_trans_seen_begin = false;
    g_trans_seen_end = false;
    g_trans_has_result = false;
    xSemaphoreTake(g_trans_done_sem, 0);

    LISA_LOGI(TAG, "%s input: %s", tag, text);

    ret = acomp_translation_set_res_type(type);
    if (ret != 0) {
        LISA_LOGE(TAG, "%s set_res_type failed: %d", tag, ret);
        return ret;
    }

    ret = acomp_translation_translate(text, strlen(text));
    if (ret != 0) {
        LISA_LOGE(TAG, "%s translate failed: %d", tag, ret);
        return ret;
    }

    if (xSemaphoreTake(g_trans_done_sem, pdMS_TO_TICKS(TRANS_TIMEOUT_MS)) != pdTRUE) {
        LISA_LOGE(TAG, "%s wait result timeout", tag);
        return -1;
    }

    LISA_LOGI(TAG, "%s output: %s", tag, g_trans_result);
    return 0;
}

int app_translation_init(void)
{
    int ret;

    static const char cn_text[] = "这是一个 translation 算法 sample。";
    static const char en_text[] = "This is a translation sample running on CP.";

    g_trans_done_sem = xSemaphoreCreateBinary();
    if (g_trans_done_sem == NULL) {
        LISA_LOGE(TAG, "create translation semaphore failed");
        return -1;
    }

#ifdef CONFIG_ACOMP_RESMGR
    ret = resmgr_init(CONFIG_ACOMP_RESMGR_FLASH_ADDR);
    if (ret != 0) {
        LISA_LOGW(TAG, "resmgr_init failed: %d", ret);
    }
#endif

    ret = acomp_translation_do_prepare(translation_event_handler, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_translation_do_prepare failed: %d", ret);
        return ret;
    }

    ret = translation_run_once(TRANS_TYPE_CN2EN, cn_text, "CN2EN");
    if (ret != 0) {
        acomp_translation_do_cleanup();
        return ret;
    }

    ret = translation_run_once(TRANS_TYPE_EN2CN, en_text, "EN2CN");
    if (ret != 0) {
        acomp_translation_do_cleanup();
        return ret;
    }

    ret = acomp_translation_do_cleanup();
    LISA_LOGI(TAG, "translation sample finished: %d", ret);

    return ret;
}
