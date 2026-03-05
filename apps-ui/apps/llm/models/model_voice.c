#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "lisa_ui_invoke.h"
#include "lisa_ui_log.h"

#include "model_voice.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "app_datas.h"
#include "voice_msg.h"
#include "voice_cloud.h"
#include "lisa_kv.h"
#include "kv.h"
#if CONFIG_OTA
#include "kv_sys.h"
#endif
#include "img_helper.h"
#include "async_task.h"
#include "cJSON.h"
#include "lisa_ui_nav_scr_ids.h"
#include "model_alarm.h"

#endif

#define IMG_REC_MODE_LSCHAT 0
#define IMG_REC_MODE_MCP    1

#define MAX_STANDBY_TEXTS 10
#define MAX_TEXT_LENGTH 128
#define MAX_LOADING_TEXT_LENGTH 128

struct model_voice_context {
    uint32_t cloud_connected: 1;
    uint32_t inited: 1;
    uint32_t voice_en: 1;
    uint32_t running: 1;
    uint32_t tts_playing: 1;
    uint32_t img_rec_in_progress: 1;

    const struct model_voice_cb *cbs;
    model_voice_wakeup_mode_t wakeup_mode;
    void *arg;
    uint8_t img_rec_mode;
    char mcp_id[64];
    char prompt[64];

    uint32_t img_rec_task_id;

    // Standby text rotation
    char standby_texts[MAX_STANDBY_TEXTS][MAX_TEXT_LENGTH];
    uint32_t standby_text_count;
    uint32_t standby_interval_ms;
    bool standby_enabled;

#if CONFIG_OTA
    char wake_word[20];
#endif
};

static struct model_voice_context model_voice_ctx = {
    .voice_en = true,
    .inited = 0,
    .cloud_connected = 0,
    .wakeup_mode = MODEL_VOICE_WAKEUP_MODE_VOICE_SINGLE,
};

static char s_last_iat_text[128] = {0};

static void notify_standby_texts_changed(void);

#ifdef LISA_UI_PLATFORM_ARCS
static void voice_cloud_show_qrcode_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data);
static void voice_cloud_show_qrcode_ui_worker(void *arg, uint32_t arg_len);
#define SHOW_QRCODE_NAV_DELAY_MS 50
#endif

#if defined(CONFIG_OTA) && defined(LISA_UI_PLATFORM_ARCS)
static void model_voice_load_wake_word_from_kv(void)
{
    char *wake_word = NULL;

    lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &wake_word);
    if (wake_word != NULL && wake_word[0] != '\0') {
        strncpy(model_voice_ctx.wake_word, wake_word, sizeof(model_voice_ctx.wake_word) - 1);
        model_voice_ctx.wake_word[sizeof(model_voice_ctx.wake_word) - 1] = '\0';
    }
    lisa_kv_free(wake_word);
}
#endif

const char *model_voice_role_name_get(void)
{
#ifdef CONFIG_OTA
    if (strnlen(model_voice_ctx.wake_word, sizeof(model_voice_ctx.wake_word)) > 0) {
        return model_voice_ctx.wake_word;
    } else {
        return "小聆小聆";
    }
#else
    return "小聆小聆";
#endif
}

#ifdef CONFIG_OTA
static const char *make_wake_hint_text(const char *base_text)
{
    static char wake_hint[MAX_TEXT_LENGTH];
    const char *wake_word = model_voice_role_name_get();

    if (!base_text) {
        return "";
    }

    const char *placeholder = "#唤醒词#";
    char *pos = strstr(base_text, placeholder);
    if (pos) {
        size_t prefix_len = pos - base_text;
        size_t wake_word_len = strlen(wake_word);
        size_t suffix_len = strlen(pos + strlen(placeholder));

        if (prefix_len + wake_word_len + suffix_len > MAX_TEXT_LENGTH - 1) {
            return base_text; // 超出长度限制，返回原文本
        }

        char *p = wake_hint;
        strncpy(p, base_text, prefix_len);
        p += prefix_len;
        strncpy(p, wake_word, wake_word_len);
        p += wake_word_len;
        strncpy(p, pos + strlen(placeholder), suffix_len);
        p += suffix_len;
        *p = '\0';
    } else {
        strncpy(wake_hint, base_text, MAX_TEXT_LENGTH - 1);
        wake_hint[MAX_TEXT_LENGTH - 1] = '\0';
    }

    return wake_hint;
}
#endif

