#define TAG "voice.cloud"

#include <stdint.h>
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
#include "async_task.h"

#include "acomp_wakeup.h"
#include "power/power_manager.h"
#include "uboot_features_api.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "timers.h"
#include "lsc_objrec.h"
#include "lsc_session_text.h"
#include "lsc_base64.h"
#include "project_version.h"
#include "sys_network_manager.h"


/*缓存500ms的音频*/
#define VOICE_CLOUD_RECORD_STREAM_BEFORE_DATA_LENGTH  (32000 + 16000)
#define VOICE_CLOUD_RECORD_STREAM_BUFFER_SIZE (VOICE_CLOUD_RECORD_STREAM_BEFORE_DATA_LENGTH + 10240)

/* DNS 备用服务器，断线时按次轮换，避免长期卡在同一个失效 DNS 上 */
static const char *const s_dns_fallback[] = {
    "114.114.114.114", /* 114 DNS */
    "223.5.5.5",       /* Alibaba DNS */
    "180.76.76.76",    /* Baidu DNS */
    "8.8.8.8",         /* Google DNS */
};
#define DNS_FALLBACK_COUNT ((uint32_t)(sizeof(s_dns_fallback) / sizeof(s_dns_fallback[0])))
static uint32_t s_disconn_cnt = 0;
static uint32_t s_dns_fallback_index = 0;

static StreamBufferHandle_t g_record_stream_buffer = NULL;
static TaskHandle_t g_audio_send_task = NULL;
static lsc_conn_t *g_lsc_conn_obj = NULL;
static TimerHandle_t g_pcm_send_en_timer = NULL;
static volatile uint8_t g_cloud_connected = 0;
static volatile uint8_t g_cloud_connecting = 0;
static volatile uint8_t g_cloud_auth_failed = 0;
static volatile uint8_t g_cloud_device_unbound = 0;
static volatile uint8_t g_voice_session_active = 0;

static volatile uint8_t pcm_send_en = 0;
static volatile uint8_t cloud_init_done = 0;
static session_objrec_t objrec = NULL;
static uint8_t *voice_cloud_token = NULL;
static uint8_t full_duplex = 0;
static volatile uint32_t g_objrec_request_seq = 0;
static volatile uint32_t g_objrec_accept_seq = 0;

/* 发起云端交互后延迟多少时间开始发送音频 */
#define PCM_SEND_AFTER_CLOUD_CHAT_START_MS (0)

#define VOICE_CLOUD_TTS_TEXT 1
#define RESOURCE_UPDATE_REBOOT_DELAY_MS_DEFAULT 3000U

static volatile uint32_t g_stream_text_generation = 1U;

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

int voice_cloud_is_device_unbound(void)
{
    return g_cloud_device_unbound ? 1 : 0;
}

voice_cloud_state_t voice_cloud_get_state(void)
{
    sys_network_status_t status;

    if (g_cloud_device_unbound) {
        return VOICE_CLOUD_STATE_CONNECT_FAILED;
    }

    if (g_cloud_connected) {
        return VOICE_CLOUD_STATE_CONNECTED;
    }

    if (sys_network_get_status(&status) != 0 ||
        status.active_bearer == SYS_NETWORK_BEARER_NONE) {
        return VOICE_CLOUD_STATE_NO_NETWORK;
    }

    /* 以 STNP 探测结果作为互联网可达标准 */
    if (!status.connected) {
        return VOICE_CLOUD_STATE_NO_INTERNET;
    }

    if (g_cloud_auth_failed) {
        return VOICE_CLOUD_STATE_TOKEN_FAILED;
    }

    if (g_cloud_connecting) {
        return VOICE_CLOUD_STATE_CONNECTING;
    }

    return VOICE_CLOUD_STATE_CONNECT_FAILED;
}

#define VOICE_QR_STATUS_BIND 3U

static void voice_cloud_handle_device_unbound(void)
{
    int pause_ret = 0;
    int session_active = voice_cloud_is_session_active();
    int uploading_audio = voice_cloud_is_uploading_audio();

    if (!session_active && !uploading_audio) {
        return;
    }

    pause_ret = voice_cloud_upload_audio_pause();
    if (pause_ret != 0) {
        LOGW("device unbound: pause upload failed: %d", pause_ret);
    }

    if (!session_active) {
        return;
    }

    LOGW("device unbound: interrupt active voice session");
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, NULL, 0);
}


