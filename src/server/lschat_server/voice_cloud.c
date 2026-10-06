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
#include "voice_player/tone_control/voice_player_tone.h"

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
#include "sysutils.h"


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
/* 非全双工唤醒会话需等本地唤醒应答音播完后才允许上传麦克风数据。 */
static volatile uint8_t g_wait_wakeup_tone = 0;
/* 上面这个等待的兜底定时器: 提示音可能压根没播 (语音输出被关、音效资源缺失、
 * 播放器报错、被更高优先级提示音抢占), 而唯一的清除点是播放完成事件 ——
 * 没有兜底的话麦克风会一直不开, 会话哑到 idle 超时 (表现为"唤醒有反应但没应答")。 */
static TimerHandle_t g_wakeup_tone_wait_timer = NULL;
#define WAKEUP_TONE_WAIT_TIMEOUT_MS (2500U)
static session_objrec_t objrec = NULL;
static uint8_t *voice_cloud_token = NULL;
static uint8_t full_duplex = 0;
static volatile uint32_t g_objrec_request_seq = 0;
static volatile uint32_t g_objrec_accept_seq = 0;
static volatile bool g_tts_text_cancelled = false;

/* 发起云端交互后延迟多少时间开始发送音频 */
#define PCM_SEND_AFTER_CLOUD_CHAT_START_MS (0)

#define VOICE_CLOUD_TTS_TEXT 1
#define RESOURCE_UPDATE_REBOOT_DELAY_MS_DEFAULT 3000U
#define TTS_TIMELINE_MAX_SENTENCES 128U
#define TTS_TIMELINE_TEXT_MAX 2048U
#define TTS_TIMELINE_DIAG_PREVIEW_BYTES 96U
#define TTS_TIMELINE_EMPTY_RETRY_MAX 3U
#define TTS_TIMELINE_EMPTY_RETRY_DELAY_MS 1500U
#define TTS_TIMELINE_URL_MAX 512U

static volatile uint32_t g_stream_text_generation = 1U;

typedef struct {
    uint32_t index;
    uint32_t start_index;
    uint32_t end_index;
    uint32_t start_ms;
    uint32_t end_ms;
} voice_tts_timeline_cursor_t;

typedef struct {
    uint32_t generation;
    uint32_t expected_index;
    uint32_t expected_start_index;
    uint32_t previous_end_ms;
    uint32_t cursor_count;
    uint32_t legacy_text_len;
    bool negotiated;
    bool valid;
    bool fallback;
    bool started;
    bool reported;
    bool ended;
    bool retry_scheduled;
    uint32_t empty_retry_count;
    voice_tts_timeline_cursor_t cursors[TTS_TIMELINE_MAX_SENTENCES];
    char legacy_text[TTS_TIMELINE_TEXT_MAX];
    char stream_url[TTS_TIMELINE_URL_MAX];
} voice_tts_timeline_state_t;

static voice_tts_timeline_state_t g_tts_timeline __psram_bss__;

static void voice_tts_timeline_log_invalid_payload(const char *data, bool json_valid,
                                                   bool text_valid)
{
    size_t len = data ? strlen(data) : 0U;
    size_t head_len = len < TTS_TIMELINE_DIAG_PREVIEW_BYTES ?
                      len : TTS_TIMELINE_DIAG_PREVIEW_BYTES;
    size_t tail_len = 0U;
    int error_offset = -1;

    if (!json_valid && data) {
        const char *error = cJSON_GetErrorPtr();
        if (error && error >= data && error <= data + len) {
            error_offset = (int)(error - data);
        }
    }

    LOGW("invalid timeline payload: len=%u, json_valid=%u, text_valid=%u, error_offset=%d",
         (unsigned)len, (unsigned)json_valid, (unsigned)text_valid, error_offset);
    if (head_len > 0U) {
        LOGH("timeline_payload_head", data, head_len);
    }
    if (len > head_len) {
        tail_len = len - head_len;
        if (tail_len > TTS_TIMELINE_DIAG_PREVIEW_BYTES) {
            tail_len = TTS_TIMELINE_DIAG_PREVIEW_BYTES;
        }
        LOGH("timeline_payload_tail", data + len - tail_len, tail_len);
    }
}

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
    voice_msg_cloud_session_interrupt_t interrupt = {
        .report_reply_position = 0,
    };

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
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, &interrupt, sizeof(interrupt));
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

