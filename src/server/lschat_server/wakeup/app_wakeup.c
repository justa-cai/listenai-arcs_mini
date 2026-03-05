#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lisa_device.h"
#include "lisa_audio.h"

#include "cJSON.h"
#include "voice_msg.h"
#include "acomp_wakeup.h"
#include "voice_cloud.h"
#include "app_wakeup_debug.h"
#include "app_wakeup.h"

#define TAG "app_wakeup"
#include "lisa_log.h"
#include "shell.h"
#ifdef CONFIG_BOARD_ARCS_MINI
#include "lisa_kv.h"
#include "kv_user.h"
#include "cmd/cmd.h"
#include "Driver_ADC_PDM.h"
#include "Driver_DAC.h"
#endif // CONFIG_BOARD_ARCS_MINI

/* Audio parameters */
#define SAMPLE_RATE         LISA_AUDIO_RATE_16K
#define SAMPLE_BITS         LISA_AUDIO_BIT_16


#define BUFFER_COUNT        12
#define BUFFER_SAMPLES      256

#ifdef CONFIG_BOARD_ARCS_MINI
static bool s_wakeup_started = false;
#endif
static bool s_audio_inited = false;

lisa_device_t *g_audio_dev = NULL;
#ifdef CONFIG_BOARD_ARCS_MINI
static int32_t g_record_mic_analog_gain = CONFIG_AUDIO_DEFAULT_MIC_GAIN_A_DB;
static int32_t g_record_mic_digital_gain = CONFIG_AUDIO_DEFAULT_MIC_GAIN_D_DB;
static int32_t g_record_ref_analog_gain = CONFIG_AUDIO_DEFAULT_REF_GAIN_A_DB;
static int32_t g_record_ref_digital_gain = CONFIG_AUDIO_DEFAULT_REF_GAIN_D_DB;
static int32_t g_play_analog_gain = CONFIG_AUDIO_DEFAULT_SPK_GAIN_A_DB;
static int32_t g_play_digital_gain = CONFIG_AUDIO_DEFAULT_SPK_GAIN_D_DB;
#else  // !CONFIG_BOARD_ARCS_MINI
/* Record gain configuration - Default values */
static int32_t g_record_analog_gain = 36;      /* 30 dB */
static int32_t g_record_digital_gain = 0;       /* 0 dB */

/* Play gain configuration - Default values */
static int32_t g_play_analog_gain = 6;       /* 6 dB */
static int32_t g_play_digital_gain = -20;
#endif // CONFIG_BOARD_ARCS_MINI

static app_wakeup_sensitivity_level_e s_sensitivity_level = APP_WAKEUP_SENSITIVITY_LEVEL_2;

#define WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX (0)
#define WAKEUP_AUDIO_MIX2CH_STREAM_CH_CNAME "stream.mix2ch"
#define WAKEUP_AUDIO_OUT_STREAM_CH_INDEX (1)
#define WAKEUP_AUDIO_OUT_STREAM_CH_CNAME "stream.tocloud"
#define WAKEUP_AUDIO_OUT_STREAM    (1)
#define WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO   (1)

typedef struct {
    short mic1;
    short mic2;
}mic_in_t;

typedef struct {
    short ref;
}ref_in_t;

#if CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC
typedef struct {
    short mic1;
    short mic2;
    short ref1;
    short ref2;
}mic_ref_in_t;
#else
typedef struct {
    short mic1;
    short ref1;
}mic_ref_in_t;
#endif

static mic_ref_in_t *mic_ref_in = NULL;
static const uint8_t echo_zero[CONFIG_AUDIO_STEP_SAMPS * sizeof(ref_in_t)] = {0};
static SemaphoreHandle_t rx_sem;

__attribute__((weak)) bool is_wakeup_keyword(char *keyword)
{
    return false;
}

/** 是否为数字 */
static bool is_digit(char str) { return (str >= '0' && str <= '9'); }

/** 是否为特殊字符 */
static bool is_special(char str) { return (str == '\r'); }

/**
 * 移除字符串中的数字
 * @param input     待处理字符串
 * @return          处理后的字符串
 */
static char* __remove_digits_and_special(char* input)
{
    char* dest = input;
    char* src = input;

    while(*src) {
        if(is_digit(*src) || is_special(*src)) { src++; continue; }
        *dest++ = *src++;
    }
    *dest = '\0';
    return input;
}

