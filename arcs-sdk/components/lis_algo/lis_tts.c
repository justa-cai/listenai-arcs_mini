#include <string.h>
#include "lis_tts.h"
#include "log_print.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "cache.h"

#define WAIT_CTRL_ACK (pdMS_TO_TICKS(2000))  // ms
#define PCM_MAX_RECV_LEN (320 * 10) // 最大的接收buf
#define TTS_TX_SIZE (10 * 1024)     // tts 文本

typedef struct
{
    uint8_t inited : 1;
    uint8_t started : 1;
    int pcm_len;
    int pcm_back;
    int comm_json_id;
    uint32_t handle;
    QueueHandle_t sync_que;       //
    SemaphoreHandle_t ctrl_sem;   //
    SemaphoreHandle_t txt_sem;    // tts文本发送完成信号量
    SemaphoreHandle_t pcm_rx_sem; // tts pcm
} tts_t;

static const char TTS_TAG[] = "tts";
static tts_t tts;

// 增强音量设置[0-10]
lis_err_t lis_tts_enhance_vol(int32_t vol)
{
    //char *pjstr;
    lis_err_t ret = lis_err_ok;

    if (vol < 0 || vol > 10)
    {
        ret = lis_err_err;
        ESP_LOGE(TTS_TAG, "enhance vol=%d, must be in[0-10]", vol);
        goto RET;
    }
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);

    xSemaphoreGive(tts.ctrl_sem);
RET:
    return ret;
}

// tts合成播放
lis_err_t lis_tts_start(char *txt, uint32_t txt_size, uint32_t speed, uint32_t vol, uint8_t role)
{
    //int ctrl_ack;
    lis_err_t ret = lis_err_ok;
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    func_ack_t ack;
    func_descriptors_t func = {
        .func = FUNC_XTTS_START_E,
        .xtts.buff = txt,
        .xtts.size = txt_size,
        .xtts.speed = speed,
        .xtts.vol = vol,
        .xtts.role = role,
    };
    HAL_FlushDCache_by_Addr((uint32_t *)func.xtts.buff, txt_size);
    if (lsf_cp2ap_func(tts.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        if (ack.status)
        {
            ret = lis_err_busy;
            ESP_LOGE(TTS_TAG, "lis_tts_start fail status: 0x%x", ack.status);
        }
    }
    else
    {
        ret = lis_err_err;
        ESP_LOGE(TTS_TAG, "lis_tts_start fail");
    }

    xSemaphoreGive(tts.ctrl_sem);
    return ret;
}
// 停止tts合成播放
lis_err_t lis_tts_stop(void)
{
    //int ctrl_ack;
    lis_err_t ret = lis_err_ok;
    uint32_t now = xTaskGetTickCount();

    // ESP_LOGI(TTS_TAG, "lis_tts_stop 1\n"); //用于测试
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    func_ack_t ack;
    func_descriptors_t func = {
        .func = FUNC_XTTS_STOP_E,
    };
    if (lsf_cp2ap_func(tts.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) < 0)
    {
        ret = lis_err_err;
        ESP_LOGE(TTS_TAG, "lis_tts_stop fail");
    }
    // ESP_LOGI(TTS_TAG, "lis_tts_stop 2\n");//用于测试
    xSemaphoreGive(tts.ctrl_sem);
    ESP_LOGI(TTS_TAG, "stop cost:%d", xTaskGetTickCount() - now);
    return ret;
}

lis_err_t lis_tts_set_param(int param, int param_value)
{
    lis_err_t ret = lis_err_ok;
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    func_ack_t ack;
    func_descriptors_t func = {
        .func = FUNC_XTTS_SET_PARAM_E,
        .xtts.param = param,
        .xtts.param_value = param_value,
    };
    if (lsf_cp2ap_func(tts.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        if (ack.status)
        {
            ret = lis_err_busy;
            ESP_LOGE(TTS_TAG, "lis_tts_set_param fail status: 0x%x", ack.status);
        }
    }
    else
    {
        ret = lis_err_err;
        ESP_LOGE(TTS_TAG, "lis_tts_set_param fail");
    }

    xSemaphoreGive(tts.ctrl_sem);
    return ret;
}

lis_err_t lis_tts_set_pcm_back(int is_back)
{
    lis_err_t ret = lis_err_ok;
    if (tts.pcm_back == is_back)
    {
        goto RET;
    }
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    tts.pcm_back = is_back;
    xSemaphoreGive(tts.ctrl_sem);
RET:
    return ret;
}

int lis_tts_get_pcm(char *buf, uint32_t buf_size)
{
    lis_err_t ret = lis_err_ok;

    // *buf_size = 0;
    if (NULL == buf)
    {
        return ret;
    }
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    extern int xtts_stream_frame_get(void *data, uint32_t size);
    ret = xtts_stream_frame_get(buf, buf_size);
    xSemaphoreGive(tts.ctrl_sem);
    return ret;
}

lis_err_t lis_tts_clear_pcm(void)
{
    lis_err_t ret = lis_err_ok;
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    extern int xtts_stream_frame_reset(void);
    ret = xtts_stream_frame_reset();
    xSemaphoreGive(tts.ctrl_sem);
    return ret;
}

// 获取tts的状态
lis_tts_status lis_tts_get_status(void)
{
    lis_tts_status st;
    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);
    func_ack_t ack;
    func_descriptors_t func = {
        .func = FUNC_XTTS_STATUS_E,
    };
    if (lsf_cp2ap_func(tts.handle, (uint8_t *)&func, sizeof(func_descriptors_t),
                                 (uint8_t *)&ack, sizeof(func_ack_t), WAIT_CTRL_ACK) > 0)
    {
        st = ack.status;
    }
    else
    {
        st = LIS_TTS_STATE_ERR;
        ESP_LOGE(TTS_TAG, "lis_tts_get_status timeout");
    }
    // ESP_LOGI(TTS_TAG, "lis_tts_get_status:%d\n", st);
    xSemaphoreGive(tts.ctrl_sem);
    return st;
}

void lis_tts_init(void)
{

    if (tts.inited)
    {
        ESP_LOGW(TTS_TAG, "already inited");
        return;
    }
    memset(&tts, 0, sizeof(tts));

    tts.handle = TYPE_TTS;
    tts.ctrl_sem = xSemaphoreCreateBinary();

    xSemaphoreGive(tts.ctrl_sem);
    tts.inited = 1;
    return;
}

void lis_tts_deinit(void)
{
    return;
}

void lis_tts_task(void)
{
#if 0
    #include "low_play.h"
    #include "lis_tts.h"
    #define TXT "上海把高标准高质量抓好主题教育作为一项重大政治任务。"
    low_play_init();
    low_play_cfg_t cfg = {.channel=1, .rate=24000, .bit=16, .vol=100};
    low_play_t *play = low_play_start(&cfg);
    lis_tts_init();
    lis_tts_start(TXT, strlen(TXT), 50, 50, 1);
    uint32_t size = 1024;
    char *buffer = (char *)os_mem_alloc(size);
    uint32_t count = 0;

    while (1)
    {
        int len = lis_tts_get_pcm(buffer, size);
        if(len > 0) {
            count += len;
            printf("count:%d len:%d\n", count, len);
            low_play_pcm_write(play, (int16_t *)buffer, len>>1);
        }

        int sta = lis_tts_get_status();
        if((LIS_TTS_STATE_TTS_END == sta) && (len == 0)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    printf("total:%d\n", count);
    vTaskDelay(1000);
    low_play_term();
    lis_tts_stop();

    lis_tts_deinit();

    if(buffer) os_mem_free(buffer);
#endif
}
