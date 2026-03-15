/**
 * @file xz_cloud.c
 * @brief 小智云端 API 实现
 */

#define TAG "xz_cloud"

#include "xz_cloud.h"
#include "xz_client.h"
#include "xz_ota.h"
#include "xz_tts_decoder.h"
#include "xz_tts_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_kv.h"
#include "evs_utils.h"
#include "assistant_controller.h"
#include "tone.h"
#include "app_client.h"
#include <string.h>

/** 云端配置 */
typedef struct {
    char url[256];
    char token[256];
    char device_id[64];
    char client_id[64];
} xz_cloud_config_t;

/** 小智云端结构 */
struct xz_cloud_s {
    xz_client_t client;
    struct app_client_s *app_client;
    listen_audiomgr_t *audio_mgr;

    /* TTS 播放器和解码器 */
    xz_tts_player_t tts_player;
    xz_tts_decoder_t tts_decoder;
    int tts_sample_rate;

    /* 状态 */
    bool wifi_connected;
    bool connected;
    bool interacting;
    bool tts_playing;

    /* 配置 */
    xz_cloud_config_t config;
};

/** 全局实例 */
static xz_cloud_t g_xz_cloud = NULL;

/* 前向声明 */
static int activate_runnable(void *arg);
static int connect_runnable(void *arg);

/**
 * @brief 从 KV 存储加载配置
 */
static void load_config(xz_cloud_config_t *config)
{
    char *value = NULL;

    /* 首先检查是否有 OTA 激活信息 */
    xz_ota_response_t activation;
    if (xz_ota_load_activation(&activation) == 0 && activation.success) {
        /* 使用激活信息 */
        strncpy(config->url, activation.websocket.url, sizeof(config->url) - 1);
        strncpy(config->token, activation.websocket.token, sizeof(config->token) - 1);
        LISA_LOGI(TAG, "Using activated credentials");
    } else {
        /* 加载 URL */
        if (lisa_kv_get_string("xz.url", &value) == 0 && value) {
            strncpy(config->url, value, sizeof(config->url) - 1);
            lisa_mem_free(value);
            value = NULL;
        } else {
            strcpy(config->url, "wss://api.tenclass.net/xiaozhi/v1/");
        }

        /* 加载 Token */
        if (lisa_kv_get_string("xz.token", &value) == 0 && value) {
            strncpy(config->token, value, sizeof(config->token) - 1);
            lisa_mem_free(value);
            value = NULL;
        } else {
            strcpy(config->token, "test-token");
        }
    }

    /* 加载或生成 Device ID (MAC 地址) */
    if (lisa_kv_get_string("xz.device_id", &value) == 0 && value) {
        strncpy(config->device_id, value, sizeof(config->device_id) - 1);
        lisa_mem_free(value);
    } else {
        /* 获取真实 MAC 地址 */
        if (xz_device_get_mac(config->device_id, sizeof(config->device_id)) == 0) {
            /* 保存到 KV 存储 */
            lisa_kv_set_string("xz.device_id", config->device_id);
            LISA_LOGI(TAG, "Generated and saved device_id: %s", config->device_id);
        } else {
            strcpy(config->device_id, "arcs_mini");
        }
    }

    /* 加载或生成 Client ID (UUID) */
    if (lisa_kv_get_string("xz.client_id", &value) == 0 && value) {
        strncpy(config->client_id, value, sizeof(config->client_id) - 1);
        lisa_mem_free(value);
    } else {
        /* 生成 UUID */
        if (xz_device_generate_uuid(config->client_id, sizeof(config->client_id)) == 0) {
            /* 保存到 KV 存储 */
            lisa_kv_set_string("xz.client_id", config->client_id);
            LISA_LOGI(TAG, "Generated and saved client_id: %s", config->client_id);
        } else {
            strcpy(config->client_id, "default");
        }
    }

    LISA_LOGI(TAG, "Config loaded: url=%s, device=%s, client=%s",
              config->url, config->device_id, config->client_id);
}

/**
 * @brief 激活可运行函数
 */