static int app_algo_keyword_and_kid_extract(const uint8_t *const in, uint8_t *out, int max_len)
{
	int ret = -1;
	if (in && out) {
		cJSON *root = cJSON_Parse(in);
		if (root) {
			cJSON *rlt_json = cJSON_GetObjectItem(root, "rlt");
			if (rlt_json) {
				if (cJSON_GetArraySize(rlt_json) > 0) {
					cJSON *rlt0_json = cJSON_GetArrayItem(rlt_json, 0);
					if (rlt0_json) {
						cJSON *key_json = cJSON_GetObjectItem(rlt0_json, "keyword");
						cJSON *kid_json = cJSON_GetObjectItem(rlt0_json, "iresid");
						cJSON *threshlod_json = cJSON_GetObjectItem(rlt0_json, "ncm");
						if (key_json && threshlod_json) {
							char *proc_keyword = __remove_digits_and_special(key_json->valuestring);
							if (proc_keyword) {
								ret = 0;
								const int kw_len = strlen(proc_keyword);
								if (kw_len < max_len) {
									memcpy(out, proc_keyword, kw_len + 1);
								} else {
									strncpy(out, proc_keyword, max_len);
								}
							}
						}
					}
				}
			}
			cJSON_Delete(root);
		}
	}

	return ret;
}

static void wakeup_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    uint8_t keyword[64] = {0};
    int ret;

    if (event & WAKEUP_CB_EVENT_ENGINE_RLT) {
        LISA_LOGI(TAG, "wakeup result:%d,%s", event_data_len, (char *)event_data);
        ret = app_algo_keyword_and_kid_extract(event_data, keyword, sizeof(keyword) - 1);
        if (ret == 0) {
            if(is_wakeup_keyword(keyword)){
                voice_msg_pub(VOICE_MSG_WAKEUP_KEYWORD, keyword, strlen(keyword) + 1);
            }
            else{
                voice_msg_pub(VOICE_MSG_WAKEUP_COMMAND, keyword, strlen(keyword) + 1);
            }
        }
    } else if (event & WAKEUP_CB_EVENT_STREAM_UPDATE) {
        // LISA_LOGI(TAG, "wakeup stream update data:%p, len:%d", event_data, event_data_len);
        
#ifdef WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        /* 自动触发模式：发送信号量通知接收线程 */
        if (rx_sem != NULL) {
            // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            // xSemaphoreGiveFromISR(rx_sem, &xHigherPriorityTaskWoken);
            // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

            xSemaphoreGive(rx_sem);
        }
#endif

    } else if (event & WAKEUP_CB_EVENT_ENGINE_TIMEOUT) {
        LISA_LOGI(TAG, "wakeup timeout");

    } else if (event & WAKEUP_CB_EVENT_ENGINE_ANGLE) {
        for (int i = 0; i < event_data_len/sizeof(short); i++) {
            short angle = *((short*)event_data + i);
            LISA_LOGD(TAG, "wakeup angle[%d]: %d", i, angle);
        }
    } else if(event & WAKEUP_CB_EVENT_ENGINE_SWITCH_MODE){
        acomp_wakeup_algo_mode_e mode = *((acomp_wakeup_algo_mode_e*)event_data);
        static acomp_wakeup_algo_mode_e prev_mode = -1;
        if(mode != prev_mode){
            if(mode == ACOMP_WAKEUP_ALGO_MODE_WAKEUP){
                voice_msg_pub(VOICE_MSG_WAKEUP_RECOGNIZED_FINISH, NULL, 0);
            }
            else{
                voice_msg_pub(VOICE_MSG_WAKEUP_RECOGNIZED_START, NULL, 0);
            }
            LISA_LOGI(TAG, "wakeup switch mode to:%d", mode);
            prev_mode = mode;}
    }
    else {
        LISA_LOGW(TAG, "unknown wakeup event:0X%X", event);
    }
}

static void ref_data_scale(ref_in_t *ref_data, int sample_cnt, int db)
{

	double gain = pow(10.0, (double)(db / 20.0));
	int16_t *sample = (int16_t *)ref_data;
	for(uint32_t i = 0; i  < sample_cnt; i++){	
		int32_t temp = sample[i] * gain;
		// 处理溢出
        if (temp > SHRT_MAX) {
            sample[i] = SHRT_MAX;
        } else if (temp < SHRT_MIN) {
            sample[i] = SHRT_MIN;
        } else {
			sample[i] = (int16_t)temp;
		}
	}
}

static void stream_data_fusion(mic_ref_in_t *algo_buf_in, mic_in_t *adc_data, ref_in_t *ref_data, int sample_cnt)
{
    for (int i = 0; i < sample_cnt; i++) {
        // 双麦软回采
#if CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC    
        algo_buf_in[i].mic1 = adc_data[i].mic1;
        algo_buf_in[i].mic2 = adc_data[i].mic2;
        algo_buf_in[i].ref1 = ref_data[i].ref;
        algo_buf_in[i].ref2 = ref_data[i].ref;
#else   // 单麦硬回采
        algo_buf_in[i].mic1 = adc_data[i].mic1;
        algo_buf_in[i].ref1 = ref_data[i].ref;
#endif
    }

}

