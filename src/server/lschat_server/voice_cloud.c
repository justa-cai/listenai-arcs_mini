#define TAG "voice.cloud"

#include <string.h>
#include "lsc.h"
#include "lsc_errno.h"
#include "lisa_log.h"
#include "lisa_thread.h"

#include "lsc_session_voice.h"
#include "lsc_stream_text_thread.h"
#include "ico_codec.h"
#include "lsc_conn.h"
#include "cJSON.h"

#include "voice_cloud.h"
#include "voice_msg.h"
#include "image_url_utils.h"
#include "service_image.h"

#include "acomp_wakeup.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "timers.h"
#include "lsc_objrec.h"
#include "lsc_session_text.h"
#include "project_version.h"

/*缓存500ms的音频*/
#define VOICE_CLOUD_RECORD_STREAM_BEFORE_DATA_LENGTH  (32000 + 16000)
#define VOICE_CLOUD_RECORD_STREAM_BUFFER_SIZE (VOICE_CLOUD_RECORD_STREAM_BEFORE_DATA_LENGTH + 10240)

static StreamBufferHandle_t g_record_stream_buffer = NULL;
static TaskHandle_t g_audio_send_task = NULL;
static lsc_conn_t *g_lsc_conn_obj = NULL;
static TimerHandle_t g_pcm_send_en_timer = NULL;
static volatile uint8_t g_cloud_connected = 0;

static volatile uint8_t pcm_send_en = 0;
static volatile uint8_t cloud_init_done = 0;
static session_objrec_t objrec = NULL;
static uint8_t *voice_cloud_token = NULL;
static uint8_t full_duplex = 0;

/* 发起云端交互后延迟多少时间开始发送音频 */
#define PCM_SEND_AFTER_CLOUD_CHAT_START_MS (0)

#define VOICE_CLOUD_TTS_TEXT 0

const char *lsc_get_firmware_type(void)
{
#ifdef CONFIG_BOARD_ARCS_MINI
    return "arcs-mini";
#elif defined(CONFIG_BOARD_ARCS_EVB)
    return "arcs-evb";
#else
    return "unknown";
#endif
}

const char *lsc_get_firmware_version(void)
{
    return PROJECT_VERSION_STR;
}

static inline uint8_t voice_is_full_duplex(void)
{
    return full_duplex;
}

int voice_cloud_is_connected(void)
{
    return g_cloud_connected ? 1 : 0;
}

static void voice_pcm_send_enable(void)
{
    LOGI("voice pcm send enable");
    pcm_send_en = 1;
}

static void voice_pcm_send_disable(void)
{
    LOGI("voice pcm send disable");
    pcm_send_en = 0;
}

static void pcm_send_en_timer_cb(TimerHandle_t xTimer)
{
    voice_pcm_send_enable();
}

static void stream_text_cb_handle(int evt, const char *data, void *user)
{
    static uint8_t is_first = 1;

    int len = data ? strlen(data) + 1 : 0;

    switch (evt) {
    case SSE_EVT_DATA:
        if (is_first) {
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_START, NULL, 0);
            is_first = 0;
        }
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, (void *)data, len);
        break;
    case SSE_EVT_DONE:
        is_first = 1;
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_END, NULL, 0);
        break;
    case SSE_EVT_ABORT:
        is_first = 1;
        break;
    default:
        break;
    }
}

static void lsc_emoji_msg_process(cJSON *emoji_data)
{
    cJSON *data = cJSON_GetObjectItem(emoji_data, "data");
    if (data == NULL) {
        return;
    }

    cJSON *emo_id = cJSON_GetObjectItem(data, "emo_id");
    if (emo_id == NULL || emo_id->valuestring == NULL) {
        return;
    }

    LOGI("cloud emoji received, name: %s", emo_id->valuestring);
    voice_msg_pub(VOICE_MSG_CLOUD_EMOJI, emo_id->valuestring, strlen(emo_id->valuestring) + 1);
}

static void lsc_mcp_msg_process(cJSON *data)
{
    char *data_str = cJSON_PrintUnformatted(data);
    if (data_str == NULL) {
        LOGE("lsc_mcp_msg_process, cJSON_PrintUnformatted failed");
        return;
    }

    /* 这里交给总线去处理mcp消息 */
    voice_msg_pub(VOICE_MSG_CLOUD_MCP, data_str, strlen(data_str) + 1);

    cJSON_free(data_str);
}

