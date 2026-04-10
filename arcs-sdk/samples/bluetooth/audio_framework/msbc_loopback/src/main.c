/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 音频框架示例 - mSBC 编解码回环测试（硬件模式）
 *
 * 演示如何使用 bt_audio_framework 配合 lisa_audio 驱动进行真实硬件测试：
 * 1. 使用 lisa_audio 采集麦克风数据
 * 2. 通过 capture session 进行 mSBC 编码
 * 3. 将编码后的 mSBC 帧送给 playback session 解码
 * 4. 将解码后的 PCM 数据通过 lisa_audio 播放到扬声器
 *
 * 数据流：麦克风 → lisa_audio采集 → mSBC编码 → mSBC解码 → lisa_audio播放 → 扬声器
 * 
 * 这是一个完整的硬件回环测试，可以听到从麦克风输入并经过编解码处理后的声音。
 */

#define LOG_TAG "bt_audio_sample"

#include <lisa_log.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bt_audio_framework.h"
#include "bt_audio_session.h"
#include "bt_audio_interface.h"

/* ========================================================================
 * 全局变量
 * ======================================================================== */
static bt_audio_session_handle_t g_capture_session = NULL;   /* 录音会话(编码) */
static bt_audio_session_handle_t g_playback_session = NULL;  /* 播放会话(解码) */
static volatile bool g_running = false;                      /* 运行标志 */

/* 音频接口 */
static const bt_audio_interface_ops_t *g_audio_interface = NULL;


/* ========================================================================
 * Session 数据回调（连接 session 和 interface）
 * ======================================================================== */

/**
 * @brief Capture 数据回调 - Session 需要 PCM 数据时从接口读取
 * @note 由 lisa_audio 驱动调用，将采集到的 PCM 数据送入 capture session
 * @note 数据流：麦克风 → lisa_audio → 本回调 → session 编码
 */
static void capture_data_callback(const void *buffer, size_t size, void *user_data)
{
    (void)user_data;
    
    /* 参数校验 */
    if (!g_capture_session || !buffer || size == 0) {
        return;  /* 静默忽略，避免在清理阶段产生大量日志 */
    }
    
    /* 将 PCM 数据写入 capture session 进行编码 */
    bt_audio_error_t ret = bt_audio_session_capture_write_pcm(g_capture_session, buffer, size);
    if (ret != BT_AUDIO_OK && ret != BT_AUDIO_ERR_INVALID_STATE) {
        /* 忽略 INVALID_STATE 错误（session 已停止，清理阶段正常情况）*/
        LISA_LOGE(LOG_TAG, "Failed to write PCM (encode): %d", ret);
    }
}

/**
 * @brief Playback 数据回调 - Session 解码后输出 PCM 到接口
 * @note 由 playback session 的播放线程调用
 * @note 数据流：session 解码 → 本回调 → interface->playback_write → lisa_audio → 扬声器
 */
static int playback_data_callback(const void *pcm_data, size_t size, void *user_data)
{
    (void)user_data;
    
    if (!g_audio_interface || !g_audio_interface->playback_write) {
        LISA_LOGW(LOG_TAG, "No audio interface available");
        return -1;
    }
    
    /* 通过 interface 将 PCM 写入硬件 */
    return g_audio_interface->playback_write(pcm_data, size);
}

/* ========================================================================
 * 会话事件回调
 * ======================================================================== */

/**
 * @brief 录音会话事件回调
 */
static void capture_event_callback(bt_audio_session_handle_t session,
                                   const bt_audio_event_t *event,
                                   void *user_data)
{
    switch (event->event) {
        case BT_AUDIO_EVENT_STARTED:
            LISA_LOGI(LOG_TAG, "Capture session started");
            break;
            
        case BT_AUDIO_EVENT_STOPPED:
            LISA_LOGI(LOG_TAG, "Capture session stopped");
            break;
            
        case BT_AUDIO_EVENT_OVERRUN:
            LISA_LOGW(LOG_TAG, "Capture buffer overrun");
            break;
            
        case BT_AUDIO_EVENT_ERROR:
            LISA_LOGE(LOG_TAG, "Capture session error");
            break;
            
        default:
            break;
    }
}