static void audio_stream_callback(const lisa_audio_event_t *event, void *user_data)
{
    int len;
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;

    bool has_record = (event->record_buffer != NULL && event->record_samples > 0);
    bool has_echo = (event->echo_buffer != NULL && event->echo_samples > 0);

    if((has_record && has_echo) != (has_record || has_echo)) {
        LISA_LOGE(LOG_TAG, "Invalid audio event: record=%d, echo=%d",
                  has_record ? 1 : 0, has_echo ? 1 : 0);
        return;

    }

    buffer = acomp_wakeup_stream_tx_buffer_alloc(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, &buf_size, &desc_idx);

    if(buffer && buf_size > 0){
        stream_data_fusion((mic_ref_in_t*)buffer, (mic_in_t*)event->record_buffer, (ref_in_t*)event->echo_buffer, BUFFER_SAMPLES);
        acomp_wakeup_stream_tx_buffer_submit(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
    } else {
        LOGE("acomp_wakeup_stream_tx_buffer_alloc failed");
    }
}


/**
 * @brief Configure and start recording
 */
static int start_recording(lisa_device_t *audio_dev)
{
    int ret;

    /* Configure recording parameters */
    lisa_audio_record_config_t record_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = LISA_AUDIO_CH_STEREO,
            .sample_bits = SAMPLE_BITS,
        },
#ifdef CONFIG_BOARD_ARCS_MINI
        .gain_l = {
            .analog_gain = g_record_mic_analog_gain,
            .digital_gain = g_record_mic_digital_gain,
        },
        .gain_r = {
            .analog_gain = g_record_ref_analog_gain,
            .digital_gain = g_record_ref_digital_gain,
        },
#else // !CONFIG_BOARD_ARCS_MINI
        .gain = {
            .analog_gain = g_record_analog_gain,
            .digital_gain = g_record_digital_gain,
        },
#endif // CONFIG_BOARD_ARCS_MINI
        .differential_input = true,
        .enable_hpf = true,

    };

    ret = lisa_audio_record_config(audio_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Record config failed: %d", ret);
        return ret;
    }

    /* Start recording */
    ret = lisa_audio_record_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Record start failed: %d", ret);
        return ret;
    }

    LISA_LOGI(LOG_TAG, "Recording started");
    return LISA_DEVICE_OK;
}


/**
 * @brief Configure and start playback
 */
static int start_playback(lisa_device_t *audio_dev)
{
    int ret;

    /* Configure playback parameters */
    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = LISA_AUDIO_CH_LEFT,
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = g_play_analog_gain,
            .digital_gain = g_play_digital_gain,
        },
        .buffer_count = BUFFER_COUNT,
        .buffer_samples = BUFFER_SAMPLES,
    };

    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Play config failed: %d", ret);
        return ret;
    }

    /* Start playback */
    ret = lisa_audio_play_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Play start failed: %d", ret);
        return ret;
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    LISA_LOGI(LOG_TAG, "Playback started (Echo collection enabled)");
#else
    LISA_LOGI(LOG_TAG, "Playback started");
#endif

    return LISA_DEVICE_OK;
}