static bool lsc_custom_error_process(cJSON *error)
{
    cJSON *code = error ? cJSON_GetObjectItem(error, "code") : NULL;
    uint32_t status = VOICE_QR_STATUS_BIND;

    if (!code || !cJSON_IsString(code) || !code->valuestring) {
        return false;
    }

    if (strcmp(code->valuestring, "FST_ERR_DEVICE_UNBOUND") != 0) {
        return false;
    }

    LOGW("custom error received: device unbound");
    g_cloud_device_unbound = 1;
    voice_cloud_handle_device_unbound();
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, &status, sizeof(status));
    return true;
}

static bool lsc_result_custom_process(cJSON *data)
{
    cJSON *type = cJSON_GetObjectItem(data, "type");

    if (!type || !cJSON_IsString(type) || !type->valuestring ||
        strcmp(type->valuestring, "CUSTOM") != 0) {
        return false;
    }

    return lsc_custom_error_process(cJSON_GetObjectItem(data, "error"));
}

int voice_cloud_is_session_active(void)
{
    return g_voice_session_active ? 1 : 0;
}

int voice_cloud_is_uploading_audio(void)
{
    return pcm_send_en ? 1 : 0;
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

int voice_cloud_upload_audio_pause(void)
{
    if (!cloud_init_done) {
        LOGW("upload audio pause ignored, cloud not init");
        return -1;
    }

#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    if (g_pcm_send_en_timer && xTimerIsTimerActive(g_pcm_send_en_timer) != pdFALSE) {
        if (xTimerStop(g_pcm_send_en_timer, 0) != pdPASS) {
            LOGW("upload audio pause: stop enable timer failed");
        }
    }
#endif

    voice_pcm_send_disable();

    if (g_record_stream_buffer) {
        xStreamBufferReset(g_record_stream_buffer);
    }

    LOGI("upload audio paused and buffer cleared");
    return 0;
}

int voice_cloud_upload_audio_resume(void)
{
    if (!cloud_init_done) {
        LOGW("upload audio resume ignored, cloud not init");
        return -1;
    }

    if (g_record_stream_buffer) {
        xStreamBufferReset(g_record_stream_buffer);
    }

#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    if (g_pcm_send_en_timer) {
        if (xTimerStop(g_pcm_send_en_timer, 0) != pdPASS &&
            xTimerIsTimerActive(g_pcm_send_en_timer) != pdFALSE) {
            LOGW("upload audio resume: stop enable timer failed");
        }

        if (xTimerStart(g_pcm_send_en_timer, pdMS_TO_TICKS(20)) != pdPASS) {
            LOGW("upload audio resume: start enable timer failed");
            return -1;
        }
    }
#else
    voice_pcm_send_enable();
#endif

    LOGI("upload audio resumed");
    return 0;
}

static void pcm_send_en_timer_cb(TimerHandle_t xTimer)
{
    voice_pcm_send_enable();
}

static void stream_text_cb_handle(int evt, const char *data, void *user)
{
    static uint8_t is_first = 1;
    static uint32_t active_generation = 0;
    uint32_t generation = (uint32_t)(uintptr_t)user;

    if (generation == 0U || generation != g_stream_text_generation) {
        return;
    }

    if (active_generation != generation) {
        active_generation = generation;
        is_first = 1;
    }

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
        active_generation = 0;
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_END, NULL, 0);
        break;
    case SSE_EVT_ABORT:
        is_first = 1;
        active_generation = 0;
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

static uint32_t lsc_cloud_reboot_delay_ms_get(cJSON *reboot_data)
{
    cJSON *delay_ms = reboot_data ? cJSON_GetObjectItem(reboot_data, "delay_ms") : NULL;

    if (delay_ms && cJSON_IsNumber(delay_ms) && delay_ms->valuedouble >= 0) {
        if (delay_ms->valuedouble > (double)UINT32_MAX) {
            return UINT32_MAX;
        }

        return (uint32_t)delay_ms->valuedouble;
    }

    return RESOURCE_UPDATE_REBOOT_DELAY_MS_DEFAULT;
}