const char *model_voice_role_propmt_get(void)
{
#ifdef CONFIG_OTA
    return make_wake_hint_text(model_voice_ctx.prompt);
#else
    return model_voice_ctx.prompt;
#endif
}

model_voice_wakeup_mode_t model_voice_wakeup_mode_get(void)
{
    return model_voice_ctx.wakeup_mode;
}

int model_voice_wakeup_mode_set(model_voice_wakeup_mode_t mode)
{
    if (mode >= MODEL_VOICE_WAKEUP_MODE_MAX) {
        return -1;
    }

    LISA_UI_LOGI("voice wakeup mode set, mode: %d", mode);

    model_voice_ctx.wakeup_mode = mode;

    if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_wakeup_mode_changed) {
        model_voice_ctx.cbs->on_wakeup_mode_changed(model_voice_ctx.arg, mode);
    }

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN_ARG_BASE(mode, {
        struct app_datas *app_datas = get_app_datas();
        if (app_datas == NULL) {
            return;
        }

        app_datas->voice_work_mode = 0;

        if (_invoke_mode == MODEL_VOICE_WAKEUP_MODE_BUTTON) {
            app_datas->full_duplex = false;
            app_datas->voice_work_mode &= ~VOICE_WORK_MODE_VOICE_WAKEUP;
            app_datas->voice_work_mode |= VOICE_WORK_MODE_BUTTON_WAKEUP;
        } else if (_invoke_mode == MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI) {
            app_datas->full_duplex = true;
            app_datas->voice_work_mode &= ~VOICE_WORK_MODE_BUTTON_WAKEUP;
            app_datas->voice_work_mode |= VOICE_WORK_MODE_VOICE_WAKEUP;
        } else if (_invoke_mode == MODEL_VOICE_WAKEUP_MODE_VOICE_SINGLE) {
            app_datas->full_duplex = false;
            app_datas->voice_work_mode &= ~VOICE_WORK_MODE_BUTTON_WAKEUP;
            app_datas->voice_work_mode |= VOICE_WORK_MODE_VOICE_WAKEUP;
        } else {
            LISA_UI_LOGW("invalid mode to save, mode: %d", _invoke_mode);
        }

        voice_cloud_chat_stop();

        lisa_kv_set_bool(KV_KEY_FULL_DUPLEX, app_datas->full_duplex);
        LISA_UI_LOGI("save voice work mode: %d", (int)app_datas->voice_work_mode);
        lisa_kv_set_int(KV_KEY_WAKEUP_MODE, (int)app_datas->voice_work_mode);
    });
#endif

    return 0;
}

const char *model_voice_wakeup_mode_name_get(model_voice_wakeup_mode_t mode)
{
    switch (mode) {
        case MODEL_VOICE_WAKEUP_MODE_BUTTON:
            return "Button Wakeup";
        case MODEL_VOICE_WAKEUP_MODE_VOICE_SINGLE:
            return "Voice Wakeup (Single)";
        case MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI:
            return "Voice Wakeup (Multi)";
        default:
            return "Unknown";
    }
}

#ifdef LISA_UI_PLATFORM_ARCS

static void voice_cloud_tts_txt(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (msg_id == VOICE_MSG_CLOUD_TTS_TEXT_START) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_tts_text_start) {
                model_voice_ctx.cbs->on_tts_text_start(model_voice_ctx.arg);
            }
        });
    } else if (msg_id == VOICE_MSG_CLOUD_TTS_TEXT_UPDATE) {
        char *text = (char *)data;
        LISA_UI_INVOKE_UI_ARG_PTR(text, len, {
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_tts_text_update) {
                model_voice_ctx.cbs->on_tts_text_update(_invoke_text, model_voice_ctx.arg);
            }
        });
    } else if (msg_id == VOICE_MSG_CLOUD_TTS_TEXT_END) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_tts_text_end) {
                model_voice_ctx.cbs->on_tts_text_end(model_voice_ctx.arg);
            }
        });
    }
}