static int audio_init(void)
{
    int ret;
    lisa_device_t *audio_dev = NULL;

    if (s_audio_inited) {
        return 0;
    }

    LISA_LOGI(TAG, "Fused format: 4-channel (mic0, mic1, ref0, ref1)");

    /* Initialize audio device */
    
    GPDMA_Initialize();

    /* Get audio device */
    audio_dev = lisa_device_get("audio0");
    if (!audio_dev) {
        LISA_LOGE(TAG, "Failed to get audio device");
        return NULL;
    }
    LISA_LOGI(TAG, "Audio device acquired");

 
#ifdef CONFIG_BOARD_ARCS_MINI
    int gain_val = 0;
    if (lisa_kv_get_int(KV_KEY_USER_MIC_GAIN_DB, &gain_val) == 0) {
        if (gain_val < ADC_PDM_GAIN_A_MIN_DB) gain_val = ADC_PDM_GAIN_A_MIN_DB;
        if (gain_val > ADC_PDM_GAIN_A_MAX_DB) gain_val = ADC_PDM_GAIN_A_MAX_DB;
        g_record_mic_analog_gain = gain_val;
    }
    if (lisa_kv_get_int(KV_KEY_USER_AEC_GAIN_DB, &gain_val) == 0) {
        if (gain_val < ADC_PDM_GAIN_A_MIN_DB) gain_val = ADC_PDM_GAIN_A_MIN_DB;
        if (gain_val > ADC_PDM_GAIN_A_MAX_DB) gain_val = ADC_PDM_GAIN_A_MAX_DB;
        g_record_ref_analog_gain = gain_val;
    }
    if (lisa_kv_get_int(KV_KEY_USER_SPK_GAIN_DB, &gain_val) == 0) {
        if (gain_val < DAC_GAIN_A_MIN_DB) gain_val = DAC_GAIN_A_MIN_DB;
        if (gain_val > DAC_GAIN_A_MAX_DB) gain_val = DAC_GAIN_A_MAX_DB;
        g_play_analog_gain = gain_val;
    }
#endif // CONFIG_BOARD_ARCS_MINI

    /* Start recording hardware (but data collection controlled by flag) */
    if (start_recording(audio_dev) != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to start recording");
        lisa_audio_unregister_callback(audio_dev, audio_stream_callback);
        return -2;
    }

    /* Start playback */
    if (start_playback(audio_dev) != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to start playback");
        return -3;
    }
    /* 设置相位补偿 */
    lisa_audio_phase_compensation_t phase_comp = {
        .record_skip_samples = 0,
        .echo_skip_samples = 0,
    };

    ret = lisa_audio_set_phase_compensation(audio_dev, &phase_comp);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "设置相位补偿失败: %d", ret);
        return ret;
    }
    g_audio_dev = audio_dev;
    s_audio_inited = true;
    LISA_LOGI(TAG, "相位补偿已设置");

    LISA_LOGI(TAG, "System running...");
    LISA_LOGI(TAG, "Use 'start_record' and 'stop_record' commands to control");

   
    return 0;
}