/**
 * @brief 播放会话事件回调
 */
static void playback_event_callback(bt_audio_session_handle_t session,
                                    const bt_audio_event_t *event,
                                    void *user_data)
{
    switch (event->event) {
        case BT_AUDIO_EVENT_STARTED:
            LISA_LOGI(LOG_TAG, "Playback session started");
            break;
            
        case BT_AUDIO_EVENT_STOPPED:
            LISA_LOGI(LOG_TAG, "Playback session stopped");
            break;
            
        case BT_AUDIO_EVENT_UNDERRUN:
            LISA_LOGW(LOG_TAG, "Playback buffer underrun");
            break;
            
        case BT_AUDIO_EVENT_ERROR:
            LISA_LOGE(LOG_TAG, "Playback session error");
            break;
            
        default:
            break;
    }
}

/* ========================================================================
 * 主要功能函数
 * ======================================================================== */

/**
 * @brief 初始化蓝牙音频框架
 */
static int init_bt_audio_framework(void)
{
    bt_audio_error_t ret;
    
    /* 初始化框架（会自动注册已配置的 codec 和 interface）*/
    ret = bt_audio_framework_init();
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize BT audio framework: %d", ret);
        return -1;
    }
    
    LISA_LOGI(LOG_TAG, "BT audio framework initialized, version: %s", 
              bt_audio_framework_get_version());

    /* 获取框架自动注册的音频接口 */
    g_audio_interface = bt_audio_interface_get_default();
    if (!g_audio_interface) {
        LISA_LOGE(LOG_TAG, "Failed to get audio interface");
        bt_audio_framework_deinit();
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Using audio interface: %s", g_audio_interface->name);
    
    return 0;
}

/**
 * @brief 创建录音会话（mSBC 编码）
 * @return 0 成功，-1 失败
 */
static int create_capture_session(void)
{
    bt_audio_error_t ret;
    
    /* 打开硬件录音接口 */
    bt_audio_format_t capture_format = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    };
    
    if (g_audio_interface && g_audio_interface->capture_open) {
        /* lisa_audio 使用事件回调机制，不需要 capture_read */
        ret = g_audio_interface->capture_open(&capture_format, capture_data_callback, NULL);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGE(LOG_TAG, "Failed to open capture interface: %d", ret);
            return -1;
        }
        LISA_LOGI(LOG_TAG, "Capture interface opened: 16kHz, mono, 16bit");
    }
    
    /* 配置录音会话 */
    bt_audio_session_config_t capture_config = {
        .codec_type = BT_CODEC_MSBC,            /* mSBC 编解码器 */
        .direction = BT_AUDIO_DIR_CAPTURE,      /* 录音方向 */
        .passthrough_mode = false,              /* 编解码模式：使用 mSBC 编码 */
        .event_callback = capture_event_callback,
        .playback_data_callback = NULL,
        .capture_data_callback = NULL,
        .user_data = NULL,
    };
    
    ret = bt_audio_session_create(&capture_config, &g_capture_session);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(LOG_TAG, "Failed to create capture session: %d", ret);
        if (g_audio_interface && g_audio_interface->capture_close) {
            g_audio_interface->capture_close();
        }
        return -1;
    }
    
    /* 配置 mSBC 编码格式 */
    bt_audio_codec_config_t codec_config = {
        .codec_type = BT_CODEC_MSBC,   /* mSBC 编码 */
        .format = {
            .sample_rate = 16000,      /* 16kHz 采样率（mSBC 标准）*/
            .channels = 1,             /* 单声道 */
            .bits_per_sample = 16,     /* 16位采样精度 */
        },
        .bitrate = 0,                  /* mSBC 固定比特率 */
        .frame_duration_us = 0,        /* mSBC 帧时长：7.5ms */
        .codec_specific_config = NULL,
        .config_size = 0,
    };
    
    ret = bt_audio_session_start(g_capture_session, &codec_config);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(LOG_TAG, "Failed to start capture session: %d", ret);
        bt_audio_session_destroy(g_capture_session);
        g_capture_session = NULL;
        if (g_audio_interface && g_audio_interface->capture_close) {
            g_audio_interface->capture_close();
        }
        return -1;
    }
    
    LISA_LOGI(LOG_TAG, "Capture session created and started (mSBC encode mode)");
    return 0;
}