/* 结束"等唤醒应答音"的等待: 清标志并停掉兜底定时器。
 * 所有清除 g_wait_wakeup_tone 的地方都应走这里, 否则残留的定时器会在
 * 下一个会话里提前把麦克风打开。 */
static void voice_wakeup_tone_wait_cancel(void)
{
    g_wait_wakeup_tone = 0;
    if (g_wakeup_tone_wait_timer != NULL) {
        xTimerStop(g_wakeup_tone_wait_timer, 0);
    }
}

/* 兜底: 唤醒应答音迟迟没播完 (或压根没播), 不能再让麦克风关着 ——
 * 直接放行音频上传, 与正常播放完成走同一条恢复路径。 */
static void wakeup_tone_wait_timeout_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!g_wait_wakeup_tone || !g_voice_session_active) {
        return;
    }

    LOGW("wakeup tone wait timeout (%u ms), resume audio upload anyway",
         (unsigned)WAKEUP_TONE_WAIT_TIMEOUT_MS);
    voice_wakeup_tone_wait_cancel();
    if (voice_cloud_upload_audio_resume() != 0) {
        LOGW("resume audio upload after wakeup tone timeout failed");
    }
}

static void voice_cloud_wakeup_tone_completed(void *unused, uint32_t msg_id,
                                              void *data, uint32_t len,
                                              void *user_data)
{
    const voice_player_tone_completed_t *completed =
        (const voice_player_tone_completed_t *)data;

    (void)unused;
    (void)user_data;

    if (msg_id != VOICE_MSG_PLAYER_TONE_COMPLETED ||
        completed == NULL || len < sizeof(*completed) ||
        completed->source != VOICE_PLAYER_TONE_SOURCE_WAKEUP) {
        return;
    }

    if (!g_wait_wakeup_tone || !g_voice_session_active) {
        return;
    }

    voice_wakeup_tone_wait_cancel();
    LOGI("wakeup tone completed, resume audio upload");
    if (voice_cloud_upload_audio_resume() != 0) {
        LOGW("resume audio upload after wakeup tone failed");
    }
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

    if (g_wait_wakeup_tone) {
        LOGI("upload audio resume deferred until wakeup tone completes");
        return 0;
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

static void voice_tts_timeline_reset(bool negotiated, uint32_t generation,
                                     const char *url)
{
    taskENTER_CRITICAL();
    g_tts_timeline.generation = generation;
    g_tts_timeline.expected_index = 0;
    g_tts_timeline.expected_start_index = 0;
    g_tts_timeline.previous_end_ms = 0;
    g_tts_timeline.cursor_count = 0;
    g_tts_timeline.legacy_text_len = 0;
    g_tts_timeline.legacy_text[0] = '\0';
    g_tts_timeline.negotiated = negotiated;
    g_tts_timeline.valid = negotiated;
    g_tts_timeline.fallback = false;
    g_tts_timeline.started = false;
    g_tts_timeline.reported = false;
    g_tts_timeline.ended = false;
    g_tts_timeline.retry_scheduled = false;
    g_tts_timeline.empty_retry_count = 0;
    g_tts_timeline.stream_url[0] = '\0';
    if (url && url[0] != '\0') {
        strncpy(g_tts_timeline.stream_url, url,
                sizeof(g_tts_timeline.stream_url) - 1U);
        g_tts_timeline.stream_url[sizeof(g_tts_timeline.stream_url) - 1U] = '\0';
    }
    taskEXIT_CRITICAL();
}

static bool voice_tts_json_uint32(const cJSON *item, uint32_t *value)
{
    double number;
    uint32_t converted;

    if (!item || !value || !cJSON_IsNumber(item)) {
        return false;
    }

    number = item->valuedouble;
    if (number < 0.0 || number > (double)UINT32_MAX) {
        return false;
    }

    converted = (uint32_t)number;
    if ((double)converted != number) {
        return false;
    }

    *value = converted;
    return true;
}

static bool voice_tts_utf8_codepoint_count(const char *text, uint32_t *count)
{
    const uint8_t *p = (const uint8_t *)text;
    uint32_t total = 0;

    if (!text || !count) {
        return false;
    }

    while (*p) {
        uint32_t codepoint;
        uint8_t continuation_count;

        if (*p < 0x80) {
            codepoint = *p++;
            continuation_count = 0;
        } else if ((*p & 0xE0) == 0xC0) {
            codepoint = *p++ & 0x1F;
            continuation_count = 1;
            if (codepoint < 2) {
                return false;
            }
        } else if ((*p & 0xF0) == 0xE0) {
            codepoint = *p++ & 0x0F;
            continuation_count = 2;
        } else if ((*p & 0xF8) == 0xF0) {
            codepoint = *p++ & 0x07;
            continuation_count = 3;
        } else {
            return false;
        }

        for (uint8_t i = 0; i < continuation_count; i++) {
            if ((p[i] & 0xC0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (p[i] & 0x3F);
        }
        p += continuation_count;

        if ((continuation_count == 2 && codepoint < 0x800) ||
            (continuation_count == 3 && codepoint < 0x10000) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
            codepoint > 0x10FFFF) {
            return false;
        }
        total++;
    }

    *count = total;
    return true;
}

static bool voice_tts_timeline_append_legacy_text(const char *text)
{
    size_t text_len = strlen(text);
    size_t remaining = sizeof(g_tts_timeline.legacy_text) -
                       g_tts_timeline.legacy_text_len - 1U;

    if (text_len > remaining) {
        return false;
    }

    memcpy(g_tts_timeline.legacy_text + g_tts_timeline.legacy_text_len,
           text, text_len);
    g_tts_timeline.legacy_text_len += (uint32_t)text_len;
    g_tts_timeline.legacy_text[g_tts_timeline.legacy_text_len] = '\0';
    return true;
}

static void voice_tts_timeline_publish_fallback(const char *reason)
{
    if (g_tts_timeline.fallback) {
        return;
    }

    LOGW("timeline unavailable, fallback legacy subtitle timing: %s",
         reason ? reason : "unknown");
    taskENTER_CRITICAL();
    g_tts_timeline.valid = false;
    g_tts_timeline.fallback = true;
    taskEXIT_CRITICAL();
    voice_msg_pub(VOICE_MSG_CLOUD_TTS_TIMELINE_FALLBACK, NULL, 0);
}

static void voice_tts_timeline_publish_entry(const voice_msg_tts_timeline_t *entry)
{
    uint32_t size = sizeof(*entry) + entry->text_len + 1U;
    voice_msg_pub(VOICE_MSG_CLOUD_TTS_TIMELINE_UPDATE, (void *)entry, size);
}

static void stream_timeline_cb_handle(int evt, const char *data, void *user);

struct voice_tts_timeline_retry_ctx {
    uint32_t generation;
    char url[TTS_TIMELINE_URL_MAX];
};

static void voice_tts_timeline_retry_complete(void *user_data, bool completed,
                                              bool interrupted)
{
    (void)completed;
    (void)interrupted;
    if (user_data) {
        lisa_mem_free(user_data);
    }
}

static void voice_tts_timeline_retry_task(void *user_data, bool *should_stop)
{
    struct voice_tts_timeline_retry_ctx *ctx = user_data;
    bool valid = false;

    if (!ctx || !should_stop) {
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(TTS_TIMELINE_EMPTY_RETRY_DELAY_MS));
    if (*should_stop) {
        return;
    }

    taskENTER_CRITICAL();
    valid = ctx->generation == g_stream_text_generation &&
            ctx->generation == g_tts_timeline.generation &&
            !g_tts_timeline.started && !g_tts_timeline.ended;
    if (ctx->generation == g_tts_timeline.generation) {
        g_tts_timeline.retry_scheduled = false;
    }
    taskEXIT_CRITICAL();

    if (!valid) {
        return;
    }

    LOGI("retry empty TTS timeline stream, generation=%u", (unsigned)ctx->generation);
    if (lsc_stream_text_request_thread_async(
            ctx->url, stream_timeline_cb_handle,
            (void *)(uintptr_t)ctx->generation, false) != 0) {
        LOGW("retry empty TTS timeline stream request failed");
    }
}

static bool voice_tts_timeline_schedule_empty_retry(uint32_t generation)
{
    struct voice_tts_timeline_retry_ctx *ctx;
    async_task_t *task;
    bool schedule = false;

    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }

    taskENTER_CRITICAL();
    if (generation == g_stream_text_generation &&
        generation == g_tts_timeline.generation &&
        !g_tts_timeline.started && !g_tts_timeline.ended &&
        !g_tts_timeline.retry_scheduled &&
        g_tts_timeline.empty_retry_count < TTS_TIMELINE_EMPTY_RETRY_MAX &&
        g_tts_timeline.stream_url[0] != '\0') {
        g_tts_timeline.retry_scheduled = true;
        g_tts_timeline.empty_retry_count++;
        ctx->generation = generation;
        strncpy(ctx->url, g_tts_timeline.stream_url, sizeof(ctx->url) - 1U);
        ctx->url[sizeof(ctx->url) - 1U] = '\0';
        schedule = true;
    }
    taskEXIT_CRITICAL();

    if (!schedule) {
        lisa_mem_free(ctx);
        return false;
    }

    task = async_task_create("tts_timeline_retry", 3072, 5,
                             voice_tts_timeline_retry_task,
                             voice_tts_timeline_retry_complete, ctx);
    if (!task || async_task_start(task) != 0) {
        LOGW("create empty TTS timeline retry task failed");
        if (task) {
            async_task_destroy(task);
        }
        taskENTER_CRITICAL();
        if (generation == g_tts_timeline.generation) {
            g_tts_timeline.retry_scheduled = false;
        }
        taskEXIT_CRITICAL();
        lisa_mem_free(ctx);
        return false;
    }

    LOGI("scheduled empty TTS timeline retry %u/%u, generation=%u",
         (unsigned)g_tts_timeline.empty_retry_count,
         (unsigned)TTS_TIMELINE_EMPTY_RETRY_MAX, (unsigned)generation);
    return true;
}

static void stream_timeline_cb_handle(int evt, const char *data, void *user)
{
    uint32_t generation = (uint32_t)(uintptr_t)user;

    if (generation == 0U || generation != g_stream_text_generation ||
        generation != g_tts_timeline.generation) {
        return;
    }

    if (g_tts_timeline.ended) {
        return;
    }

    if (evt == SSE_EVT_DONE) {
        if (!g_tts_timeline.started && !g_tts_timeline.fallback &&
            voice_tts_timeline_schedule_empty_retry(generation)) {
            LOGW("empty TTS timeline stream, retry pending");
            return;
        }
        g_tts_timeline.ended = true;
        if (g_tts_timeline.fallback) {
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_END, NULL, 0);
        } else if (g_tts_timeline.started) {
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_TIMELINE_END, NULL, 0);
        } else {
            voice_tts_timeline_publish_fallback("empty timeline stream");
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_END, NULL, 0);
        }
        return;
    }

    if (evt == SSE_EVT_ABORT) {
        g_tts_timeline.ended = true;
        voice_tts_timeline_publish_fallback("timeline stream aborted");
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_END, NULL, 0);
        return;
    }

    if (evt != SSE_EVT_DATA || !data) {
        return;
    }

    cJSON *root = cJSON_Parse(data);
    cJSON *text_item = root ? cJSON_GetObjectItem(root, "text") : NULL;
    const char *text = (text_item && cJSON_IsString(text_item) && text_item->valuestring) ?
                       text_item->valuestring : NULL;

    if (g_tts_timeline.fallback) {
        if (text && text[0] != '\0') {
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, (void *)text, strlen(text) + 1U);
        }
        cJSON_Delete(root);
        return;
    }

    if (!root || !text || text[0] == '\0') {
        voice_tts_timeline_log_invalid_payload(data, root != NULL,
                                               text != NULL && text[0] != '\0');
        cJSON_Delete(root);
        voice_tts_timeline_publish_fallback("invalid timeline JSON or text");
        return;
    }

    bool text_saved = voice_tts_timeline_append_legacy_text(text);
    uint32_t codepoints = 0;
    uint32_t index = 0;
    uint32_t start_index = 0;
    uint32_t end_index = 0;
    uint32_t start_ms = 0;
    uint32_t end_ms = 0;
    bool utf8_valid = text && voice_tts_utf8_codepoint_count(text, &codepoints);
    bool fields_valid = root &&
        voice_tts_json_uint32(cJSON_GetObjectItem(root, "index"), &index) &&
        voice_tts_json_uint32(cJSON_GetObjectItem(root, "start_index"), &start_index) &&
        voice_tts_json_uint32(cJSON_GetObjectItem(root, "end_index"), &end_index) &&
        voice_tts_json_uint32(cJSON_GetObjectItem(root, "start_ms"), &start_ms) &&
        voice_tts_json_uint32(cJSON_GetObjectItem(root, "end_ms"), &end_ms);
    size_t text_len = text ? strlen(text) : 0U;
    bool sequence_valid = fields_valid &&
        index == g_tts_timeline.expected_index &&
        start_index == g_tts_timeline.expected_start_index &&
        end_index > start_index && end_index - start_index == codepoints &&
        end_ms > start_ms && start_ms >= g_tts_timeline.previous_end_ms &&
        g_tts_timeline.cursor_count < TTS_TIMELINE_MAX_SENTENCES;
    bool valid = utf8_valid && fields_valid && sequence_valid &&
        text_len <= UINT16_MAX;

    if (!valid) {
        LOGW("timeline validation failed: saved=%u utf8=%u fields=%u sequence=%u "
             "text_len=%u codepoints=%u index=%u expected_index=%u "
             "start_index=%u expected_start_index=%u end_index=%u "
             "start_ms=%u previous_end_ms=%u end_ms=%u cursor_count=%u",
             (unsigned)text_saved, (unsigned)utf8_valid, (unsigned)fields_valid,
             (unsigned)sequence_valid, (unsigned)text_len, (unsigned)codepoints,
             (unsigned)index, (unsigned)g_tts_timeline.expected_index,
             (unsigned)start_index, (unsigned)g_tts_timeline.expected_start_index,
             (unsigned)end_index, (unsigned)start_ms,
             (unsigned)g_tts_timeline.previous_end_ms, (unsigned)end_ms,
             (unsigned)g_tts_timeline.cursor_count);
        voice_tts_timeline_publish_fallback("invalid sequence, range, or timestamp");
        if (text && text[0] != '\0') {
            voice_msg_pub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, (void *)text, text_len + 1U);
        }
        cJSON_Delete(root);
        return;
    }

    uint32_t message_size = sizeof(voice_msg_tts_timeline_t) + strlen(text) + 1U;
    voice_msg_tts_timeline_t *message = lisa_mem_alloc(message_size);
    if (!message) {
        cJSON_Delete(root);
        voice_tts_timeline_publish_fallback("timeline message allocation failed");
        return;
    }

    message->index = index;
    message->start_index = start_index;
    message->end_index = end_index;
    message->start_ms = start_ms;
    message->end_ms = end_ms;
    message->text_len = (uint16_t)strlen(text);
    message->reserved = 0;
    memcpy(message->text, text, message->text_len + 1U);

    taskENTER_CRITICAL();
    voice_tts_timeline_cursor_t *cursor =
        &g_tts_timeline.cursors[g_tts_timeline.cursor_count];
    cursor->index = index;
    cursor->start_index = start_index;
    cursor->end_index = end_index;
    cursor->start_ms = start_ms;
    cursor->end_ms = end_ms;
    g_tts_timeline.cursor_count++;
    g_tts_timeline.expected_index = index + 1U;
    g_tts_timeline.expected_start_index = end_index;
    g_tts_timeline.previous_end_ms = end_ms;
    taskEXIT_CRITICAL();

    if (!g_tts_timeline.started) {
        g_tts_timeline.started = true;
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_TIMELINE_START, NULL, 0);
    }
    voice_tts_timeline_publish_entry(message);

    lisa_mem_free(message);
    cJSON_Delete(root);
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