static void wakeup_in_task(void *pvParameters)
{
    int ret = 0;
    mic_in_t *adc_data = NULL;
    ref_in_t *ref_data = NULL;
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    static int first = 1;

    GPDMA_Initialize();
    audio_init();
    while(1){
        vTaskDelay(pdMS_TO_TICKS(5));
    }
#if 0
    lite_dac_init();
    dac_aud_t aud = {.rate=16000};
    lite_dac_ctrl(ADAC_CTRL_AUD_CFG, &aud);

    dac_gain_t gain_dac = {.a_gain=6, .d_gain=-20};
    lite_dac_ctrl(ADAC_CTRL_VOLUME, &gain_dac);

    lite_adc_init();
    struct {
        int al;
        int ar;
        int dl;
        int dr;
    } gain = {36,36, 0,0};
    lite_adc_ctrl(MAPI_AADC_CTRL_SET_GAIN, &gain);
    
    lite_dac_ctrl(ADAC_CTRL_START, NULL);
    lite_adc_ctrl(MAPI_AADC_CTRL_REC_START, NULL);

    while (1) {
#if CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC
        if (first) {
            ret = lite_dac_get_echo_buf((uint16_t **)&ref_data, portMAX_DELAY);
            first = 0;
        }
#endif
        ret = lite_adc_read(&adc_data, CONFIG_AUDIO_STEP_SAMPS * sizeof(mic_in_t), portMAX_DELAY);
        if (ret == 0) {
            LISA_LOGE(TAG, "wakeup stream lite_adc_read failed");
            continue;
        }
#if CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC
        ret = lite_dac_get_echo_buf((uint16_t **)&ref_data, portMAX_DELAY);
       if (ret != CONFIG_AUDIO_STEP_SAMPS) {
            ref_data = (ref_in_t *)echo_zero;
        }
#endif
        buffer = acomp_wakeup_stream_tx_buffer_alloc(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, &buf_size, &desc_idx);

        if(buffer && buf_size > 0){
            stream_data_fusion((mic_ref_in_t*)buffer, adc_data, ref_data, CONFIG_AUDIO_STEP_SAMPS);
            acomp_wakeup_stream_tx_buffer_submit(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
        } else {
            LOGE("acomp_wakeup_stream_tx_buffer_alloc failed");
        }
    }
#endif
}
#if WAKEUP_AUDIO_OUT_STREAM
static void wakeup_out_stream_to_cloud(uint8_t *data, int len)
{
#define UAS_REC_FRM_SAMPS         256
#define UAS_REC_CHANNELS          5
#define LS_RECORD_REC_CHANNEL_NUM (UAS_REC_CHANNELS - 1)
#define LS_RECORD_ONE_CHNNEL_SIZE (UAS_REC_FRM_SAMPS * sizeof(short))

    for (int j = 0; j < len / (LS_RECORD_ONE_CHNNEL_SIZE * UAS_REC_CHANNELS); j++) {
        short rec_buf[UAS_REC_FRM_SAMPS] = {0};
        short(*uac_rec)[UAS_REC_CHANNELS] =
            (short(*)[UAS_REC_CHANNELS])((uint8_t *)data + LS_RECORD_ONE_CHNNEL_SIZE * UAS_REC_CHANNELS * j);
        for (int i = 0; i < UAS_REC_FRM_SAMPS; i++) {
            rec_buf[i] = uac_rec[i][3];
        }
    
        voice_cloud_chat_send_audio((uint8_t *)rec_buf, LS_RECORD_ONE_CHNNEL_SIZE);
    }
}

static void wakeup_out_task(void *pvParameters)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    int ret;

    while (1) {

#ifdef WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        /* 自动触发模式：等待来自回调的信号量 */
        xSemaphoreTake(rx_sem, pdMS_TO_TICKS(50));
#else
        /* 手动触发模式：等待超时时间 */
        vTaskDelay(pdMS_TO_TICKS(5));
#endif
        /* 从 R2M 流中获取数据 */
        while (1) {
            buffer = acomp_wakeup_stream_rx_buffer_get(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX, &len, &desc_idx);
            if (buffer != NULL && len > 0) {

                LISA_LOGD(TAG, "acomp_wakeup_stream_rx_buffer_get ok");

                wakeup_out_stream_to_cloud(buffer, len);

                wakeup_stream_debug_data_output(buffer, len);

                /* 释放缓冲区 */
                ret = acomp_wakeup_stream_rx_buffer_release(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX, desc_idx, len, buffer);
                if (ret != ACOMP_ERR_OK) {
                    LISA_LOGW(TAG, "Failed to release buffer: %d", ret);
                }
            } else {
                /* 没有更多数据，跳出内层循环 */
                break;
            }
        }
    }

    vTaskDelete(NULL);
}
#endif
int app_wakeup_init(struct wakeup_algo_resources *algo_res)
{
    int ret = 0;
    bool wakeup_res_valid = false;

    ret = audio_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "Audio init failed: %d", ret);
        return ret;
    }

    wakeup_res_valid = (algo_res != NULL && algo_res->mlp.addr != NULL && algo_res->mlp.size > 0 &&
                        algo_res->wrap.addr != NULL && algo_res->wrap.size > 0);
    if (!wakeup_res_valid) {
        LISA_LOGW(TAG, "Wakeup resources invalid, skip wakeup engine init");
#if CONFIG_BOARD_ARCS_MINI
        s_wakeup_started = false;
#endif
        return 0;
    }

    acomp_stream_chn_create_desc_t desc = {
        .cname = WAKEUP_AUDIO_MIX2CH_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX,
#if CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC
        .buffer_size = 256 * 4 * 2,
#else
        .buffer_size = 256 * 2 * 2,
#endif
        .num_descs = 4,
        .kick_policy = 1,
    };
#if WAKEUP_AUDIO_OUT_STREAM
    acomp_stream_chn_create_desc_t rx_desc = {
        .cname = WAKEUP_AUDIO_OUT_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = WAKEUP_AUDIO_OUT_STREAM_CH_INDEX,
        .buffer_size = 15360,
        .num_descs = 8,
#if WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        .kick_policy = 1,  /* 手动触发 */
#else
        .kick_policy = 0,  /* 手动触发 */
#endif
    };
#if WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
    rx_sem = xSemaphoreCreateBinary();
    if (rx_sem == NULL) {
        LISA_LOGE(TAG, "Failed to create rx semaphore");
    }
#endif
#endif
    acomp_wakeup_init();
    acomp_wakeup_add_callback(    WAKEUP_CB_EVENT_ENGINE_RLT 
                                | WAKEUP_CB_EVENT_STREAM_UPDATE 
                                | WAKEUP_CB_EVENT_ENGINE_TIMEOUT 
                                | WAKEUP_CB_EVENT_ENGINE_ANGLE
                                | WAKEUP_CB_EVENT_ENGINE_SWITCH_MODE,
                           wakeup_event_handler, NULL);

    acomp_ipc_prepare_t *prepare;
    uint32_t size;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_WAKEUP_RES_NUMBER;
    size = ALIGN_SIZE(size);

    prepare = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (prepare == NULL) {
        LOGE("prepare mem alloc failed");
        return -1;
    }

    prepare->number = ACOMP_WAKEUP_RES_NUMBER;
    prepare->item[0].index = WAKEUP_INDEX_CAE_ESR_MLP;
    prepare->item[0].addr = algo_res->mlp.addr;
    prepare->item[0].offset = 0;
    prepare->item[0].size = algo_res->mlp.size;

    prepare->item[1].index = WAKEUP_INDEX_AI_WRAP;
    prepare->item[1].addr = algo_res->wrap.addr;
    prepare->item[1].offset = 0;
    prepare->item[1].size = algo_res->wrap.size;

    ret = acomp_wakeup_prepare_with_config(prepare);
    LISA_LOGI(TAG, "acomp_wakeup_prepare ret:%d", ret);
    ret = acomp_wakeup_start();
    LISA_LOGI(TAG, "acomp_wakeup_start ret:%d", ret);

    ret = acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);
    LISA_LOGI(TAG, "acomp_wakeup_set_algo_mode ret:%d", ret);

    acomp_wakeup_stream_ch_enable(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX,&desc);
