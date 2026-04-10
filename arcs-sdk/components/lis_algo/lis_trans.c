#include <string.h>
#include "lis_trans.h"
#include "log_print.h"
#include "task.h"
#include "semphr.h"
#include "acomp_translation.h"

#define WAIT_CTRL_ACK (pdMS_TO_TICKS(500))

typedef struct
{
    uint8_t inited : 1;
    char *result;
    lis_trans_status status;
    SemaphoreHandle_t ctrl_sem;
} trans_t;

static const char TRANS_TAG[] = "trans";
static trans_t trans;

/* acomp translation event callback */
static void trans_event_cb(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    if (event & TRANS_CB_EVENT_RESULT) {
        if (event_data && event_data_len > 0) {
            uint32_t copy_len = event_data_len < TRANSLATION_RESULT_SIZE - 1
                              ? event_data_len : TRANSLATION_RESULT_SIZE - 1;
            memcpy(trans.result, event_data, copy_len);
            trans.result[copy_len] = '\0';
            trans.status = LIS_TRANS_STATE_RESULT;
        }
    }
    if (event & TRANS_CB_EVENT_STATUS) {
        if (event_data && event_data_len >= sizeof(uint32_t)) {
            trans.status = *(uint32_t *)event_data;
        }
    }
}

lis_err_t lis_trans_start(char *txt, uint32_t txt_size, lis_trans_type type)
{
    if (txt == NULL || txt_size == 0) {
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail txt is NULL");
        return lis_err_err;
    }
    if (txt_size > TRANSLATION_INPUT_SIZE) {
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail txt_size(%d) is too large", txt_size);
        return lis_err_err;
    }

    xSemaphoreTake(trans.ctrl_sem, portMAX_DELAY);

    lis_err_t ret = lis_err_ok;
    int rc;

    rc = acomp_translation_set_res_type((int)type);
    if (rc != 0) {
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail: set_res_type %d", rc);
        ret = lis_err_err;
        goto out;
    }

    rc = acomp_translation_translate(txt, txt_size);
    if (rc != 0) {
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail: translate %d", rc);
        ret = lis_err_err;
    }

out:
    xSemaphoreGive(trans.ctrl_sem);
    return ret;
}

lis_err_t lis_trans_stop(void)
{
    lis_err_t ret = lis_err_ok;
    uint32_t now = xTaskGetTickCount();

    xSemaphoreTake(trans.ctrl_sem, portMAX_DELAY);

    int rc = acomp_translation_stop();
    if (rc != 0) {
        ret = lis_err_err;
        ESP_LOGE(TRANS_TAG, "lis_trans_stop fail");
    }

    xSemaphoreGive(trans.ctrl_sem);
    ESP_LOGI(TRANS_TAG, "stop cost:%d", xTaskGetTickCount() - now);
    return ret;
}

lis_trans_status lis_trans_get_status(void)
{
    return trans.status;
}

char *lis_trans_get_result(void)
{
    return trans.result;
}

void lis_trans_init(void)
{
    if (trans.inited) {
        ESP_LOGW(TRANS_TAG, "already inited");
        return;
    }
    memset(&trans, 0, sizeof(trans));

    trans.ctrl_sem = xSemaphoreCreateBinary();

#if TAOYUN_OS
    trans.result = os_mem_alloc(TRANSLATION_RESULT_SIZE);
#else
    trans.result = heap_caps_malloc(TRANSLATION_RESULT_SIZE, MALLOC_CAP_SPIRAM);
#endif

    if (!trans.result) {
        ESP_LOGE(TRANS_TAG, "lis_trans_init fail: alloc result");
        vSemaphoreDelete(trans.ctrl_sem);
        return;
    }

    /* acomp_translation_init() and prepare are deferred to lis_trans_prepare().
     * Callback is registered in acomp_translation_do_prepare(). */

    xSemaphoreGive(trans.ctrl_sem);
    trans.inited = 1;
    ESP_LOGI(TRANS_TAG, "lis_trans_init ok (deferred acomp init)");
}

/* XTTS cleanup for resource sharing (AP PSRAM is shared) */
extern int acomp_xtts_do_cleanup(void);

int lis_trans_prepare(void)
{
    /* Release XTTS to free shared pool */
    acomp_xtts_do_cleanup();

    return acomp_translation_do_prepare((trans_event_cb_t)trans_event_cb, NULL);
}

int lis_trans_cleanup(void)
{
    return acomp_translation_do_cleanup();
}

void lis_trans_deinit(void)
{
    if (!trans.inited) return;

    acomp_translation_remove_callback(trans_event_cb);
    acomp_translation_cleanup();

#if TAOYUN_OS
    os_mem_free(trans.result);
#else
    heap_caps_free(trans.result);
#endif
    trans.result = NULL;

    vSemaphoreDelete(trans.ctrl_sem);
    trans.inited = 0;
}

/* Translation test task */
#define TXT "融合位置编码是一种结合绝对位置编码和相对位置编码优点的位置编码方法。"
#define SIZE (sizeof(TXT) - 1)
void lis_trans_task(void)
{
    lis_trans_init();

    lis_trans_start(TXT, SIZE, LIS_TRANS_CN2EN);
    while (1) {
        int sta = lis_trans_get_status();
        if (LIS_TRANS_STATE_OVER == sta) break;
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    lis_trans_get_result();
    lis_trans_stop();

    lis_trans_deinit();
}