static void lsc_resource_update_reboot_process(uint32_t delay_ms)
{
    /* 新 boot 可靠软重启；老 boot 纯电池下会掉电变静悄悄关机，提示用户手动重启。*/
    bool auto_reboot = uboot_features_has(UBOOT_FEATURE_POWER_GUARD) || power_is_usb_plugged();
    voice_msg_cloud_reboot_t reboot_msg = {
        .auto_reboot = auto_reboot ? 1 : 0,
        .delay_ms = delay_ms,
    };

    LOGI("resource update reboot process, auto_reboot=%u, delay_ms=%u",
         reboot_msg.auto_reboot, reboot_msg.delay_ms);

    voice_msg_pub(VOICE_MSG_CLOUD_RESOURCE_UPDATE_REBOOT, &reboot_msg, sizeof(reboot_msg));
}

static bool lsc_pushup_device_control_process(cJSON *data)
{
    cJSON *device_control = cJSON_GetObjectItem(data, "device_control");
    cJSON *command = device_control ? cJSON_GetObjectItem(device_control, "command") : NULL;

    if (!device_control || !cJSON_IsObject(device_control)) {
        return false;
    }

    if (!command || !cJSON_IsString(command) || command->valuestring == NULL) {
        LOGW("invalid pushup device_control command");
        return true;
    }

    if (strcmp(command->valuestring, "reboot") != 0) {
        LOGW("unsupported pushup device_control command: %s", command->valuestring);
        return true;
    }

    lsc_resource_update_reboot_process(lsc_cloud_reboot_delay_ms_get(device_control));
    return true;
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
    if (lsc_pushup_device_control_process(data)) {
        return;
    }

    cJSON *need_reboot = cJSON_GetObjectItem(data, "need_reboot");
    if (need_reboot && cJSON_IsTrue(need_reboot)) {
        LOGI("resource update need reboot detected");
        lsc_resource_update_reboot_process(lsc_cloud_reboot_delay_ms_get(data));
        return;
    }

    cJSON *type = cJSON_GetObjectItem(data, "type");
    if (type && cJSON_IsString(type) && type->valuestring &&
        strcmp(type->valuestring, "TTS") == 0) {
        cJSON *url = cJSON_GetObjectItem(data, "url");
        if (url == NULL || !cJSON_IsString(url) || url->valuestring == NULL ||
            url->valuestring[0] == '\0') {
            LOGE("pushup tts url is null");
            return;
        }

        LOGI("pushup tts raw url: %s", url->valuestring);
        voice_msg_pub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, url->valuestring, strlen(url->valuestring) + 1);
        return;
    }

    cJSON *sub = cJSON_GetObjectItem(data, "sub");
    if (sub == NULL) {
        LOGE("sub is null");
        return;
    }

    if (cJSON_IsString(sub) && strcmp(sub->valuestring, "tts") == 0) {
        cJSON *content = cJSON_GetObjectItem(data, "content");
        if (content == NULL || !cJSON_IsString(content) || content->valuestring == NULL) {
            LOGE("pushup tts content is null");
            return;
        }
        int content_len = strlen(content->valuestring);
        int dec_len = content_len / 4 * 3 + 8;
        char *url = lisa_mem_alloc(dec_len);
        if (url == NULL) {
            LOGE("pushup tts url alloc failed");
            return;
        }
        int out_len = 0;
        if (lsc_base64_decode(content->valuestring, content_len, url, &out_len) != 0) {
            LOGE("pushup tts url decode failed");
            lisa_mem_free(url);
            return;
        }
        url[out_len] = '\0';
        LOGI("pushup tts url: %s", url);
        voice_msg_pub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, url, out_len + 1);
        lisa_mem_free(url);
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
        cJSON *from = cJSON_GetObjectItem(root, "from");
        bool is_pushup = (pushup != NULL) ||
            (from != NULL && cJSON_IsString(from) && strcmp(from->valuestring, "pushup") == 0);
        if (lsc_result_custom_process(data)) {
            return;
        }
        if (nlp_origin != NULL && nlp_origin->valuestring
                && strcmp(nlp_origin->valuestring, "emoji") == 0) {
            lsc_emoji_msg_process(data);
        } else if (is_pushup) {
            lsc_pushup_msg_process(data);
        }
    }
}

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
    LOGI("lsc_event_cb, evt:%d", evt);

    switch (evt) {
    case LSC_CONNECTING:
        g_cloud_connecting = 1;
        voice_msg_pub(VOICE_MSG_CLOUD_CONNECTING, NULL, 0);
        break;
    case LSC_CONNECTED: {
        g_lsc_conn_obj = (lsc_conn_t *)data;
        g_cloud_connected = 1;
        g_cloud_connecting = 0;
        g_cloud_auth_failed = 0;
        g_cloud_device_unbound = 0;
        voice_msg_pub(VOICE_MSG_CLOUD_CONNECTED, NULL, 0);
    } break;
    case LSC_DISCONNECTED: {
        uint8_t was_connected = g_cloud_connected;
        g_cloud_connected = 0;
        g_cloud_connecting = 0;
        g_voice_session_active = 0;
        voice_pcm_send_disable();
        xStreamBufferReset(g_record_stream_buffer);
        voice_msg_pub(VOICE_MSG_CLOUD_DISCONNECTED, NULL, 0);
        if (was_connected && !g_cloud_auth_failed) {
            /* 已连通后掉线，先视作互联网不可达，并立即触发一次 STNP 复检 */
            sys_network_report_probe_result(false);
            extern int network_probe_start(void);
            network_probe_start();
        }
        /* 断线时轮换 DNS */
        s_disconn_cnt++;
        if(s_disconn_cnt > 3){
            s_dns_fallback_index++;
            const char *dns = s_dns_fallback[(s_dns_fallback_index - 1) % DNS_FALLBACK_COUNT];
            LOGI("[dns-rotate] disconnect #%u, switching primary DNS -> %s", (unsigned)s_disconn_cnt, dns);
            sys_wifi_refresh_dnsserver(dns);
            s_disconn_cnt = 0;
        }
    } break;
    case LSC_CLOUD_AUTH_FAILD:
        g_cloud_connecting = 0;
        g_cloud_auth_failed = 1;
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
        g_cloud_connecting = 1;
        g_cloud_auth_failed = 0;
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
        g_voice_session_active = 0;
        voice_pcm_send_disable();
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
        break;
    case SESSION_VOICE_TTS:
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_URL, (char *)data, size);
        break;
    case SESSION_VOICE_REPLY_URL:
        lsc_stream_text_request_thread_async((char *)data, stream_text_cb_handle,
                                             (void *)(uintptr_t)g_stream_text_generation, true);
        break;
    case SESSION_VOICE_IAT:
        if (data != NULL && size > 0 && ((char *)data)[0] != '\0') {
            voice_msg_pub(VOICE_MSG_CLOUD_IAT_UPDATE, (char *)data, size);
        }
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