#if WAKEUP_AUDIO_OUT_STREAM
    acomp_wakeup_stream_ch_enable(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX,&rx_desc);
#endif
    mic_ref_in = (mic_ref_in_t *)psram_malloc(sizeof(mic_ref_in_t) * CONFIG_AUDIO_STEP_SAMPS);
    if(mic_ref_in == NULL)
    {
        LISA_LOGE(TAG,"mic_ref_in psram_malloc_align failed");
    }

#if WAKEUP_AUDIO_OUT_STREAM
    ret = xTaskCreate(wakeup_out_task, "wakeup_out_task", 4096, NULL, 9, NULL);
#endif

    lisa_device_t *audio_dev = NULL;

    /* Get audio device */
    audio_dev = lisa_device_get("audio0");
    if (!audio_dev) {
        LISA_LOGE(TAG, "Failed to get audio device");
        return -1;
    }
    LISA_LOGI(TAG, "Audio device acquired");
 
    /* Register unified callback */
    ret = lisa_audio_register_callback(audio_dev, audio_stream_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to register callback: %d", ret);
        return -1;
    }
    LISA_LOGI(TAG, "Audio callback registered");

    if (!audio_dev) {
        LISA_LOGE(TAG, "Failed to initialize audio device");
        return -1;
    }

#if CONFIG_BOARD_ARCS_MINI
    s_wakeup_started = true;
#endif

    return ret;
}

#ifdef CONFIG_BOARD_ARCS_MINI
int app_wakeup_stop(void)
{
    if (!s_wakeup_started) {
        return 0;
    }
    
    acomp_wakeup_stop();
    s_wakeup_started = false;

    return 0;
}
#endif

int app_wakeup_sensitivity_level_set(app_wakeup_sensitivity_level_e level){
    
    s_sensitivity_level = level;
    LISA_LOGI(TAG,"app_wakeup_sensitivity_level_set %d",level);
}

app_wakeup_sensitivity_level_e app_wakeup_sensitivity_level_get(void){

    return s_sensitivity_level;
}

static int shell_algo_restart(int argc, char **argv)
{
    if (argc < 2) {
        return -1;
    }
    Shell* shell = shellGetCurrent();
    if (strncmp(argv[1], "1", 1) == 0) {
        shellPrint(shell, "algo restart 1\r\n");

        int ret = acomp_wakeup_stop();
        shellPrint(shell, "acomp_wakeup_stop ret:%d\r\n", ret);

        ret = acomp_wakeup_start();
        shellPrint(shell, "acomp_wakeup_start ret:%d\r\n", ret);

        ret = acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);
        shellPrint(shell, "acomp_wakeup_set_algo_mode ret:%d\r\n", ret);
    } else {
        shellPrint(shell, "algo restart error\r\n");
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, algo_restart,
                 shell_algo_restart, "algo_restart 1/2");

#ifdef CONFIG_BOARD_ARCS_MINI
static int gain_cmd_set_mic(int argc, char **argv);
static int gain_cmd_set_aec(int argc, char **argv);
static int gain_cmd_set_spk(int argc, char **argv);
static int gain_cmd_print(int argc, char **argv);
static int gain_cmd_help(int argc, char **argv);

static const struct listen_cmd_t g_gain_cmds[] = {
    {"set_mic", gain_cmd_set_mic, "Set gain for microphone signal"},
    {"set_aec", gain_cmd_set_aec, "Set gain for AEC reference signal"},
    {"set_spk", gain_cmd_set_spk, "Set gain for speaker"},
    {"print", gain_cmd_print, "Print current gain settings"},
    {"help", gain_cmd_help, "Show help information"},
};