static void voice_cloud_publish_image_url(const char *input_url)
{
    char display_url[768] = {0};
    bool is_qr_url = false;

    if (!input_url || input_url[0] == '\0') {
        return;
    }

    is_qr_url = (strstr(input_url, "/qr") != NULL);

    if (is_qr_url) {
        LOGI("image_generation qr url detected, enter waiting state");
        service_image_waiting_start();
        char loading_text[]="图片绘制中...";
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_LOADING, (void *)loading_text, strlen(loading_text) + 1);
        return;
    } else {
        bool waiting = service_image_waiting_get();
        if (waiting) {
            LOGI("image_generation final url detected, waiting hit");
        } else {
            LOGI("direct image url detected, show without waiting gate");
        }
    }

    if (build_image_display_url(input_url, display_url, sizeof(display_url)) == 0) {
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_LOADING, NULL, 0);
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_IMAGE_URL, display_url, strlen(display_url) + 1);
    } else {
        LOGW("build image display url failed, fallback raw url");
    }
}

static void lsc_pushup_msg_process(cJSON *data)
{
    LOGI("push msg process");

    /* 小聆1.2.3高质量文生图 */
    cJSON *nlp_origin = cJSON_GetObjectItem(data, "nlp_origin");
    if (nlp_origin && cJSON_IsString(nlp_origin) && nlp_origin->valuestring &&
        strcmp(nlp_origin->valuestring, "image_generation") == 0) {
        cJSON *biz_data = cJSON_GetObjectItem(data, "data");
        cJSON *result = biz_data ? cJSON_GetObjectItem(biz_data, "result") : NULL;
        cJSON *first = (result && cJSON_IsArray(result)) ? cJSON_GetArrayItem(result, 0) : NULL;
        cJSON *url = first ? cJSON_GetObjectItem(first, "url") : NULL;

        if (url && cJSON_IsString(url) && url->valuestring && url->valuestring[0] != '\0') {
            LOGI("cloud draw image url: %s", url->valuestring);
            voice_cloud_publish_image_url(url->valuestring);
        } else {
            LOGW("image_generation pushup url not found");
        }
    }

    cJSON *sub = cJSON_GetObjectItem(data, "sub");
    if (sub == NULL) {
        LOGE("sub is null");
        return;
    }

    cJSON *mp_guide = cJSON_GetObjectItem(data, "mp_guide");
    if (mp_guide == NULL) {
        LOGE("mp_guide is null");
        return;
    }

    cJSON *role_config = cJSON_GetObjectItem(mp_guide, "role_config");
    if (role_config == NULL) {
        LOGE("role_config is null");
        return;
    }

    cJSON *image_url = cJSON_GetObjectItem(role_config, "image_url");
    if (image_url == NULL || image_url->valuestring == NULL) {
        LOGE("image_url is null");
        return;
    }
    LOGI("cloud role setting qrcode url: %s", image_url->valuestring);

    voice_msg_pub(VOICE_MSG_CLOUD_ROLE_SETTING_QRCODE, image_url->valuestring, strlen(image_url->valuestring) + 1);

    // Process standby texts banner
    cJSON *theme = cJSON_GetObjectItem(data, "theme");
    if (theme != NULL) {
        cJSON *frontend = cJSON_GetObjectItem(theme, "frontend");
        if (frontend != NULL) {
            cJSON *banner = cJSON_GetObjectItem(frontend, "banner");
            if (banner != NULL) {
                char *banner_str = cJSON_PrintUnformatted(banner);
                if (banner_str != NULL) {
                    LOGI("cloud standby texts banner: %s", banner_str);
                    voice_msg_pub(VOICE_MSG_CLOUD_STANDBY_TEXTS, banner_str, strlen(banner_str) + 1);
                    cJSON_free(banner_str);
                } else {
                    LOGE("failed to serialize banner json");
                }
            }
        }
    }
}