static void voice_cloud_iat_txt(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (msg_id == VOICE_MSG_CLOUD_IAT_START) {
        s_last_iat_text[0] = '\0';
        LISA_UI_INVOKE_UI_ARG_NONE({
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_iat_text_start) {
                model_voice_ctx.cbs->on_iat_text_start(model_voice_ctx.arg);
            }
        });
    } else if (msg_id == VOICE_MSG_CLOUD_IAT_UPDATE) {
        char *p = (char *)data;
        if (p && len > 0 && p[0] != '\0') {
            uint32_t cpy_len = len > (sizeof(s_last_iat_text) - 1) ? (sizeof(s_last_iat_text) - 1) : len;
            memcpy(s_last_iat_text, p, cpy_len);
            s_last_iat_text[cpy_len] = '\0';
        }
        LISA_UI_INVOKE_UI_ARG_PTR(p, len, {
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_iat_text_update) {
                model_voice_ctx.cbs->on_iat_text_update(_invoke_p, model_voice_ctx.arg);
            }
        });
    } else if (msg_id == VOICE_MSG_CLOUD_IAT_END) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_iat_text_end) {
                model_voice_ctx.cbs->on_iat_text_end(model_voice_ctx.arg);
            }
        });
    }
}


static void voice_cloud_session_starting(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    service_image_waiting_cancel();
    s_last_iat_text[0] = '\0';
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.running = 1;
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_start) {
            model_voice_ctx.cbs->on_start(model_voice_ctx.arg);
        }
    });
}

static void voice_cloud_session_finished(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.running = 0;
        
#ifdef LISA_UI_PLATFORM_ARCS
        LISA_UI_LOGI("Audio recognition stopped on session finished");
#endif
        
        LISA_UI_LOGI("Session finished, tts_playing=%d", model_voice_ctx.tts_playing);
        
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_finished) {
            model_voice_ctx.cbs->on_finished(model_voice_ctx.arg);
        }
    });
}

static void voice_cloud_tts_url_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_LOGI("Audio recognition stopped on TTS URL received");
#endif
}

static void voice_cloud_connected(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.cloud_connected = 1;
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_connected) {
            model_voice_ctx.cbs->on_connected(model_voice_ctx.arg);
        }
    });
}

static void voice_cloud_disconnected(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.cloud_connected = 0;
        model_voice_ctx.running = 0;

        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_disconnected) {
            model_voice_ctx.cbs->on_disconnected(model_voice_ctx.arg);
        }
    });
}

static void voice_cloud_emoji_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    char *emoji = (char *)data;
    LISA_UI_INVOKE_UI_ARG_PTR(emoji, len, {
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_emoji) {
            model_voice_ctx.cbs->on_emoji(model_voice_ctx.arg, _invoke_emoji);
        }
    });
}

static void voice_cloud_mcp_emoji_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    char *emoji = (char *)data;
    LISA_UI_INVOKE_UI_ARG_PTR(emoji, len, {
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_mcp_emoji) {
            model_voice_ctx.cbs->on_mcp_emoji(model_voice_ctx.arg, _invoke_emoji);
        }
    });
}

static void voice_cloud_mcp_loading_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (!data || len == 0) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_mcp_loading) {
                model_voice_ctx.cbs->on_mcp_loading(model_voice_ctx.arg, false, NULL);
            }
        });
        return;
    }

    char loading_text[MAX_LOADING_TEXT_LENGTH] = {0};
    uint32_t copy_len = len > (sizeof(loading_text) - 1) ? (sizeof(loading_text) - 1) : len;
    memcpy(loading_text, data, copy_len);

    char *text_ptr = loading_text;
    LISA_UI_INVOKE_UI_ARG_PTR(text_ptr, strlen(text_ptr) + 1, {
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_mcp_loading) {
            model_voice_ctx.cbs->on_mcp_loading(model_voice_ctx.arg, true, _invoke_text_ptr);
        }
    });
}