static int gain_cmd_set_mic(int argc, char **argv)
{
    if (argc != 1) {
        shellPrint(shellGetCurrent(), "Usage: gain set_mic [gain_db] (valid range: %d to %d dB)\n", ADC_PDM_GAIN_A_MIN_DB, ADC_PDM_GAIN_A_MAX_DB);
        return -1;
    }

    int gain_db = atoi(argv[1]);
    if (gain_db < ADC_PDM_GAIN_A_MIN_DB || gain_db > ADC_PDM_GAIN_A_MAX_DB) {
        shellPrint(shellGetCurrent(), "Error: gain_db out of range (%d to %d dB)\n", ADC_PDM_GAIN_A_MIN_DB, ADC_PDM_GAIN_A_MAX_DB);
        return -1;
    }

    g_record_mic_analog_gain = gain_db;

    lisa_audio_gain_t gain[] = {
        {
            .analog_gain = g_record_mic_analog_gain,
            .digital_gain = g_record_mic_digital_gain,
        },
        {
            .analog_gain = g_record_ref_analog_gain,
            .digital_gain = g_record_ref_digital_gain,
        },
    };
    lisa_audio_record_set_gain(g_audio_dev, gain);

    shellPrint(shellGetCurrent(), "Microphone analog gain set to %d dB\n", gain_db);

    return 0;
}

static int gain_cmd_set_aec(int argc, char **argv)
{
    if (argc != 1) {
        shellPrint(shellGetCurrent(), "Usage: gain set_aec [gain_db] (valid range: %d to %d dB)\n", ADC_PDM_GAIN_A_MIN_DB, ADC_PDM_GAIN_A_MAX_DB);
        return -1;
    }

    int gain_db = atoi(argv[1]);
    if (gain_db < ADC_PDM_GAIN_A_MIN_DB || gain_db > ADC_PDM_GAIN_A_MAX_DB) {
        shellPrint(shellGetCurrent(), "Error: gain_db out of range (%d to %d dB)\n", ADC_PDM_GAIN_A_MIN_DB, ADC_PDM_GAIN_A_MAX_DB);
        return -1;
    }

    g_record_ref_analog_gain = gain_db;

    lisa_audio_gain_t gain[] = {
        {
            .analog_gain = g_record_mic_analog_gain,
            .digital_gain = g_record_mic_digital_gain,
        },
        {
            .analog_gain = g_record_ref_analog_gain,
            .digital_gain = g_record_ref_digital_gain,
        },
    };
    lisa_audio_record_set_gain(g_audio_dev, gain);

    shellPrint(shellGetCurrent(), "AEC reference analog gain set to %d dB\n", gain_db);

    return 0;
}

static int gain_cmd_set_spk(int argc, char **argv)
{
    if (argc != 1) {
        shellPrint(shellGetCurrent(), "Usage: gain set_spk [gain_db] (valid range: %d to %d dB)\n", DAC_GAIN_A_MIN_DB, DAC_GAIN_A_MAX_DB);
        return -1;
    }

    int gain_db = atoi(argv[1]);
    if (gain_db < DAC_GAIN_A_MIN_DB || gain_db > DAC_GAIN_A_MAX_DB) {
        shellPrint(shellGetCurrent(), "Error: gain_db out of range (%d to %d dB)\n", DAC_GAIN_A_MIN_DB, DAC_GAIN_A_MAX_DB);
        return -1;
    }

    g_play_analog_gain = gain_db;

    lisa_audio_gain_t gain = {
        .analog_gain = g_play_analog_gain,
        .digital_gain = g_play_digital_gain,
    };
    lisa_audio_play_set_gain(g_audio_dev, &gain);

    shellPrint(shellGetCurrent(), "Speaker analog gain set to %d dB\n", gain_db);

    return 0;
}

static int gain_cmd_print(int argc, char **argv)
{
    shellPrint(shellGetCurrent(), "Current Gain Settings:\n");
    shellPrint(shellGetCurrent(), "      MIC: %d dB\n", g_record_mic_analog_gain);
    shellPrint(shellGetCurrent(), "      AEC: %d dB\n", g_record_ref_analog_gain);
    shellPrint(shellGetCurrent(), "  Speaker: %d dB\n", g_play_analog_gain);
    return 0;
}

static int gain_cmd_help(int argc, char **argv)
{
    shellPrint(shellGetCurrent(), "Gain commands:\n");
    for (size_t i = 0; i < sizeof(g_gain_cmds) / sizeof(g_gain_cmds[0]); i++) {
        if (g_gain_cmds[i].help != NULL) {
            shellPrint(shellGetCurrent(), "%-17s\t:\t%s\n", g_gain_cmds[i].name, g_gain_cmds[i].help);
        }
    }
    return 0;
}

