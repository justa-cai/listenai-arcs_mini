#include <string.h>
#include "lis_trans.h"
#include "log_print.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define TRANS_STOP_SYNC (1)
#define WAIT_CTRL_ACK (pdMS_TO_TICKS(500)) // ms
#define POLL_TIME (5 / portTICK_PERIOD_MS)
#define POLL_CNT (200)


typedef struct
{
    uint8_t inited : 1;
    uint8_t started : 1;
    int comm_json_id;
    char *result;
    uint32_t handle;
    QueueHandle_t sync_que;
    SemaphoreHandle_t ctrl_sem;
    SemaphoreHandle_t txt_sem; // trans文本发送完成信号量
} trans_t;

static const char TRANS_TAG[] = "trans";
static trans_t trans;

lis_err_t lis_trans_start(char *txt, uint32_t txt_size, lis_trans_type type)
{
    if(txt == NULL || txt_size == 0) {
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail txt is NULL");
        return lis_err_err;
    }
    if(txt_size > TRANSLATION_INPUT_SIZE) {
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail txt_size(%d) is too large", txt_size);
        return lis_err_err;
    }
    xSemaphoreTake(trans.ctrl_sem, portMAX_DELAY);
    
    lis_err_t ret = lis_err_ok;
    func_ack_t ack = {0};
    func_descriptors_t func = {
        .func = FUNC_TRANS_START_E,
        .trans.size = txt_size,
        .trans.type = type,
    };
    memcpy(func.trans.buff, txt, txt_size);
    if (lsf_cp2ap_func(trans.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        if (ack.status)
        {
            ret = lis_err_busy;
            ESP_LOGE(TRANS_TAG, "lis_trans_start fail status: 0x%x", ack.status);
        }
    }
    else
    {
        ret = lis_err_err;
        ESP_LOGE(TRANS_TAG, "lis_trans_start fail");
    }
    xSemaphoreGive(trans.ctrl_sem);
    return ret;
}

lis_err_t lis_trans_stop(void)
{
    lis_err_t ret = lis_err_ok;
    uint32_t now = xTaskGetTickCount();

    xSemaphoreTake(trans.ctrl_sem, portMAX_DELAY);
    func_ack_t ack;
    func_descriptors_t func = {
        .func = FUNC_TRANS_STOP_E,
    };
    if (lsf_cp2ap_func(trans.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(TRANS_TAG, "lis_trans_stop fail");
    }
    xSemaphoreGive(trans.ctrl_sem);
    ESP_LOGI(TRANS_TAG, "stop cost:%d", xTaskGetTickCount() - now);
    return ret;
}

lis_trans_status lis_trans_get_status(void)
{
    lis_trans_status st;
    xSemaphoreTake(trans.ctrl_sem, portMAX_DELAY);
    func_ack_t ack;
    func_descriptors_t func = {
        .func = FUNC_TRANS_STATUS_E,
    };
    if (lsf_cp2ap_func(trans.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        st = ack.status;
    }
    else
    {
        st = LIS_TRANS_STATE_ERR;
        ESP_LOGE(TRANS_TAG, "lis_trans_get_status fail");
    }
    // ESP_LOGI(TRANS_TAG, "trans status:%d", ack.status);
    xSemaphoreGive(trans.ctrl_sem);
    return st;
}

char *lis_trans_get_result(void)
{
    xSemaphoreTake(trans.ctrl_sem, portMAX_DELAY);
    func_descriptors_t func = {
        .func = FUNC_TRANS_RESULT_E,
    };
    func_ack_t ack;
    if (lsf_cp2ap_func(trans.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        memset(trans.result, 0, TRANSLATION_RESULT_SIZE);
        ESP_LOGE(TRANS_TAG, "lis_trans_get_status fail");
    } else {
        memcpy(trans.result, ack.result, TRANSLATION_RESULT_SIZE);
    }
    xSemaphoreGive(trans.ctrl_sem);
    CLOGD("result:%s\n", trans.result?:"");
    
    return trans.result;
}

void lis_trans_init(void)
{
    if (trans.inited)
    {
        ESP_LOGW(TRANS_TAG, "already inited");
        return;
    }
    memset(&trans, 0, sizeof(trans));

    trans.ctrl_sem = xSemaphoreCreateBinary();
    if(trans.result == NULL)
    {
#if TAOYUN_OS
        trans.result = os_mem_alloc(TRANSLATION_RESULT_SIZE);
#else
        trans.result = heap_caps_malloc(TRANSLATION_RESULT_SIZE, MALLOC_CAP_SPIRAM);
#endif
    }

    trans.handle = TYPE_TRANS;

    xSemaphoreGive(trans.ctrl_sem);
    trans.inited = 1;
    return;
}

void lis_trans_deinit(void)
{
#if TAOYUN_OS
            os_mem_free(trans.result);
#else
            heap_caps_free(trans.result);
#endif
    vSemaphoreDelete(trans.ctrl_sem);
    trans.inited = 0;
    return;
}

// 翻译测试任务
#define TXT "融合位置编码是一种结合绝对位置编码和相对位置编码优点的位置编码方法。"
#define SIZE (sizeof(TXT) - 1)
void lis_trans_task(void)
{
    lis_trans_init();

    lis_trans_start(TXT, SIZE, LIS_TRANS_CN2EN);
    while (1)
    {
        int sta = lis_trans_get_status();
        if(LIS_TRANS_STATE_OVER == sta) break;
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    lis_trans_get_result();
    lis_trans_stop();

    lis_trans_deinit();
}