static int activate_runnable(void *arg)
{
    (void)arg;
    if (g_xz_cloud && g_xz_cloud->wifi_connected) {
        /* 检查是否已激活 */
        if (xz_ota_is_activated()) {
            LISA_LOGI(TAG, "Device already activated, proceeding to connection");
            evs_handler_post_runnable_delay(connect_runnable, NULL, 100);
            return 0;
        }

        LISA_LOGI(TAG, "Device not activated, starting OTA activation...");

        /* 执行 OTA 激活 */
        xz_device_info_t device;
        xz_ota_response_t response;

        if (xz_device_init_info(&device) != 0) {
            LISA_LOGE(TAG, "Failed to initialize device info for activation");
            return -1;
        }

        const char *ota_url = "https://api.tenclass.net/xiaozhi/ota/";
        if (xz_ota_activate(ota_url, &device, &response) != 0 || !response.success) {
            LISA_LOGE(TAG, "OTA activation failed, cannot connect to cloud");
            LISA_LOGE(TAG, "Please use 'xz_activate' command to activate manually");
            /* 不重试激活，避免网络风暴 */
            return -1;
        }

        LISA_LOGI(TAG, "Activation successful! Proceeding to connection...");
        /* 激活成功，尝试连接 */
        evs_handler_post_runnable_delay(connect_runnable, NULL, 100);
    }
    return 0;
}

/**
 * @brief 连接可运行函数
 */
static int connect_runnable(void *arg)
{
    (void)arg;
    if (g_xz_cloud && !g_xz_cloud->connected && g_xz_cloud->wifi_connected) {
        /* 检查是否已激活 */
        if (!xz_ota_is_activated()) {
            LISA_LOGW(TAG, "Device not activated, skipping connection");
            LISA_LOGI(TAG, "Please use 'xz_activate' command to activate device");
            return -1;
        }

        LISA_LOGI(TAG, "Connecting to xiaozhi cloud...");
        if (xz_client_connect(g_xz_cloud->client) == 0) {
            LISA_LOGI(TAG, "Connection initiated");
        } else {
            /* 重试 */
            evs_handler_post_runnable_delay(connect_runnable, NULL, 5000);
        }
    }
    return 0;
}

/**
 * @brief 客户端连接回调
 */
static void on_connected(void *user)
{
    (void)user;
    LISA_LOGI(TAG, "Connected to xiaozhi cloud");

    if (g_xz_cloud) {
        g_xz_cloud->connected = true;
    }

    /* 播放连接提示音 */
    extern void recongizer_play_audio_id(uint8_t id);
    recongizer_play_audio_id(TONE_ID_59);
}

/**
 * @brief 客户端断开回调
 */
static void on_disconnected(void *user)
{
    (void)user;
    LISA_LOGW(TAG, "Disconnected from xiaozhi cloud");

    if (g_xz_cloud) {
        g_xz_cloud->connected = false;
        g_xz_cloud->interacting = false;
    }

    /* 触发控制器事件 */
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);

    /* 如果 WiFi 连接正常，尝试重连 */
    if (g_xz_cloud && g_xz_cloud->wifi_connected) {
        evs_handler_post_runnable_delay(connect_runnable, NULL, 3000);
    }
}

/**
 * @brief 客户端错误回调
 */
static void on_error(int code, const char *message, void *user)
{
    (void)user;
    LISA_LOGE(TAG, "Error: code=%d, message=%s", code, message);
}

/**
 * @brief STT 文本回调
 */
static void on_stt_text(const char *text, bool is_final, void *user)
{
    (void)user;
    LISA_LOGI(TAG, "STT: %s (final=%d)", text, is_final);

    /* 触发控制器事件显示文本 */
    if (is_final) {
        /* 这里可以添加显示逻辑 */
    }
}

/**
 * @brief LLM 内容回调
 */
static void on_llm_content(const char *content, bool is_end, void *user)
{
    (void)user;
    LISA_LOGI(TAG, "LLM: %s (end=%d)", content, is_end);

    /* 触发控制器事件显示内容 */
    /* 这里可以添加显示逻辑 */
}

/**
 * @brief TTS 开始回调
 */