static void voice_cloud_tts_player_playing(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.tts_playing = 1;
        LISA_UI_LOGI("TTS playing started, tts_playing=1");
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_tts_playing) {
            model_voice_ctx.cbs->on_tts_playing(model_voice_ctx.arg);
        }
    });
}

static void voice_cloud_tts_player_stoped(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.tts_playing = 0;
        LISA_UI_LOGI("TTS playing stopped, tts_playing=0, running=%d", model_voice_ctx.running);
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_tts_stoped) {
            model_voice_ctx.cbs->on_tts_stoped(model_voice_ctx.arg);
        }
    });
}

static void voice_button_changed(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    voice_msg_button_evt_t *evt = (voice_msg_button_evt_t *)data;

    if (evt->button_id == 1) {
        if (evt->action == VOICE_MSG_BUTTON_ACTION_PRESS_DOWN) {
            LISA_UI_INVOKE_UI_ARG_NONE({
                model_voice_ctx.img_rec_mode = IMG_REC_MODE_LSCHAT;
                if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_image_preview) {
                    model_voice_ctx.cbs->on_image_preview(model_voice_ctx.arg);
                }
            });
        } else if (evt->action == VOICE_MSG_BUTTON_ACTION_SHORT_UP
                     || evt->action == VOICE_MSG_BUTTON_ACTION_LONG_UP
                    || evt->action == VOICE_MSG_BUTTON_ACTION_LONG_HOLD_UP) {
            LISA_UI_INVOKE_UI_ARG_NONE({
                model_voice_ctx.img_rec_mode = IMG_REC_MODE_LSCHAT;
                if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_image_rec) {
                    model_voice_ctx.cbs->on_image_rec(model_voice_ctx.arg);
                }
            });
        }
    } else if (evt->button_id == 2 && evt->action == VOICE_MSG_BUTTON_ACTION_CLICK) {
        LISA_UI_INVOKE_UI_ARG_NONE({
            if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_info_show) {
                model_voice_ctx.cbs->on_info_show(model_voice_ctx.arg);
            }
        });
    }
}

static void voice_mcp_image_recognition_stop_worker(void *arg, uint32_t len)
{
    uint32_t task_id = 0;

    /* 检查任务ID是否匹配，如果不匹配说明已被新任务打断 */
    if (arg && len >= sizeof(uint32_t)) {
        task_id = *(uint32_t *)arg;
        if (task_id != model_voice_ctx.img_rec_task_id) {
            LISA_UI_LOGW("Image recognition task %u canceled by newer task %u", task_id, model_voice_ctx.img_rec_task_id);
            return;
        }
    }

    if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_image_rec) {
        model_voice_ctx.cbs->on_image_rec(model_voice_ctx.arg);
    }

    /* 清除识别进行中标志 */
    model_voice_ctx.img_rec_in_progress = 0;
    LISA_UI_LOGI("Image recognition task %u completed, flag cleared", model_voice_ctx.img_rec_task_id);
}

static void voice_mcp_image_recognition(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    char *mcp_id = data;

    LISA_UI_INVOKE_UI_ARG_PTR(mcp_id, len, {
        /* 如果已有识别任务在进行，递增任务ID打断上一次 */
        if (model_voice_ctx.img_rec_in_progress) {
            model_voice_ctx.img_rec_task_id++;
            LISA_UI_LOGW("Image recognition already in progress, interrupting with new task %u", model_voice_ctx.img_rec_task_id);
        } else {
            /* 设置识别进行中标志 */
            model_voice_ctx.img_rec_in_progress = 1;
            model_voice_ctx.img_rec_task_id++;
            LISA_UI_LOGI("Starting image recognition task %u", model_voice_ctx.img_rec_task_id);
        }

        memcpy(model_voice_ctx.mcp_id, _invoke_mcp_id,
               _invoke_len > sizeof(model_voice_ctx.mcp_id) ? sizeof(model_voice_ctx.mcp_id) : _invoke_len);
        model_voice_ctx.img_rec_mode = IMG_REC_MODE_MCP;

        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_image_preview) {
            model_voice_ctx.cbs->on_image_preview(model_voice_ctx.arg);
        }

        /* 启动预览后200ms停止预览并进行图像识别，传递当前任务ID */
        static uint32_t current_task_id;
        current_task_id = model_voice_ctx.img_rec_task_id;
        lisa_ui_invoke_ui_delayed(voice_mcp_image_recognition_stop_worker, &current_task_id, sizeof(current_task_id), 200);
    });
}