typedef struct {
    char *resp;
} mcp_call_resp_async_ctx_t;

static void mcp_call_resp_send_sync(const char *resp)
{
    int ret = -1;

    if (!resp) {
        LOGE("mcp_call_resp_send_sync, response is null");
        return;
    }

    if (g_lsc_conn_obj && g_lsc_conn_obj->send_text) {
        ret = g_lsc_conn_obj->send_text((char *)resp);
        if (ret == 0) {
            LOGI("MCP async response sent successfully to cloud");
        } else {
            LOGE("mcp_call_resp_send_sync, send_text failed with ret=%d", ret);
        }
    } else {
        LOGE("mcp_call_resp_send_sync, mcp response send failed, lsc_conn_obj is null or send_text is null");
    }
}

static void mcp_call_resp_send_task(void *user_data, bool *should_stop)
{
    mcp_call_resp_async_ctx_t *ctx = (mcp_call_resp_async_ctx_t *)user_data;

    if (!ctx || !ctx->resp || (should_stop && *should_stop)) {
        return;
    }

    mcp_call_resp_send_sync(ctx->resp);
}

static void mcp_call_resp_send_done(void *user_data, bool completed, bool interrupted)
{
    mcp_call_resp_async_ctx_t *ctx = (mcp_call_resp_async_ctx_t *)user_data;

    (void)completed;
    (void)interrupted;

    if (!ctx) {
        return;
    }

    if (ctx->resp) {
        lisa_mem_free(ctx->resp);
        ctx->resp = NULL;
    }
    lisa_mem_free(ctx);
}