static int gain_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        return gain_cmd_help(argc, argv);
    }

    for (size_t i = 0; i < sizeof(g_gain_cmds) / sizeof(g_gain_cmds[0]); i++) {
        if (strcmp(g_gain_cmds[i].name, argv[1]) == 0) {
            return g_gain_cmds[i].exec(argc - 2, &argv[2]);
        }
    }

    return gain_cmd_help(argc, argv);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, gain,
                 gain_cmd_handler, audio gain settings);
#else  // !CONFIG_BOARD_ARCS_MINI
/**
* @brief Shell command to set record gain
* Usage: record_gain <analog_gain> <digital_gain>
*/
static int shell_record_gain(int argc, char **argv)
{
 if (argc != 3) {
     shellPrint(shellGetCurrent(), "Usage: record_gain <analog_gain> <digital_gain>\r\n");
     shellPrint(shellGetCurrent(), "  analog_gain: -12 to 36 dB (step 2 dB)\r\n");
     shellPrint(shellGetCurrent(), "  digital_gain: -83 to 42 dB (step 1 dB)\r\n");
     shellPrint(shellGetCurrent(), "  Example: record_gain 36 0\r\n");
     shellPrint(shellGetCurrent(), "  Current: analog=%d, digital=%d\r\n",
                g_record_analog_gain, g_record_digital_gain);
     return -1;
 }

 int32_t analog_gain = atoi(argv[1]);
 int32_t digital_gain = atoi(argv[2]);

 /* 检查模拟增益范围: -12 to 36 dB */
 if (analog_gain < -12 || analog_gain > 36) {
     shellPrint(shellGetCurrent(), "Error: analog_gain out of range (-12 to 36 dB)\r\n");
     return -1;
 }

 /* 检查数字增益范围: -83 to 42 dB */
 if (digital_gain < -83 || digital_gain > 42) {
     shellPrint(shellGetCurrent(), "Error: digital_gain out of range (-83 to 42 dB)\r\n");
     return -1;
 }

 g_record_analog_gain = analog_gain;
 g_record_digital_gain = digital_gain;
 lisa_audio_gain_t gain = {
     .analog_gain = g_record_analog_gain,
     .digital_gain = g_record_digital_gain,
 };

 lisa_audio_record_set_gain(g_audio_dev, &gain);
 shellPrint(shellGetCurrent(), "Record gain updated: analog=%d, digital=%d\r\n",
            g_record_analog_gain, g_record_digital_gain);

 return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, record_gain,
                 shell_record_gain, "set record gain <analog> <digital>");

/**
* @brief Shell command to set play gain
* Usage: play_gain <analog_gain> <digital_gain>
*/
static int shell_play_gain(int argc, char **argv)
{
 if (argc != 3) {
     shellPrint(shellGetCurrent(), "Usage: play_gain <analog_gain> <digital_gain>\r\n");
     shellPrint(shellGetCurrent(), "  analog_gain: -24 to 6 dB (step 2 dB)\r\n");
     shellPrint(shellGetCurrent(), "  digital_gain: -113 to 30 dB (step 1 dB)\r\n");
     shellPrint(shellGetCurrent(), "  Example: play_gain 0 -12\r\n");
     shellPrint(shellGetCurrent(), "  Current: analog=%d, digital=%d\r\n",
                g_play_analog_gain, g_play_digital_gain);
     return -1;
 }

 int32_t analog_gain = atoi(argv[1]);
 int32_t digital_gain = atoi(argv[2]);

 /* 检查模拟增益范围: -24 to 6 dB */
 if (analog_gain < -24 || analog_gain > 6) {
     shellPrint(shellGetCurrent(), "Error: analog_gain out of range (-24 to 6 dB)\r\n");
     return -1;
 }

 /* 检查数字增益范围: -113 to 30 dB */
 if (digital_gain < -113 || digital_gain > 30) {
     shellPrint(shellGetCurrent(), "Error: digital_gain out of range (-113 to 30 dB)\r\n");
     return -1;
 }

 g_play_analog_gain = analog_gain;
 g_play_digital_gain = digital_gain;
 lisa_audio_gain_t gain = {
     .analog_gain = g_play_analog_gain,
     .digital_gain = g_play_digital_gain,
 };
 lisa_audio_play_set_gain(g_audio_dev, &gain);
 shellPrint(shellGetCurrent(), "Play gain updated: analog=%d, digital=%d\r\n",
            g_play_analog_gain, g_play_digital_gain);

 return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, play_gain,
                 shell_play_gain, "set play gain <analog> <digital>");
#endif // CONFIG_BOARD_ARCS_MINI