static void voice_button_image_recognition(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_voice_ctx.img_rec_mode = IMG_REC_MODE_LSCHAT;

        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_image_preview) {
            model_voice_ctx.cbs->on_image_preview(model_voice_ctx.arg);
        }

        /* 延迟触发识别，给预览/抓帧留出时间 */
        lisa_ui_invoke_ui_delayed(voice_mcp_image_recognition_stop_worker, NULL, 0, 200);
    });
}

static void voice_mcp_image_url_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    char *url = (char *)data;
    if (!url || len == 0) {
        return;
    }

    LISA_UI_INVOKE_UI_ARG_PTR(url, len, {
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_image_url) {
            model_voice_ctx.cbs->on_image_url(model_voice_ctx.arg, _invoke_url);
        }
    });
}

static void notify_standby_texts_changed(void)
{
    if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_standby_texts_changed) {
        model_voice_ctx.cbs->on_standby_texts_changed(model_voice_ctx.arg);
    }
}

static void voice_cloud_show_qrcode_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_LOGI("Show qrcode message received in model_voice");
    lisa_ui_invoke_ui_delayed(voice_cloud_show_qrcode_ui_worker, NULL, 0, SHOW_QRCODE_NAV_DELAY_MS);
}

static void voice_cloud_show_qrcode_ui_worker(void *arg, uint32_t arg_len)
{
    (void)arg;
    (void)arg_len;

    if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_show_qrcode) {
        LISA_UI_LOGI("Invoking on_show_qrcode callback");
        model_voice_ctx.cbs->on_show_qrcode(model_voice_ctx.arg);
    } else {
        LISA_UI_LOGE("on_show_qrcode callback is NULL");
    }
}

static void voice_cloud_open_info_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_LOGI("Open info page message received in model_voice");
    LISA_UI_INVOKE_UI_ARG_NONE({
        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_info_show) {
            LISA_UI_LOGI("Invoking on_info_show callback");
            model_voice_ctx.cbs->on_info_show(model_voice_ctx.arg);
        } else {
            LISA_UI_LOGE("on_info_show callback is NULL");
        }
    });
}

static void voice_cloud_standby_texts_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    char *banner_json = (char *)data;
    if (!banner_json || len == 0) {
        LISA_UI_LOGE("Invalid standby texts banner data");
        return;
    }

    LISA_UI_LOGI("Standby texts banner received: %s", banner_json);

    LISA_UI_INVOKE_UI_ARG_PTR(banner_json, len, {
        // 解析 banner JSON 数据
        cJSON *banner = cJSON_Parse(_invoke_banner_json);
        if (!banner) {
            LISA_UI_LOGE("Failed to parse banner JSON");
            return;
        }

        // 获取 resources 数组
        cJSON *resources = cJSON_GetObjectItem(banner, "resources");
        if (!resources || !cJSON_IsArray(resources)) {
            LISA_UI_LOGE("Invalid resources array");
            cJSON_Delete(banner);
            return;
        }

        // 获取 interval_ms
        cJSON *interval_ms_item = cJSON_GetObjectItem(banner, "interval_ms");
        uint32_t interval_ms = 3000;  // 默认3秒
        if (interval_ms_item && cJSON_IsNumber(interval_ms_item)) {
            interval_ms = (uint32_t)interval_ms_item->valueint;
        }

        int resources_count = cJSON_GetArraySize(resources);
        if (resources_count <= 0 || resources_count > MAX_STANDBY_TEXTS) {
            LISA_UI_LOGE("Invalid resources count: %d", resources_count);
            cJSON_Delete(banner);
            return;
        }

        // 提取文本数组
        int valid_count = 0;
        for (int i = 0; i < resources_count && i < MAX_STANDBY_TEXTS; i++) {
            cJSON *resource = cJSON_GetArrayItem(resources, i);
            if (resource) {
                cJSON *text_item = cJSON_GetObjectItem(resource, "text");
                if (text_item && cJSON_IsString(text_item) && strlen(text_item->valuestring) > 0) {
                    strncpy(model_voice_ctx.standby_texts[valid_count], text_item->valuestring, MAX_TEXT_LENGTH - 1);
                    model_voice_ctx.standby_texts[valid_count][MAX_TEXT_LENGTH - 1] = '\0';
                    LISA_UI_LOGI("Found standby text %d: %s", valid_count, model_voice_ctx.standby_texts[valid_count]);
                    valid_count++;
                }
            }
        }

        

        cJSON_Delete(banner);

        if (valid_count > 0) {
            // 更新配置
            model_voice_ctx.standby_text_count = valid_count;
            model_voice_ctx.standby_interval_ms = interval_ms > 0 ? interval_ms : 3000;
            model_voice_ctx.standby_enabled = true;
            
            LISA_UI_LOGI("Successfully set %d standby texts with interval %dms", valid_count, interval_ms);
            
            notify_standby_texts_changed();
        } else {
            LISA_UI_LOGE("No valid texts found");
            model_voice_ctx.standby_enabled = false;
            model_voice_ctx.standby_text_count = 0;
            model_voice_ctx.standby_interval_ms = 0;
            notify_standby_texts_changed();
        }
    });
}