static void on_tts_start(void *user)
{
    xz_cloud_t cloud = (xz_cloud_t)user;
    if (!cloud) {
        return;
    }

    LISA_LOGI(TAG, "TTS started");

    /* 创建 TTS 播放器 */
    if (!cloud->tts_player) {
        xz_tts_player_config_t player_config = {
            .sample_rate = cloud->tts_sample_rate > 0 ? cloud->tts_sample_rate : 24000,
            .channels = 1,
            .bits = 16,
        };

        xz_tts_player_callbacks_t player_cbs = {
            .on_play_start = NULL,
            .on_play_complete = NULL,
            .on_error = NULL,
        };

        cloud->tts_player = xz_tts_player_create(cloud->audio_mgr, &player_config, &player_cbs);
        if (!cloud->tts_player) {
            LISA_LOGE(TAG, "Failed to create TTS player");
            return;
        }

        LISA_LOGI(TAG, "TTS player created: %dHz", player_config.sample_rate);
    }

    /* 创建并启动 TTS 解码器 */
    if (!cloud->tts_decoder) {
        /* 获取服务器配置的采样率 */
        int server_rate = xz_client_get_tts_sample_rate(cloud->client);
        if (server_rate > 0) {
            cloud->tts_sample_rate = server_rate;
        }

        xz_tts_decoder_config_t decoder_config = {
            .sample_rate = cloud->tts_sample_rate > 0 ? cloud->tts_sample_rate : 24000,
            .channels = 1,
        };

        cloud->tts_decoder = xz_tts_decoder_create(&decoder_config, cloud->tts_player);
        if (!cloud->tts_decoder) {
            LISA_LOGE(TAG, "Failed to create TTS decoder");
            return;
        }

        LISA_LOGI(TAG, "TTS decoder created: %dHz", decoder_config.sample_rate);
    }

    /* 启动解码器 */
    xz_tts_decoder_start(cloud->tts_decoder);
    xz_tts_player_start(cloud->tts_player);
    cloud->tts_playing = true;

    /* 触发控制器事件 */
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_START, NULL, 0);
}

/**
 * @brief TTS 数据回调 - 将 Opus 数据送入解码器
 */
static void on_tts_data(const uint8_t *data, uint32_t len, void *user)
{
    xz_cloud_t cloud = (xz_cloud_t)user;
    if (!cloud || !data || len == 0) {
        return;
    }

    /* 检查是否有解码器 */
    if (!cloud->tts_decoder) {
        LISA_LOGE(TAG, "TTS data received but no decoder");
        return;
    }

    /* 将 Opus 数据送入解码器 (在工作线程中处理) */
    xz_tts_decoder_write(cloud->tts_decoder, data, len);
}

/**
 * @brief TTS 结束回调
 */
static void on_tts_end(void *user)
{
    xz_cloud_t cloud = (xz_cloud_t)user;
    if (!cloud) {
        return;
    }

    LISA_LOGI(TAG, "TTS ended");

    /* 停止解码器 */
    if (cloud->tts_decoder) {
        xz_tts_decoder_stop(cloud->tts_decoder);
        xz_tts_decoder_destroy(cloud->tts_decoder);
        cloud->tts_decoder = NULL;
    }

    /* 结束播放流 */
    if (cloud->tts_player) {
        xz_tts_player_end_stream(cloud->tts_player);
    }

    cloud->tts_playing = false;

    /* 触发控制器事件 */
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_IDLE, NULL, 0);
}

/**
 * @brief IoT 指令回调
 */
static void on_iot_command(const char *command, const char *param, void *user)
{
    (void)user;
    LISA_LOGI(TAG, "IoT: command=%s, param=%s", command, param);

    /* 这里可以添加 IoT 控制逻辑 */
}