/**
 * @brief 创建播放会话（mSBC 解码）
 * @return 0 成功，-1 失败
 */
static int create_playback_session(void)
{
    bt_audio_error_t ret;
    
    /* 打开硬件播放接口 */
    bt_audio_format_t playback_format = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    };
    
    if (g_audio_interface && g_audio_interface->playback_open) {
        ret = g_audio_interface->playback_open(&playback_format);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGE(LOG_TAG, "Failed to open playback interface: %d", ret);
            return -1;
        }
        LISA_LOGI(LOG_TAG, "Playback interface opened: 16kHz, mono, 16bit");
    }
    
    /* 配置播放会话 */
    bt_audio_session_config_t playback_config = {
        .codec_type = BT_CODEC_MSBC,            /* mSBC 编解码器 */
        .direction = BT_AUDIO_DIR_PLAYBACK,     /* 播放方向 */
        .passthrough_mode = false,              /* 编解码模式：使用 mSBC 解码 */
        .event_callback = playback_event_callback,
        .playback_data_callback = playback_data_callback, /* 输出 PCM 到 interface */
        .capture_data_callback = NULL,          /* 播放不需要 */
        .user_data = NULL,
    };
    
    ret = bt_audio_session_create(&playback_config, &g_playback_session);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(LOG_TAG, "Failed to create playback session: %d", ret);
        if (g_audio_interface && g_audio_interface->playback_close) {
            g_audio_interface->playback_close();
        }
        return -1;
    }
    
    /* 配置 mSBC 解码格式 */
    bt_audio_codec_config_t codec_config = {
        .codec_type = BT_CODEC_MSBC,   /* mSBC 解码 */
        .format = {
            .sample_rate = 16000,      /* 16kHz 采样率（mSBC 标准）*/
            .channels = 1,             /* 单声道 */
            .bits_per_sample = 16,     /* 16位采样精度 */
        },
        .bitrate = 0,                  /* mSBC 固定比特率 */
        .frame_duration_us = 0,        /* mSBC 帧时长：7.5ms */
        .codec_specific_config = NULL,
        .config_size = 0,
    };
    
    ret = bt_audio_session_start(g_playback_session, &codec_config);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(LOG_TAG, "Failed to start playback session: %d", ret);
        bt_audio_session_destroy(g_playback_session);
        g_playback_session = NULL;
        if (g_audio_interface && g_audio_interface->playback_close) {
            g_audio_interface->playback_close();
        }
        return -1;
    }
    
    LISA_LOGI(LOG_TAG, "Playback session created and started (mSBC decode mode)");
    return 0;
}

/* ========================================================================
 * 数据处理任务
 * ======================================================================== */

/**
 * @brief 音频回环任务 - 从录音会话获取 mSBC 编码数据并喂给播放会话解码
 * @note 完整数据流：
 *       麦克风 → lisa_audio(callback) → interface->capture_read →
 *       capture_data_callback → session编码 → mSBC帧 →
 *       session解码 → playback_data_callback → interface->playback_write →
 *       lisa_audio → 扬声器
 * @param param 任务参数（未使用）
 */