#endif


#ifdef CONFIG_OTA
static void voice_cloud_ota_state_change(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    ota_state_t *state = (ota_state_t *)data;

    LISA_UI_INVOKE_UI_ARG_PTR(state, sizeof(ota_state_t), {
        if (_invoke_state->state == OTA_STATE_UP_TO_DATE) {
            strncpy(model_voice_ctx.wake_word, _invoke_state->wake_word, sizeof(model_voice_ctx.wake_word) - 1);
        }

        if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_ota_state_change) {
            model_voice_ctx.cbs->on_ota_state_change(_invoke_state, model_voice_ctx.arg);
        }
    });
}
#endif


int model_voice_off(void)
{
    model_voice_ctx.voice_en = false;

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN_ARG_NONE({
        struct app_datas *app_datas = get_app_datas();
        if (app_datas == NULL) {
            return;
        }
        
        alarm_data_t alarm_data;
        bool alarm_is_ringing = (model_alarm_get_data(&alarm_data) == 0 && alarm_data.timestamp > 0);
        
        if (!alarm_is_ringing) {
            app_datas->can_wakeup = false;
            LISA_UI_LOGI("Voice off: can_wakeup set to false");
        } else {
            LISA_UI_LOGI("Voice off: alarm is ringing, keeping can_wakeup=%d", app_datas->can_wakeup);
        }
        
        voice_cloud_chat_stop();
    });
#endif

    return 0;
}

int model_voice_on(void)
{
    model_voice_ctx.voice_en = true;

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN_ARG_NONE({
        struct app_datas *app_datas = get_app_datas();
        if (app_datas == NULL) {
            return;
        }
        app_datas->can_wakeup = true;
    });
#endif

    return 0;
}

int model_voice_init(void)
{
    if (model_voice_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    struct app_datas *app_datas = get_app_datas();
    if (app_datas->voice_work_mode & VOICE_WORK_MODE_BUTTON_WAKEUP) {
        model_voice_ctx.wakeup_mode = MODEL_VOICE_WAKEUP_MODE_BUTTON;
    } else {
        if (app_datas->full_duplex) {
            model_voice_ctx.wakeup_mode = MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI;
        } else {
            model_voice_ctx.wakeup_mode = MODEL_VOICE_WAKEUP_MODE_VOICE_SINGLE;
        }
    }
    strncpy(model_voice_ctx.prompt, app_datas->wakeup_prompt, sizeof(model_voice_ctx.prompt) - 1);
    model_voice_ctx.prompt[sizeof(model_voice_ctx.prompt) - 1] = 0;

#ifdef CONFIG_OTA
    model_voice_load_wake_word_from_kv();
#endif

    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, voice_cloud_session_starting, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_cloud_session_finished, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, voice_cloud_tts_url_received, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_START, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_END, voice_cloud_tts_txt, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_IAT_START, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_END, voice_cloud_iat_txt, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_cloud_disconnected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_EMOJI, voice_cloud_emoji_received, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_EMOJI, voice_cloud_mcp_emoji_received, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_LOADING, voice_cloud_mcp_loading_received, NULL);

    voice_msg_sub(VOICE_MSG_PLAYER_TTS_PLAYING, voice_cloud_tts_player_playing, NULL);
    voice_msg_sub(VOICE_MSG_PLAYER_TTS_STOPED, voice_cloud_tts_player_stoped, NULL);
    voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, voice_button_changed, NULL);
    voice_msg_sub(VOICE_MSG_BUTTON_IMAGE_RECOGNITION, voice_button_image_recognition, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_IMAGE_RECOGNITION, voice_mcp_image_recognition, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_IMAGE_URL, voice_mcp_image_url_received, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_STANDBY_TEXTS, voice_cloud_standby_texts_received, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SHOW_QRCODE, voice_cloud_show_qrcode_received, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_OPEN_INFO, voice_cloud_open_info_received, NULL);