xz_cloud_t xz_cloud_create(struct app_client_s *app_client)
{
    /* 如果已存在，先销毁 */
    if (g_xz_cloud) {
        xz_cloud_destroy(g_xz_cloud);
    }

    /* 分配结构 */
    struct xz_cloud_s *cloud = (struct xz_cloud_s *)lisa_mem_calloc(1, sizeof(struct xz_cloud_s));
    if (!cloud) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    cloud->app_client = app_client;
    cloud->audio_mgr = app_client ? app_client->audio_mgr : NULL;
    cloud->wifi_connected = false;
    cloud->connected = false;
    cloud->interacting = false;
    cloud->tts_player = NULL;
    cloud->tts_sample_rate = 24000;  /* 默认 24kHz */
    cloud->tts_playing = false;

    /* 加载配置 */
    load_config(&cloud->config);

    /* 创建小智客户端 */
    xz_client_config_t client_config = {
        .url = cloud->config.url,
        .token = cloud->config.token,
        .device_id = cloud->config.device_id,
        .client_id = cloud->config.client_id,
        .timeout_ms = 10000,
    };

    cloud->client = xz_client_create(&client_config);
    if (!cloud->client) {
        LISA_LOGE(TAG, "Failed to create xiaozhi client");
        lisa_mem_free(cloud);
        return NULL;
    }

    /* 设置回调 */
    xz_client_callbacks_t callbacks = {
        .on_connected = on_connected,
        .on_disconnected = on_disconnected,
        .on_error = on_error,
        .on_stt_text = on_stt_text,
        .on_llm_content = on_llm_content,
        .on_tts_start = on_tts_start,
        .on_tts_data = on_tts_data,
        .on_tts_end = on_tts_end,
        .on_iot_command = on_iot_command,
        .user_data = cloud,
    };
    xz_client_set_callbacks(cloud->client, &callbacks);

    g_xz_cloud = cloud;

    LISA_LOGI(TAG, "XiaoZhi cloud created");
    return cloud;
}

void xz_cloud_destroy(xz_cloud_t cloud)
{
    if (!cloud) {
        return;
    }

    if (cloud->client) {
        xz_client_destroy(cloud->client);
    }

    if (cloud == g_xz_cloud) {
        g_xz_cloud = NULL;
    }

    lisa_mem_free(cloud);
}

void xz_cloud_process_wifi_connected(xz_cloud_t cloud)
{
    if (!cloud) {
        return;
    }

    cloud->wifi_connected = true;
    LISA_LOGI(TAG, "WiFi connected, checking activation status...");

    /* 延迟执行激活检查，等待系统稳定 */
    evs_handler_post_runnable_delay(activate_runnable, NULL, 1000);
}

void xz_cloud_process_wifi_disconnected(xz_cloud_t cloud)
{
    if (!cloud) {
        return;
    }

    cloud->wifi_connected = false;
    cloud->connected = false;
    cloud->interacting = false;

    LISA_LOGI(TAG, "WiFi disconnected");

    /* 断开客户端连接 */
    if (cloud->client) {
        xz_client_disconnect(cloud->client);
    }
}

void xz_cloud_txt(const char *txt)
{
    if (!g_xz_cloud || !g_xz_cloud->client) {
        LISA_LOGE(TAG, "Cloud not initialized");
        return;
    }

    if (!g_xz_cloud->connected) {
        LISA_LOGE(TAG, "Not connected");
        return;
    }

    xz_client_send_text(g_xz_cloud->client, txt);
}

void xz_cloud_tts(const char *text)
{
    /* TTS 与文本处理相同 */
    xz_cloud_txt(text);
}

bool xz_cloud_is_connected(void)
{
    return g_xz_cloud && g_xz_cloud->connected;
}

bool xz_cloud_is_wifi_connected(void)
{
    return g_xz_cloud && g_xz_cloud->wifi_connected;
}

void xz_cloud_audio(xz_cloud_t cloud, const char *audio, uint32_t len)
{
    if (!cloud || !cloud->client) {
        return;
    }

    if (!cloud->interacting) {
        /* 静默丢弃 */
        return;
    }

    /* audio 是 16kHz, 单声道, int16 PCM */
    /* len 是字节数 = samples * 2 */
    int samples = len / 2;
    xz_client_write_audio(cloud->client, (const int16_t *)audio, samples);
}

void xz_cloud_wakeup(xz_cloud_t cloud)
{
    if (!cloud || !cloud->client) {
        return;
    }

    if (!cloud->connected) {
        LISA_LOGE(TAG, "Not connected");
        extern void recongizer_play_audio_id(uint8_t id);
        recongizer_play_audio_id(TONE_ID_85);
        return;
    }

    LISA_LOGI(TAG, "Starting interaction");
    xz_client_start_interaction(cloud->client);
    cloud->interacting = true;

    /* 触发控制器事件 */
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
}