static void lsc_raw_msg_process(cJSON *root)
{
    if (root == NULL) {
        LOGE("lsc_mcp_msg_process, root is null");
        return;
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action == NULL || action->valuestring == NULL) {
        return;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL) {
        LOGE("lsc_mcp_msg_process, data is null");
        return;
    }

    if (strcmp(action->valuestring, "mcp") == 0) {
        lsc_mcp_msg_process(data);
        return;
    }

    if (strcmp(action->valuestring, "result") == 0) {
        cJSON *nlp_origin = cJSON_GetObjectItem(data, "nlp_origin");
        cJSON *pushup = cJSON_GetObjectItem(root, "pushup");
        if (nlp_origin != NULL && nlp_origin->valuestring
                && strcmp(nlp_origin->valuestring, "emoji") == 0) {
            lsc_emoji_msg_process(data);
        } else if (pushup != NULL) {
            lsc_pushup_msg_process(data);
        }
    }
}

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
    LOGI("lsc_event_cb, evt:%d", evt);

    switch (evt) {
    case LSC_CONNECTED: {
        g_lsc_conn_obj = (lsc_conn_t *)data;
        g_cloud_connected = 1;
        voice_msg_pub(VOICE_MSG_CLOUD_CONNECTED, NULL, 0);
    } break;
    case LSC_DISCONNECTED:
        g_cloud_connected = 0;
        voice_pcm_send_disable();
        xStreamBufferReset(g_record_stream_buffer);
        voice_msg_pub(VOICE_MSG_CLOUD_DISCONNECTED, NULL, 0);
        break;
    case LSC_CLOUD_AUTH_FAILD:
        voice_msg_pub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, NULL, 0);
        break;
    case LSC_GOT_TOKEN:
        if (voice_cloud_token) {
            lisa_mem_free(voice_cloud_token);
            voice_cloud_token = NULL;
        }

        voice_cloud_token = lisa_mem_alloc(size + 1);
        if (voice_cloud_token) {
            memcpy(voice_cloud_token, data, size);
            voice_cloud_token[size] = 0;
        }
        voice_msg_pub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS, NULL, 0);
        break;
    case LSC_DATA_RECEIVED: {
        cJSON *root = (cJSON *)data;
        lsc_raw_msg_process(root);
    } break;
    default:
        break;
    }
}

static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
{
    LISA_NLOGI("[%s] evt:%d", __FUNCTION__, evt);

    switch (evt) {
    case SESSION_VOICE_FINISH:
        voice_pcm_send_disable();
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
        break;
    case SESSION_VOICE_TTS:
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_URL, (char *)data, size);
        break;
    case SESSION_VOICE_REPLY_URL:
        lsc_stream_text_request_thread_async((char *)data, stream_text_cb_handle, NULL, true);
        break;
    case SESSION_VOICE_IAT:
        voice_msg_pub(VOICE_MSG_CLOUD_IAT_UPDATE, (char *)data, size);
        break;
    case SESSION_VOICE_IAT_START:
        voice_msg_pub(VOICE_MSG_CLOUD_IAT_START, NULL, 0);
        break;
    case SESSION_VOICE_IAT_END:
        voice_msg_pub(VOICE_MSG_CLOUD_IAT_END, NULL, 0);
        break;
    case SESSION_VOICE_GOT_VAD:
        voice_msg_pub(VOICE_MSG_CLOUD_VAD, NULL, 0);
        if (!voice_is_full_duplex()) {
            LOGI("voice is not full duplex mode, disable audio send.");
            voice_pcm_send_disable();
        }
        break;
    case SESSION_VOICE_VPR_INFO:
        voice_msg_pub(VOICE_MSG_CLOUD_VPR_INFO, data, size);
        break;
    case SESSION_VOICE_VPR_FEATURE:
        voice_msg_pub(VOICE_MSG_CLOUD_VPR_FEATURE, data, size);
        break;
    case SESSION_VOICE_DRAW:
    {
        char input_url[768] = {0};
        size_t copy_len = 0;

        if (!data || size == 0) {
            break;
        }

        copy_len = (size_t)(size - 1);
        if (copy_len >= sizeof(input_url)) {
            copy_len = sizeof(input_url) - 1;
        }

        memcpy(input_url, data, copy_len);
        input_url[copy_len] = '\0';

        voice_cloud_publish_image_url(input_url);
    }
        break;
    default:
        break;
    }
}

static void text_event_cb(session_text_event_e evt, void *data, uint32_t size, void *usr)
{
    switch (evt) {
    case SESSION_TEXT_TTS_URL:
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_URL, (char *)data, size);
        break;
    default:
        break;
    }
}