static void loopback_task(void *param)
{
    uint8_t buffer[1024];
    size_t read_size;
    uint32_t frame_count = 0;
    uint32_t total_bytes = 0;
    uint32_t error_count = 0;
    
    LISA_LOGI(LOG_TAG, "mSBC loopback task started");
    
    while (g_running) {
        /* 从录音会话读取 mSBC 编码帧 */
        bt_audio_error_t ret = bt_audio_session_capture_read_frame(
            g_capture_session, buffer, sizeof(buffer), &read_size);
        
        if (ret == BT_AUDIO_OK && read_size > 0) {
            frame_count++;
            total_bytes += read_size;
            
            /* 将 mSBC 编码数据喂给播放会话进行解码 */
            ret = bt_audio_session_playback_write(
                g_playback_session, buffer, read_size);
            
            if (ret != BT_AUDIO_OK) {
                error_count++;
            }
        } else if (ret == BT_AUDIO_ERR_TIMEOUT) {
            /* 超时是正常的，表示暂无数据 */
            vTaskDelay(pdMS_TO_TICKS(5));
        } else if (ret != BT_AUDIO_OK) {
            error_count++;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        /* 让出CPU给其他任务 */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    LISA_LOGI(LOG_TAG, "mSBC loopback task stopped (frames: %u, bytes: %u, errors: %u)", 
              frame_count, total_bytes, error_count);
    vTaskDelete(NULL);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "BT Audio Framework Sample - mSBC Hardware Loopback");
    LISA_LOGI(LOG_TAG, "====================================================");
    LISA_LOGI(LOG_TAG, "This sample demonstrates real hardware audio loopback:");
    LISA_LOGI(LOG_TAG, "Microphone -> mSBC Encode -> mSBC Decode -> Speaker");
    
    /* 1. 初始化蓝牙音频框架 */
    if (init_bt_audio_framework() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize framework");
        return -1;
    }
    
    /* 2. 创建录音会话（编码） */
    if (create_capture_session() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to create capture session");
        bt_audio_framework_deinit();
        return -1;
    }
    
    /* 3. 创建播放会话（解码） */
    if (create_playback_session() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to create playback session");
        if (g_capture_session) {
            bt_audio_session_stop(g_capture_session);
            bt_audio_session_destroy(g_capture_session);
        }
        bt_audio_framework_deinit();
        return -1;
    }
    
    /* 4. 启动 mSBC 回环任务 */
    g_running = true;
    
    BaseType_t ret = xTaskCreate(loopback_task, "msbc_loopback", 4096, NULL, 10, NULL);
    if (ret != pdPASS) {
        LISA_LOGE(LOG_TAG, "Failed to create mSBC loopback task");
        goto cleanup;
    }
    
    LISA_LOGI(LOG_TAG, "Sample running...");
    LISA_LOGI(LOG_TAG, "You should hear your voice from the speaker with encoding latency!");
    LISA_LOGI(LOG_TAG, "Data flow: Microphone -> mSBC Encode -> mSBC Decode -> Speaker");
    
    /* 5. 运行一段时间 */
    vTaskDelay(pdMS_TO_TICKS(10000));
    
cleanup:
    /* 6. 清理资源 */
    LISA_LOGI(LOG_TAG, "Cleaning up...");
    
    /* 6.1 通知回环任务退出 */
    g_running = false;
    
    /* 6.2 等待回环任务完全退出 */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* 6.3 先关闭硬件接口（停止数据产生，防止回调继续触发）*/
    if (g_audio_interface) {
        if (g_audio_interface->capture_close) {
            g_audio_interface->capture_close();
            LISA_LOGI(LOG_TAG, "Capture interface closed");
        }
        if (g_audio_interface->playback_close) {
            g_audio_interface->playback_close();
            LISA_LOGI(LOG_TAG, "Playback interface closed");
        }
    }
    
    /* 6.4 等待最后的回调完全结束 */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* 6.5 停止录音和播放会话（停止编解码线程）*/
    if (g_capture_session) {
        bt_audio_session_stop(g_capture_session);
        LISA_LOGI(LOG_TAG, "Capture session stopped");
    }
    
    if (g_playback_session) {
        bt_audio_session_stop(g_playback_session);
        LISA_LOGI(LOG_TAG, "Playback session stopped");
    }
    
    /* 6.6 销毁会话 */
    if (g_capture_session) {
        bt_audio_session_destroy(g_capture_session);
    }
    
    if (g_playback_session) {
        bt_audio_session_destroy(g_playback_session);
    }
    
    /* 6.7 反初始化框架 */
    bt_audio_framework_deinit();
    
    LISA_LOGI(LOG_TAG, "Sample finished");
    return 0;
}

