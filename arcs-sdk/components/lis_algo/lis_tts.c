#include <string.h>
#include "lis_tts.h"
#include "log_print.h"
#include "task.h"
#include "semphr.h"
#include "acomp_xtts.h"

#define WAIT_CTRL_ACK (pdMS_TO_TICKS(2000))
#define PCM_MAX_RECV_LEN (320 * 10)
#define TTS_TX_SIZE (10 * 1024)

typedef struct
{
    uint8_t inited : 1;
    SemaphoreHandle_t ctrl_sem;
    lis_tts_status status;
} tts_t;

static const char TTS_TAG[] = "tts";
static tts_t tts = {
    .status = -1,
};

/* AP XTTS status event values (bitmask) → lis_tts_status mapping */
#define XTTS_ALGO_EVENT_PCM_BEGIN   (1UL << 0)  /* 0x1 */
#define XTTS_ALGO_EVENT_PCM_END     (1UL << 2)  /* 0x4 */
#define XTTS_ALGO_EVENT_PCM_STOP    (1UL << 3)  /* 0x8 */

/* acomp XTTS event callback (non-static, registered by acomp_xtts_do_prepare) */
void tts_event_cb(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    if (event & XTTS_CB_EVENT_STATUS) {
        if (event_data && event_data_len >= sizeof(uint32_t)) {
            uint32_t raw = *(uint32_t *)event_data;
            /* Map AP bitmask event to lis_tts_status enum */
            if (raw == XTTS_ALGO_EVENT_PCM_BEGIN) {
                tts.status = LIS_TTS_STATE_ING;
            } else if (raw == XTTS_ALGO_EVENT_PCM_END) {
                tts.status = LIS_TTS_STATE_TTS_END;
            } else if (raw == XTTS_ALGO_EVENT_PCM_STOP) {
                tts.status = LIS_TTS_STATE_EARLY_OVER;
            }
        }
    }
}

lis_err_t lis_tts_enhance_vol(int32_t vol)
{
    lis_err_t ret = lis_err_ok;

    if (vol < 0 || vol > 10) {
        ESP_LOGE(TTS_TAG, "enhance vol=%d, must be in[0-10]", vol);
        return lis_err_err;
    }
    /* stub - kept as-is, no real implementation */
    return ret;
}

uint32_t app_tts_speed = 0;
void app_misc_set_tts_speed(uint32_t speed)
{
    app_tts_speed = speed;
}

lis_err_t lis_tts_start(char *txt, uint32_t txt_size, uint32_t speed, uint32_t vol, uint8_t role)
{
    lis_err_t ret = lis_err_ok;
    uint32_t actual_speed = (app_tts_speed > 0) ? app_tts_speed : speed;

    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);

    ESP_LOGI(TTS_TAG, "lis_tts_start txt_size:%d, speed:%d, vol:%d, role:%d\n",
             txt_size, actual_speed, vol, role);

    acomp_xtts_set_speed((int)actual_speed);
    acomp_xtts_set_volume((int)vol);
    acomp_xtts_set_role((int)role);

    int rc = acomp_xtts_synth_text(txt, txt_size);
    if (rc != 0) {
        ret = lis_err_err;
        ESP_LOGE(TTS_TAG, "lis_tts_start fail: acomp_xtts_synth_text %d", rc);
    }

    xSemaphoreGive(tts.ctrl_sem);
    return ret;
}

lis_err_t lis_tts_stop(void)
{
    lis_err_t ret = lis_err_ok;
    uint32_t now = xTaskGetTickCount();

    xSemaphoreTake(tts.ctrl_sem, portMAX_DELAY);

    int rc = acomp_xtts_stop();
    if (rc != 0) {
        ret = lis_err_err;
        ESP_LOGE(TTS_TAG, "lis_tts_stop fail");
    }

    xSemaphoreGive(tts.ctrl_sem);
    ESP_LOGI(TTS_TAG, "stop cost:%d", xTaskGetTickCount() - now);
    return ret;
}

/* TODO: stub - new architecture uses set_speed/volume/role directly */
lis_err_t lis_tts_set_param(int param, int param_value)
{
    ESP_LOGW(TTS_TAG, "lis_tts_set_param: stub, use set_speed/volume/role instead");
    return lis_err_ok;
}

lis_err_t lis_tts_set_pcm_back(int is_back)
{
    /* Kept for API compatibility; no internal pcm_back field used in new arch */
    return lis_err_ok;
}

int lis_tts_get_pcm(char *buf, uint32_t buf_size)
{
    if (NULL == buf) return 0;

    uint32_t len = 0;
    uint16_t idx = 0;
    void *ptr = acomp_xtts_stream_rx_buffer_get(0, &len, &idx);
    if (ptr == NULL || len == 0) return 0;

    uint32_t copy_len = len < buf_size ? len : buf_size;
    memcpy(buf, ptr, copy_len);
    acomp_xtts_stream_rx_buffer_release(0, idx, len, ptr);

    return (int)copy_len;
}

lis_err_t lis_tts_clear_pcm(void)
{
    uint32_t len = 0;
    uint16_t idx = 0;

    /* Drain all remaining RX buffers */
    while (1) {
        void *ptr = acomp_xtts_stream_rx_buffer_get(0, &len, &idx);
        if (ptr == NULL || len == 0) break;
        acomp_xtts_stream_rx_buffer_release(0, idx, len, ptr);
    }
    return lis_err_ok;
}

lis_tts_status lis_tts_get_status_local(void)
{
    return tts.status;
}

lis_tts_status lis_tts_get_status(void)
{
    return tts.status;
}

void lis_tts_init(void)
{
    if (tts.inited) {
        ESP_LOGW(TTS_TAG, "already inited");
        return;
    }
    memset(&tts, 0, sizeof(tts));

    tts.ctrl_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(tts.ctrl_sem);

    /* acomp_xtts_init() 和 prepare 由 acomp_xtts_do_prepare() 按需调用。
     * 这里只初始化 lis_tts 内部状态。
     * callback 在 acomp_xtts_do_prepare() 中注册。 */

    tts.status = -1;
    tts.inited = 1;
    ESP_LOGI(TTS_TAG, "lis_tts_init ok (deferred acomp init)");
}

/* CV/Translation cleanup for resource sharing (AP PSRAM is shared) */
extern int acomp_cv_cleanup(void);
extern int acomp_translation_do_cleanup(void);

int lis_tts_prepare(void)
{
    /* Release other algorithm resources to free AP PSRAM for XTTS */
    ESP_LOGI(TTS_TAG, "cleanup CV/Trans to free AP PSRAM");
    acomp_cv_cleanup();
    acomp_translation_do_cleanup();

    return acomp_xtts_do_prepare((xtts_event_cb_t)tts_event_cb, NULL);
}

int lis_tts_cleanup(void)
{
    return acomp_xtts_do_cleanup();
}

void lis_tts_deinit(void)
{
    if (!tts.inited) return;

    acomp_xtts_remove_callback(tts_event_cb);
    acomp_xtts_cleanup();
    vSemaphoreDelete(tts.ctrl_sem);
    tts.inited = 0;
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