#ifdef CONFIG_OTA
    voice_msg_sub(VOICE_MSG_OTA_CHECKING, voice_cloud_ota_state_change, NULL);
    voice_msg_sub(VOICE_MSG_OTA_UPDATING, voice_cloud_ota_state_change, NULL);
    voice_msg_sub(VOICE_MSG_OTA_UP_TO_DATE, voice_cloud_ota_state_change, NULL);
#endif
    
    model_voice_ctx.cloud_connected = (uint32_t)(voice_cloud_is_connected() ? 1 : 0);
    LISA_UI_LOGI("Sync cloud state on init: connected=%d", (int)model_voice_ctx.cloud_connected);
#endif

    model_voice_ctx.inited = 1;

    return 0;
}

int model_voice_cb_register(const struct model_voice_cb *cb, void *arg)
{
    model_voice_ctx.cbs = cb;
    model_voice_ctx.arg = arg;

    if (model_voice_ctx.cbs && model_voice_ctx.cbs->on_wakeup_mode_changed) {
        model_voice_ctx.cbs->on_wakeup_mode_changed(model_voice_ctx.arg, model_voice_ctx.wakeup_mode);
    }

    return 0;
}

int model_voice_cb_unregister(const struct model_voice_cb *cb)
{
    model_voice_ctx.cbs = NULL;

    return 0;
}

uint8_t model_voice_cloud_is_connected(void)
{
    return model_voice_ctx.cloud_connected;
}

uint8_t model_voice_cloud_is_running(void)
{
    return model_voice_ctx.cloud_connected & model_voice_ctx.running;
}

uint8_t model_voice_tts_is_playing(void)
{
    return (uint8_t)model_voice_ctx.tts_playing;
}

const char *model_voice_last_iat_text_get(void)
{
    return s_last_iat_text;
}

uint8_t model_voice_img_rec_is_mcp(void)
{
    return (uint8_t)(model_voice_ctx.img_rec_mode == IMG_REC_MODE_MCP);
}

uint32_t model_voice_standby_text_count_get(void)
{
    if (!model_voice_ctx.standby_enabled) {
        return 0;
    }
    return model_voice_ctx.standby_text_count;
}

uint32_t model_voice_standby_text_interval_ms_get(void)
{
    if (!model_voice_ctx.standby_enabled) {
        return 0;
    }
    return model_voice_ctx.standby_interval_ms;
}

const char *model_voice_standby_text_get(uint32_t index)
{
    if (!model_voice_ctx.standby_enabled) {
        return NULL;
    }
    if (index >= model_voice_ctx.standby_text_count) {
        return NULL;
    }
#ifdef CONFIG_OTA
    return make_wake_hint_text(model_voice_ctx.standby_texts[index]);
#else
    return model_voice_ctx.standby_texts[index];
#endif
}

#ifdef LISA_UI_PLATFORM_ARCS

struct voice_img_upload_msg {
    uint8_t *data;
    uint32_t len;
    uint32_t w;
    uint32_t h;
    uint8_t mode;
    uint8_t mcp_id[64];
};

struct voice_img_cloud_sync_msg {
    uint8_t *jpg_img;
    uint32_t len;
};