static void mcp_call_resp_cb_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    mcp_call_resp_async_ctx_t *ctx = NULL;
    async_task_t *task = NULL;
    size_t resp_len = 0;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    LOGI("MCP call response received: %s", (char *)data);
    if (data == NULL || len == 0) {
        LOGE("mcp_call_resp_cb_handle, data is null");
        return;
    }

    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        LOGE("mcp_call_resp_cb_handle, alloc ctx failed");
        mcp_call_resp_send_sync((const char *)data);
        return;
    }

    resp_len = strnlen((const char *)data, len);
    ctx->resp = lisa_mem_alloc(resp_len + 1);
    if (!ctx->resp) {
        LOGE("mcp_call_resp_cb_handle, alloc resp failed");
        lisa_mem_free(ctx);
        mcp_call_resp_send_sync((const char *)data);
        return;
    }

    memcpy(ctx->resp, data, resp_len);
    ctx->resp[resp_len] = '\0';

    task = async_task_create("mcp_rsp", 3072, 5, mcp_call_resp_send_task, mcp_call_resp_send_done, ctx);
    if (!task) {
        LOGE("mcp_call_resp_cb_handle, create async task failed");
        mcp_call_resp_send_sync(ctx->resp);
        mcp_call_resp_send_done(ctx, true, false);
        return;
    }

    if (async_task_start(task) != 0) {
        LOGE("mcp_call_resp_cb_handle, start async task failed");
        async_task_destroy(task);
        mcp_call_resp_send_sync(ctx->resp);
        mcp_call_resp_send_done(ctx, true, false);
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

    r = lsc_add_callback(LSC_CONNECTED | LSC_CONNECTING | LSC_DISCONNECTED |
                             LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN | LSC_DATA_RECEIVED,
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

int voice_cloud_disconnect(void)
{
    if (!cloud_init_done) {
        return 0;
    }

    // 断开连接并清除 token，触发重新认证
    // 重连线程会自动使用新账号认证
    int ret = lsc_disconnect();
    if (ret != 0) {
        LOGE("lsc_disconnect failed: %d", ret);
        return ret;
    }

    g_cloud_connected = 0;
    g_cloud_connecting = 0;
    g_cloud_auth_failed = 0;
    g_cloud_device_unbound = 0;
    LOGI("voice_cloud_disconnect completed");
    return 0;
}

int voice_cloud_connect(struct voice_cloud_connect_config *config)
{
    int r;

    g_cloud_connected = 0;
    g_cloud_connecting = 1;
    g_cloud_auth_failed = 0;
    g_cloud_device_unbound = 0;

    r = voice_cloud_init(config);
    if (r != 0) {
        g_cloud_connecting = 0;
        LOGE("voice_server_init failed");
        return r;
    }

    r = lsc_connect();
    if (r != 0) {
        g_cloud_connecting = 0;
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

    g_stream_text_generation++;
    if (g_stream_text_generation == 0U) {
        g_stream_text_generation = 1U;
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

    g_voice_session_active = 1;

    #if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    if (xTimerStart(g_pcm_send_en_timer, pdMS_TO_TICKS(20)) != pdPASS) {
        LOGE("pcm send en timer start failed");
        g_voice_session_active = 0;
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
    g_voice_session_active = 0;
    voice_pcm_send_disable();

    return ret;
}

int voice_cloud_chat_stop_local(void)
{
    int ret;

    ret = voice_cloud_upload_audio_pause();
    if (ret != 0) {
        LOGE("voice chat local stop failed: %d", ret);
        return ret;
    }

    g_voice_session_active = 0;
    return 0;
}

int voice_cloud_cancel_current_response(void)
{
    int ret;

    if (!cloud_init_done || !g_voice_session_active) {
        LOGW("cancel current response ignored, session inactive");
        return -1;
    }
    if (!full_duplex) {
        LOGW("cancel current response ignored, not full duplex");
        return -1;
    }

    ret = session_voice_cancel();
    if (ret != 0) {
        LOGE("cancel current response failed");
        return ret;
    }

    LOGI("cancel current response on new full-duplex IAT");
    return 0;
}

int voice_cloud_chat_send_audio(uint8_t *data, int len)
{
    if (!g_record_stream_buffer) {
        return -1;
    }

    if (xStreamBufferSend(g_record_stream_buffer, data, len, 0) == pdFALSE) {
        LOGE("voice_cloud_chat_send_audio, xStreamBufferSend failed");
        return -1;
    }

    return 0;
}

static void session_objrec_evt_cb(session_objrec_t s, int evt, void *data, uint32_t data_len, void *user)
{
    uint32_t request_seq = (uint32_t)(uintptr_t)user;

    if (request_seq == 0 || request_seq != g_objrec_accept_seq) {
        LOGW("ignore stale objrec evt=%d, request_seq=%u, accept_seq=%u",
             evt, (unsigned)request_seq, (unsigned)g_objrec_accept_seq);
        return;
    }

    if (evt == SESSION_OBJREC_EVT_TEXT_URL) {
        if (data) {
#if VOICE_CLOUD_TTS_TEXT
            lsc_stream_text_request_thread_async((char *)data, stream_text_cb_handle,
                                                 (void *)(uintptr_t)g_stream_text_generation, true);
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
    } else if (evt == SESSION_OBJREC_EVT_FINISH ||
               evt == SESSION_OBJREC_EVT_ERROR ||
               evt == SESSION_OBJREC_EVT_TIMEOUT) {
        LOGW("object recognition ended without result, evt=%d, request_seq=%u",
             evt, (unsigned)request_seq);
        g_objrec_accept_seq = 0;
        voice_msg_pub(VOICE_MSG_CLOUD_IMAGE_RECOGNITION_FAILED, NULL, 0);
    } else {
        LOGE("unknown objrec evt: %d", evt);
    }
}

int voice_cloud_image_recognition(uint8_t *jpg_image, uint32_t len)
{
    int err;
    uint32_t request_seq = 0;

    if (!cloud_init_done) {
        return -1;
    }

    if (objrec == NULL) {
        objrec = session_objrec_new();
        if (objrec == NULL) {
            LOGE("session_objrec_init failed");
            return -1;
        }
    }

    request_seq = ++g_objrec_request_seq;
    g_objrec_accept_seq = request_seq;
    LOGI("start object recognition, request_seq=%u, size=%u",
         (unsigned)request_seq, (unsigned)len);

    err = session_objrec_run_async(objrec, jpg_image, len, session_objrec_evt_cb,
                                   (void *)(uintptr_t)request_seq);
    if (err) {
        LOGE("session_objrec_run failed, err:%d", err);
        if (g_objrec_accept_seq == request_seq) {
            g_objrec_accept_seq = 0;
        }
        return -1;
    }

    LOGI("session object async run successfully");

    return 0;
}

void voice_cloud_image_recognition_drop_pending_result(void)
{
    if (g_objrec_accept_seq == 0) {
        LOGI("object recognition cancel ignored, no pending request");
        return;
    }

    LOGI("cancel pending object recognition, request_seq=%u", (unsigned)g_objrec_accept_seq);
    g_objrec_accept_seq = 0;

    if (objrec != NULL) {
        int ret = session_objrec_cancel(objrec);
        if (ret != 0) {
            LOGW("cancel object recognition session failed, ret=%d", ret);
        }
    }
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

    ret = session_voice_start();
    if (ret != 0) {
        LOGE("session_voice_start failed");
        return ret;
    }

    g_voice_session_active = 1;

#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    if (xTimerStart(g_pcm_send_en_timer, pdMS_TO_TICKS(20)) != pdPASS) {
        LOGE("pcm send en timer start failed");
        g_voice_session_active = 0;
        return -1;
    }
#else
    voice_pcm_send_enable();
#endif

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