static int voice_tts_text_stream_start(const char *url, bool timeline)
{
    uint32_t generation;

    taskENTER_CRITICAL();
    generation = ++g_stream_text_generation;
    if (generation == 0U) {
        generation = 1U;
        g_stream_text_generation = generation;
    }
    taskEXIT_CRITICAL();

    /* Invalidate and drain the old stream before resetting shared subtitle state. */
    lsc_stream_text_request_thread_abort_all();
    voice_tts_timeline_reset(timeline, generation, url);

    return lsc_stream_text_request_thread_async(
        url,
        timeline ? stream_timeline_cb_handle : stream_text_cb_handle,
        (void *)(uintptr_t)generation,
        false);
}

void voice_cloud_tts_text_cancel(void)
{
    uint32_t generation;

    taskENTER_CRITICAL();
    generation = ++g_stream_text_generation;
    if (generation == 0U) {
        generation = 1U;
        g_stream_text_generation = generation;
    }
    taskEXIT_CRITICAL();

    lsc_stream_text_request_thread_abort_all();
    voice_tts_timeline_reset(false, generation, NULL);
    LOGI("cancel active TTS text stream, generation=%u", (unsigned)generation);
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
        voice_wakeup_tone_wait_cancel();
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
        voice_wakeup_tone_wait_cancel();
        voice_pcm_send_disable();
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
        break;
    case SESSION_VOICE_TTS:
        voice_msg_pub(VOICE_MSG_CLOUD_TTS_URL, (char *)data, size);
        break;
    case SESSION_VOICE_REPLY_URL:
        if (voice_tts_text_stream_start((char *)data, false) != 0) {
            LOGE("start TTS text stream failed");
        }
        break;
    case SESSION_VOICE_REPLY_TIMELINE_URL:
        if (voice_tts_text_stream_start((char *)data, true) != 0) {
            LOGE("start TTS timeline stream failed");
        }
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
        if (g_tts_text_cancelled) {
            LOGI("drop canceled text TTS URL");
            break;
        }
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
    r |= session_voice_add_evt_callback(voice_event_cb,
                                        SESSION_VOICE_REPLY_URL |
                                            SESSION_VOICE_REPLY_TIMELINE_URL,
                                        NULL);
#endif

    if (r != 0) {
        LOGE("session_voice_add_evt_callback failed");
        return r;
    }

    voice_msg_sub(VOICE_MSG_CLOUD_MCP, mcp_msg_cb_handle, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CALL_RESP, mcp_call_resp_cb_handle, NULL);

    if (voice_msg_sub(VOICE_MSG_PLAYER_TONE_COMPLETED,
                      voice_cloud_wakeup_tone_completed, NULL) != 0) {
        LOGE("failed to subscribe wakeup tone completion");
        return -1;
    }

#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
    g_pcm_send_en_timer = xTimerCreate("voice.cloud.pcm.en", pdMS_TO_TICKS(PCM_SEND_AFTER_CLOUD_CHAT_START_MS), pdFALSE,
                                       NULL, pcm_send_en_timer_cb);
    assert(g_pcm_send_en_timer != NULL);
#endif

    /* 等唤醒应答音的兜底定时器 (one-shot), 见 wakeup_tone_wait_timeout_cb */
    g_wakeup_tone_wait_timer = xTimerCreate("voice.cloud.wake.tone",
                                            pdMS_TO_TICKS(WAKEUP_TONE_WAIT_TIMEOUT_MS),
                                            pdFALSE, NULL, wakeup_tone_wait_timeout_cb);
    assert(g_wakeup_tone_wait_timer != NULL);

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
    bool preserve_tts_timeline = false;

    if (!cloud_init_done || !config) {
        return -1;
    }

    if (config->preserve_tts_timeline) {
        taskENTER_CRITICAL();
        preserve_tts_timeline =
            g_tts_timeline.negotiated &&
            !g_tts_timeline.ended &&
            g_tts_timeline.generation == g_stream_text_generation;
        taskEXIT_CRITICAL();
    }

    if (preserve_tts_timeline) {
        LOGI("preserve active TTS timeline while starting silent barge-in, generation=%u",
             (unsigned)g_stream_text_generation);
    } else {
        g_stream_text_generation++;
        if (g_stream_text_generation == 0U) {
            g_stream_text_generation = 1U;
        }
        voice_tts_timeline_reset(false, g_stream_text_generation, NULL);
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
    g_wait_wakeup_tone = config->wait_wakeup_tone ? 1 : 0;

    ret = session_voice_set_config(&voice_config);
    if (ret != 0) {
        LOGE("session_voice_set_config failed");
        voice_wakeup_tone_wait_cancel();
        return ret;
    }

    ret = session_voice_start_ex(preserve_tts_timeline);
    if (ret != 0) {
        LOGE("session_voice_start failed");
        voice_wakeup_tone_wait_cancel();
        return ret;
    }

    g_voice_session_active = 1;

    if (!g_wait_wakeup_tone) {
#if PCM_SEND_AFTER_CLOUD_CHAT_START_MS > 0
        if (xTimerStart(g_pcm_send_en_timer, pdMS_TO_TICKS(20)) != pdPASS) {
            LOGE("pcm send en timer start failed");
            g_voice_session_active = 0;
            return -1;
        }
#else
        voice_pcm_send_enable();
#endif
    } else {
        LOGI("wakeup session: wait for wakeup tone before audio upload");
        /* 兜底: 提示音没播或被丢弃时也要开麦, 否则整个会话收不到音频 */
        if (g_wakeup_tone_wait_timer != NULL &&
            xTimerStart(g_wakeup_tone_wait_timer,
                        pdMS_TO_TICKS(WAKEUP_TONE_WAIT_TIMEOUT_MS)) != pdPASS) {
            LOGW("wakeup tone fallback timer start failed, open mic now");
            voice_wakeup_tone_wait_cancel();
            voice_pcm_send_enable();
        }
    }

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
    voice_wakeup_tone_wait_cancel();
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
    voice_wakeup_tone_wait_cancel();
    return 0;
}

int voice_cloud_report_reply_interrupted(uint32_t playback_position_ms)
{
    voice_tts_timeline_cursor_t selected = {0};
    bool found = false;

    taskENTER_CRITICAL();
    if (g_tts_timeline.negotiated && g_tts_timeline.valid &&
        !g_tts_timeline.fallback && !g_tts_timeline.reported) {
        for (uint32_t i = 0; i < g_tts_timeline.cursor_count; i++) {
            if (g_tts_timeline.cursors[i].end_ms > playback_position_ms) {
                selected = g_tts_timeline.cursors[i];
                g_tts_timeline.reported = true;
                found = true;
                break;
            }
        }
    }
    taskEXIT_CRITICAL();

    if (!found) {
        LOGW("reply interruption cursor unavailable at %u ms",
             (unsigned)playback_position_ms);
        return -1;
    }

    return session_voice_reply_interrupted(selected.index,
                                           selected.start_index,
                                           selected.end_index);
}

int voice_cloud_chat_send_audio(uint8_t *data, int len)
{
    if (!g_record_stream_buffer) {
        return -1;
    }

    /* The wakeup stream is produced continuously; never queue audio while upload is paused. */
    if (!pcm_send_en) {
        return 0;
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

    g_stream_text_generation++;
    if (g_stream_text_generation == 0U) {
        g_stream_text_generation = 1U;
    }
    voice_tts_timeline_reset(false, g_stream_text_generation, NULL);
    
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
    voice_wakeup_tone_wait_cancel();

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

    g_tts_text_cancelled = false;
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
    return session_text_tts_synth(txt);
}

int voice_cloud_tts_cancel(void)
{
    g_tts_text_cancelled = true;
    return session_text_cancel();
}