static void voice_audio_send_task(void *pvParameters)
{
#define AUDIO_SEND_DATA_MAX_SIZE    (2560)
#define AUDIO_STREAM_PCM_FRAME_SIZE (640)
#define AUDIO_STREAM_ICO_FRAME_SIZE (1200)

    short ico_encoded_len = 0;
    uint8_t *pcm = lisa_mem_alloc(AUDIO_STREAM_PCM_FRAME_SIZE);
    assert(pcm != NULL);
    uint8_t *ico = lisa_mem_alloc(AUDIO_STREAM_ICO_FRAME_SIZE);
    short ico_frame_len = 0;
    assert(ico != NULL);
    int ret = 0;
    ret = ico_encode_init();
    assert(ret == 0);

    while (1) {
        size_t received_size = 0;
        size_t bytes_available = xStreamBufferBytesAvailable(g_record_stream_buffer);

        /* 当未启用发送时，检查缓冲区阈值 */
        if (!pcm_send_en) {
           
            if (bytes_available < VOICE_CLOUD_RECORD_STREAM_BEFORE_DATA_LENGTH) {
                /* 数据未达到阈值，等待后重试 */
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            /* 数据超过阈值，解除阻塞，开始处理 */
            xStreamBufferReceive(g_record_stream_buffer, pcm, AUDIO_STREAM_PCM_FRAME_SIZE, pdMS_TO_TICKS(20));
            // LISA_LOGI(TAG,"bytes_available:%d",bytes_available);
            continue;
        }

        if (bytes_available < AUDIO_STREAM_PCM_FRAME_SIZE) {
            /* 数据未达到阈值，等待后重试 */
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        received_size =
            xStreamBufferReceive(g_record_stream_buffer, pcm, AUDIO_STREAM_PCM_FRAME_SIZE, pdMS_TO_TICKS(20));
        if (received_size <= 0) {
            continue;
        }

        ico_frame_len = 0;
        int r = ico_codec_encode((short *)pcm, (void *)&ico[ico_encoded_len], &ico_frame_len);
        if (r != 0) {
            LOGE("ico_codec_encode failed");
            continue;
        } else {
            ico_frame_len <<= 1;
            ico_encoded_len += ico_frame_len;
            if((ico_encoded_len + 40 >= AUDIO_STREAM_ICO_FRAME_SIZE) || (bytes_available < (AUDIO_STREAM_PCM_FRAME_SIZE << 1))){

                ret = session_voice_send_audio(ico, ico_encoded_len);
                if ((ret != 0) && (ret != LSC_INVALID_STATE)) {
                    /* 其他错误，重试最多2次 */
                    for(int retry = 0; retry < 2; retry++){
                        LOGW("session_voice_send_audio failed, retrying...");
                        vTaskDelay(pdMS_TO_TICKS(20));
                        ret = session_voice_send_audio(ico, ico_encoded_len);
                        if (ret == 0) {
                            break;
                        }
                        if (ret == LSC_INVALID_STATE) {
                            /* Session在重试过程中关闭了 */
                            break;
                        }
                    }
                }
                ico_encoded_len = 0;
            }

        }
    }
}

static void mcp_msg_cb_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (data == NULL) {
        LOGE("mcp_msg_cb_handle, data is null");
        return;
    }

    LOGI("MCP message received: %s", (char *)data);
    cJSON *root = cJSON_Parse((char *)data);
    if (root == NULL) {
        LOGE("mcp_msg_cb_handle, cJSON_Parse failed");
        return;
    }

    cJSON *resp = mcp_process(root);
    cJSON_Delete(root);
    if (resp == NULL) {
        return;
    }
    cJSON_AddStringToObject(resp, "action", "mcp");
    char *resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_str == NULL) {
        LOGE("mcp_msg_cb_handle, cJSON_PrintUnformatted failed");
        return;
    }

    LOGI("MCP response: %s", resp_str);
    if (g_lsc_conn_obj && g_lsc_conn_obj->send_text) {
        g_lsc_conn_obj->send_text(resp_str);
    } else {
        LOGE("mcp_msg_cb_handle, mcp response send failed, lsc_conn_obj is null or send_text is null");
    }

    cJSON_free(resp_str);
}

static void mcp_call_resp_cb_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("MCP call response received: %s", (char *)data);
    if (data == NULL) {
        LOGE("mcp_call_resp_cb_handle, data is null");
        return;
    }

    if (g_lsc_conn_obj && g_lsc_conn_obj->send_text) {
        int ret = g_lsc_conn_obj->send_text((char *)data);
        if (ret == 0) {
            LOGI("MCP async response sent successfully to cloud");
        } else {
            LOGE("mcp_call_resp_cb_handle, send_text failed with ret=%d", ret);
        }
    } else {
        LOGE("mcp_call_resp_cb_handle, mcp response send failed, lsc_conn_obj is null or send_text is null");
    }
}