static void voice_img_rec(void *data, uint32_t len, struct voice_invoke_rsp *rsp)
{
    struct voice_img_cloud_sync_msg *msg = (struct voice_img_cloud_sync_msg *)data;
    int r = voice_cloud_image_recognition(msg->jpg_img, msg->len);
    rsp->err = r;
}

static cJSON *mcp_image_result_create(const char *url)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        cJSON_Delete(result);
        return NULL;
    }

    cJSON *content_item = cJSON_CreateObject();
    if (!content_item) {
        cJSON_Delete(content_array);
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(content_item, "type", "image");
    if (url && url[0] != '\0') {
        cJSON_AddStringToObject(content_item, "data", url);
    } else {
        cJSON_AddStringToObject(content_item, "data", "");
    }
    cJSON_AddStringToObject(content_item, "mimeType", "url");

    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);

    if (url && url[0] != '\0') {
        cJSON_AddBoolToObject(result, "isError", false);
    } else {
        cJSON_AddBoolToObject(result, "isError", true);
    }

    return result;
}

static void async_task_img_upload(void *p, bool *should_stop)
{
    struct voice_img_upload_msg *msg = (struct voice_img_upload_msg *)p;

    uint8_t *jpeg_data = NULL;
    uint32_t jpeg_len = 0;

    int ret = img_helper_rgb565_to_jpeg(msg->data, msg->len, msg->w, msg->h, &jpeg_data, &jpeg_len);
    if (ret != 0 || !jpeg_data) {
        lisa_ui_free(msg->data);
        lisa_ui_free(msg);
        LISA_UI_LOGE("Failed to encode JPEG: %d", ret);
        return;
    }

    if (msg->mode == IMG_REC_MODE_LSCHAT) {
        struct voice_invoke_rsp rsp = {
            .err = -1,
        };
        struct voice_img_cloud_sync_msg sync_msg = {
            .jpg_img = jpeg_data,
            .len = jpeg_len,
        };
        int r = voice_invoke_sync(voice_img_rec, &sync_msg, sizeof(struct voice_img_cloud_sync_msg),
                                  (struct voice_invoke_rsp *)&rsp, 1000);
    } else if (msg->mode == IMG_REC_MODE_MCP) {
        char *url = NULL;
        int upload_ret = voice_cloud_upload_jpeg_img(jpeg_data, jpeg_len, &url);
        cJSON *result = mcp_image_result_create(url);
        if (result) {
            if (url && upload_ret == 0) {
                LISA_UI_LOGI("jpeg upload success, url: %s", url);
            } else {
                LISA_UI_LOGE("jpeg upload failed");
            }
            mcp_tool_call_result_response(msg->mcp_id, result);
            cJSON_Delete(result);
        }

        if (url) {
            voice_cloud_jpeg_img_url_free(url);
        }
    }

    img_helper_jpeg_free(jpeg_data);
    lisa_ui_free(msg->data);
    lisa_ui_free(msg);
}
#endif

int model_voice_img_recognition(uint8_t *rgb565, uint32_t len, int width, int height)
{
#ifdef LISA_UI_PLATFORM_ARCS

    uint8_t *rgb565_cpy = lisa_ui_malloc(len);
    if (rgb565_cpy == NULL) {
        return -3;
    }

    memcpy(rgb565_cpy, rgb565, len);

    struct voice_img_upload_msg *msg = lisa_ui_malloc(sizeof(struct voice_img_upload_msg));
    if (msg == NULL) {
        lisa_ui_free(rgb565_cpy);
        return -1;
    }

    msg->data = rgb565_cpy;
    msg->len = len;
    msg->w = width;
    msg->h = height;
    msg->mode = model_voice_ctx.img_rec_mode;
    memcpy(msg->mcp_id, model_voice_ctx.mcp_id, sizeof(msg->mcp_id));

    async_task_t *async_task = async_task_create("img_rec", 4096, 5, async_task_img_upload, NULL, msg);
    if (async_task == NULL) {
        lisa_ui_free(rgb565_cpy);
        return -1;
    }

    async_task_start(async_task);

    return 0;
#else
    return -5;
#endif
}