xz_cloud_t xz_cloud_get_instance(void)
{
    return g_xz_cloud;
}

void xz_cloud_stop_interaction(xz_cloud_t cloud)
{
    if (!cloud || !cloud->client) {
        return;
    }

    if (!cloud->interacting) {
        LISA_LOGW(TAG, "Not interacting, nothing to stop");
        return;
    }

    LISA_LOGI(TAG, "Stopping interaction (S key equivalent)");
    xz_client_stop_interaction(cloud->client);
    cloud->interacting = false;

    /* 触发控制器事件 */
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_IDLE, NULL, 0);
}

bool xz_cloud_is_interacting(xz_cloud_t cloud)
{
    return cloud && cloud->interacting;
}

int xz_cloud_activate(const char *server_url)
{
    xz_device_info_t device;
    xz_ota_response_t response;

    /* 默认激活服务器 */
    const char *default_url = "https://api.tenclass.net/xiaozhi/ota/";
    if (!server_url) {
        server_url = default_url;
    }

    LISA_LOGI(TAG, "Starting OTA activation to %s", server_url);

    /* 初始化设备信息 */
    if (xz_device_init_info(&device) != 0) {
        LISA_LOGE(TAG, "Failed to initialize device info");
        return -1;
    }

    /* 发送激活请求 */
    if (xz_ota_activate(server_url, &device, &response) != 0) {
        LISA_LOGE(TAG, "Activation failed: %s", response.error_message);
        return -1;
    }

    if (!response.success) {
        LISA_LOGE(TAG, "Activation failed: %s", response.error_message);
        return -1;
    }

    LISA_LOGI(TAG, "Activation successful!");
    LISA_LOGI(TAG, "  URL: %s", response.websocket.url);
    LISA_LOGI(TAG, "  Token: %.*s...", 20, response.websocket.token);

    /* 重新创建客户端以使用新凭证 */
    if (g_xz_cloud) {
        /* 销毁旧客户端 */
        if (g_xz_cloud->client) {
            if (g_xz_cloud->connected) {
                xz_client_disconnect(g_xz_cloud->client);
            }
            xz_client_destroy(g_xz_cloud->client);
            g_xz_cloud->client = NULL;
        }

        /* 重新加载配置（包含新的激活凭证） */
        load_config(&g_xz_cloud->config);

        /* 创建新客户端 */
        xz_client_config_t client_config = {
            .url = g_xz_cloud->config.url,
            .token = g_xz_cloud->config.token,
            .device_id = g_xz_cloud->config.device_id,
            .client_id = g_xz_cloud->config.client_id,
            .timeout_ms = 10000,
        };

        g_xz_cloud->client = xz_client_create(&client_config);
        if (!g_xz_cloud->client) {
            LISA_LOGE(TAG, "Failed to recreate client after activation");
            return -1;
        }

        /* 重新设置回调 */
        xz_client_callbacks_t callbacks = {
            .on_connected = on_connected,
            .on_disconnected = on_disconnected,
            .on_error = on_error,
            .on_stt_text = on_stt_text,
            .on_llm_content = on_llm_content,
            .on_tts_start = on_tts_start,
            .on_tts_data = on_tts_data,
            .on_tts_end = on_tts_end,
            .on_iot_command = on_iot_command,
            .user_data = g_xz_cloud,
        };
        xz_client_set_callbacks(g_xz_cloud->client, &callbacks);

        LISA_LOGI(TAG, "Client recreated with new credentials");

        /* 如果 WiFi 已连接，触发重连 */
        if (g_xz_cloud->wifi_connected) {
            LISA_LOGI(TAG, "Reconnecting with new credentials...");
            g_xz_cloud->connected = false;
            evs_handler_post_runnable_delay(connect_runnable, NULL, 1000);
        }
    }

    return 0;
}

bool xz_cloud_is_activated(void)
{
    return xz_ota_is_activated();
}