int voice_cloud_init(struct voice_cloud_connect_config *config)
{
    int r;

    if (cloud_init_done) {
        return 0;
    }

    if (config == NULL || config->did == NULL || config->pid == NULL || config->sid == NULL) {
        LOGE("invalid connection params");
        return -1;
    }

    lsc_server_config_t server_config = {
        .host = config->host,
        .host_staging = config->host_staging,
        .host_integration = config->host_integration,
        .token_url = config->token_url,
        .token_url_staging = config->token_url_staging,
        .token_url_integration = config->token_url_integration,
        .music_active_url = config->music_active_url,
        .music_tranlink_url = config->music_tranlink_url,
        .port = config->port,
        .scheme = config->scheme,
    };

    lsc_config_t cfg = {
        .device_id = config->did,
        .product_id = config->pid,
        .secret_id = config->sid,
        .if_auto_reconn = config->auto_reconn,
        .reconn_interval_ms = config->reconn_interval_ms,
        .device_mode = config->device_mode,
        .server_config = &server_config,
    };

    LOGI("device mode: %d", cfg.device_mode);

#if VOICE_CLOUD_TTS_TEXT
    r = lsc_stream_text_request_thread_init();
    if (r != 0) {
        LOGE("lsc_stream_text_request_thread_init failed");
        return r;
    }
#endif

    if (g_record_stream_buffer == NULL) {
        g_record_stream_buffer = xStreamBufferCreate(VOICE_CLOUD_RECORD_STREAM_BUFFER_SIZE, 1);
        assert(g_record_stream_buffer != NULL);
    }

    r = lsc_init(&cfg);
    if (r != 0) {
        LOGE("lsc_init failed");
        return r;
    }

    r = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN | LSC_DATA_RECEIVED,
                         lsc_event_cb, NULL);
    if (r != 0) {
        LOGE("lsc_add_callback failed");
        return r;
    }

    xTaskCreate(voice_audio_send_task, "audio_send", 2048, NULL, 8, &g_audio_send_task);
    assert(g_audio_send_task != NULL);

    r = session_voice_init();
    r = session_voice_add_evt_callback(voice_event_cb,
                                       SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_MUSIC_LISTS |
                                           SESSION_VOICE_ALARM_INTENT | SESSION_VOICE_AIUI_CTRL |
                                           SESSION_VOICE_MUSIC_INSTR | SESSION_VOICE_FINISH | SESSION_VOICE_IAT |
                                           SESSION_VOICE_IAT_START | SESSION_VOICE_IAT_END |SESSION_VOICE_VPR_INFO |
                                           SESSION_VOICE_VPR_FEATURE | SESSION_VOICE_DRAW,
                                       NULL);
    r = session_text_init();
    if (r != 0) {
        LOGE("session_text_init failed");
        return r;
    }
    session_text_add_evt_callback(text_event_cb, SESSION_TEXT_TTS_URL, NULL);

#if VOICE_CLOUD_TTS_TEXT
    r |= session_voice_add_evt_callback(voice_event_cb, SESSION_VOICE_REPLY_URL, NULL);
#endif

    if (r != 0) {
        LOGE("session_voice_add_evt_callback failed");
        return r;
    }

    voice_msg_sub(VOICE_MSG_CLOUD_MCP, mcp_msg_cb_handle, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CALL_RESP, mcp_call_resp_cb_handle, NULL);

#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    g_pcm_send_en_timer = xTimerCreate("voice.cloud.pcm.en", pdMS_TO_TICKS(PCM_SEND_AFTER_CLOUD_CHAT_START_MS), pdFALSE,
                                       NULL, pcm_send_en_timer_cb);
    assert(g_pcm_send_en_timer != NULL);
#endif

    cloud_init_done = 1;

    return 0;
}

int voice_cloud_connect(struct voice_cloud_connect_config *config)
{
    int r;

    r = voice_cloud_init(config);
    if (r != 0) {
        LOGE("voice_server_init failed");
        return r;
    }

    r = lsc_connect();
    if (r != 0) {
        LOGE("lsc_connect failed");
        return r;
    }

    return r;
}

int voice_cloud_chat_start(struct voice_cloud_chat_config *config)
{
    int ret;

    if (!cloud_init_done) {
        return -1;
    }
    
    /* Disable audio sending first to prevent uploading wakeup word */
    voice_pcm_send_disable();
    
    /* Reset stream buffer to clear any cached audio data including wakeup word */
    xStreamBufferReset(g_record_stream_buffer);

    session_voice_config_t voice_config = {
        .vad_enable = true,
        .speex_size = 0,
        .aue = "ico",
        .inter_mode = config->full_duplex ? SESSION_VOICE_INTER_MODE_CONTINUE : SESSION_VOICE_INTER_MODE_ONESHOT,
        .session_timeout_ms = config->timeout_ms,
        .oneshot = config->oneshot,
        .words = config->words,
        .words_cnt = config->words_cnt,
    };

    full_duplex = config->full_duplex;

    ret = session_voice_set_config(&voice_config);
    if (ret != 0) {
        LOGE("session_voice_set_config failed");
        return ret;
    }

    ret = session_voice_start();
    if (ret != 0) {
        LOGE("session_voice_start failed");
        return ret;
    }

    #if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    if (xTimerStart(g_pcm_send_en_timer, pdMS_TO_TICKS(20)) != pdPASS) {
        LOGE("pcm send en timer start failed");
        return -1;
    }
#else
    voice_pcm_send_enable();
#endif

    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_STARTING, NULL, 0);

    return ret;
}

int voice_cloud_chat_stop(void)
{
    int ret;

    ret = session_voice_cancel();
    if (ret != 0) {
        LOGE("session_voice_cancel failed");
        return ret;
    }
    voice_pcm_send_disable();

    return ret;
}

int voice_cloud_chat_send_audio(uint8_t *data, int len)
{
    if (g_record_stream_buffer) {
        if (xStreamBufferSend(g_record_stream_buffer, data, len, 0) == pdFALSE) {
            LOGE("voice_cloud_chat_send_audio, xStreamBufferSend failed");
            return -1;
        }
    }
}

static void session_objrec_evt_cb(session_objrec_t s, int evt, void *data, uint32_t data_len, void *user)
{
    if (evt == SESSION_OBJREC_EVT_TEXT_URL) {
        if (data) {
#if VOICE_CLOUD_TTS_TEXT
            lsc_stream_text_request_thread_async((char *)data, stream_text_cb_handle, NULL, true);
#endif
        } else {
            LOGE("objec evt %d data is null", evt);
        }
    } else if (evt == SESSION_OBJREC_EVT_TTS_URL) {
        if (data) {
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_URL, (char *)data, data_len);
        } else {
            LOGE("objec evt %d data is null", evt);
        }
    } else {
        LOGE("unknown objrec evt: %d", evt);
    }
}

int voice_cloud_image_recognition(uint8_t *jpg_image, uint32_t len)
{
    int err;

    if (!cloud_init_done) {
        return -1;
    }

    if (objrec == NULL) {
        objrec = session_objrec_new();
        LOGE("session_objrec_init failed");
        assert(objrec);
    }

    err = session_objrec_run_async(objrec, jpg_image, len, session_objrec_evt_cb, NULL);
    if (err) {
        LOGE("session_objrec_run failed, err:%d", err);
        return -1;
    }

    LOGI("session object async run successfully");

    return 0;
}

int voice_cloud_audio_recognition_start(void)
{
    int ret;

    if (!cloud_init_done) {
        return -1;
    }
    
    /* Disable audio sending first to prevent uploading wakeup word */
    voice_pcm_send_disable();
    
    /* Reset stream buffer to clear any cached audio data including wakeup word */
    xStreamBufferReset(g_record_stream_buffer);

    session_voice_config_t voice_config = {
        .vad_enable = false,
        .speex_size = 0,
        .aue = "ico",
        .inter_mode = SESSION_VOICE_INTER_MODE_ONESHOT,
        .session_timeout_ms = 0,
        .oneshot = false,
        .words = NULL,
        .words_cnt = 0,
    };

    ret = session_voice_set_config(&voice_config);
    if (ret != 0) {
        LOGE("session_voice_set_config failed");
        return ret;
    }

#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    if (xTimerStart(g_pcm_send_en_timer, pdMS_TO_TICKS(20)) != pdPASS) {
        LOGE("pcm send en timer start failed");
        return -1;
    }
#else
    voice_pcm_send_enable();
#endif

    ret = session_voice_start();
    if (ret != 0) {
        LOGE("session_voice_start failed");
        return ret;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_STARTING, NULL, 0);

    return ret;
}

int voice_cloud_audio_recognition_stop(void)
{
    if (!cloud_init_done) {
        return -1;
    }

    voice_pcm_send_disable();

    return session_voice_end_audio();
}

uint8_t *voice_token_get(void)
{
    return voice_cloud_token;
}

int voice_cloud_tts_synth(const char *txt)
{
    if (!txt || txt[0] == '\0') {
        LOGE("voice_cloud_tts_synth: invalid text (null or empty)");
        return -1;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
    return session_text_tts_synth(txt);
}
