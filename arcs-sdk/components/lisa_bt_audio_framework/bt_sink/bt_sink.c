/**
 * @file bt_sink.c
 * @brief 蓝牙 Sink 场景适配器
 * 
 * 职责说明：
 * ========================================================================
 * 本文件实现 Sink 场景的适配层，负责：
 * 1. 接收蓝牙协议栈事件（bt_audio_adapter_event_ops_t）
 * 2. 创建和管理 session（编解码处理）
 * 3. 连接 session 与硬件（通过回调机制）
 * 
 * 数据流向：
 * ========================================================================
 * 【下行播放】
 * 蓝牙数据 → bt_event_rcv_data → session 解码 → playback_data_callback → 硬件接口
 * 
 * 【上行录音】
 * 硬件接口 → capture_data_callback → session 编码 → 蓝牙发送
 * 
 * 架构设计：
 * ========================================================================
 * - session 不依赖硬件：通过回调抛出/接收 PCM 数据
 * - bt_sink 连接两层：向上处理蓝牙事件，向下操作硬件接口
 * - 硬件抽象：通过 interface_mgr 管理多种硬件实现
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_sink.h"
#include "bt_audio_adapter.h"
#include "bt_audio_session.h"
#include "bt_audio_stream_lifecycle.h"
#include "aud_common.h"
#include "lisa_log.h"
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define TAG "BT_SINK"

/* ========================================================================
 * 全局上下文
 * ======================================================================== */

typedef struct {
    /* 音频接口（通过接口管理器获取） */
    const bt_audio_interface_ops_t *audio_interface;
    bool audio_interface_explicit;
    
    /* session 管理 */
    bt_audio_session_handle_t playback_session;  /* 下行播放 session */
    bt_audio_session_handle_t capture_session;   /* 上行录音 session */
    bt_audio_stream_lifecycle_t lifecycle;        /* 保护 stream/session 生命周期 */
    bool initialized;
    
} bt_sink_context_t;

static bt_sink_context_t g_sink_ctx = {0};

/* ========================================================================
 * 前向声明
 * ======================================================================== */

/* Session 数据回调（从 session 接收数据） */
static int sink_playback_data_callback(const void *pcm_data, size_t size, void *user_data);
static int sink_capture_data_callback(void *pcm_buffer, size_t buffer_size, void *user_data);

/* bt_audio_adapter_event_ops_t 实现 */
static void sink_on_bt_event_start(aud_codec_info_t codec_info);
static void sink_on_bt_event_stop(uint8_t conidx, uint8_t status);
static void sink_on_bt_event_rcv_data(const uint8_t *data, size_t size);
static void sink_on_bt_event_pause(void);
static void sink_on_bt_event_resume(aud_codec_info_t codec_info);
static void sink_on_bt_audio_send_complete(void);
static bool sink_playback_begin(bt_audio_session_handle_t *session);
static void sink_session_op_end(void);
static void sink_reset_runtime(bool preserve_explicit_interface);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

static inline bt_audio_codec_type_t aud_type_to_codec_type(uint8_t aud_type)
{
    switch (aud_type) {
        case AUD_TYPE_SBC:
            return BT_CODEC_SBC;
        case AUD_TYPE_MSBC:
            return BT_CODEC_MSBC;
        case AUD_TYPE_CVSD:
            return BT_CODEC_CVSD;
        case AUD_TYPE_LC3:
            return BT_CODEC_LC3;
        default:
            LISA_LOGE(TAG, "Unknown aud_type: %d", aud_type);
            return BT_CODEC_NONE;
    }
}

static bool sink_playback_begin(bt_audio_session_handle_t *session)
{
    return bt_audio_stream_lifecycle_begin(&g_sink_ctx.lifecycle,
                                           BT_AUDIO_STREAM_SESSION_DOWNLINK,
                                           session);
}

static void sink_session_op_end(void)
{
    bt_audio_stream_lifecycle_end(&g_sink_ctx.lifecycle);
}

static void sink_reset_runtime(bool preserve_explicit_interface)
{
    const bt_audio_interface_ops_t *explicit_interface = NULL;
    bool explicit_set = false;

    if (preserve_explicit_interface && g_sink_ctx.audio_interface_explicit) {
        explicit_interface = g_sink_ctx.audio_interface;
        explicit_set = true;
    }

    memset(&g_sink_ctx, 0, sizeof(g_sink_ctx));
    g_sink_ctx.audio_interface = explicit_interface;
    g_sink_ctx.audio_interface_explicit = explicit_set;
}

/* ========================================================================
 * Session 数据回调实现（连接 session 和硬件）
 * ======================================================================== */

/**
 * @brief Playback 数据回调 - 从 session 接收 PCM 数据并写入硬件
 * @note 这是 session playback_task 调用的回调函数
 */
static int sink_playback_data_callback(const void *pcm_data, size_t size, void *user_data)
{
    (void)user_data;
    
    if (!g_sink_ctx.audio_interface || !g_sink_ctx.audio_interface->playback_write) {
        LISA_LOGW(TAG, "Audio interface not available, dropping data");
        return -1;
    }
    
    /* 通过注册的接口写入硬件 */
    return g_sink_ctx.audio_interface->playback_write(pcm_data, size);
}

/**
 * @brief Capture 数据回调 - 从硬件读取 PCM 数据供 session 编码
 * @note 这是 session 需要数据时调用的回调函数
 */
static int sink_capture_data_callback(void *pcm_buffer, size_t buffer_size, void *user_data)
{
    (void)user_data;
    
    if (!g_sink_ctx.audio_interface || !g_sink_ctx.audio_interface->capture_read) {
        return 0; /* 暂无数据 */
    }
    
    /* 通过注册的接口从硬件读取 */
    return g_sink_ctx.audio_interface->capture_read(pcm_buffer, buffer_size);
}

/* ========================================================================
 * bt_audio_adapter_event_ops_t 实现
 * ======================================================================== */

static void sink_on_bt_event_start(aud_codec_info_t codec_info)
{
    bt_audio_error_t ret;
    bt_audio_format_t format;
    bt_audio_session_handle_t playback_session = NULL;
    bt_audio_codec_type_t codec;
    bool session_started = false;
    bool interface_opened = false;

    LISA_LOGI(TAG, "BT audio stream starting: type=%d, ch=%d, sample=%d",
              codec_info.aud_type, codec_info.aud_ch, codec_info.aud_sample);

    if (!bt_audio_stream_lifecycle_lock(&g_sink_ctx.lifecycle)) {
        return;
    }

    if (g_sink_ctx.lifecycle.state != BT_AUDIO_STREAM_IDLE || g_sink_ctx.playback_session) {
        bt_audio_stream_lifecycle_unlock(&g_sink_ctx.lifecycle);
        LISA_LOGW(TAG, "Playback session already exists");
        return;
    }
    g_sink_ctx.lifecycle.state = BT_AUDIO_STREAM_OPENING;
    g_sink_ctx.lifecycle.ready = false;
    bt_audio_stream_lifecycle_unlock(&g_sink_ctx.lifecycle);

    codec = aud_type_to_codec_type(codec_info.aud_type);
    if (codec == BT_CODEC_NONE) {
        bt_audio_stream_lifecycle_set_state(&g_sink_ctx.lifecycle,
                                            BT_AUDIO_STREAM_IDLE,
                                            false);
        return;
    }
    
    /* 创建下行播放 session */
    bt_audio_session_config_t session_config = {
        .codec_type = codec,
        .direction = BT_AUDIO_DIR_PLAYBACK,
        .passthrough_mode = false,
        .event_callback = NULL,
        .playback_data_callback = sink_playback_data_callback,  /* 注册回调：session → 硬件 */
        .capture_data_callback = NULL,
        .user_data = NULL,
    };
    
    ret = bt_audio_session_create(&session_config, &playback_session);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to create playback session: %d", ret);
        goto exit;
    }
    
    bt_audio_codec_config_t codec_config = {
        .format = {
            .sample_rate = codec_info.aud_sample,
            .channels = codec_info.aud_ch,
            .bits_per_sample = 16,
        }
    };
    
    ret = bt_audio_session_start(playback_session, &codec_config);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to start playback session: %d", ret);
        goto exit;
    }
    session_started = true;
    
    /* 打开硬件播放接口 */
    if (!g_sink_ctx.audio_interface || !g_sink_ctx.audio_interface->playback_open) {
        LISA_LOGE(TAG, "Audio interface not available");
        goto exit;
    }
    
    format = (bt_audio_format_t) {
        .sample_rate = codec_info.aud_sample,
        .channels = codec_info.aud_ch,
        .bits_per_sample = 16,
    };
    ret = g_sink_ctx.audio_interface->playback_open(&format);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to open playback interface: %d", ret);
        goto exit;
    }
    interface_opened = true;

    if (!bt_audio_stream_lifecycle_lock(&g_sink_ctx.lifecycle)) {
        goto exit;
    }
    g_sink_ctx.playback_session = playback_session;
    g_sink_ctx.lifecycle.sessions[BT_AUDIO_STREAM_SESSION_DOWNLINK] = playback_session;
    g_sink_ctx.lifecycle.state = BT_AUDIO_STREAM_OPENED;
    g_sink_ctx.lifecycle.ready = true;
    bt_audio_stream_lifecycle_unlock(&g_sink_ctx.lifecycle);
    playback_session = NULL;
    
    LISA_LOGI(TAG, "Playback session and interface opened successfully");

exit:
    if (playback_session) {
        if (interface_opened && g_sink_ctx.audio_interface &&
            g_sink_ctx.audio_interface->playback_close) {
            g_sink_ctx.audio_interface->playback_close();
        }
        if (session_started) {
            bt_audio_session_stop(playback_session);
        }
        bt_audio_session_destroy(playback_session);
        bt_audio_stream_lifecycle_set_state(&g_sink_ctx.lifecycle,
                                            BT_AUDIO_STREAM_IDLE,
                                            false);
    }
}

static void sink_on_bt_event_stop(uint8_t conidx, uint8_t status)
{
    bt_audio_session_handle_t playback_session = NULL;
    bt_audio_session_handle_t capture_session = NULL;

    LISA_LOGI(TAG, "BT audio stream stopping: conidx=%d, status=0x%x", conidx, status);

    if (!bt_audio_stream_lifecycle_lock(&g_sink_ctx.lifecycle)) {
        return;
    }

    playback_session = g_sink_ctx.playback_session;
    capture_session = g_sink_ctx.capture_session;
    g_sink_ctx.playback_session = NULL;
    g_sink_ctx.capture_session = NULL;
    g_sink_ctx.lifecycle.ready = false;
    g_sink_ctx.lifecycle.state = BT_AUDIO_STREAM_CLOSING;
    g_sink_ctx.lifecycle.sessions[BT_AUDIO_STREAM_SESSION_DOWNLINK] = NULL;
    g_sink_ctx.lifecycle.sessions[BT_AUDIO_STREAM_SESSION_UPLINK] = NULL;
    bt_audio_stream_lifecycle_unlock(&g_sink_ctx.lifecycle);

    bt_audio_stream_lifecycle_wait_idle(&g_sink_ctx.lifecycle);

    if (playback_session) {
        bt_audio_session_stop(playback_session);
        bt_audio_session_destroy(playback_session);

        /* 关闭硬件播放接口 */
        if (g_sink_ctx.audio_interface && g_sink_ctx.audio_interface->playback_close) {
            g_sink_ctx.audio_interface->playback_close();
        }
    }
    
    if (capture_session) {
        bt_audio_session_stop(capture_session);
        bt_audio_session_destroy(capture_session);
        
        /* 关闭硬件录音接口 */
        if (g_sink_ctx.audio_interface && g_sink_ctx.audio_interface->capture_close) {
            g_sink_ctx.audio_interface->capture_close();
        }
    }

    bt_audio_stream_lifecycle_set_state(&g_sink_ctx.lifecycle,
                                        BT_AUDIO_STREAM_IDLE,
                                        false);
}

static void sink_on_bt_event_rcv_data(const uint8_t *data, size_t size)
{
    bt_audio_error_t ret;
    bt_audio_session_handle_t playback_session = NULL;

    if (!data || size == 0) {
        return;
    }

    if (!sink_playback_begin(&playback_session)) {
        return;
    }
    
    /* 将蓝牙编码数据写入 session，由 session 解码后调用 playback_write */
    ret = bt_audio_session_playback_write(playback_session, data, size);
    sink_session_op_end();
    if (ret != BT_AUDIO_OK) {
        LISA_LOGW(TAG, "Failed to write data to playback session: %d", ret);
    }
}

static void sink_on_bt_event_pause(void)
{
    bt_audio_session_handle_t playback_session = NULL;

    LISA_LOGI(TAG, "BT audio stream paused");
    if (sink_playback_begin(&playback_session)) {
        bt_audio_session_pause(playback_session);
        sink_session_op_end();
    }
}

static void sink_on_bt_event_resume(aud_codec_info_t codec_info)
{
    bt_audio_session_handle_t playback_session = NULL;

    (void)codec_info;

    LISA_LOGI(TAG, "BT audio stream resumed");
    if (sink_playback_begin(&playback_session)) {
        bt_audio_session_resume(playback_session);
        sink_session_op_end();
    }
}

static void sink_on_bt_audio_send_complete(void)
{
    /* Sink 模式的上行数据发送完成处理 */
}

static bt_audio_adapter_event_ops_t g_sink_adapter_event_ops = {
    .bt_event_start = sink_on_bt_event_start,
    .bt_event_stop = sink_on_bt_event_stop,
    .bt_event_rcv_data = sink_on_bt_event_rcv_data,
    .bt_event_pause = sink_on_bt_event_pause,
    .bt_event_resume = sink_on_bt_event_resume,
    .bt_audio_send_complete = sink_on_bt_audio_send_complete,
};

/* ========================================================================
 * 公共接口
 * ======================================================================== */

bt_audio_error_t bt_sink_init(void)
{
    bt_audio_error_t ret = BT_AUDIO_OK;

    if (g_sink_ctx.initialized) {
        return BT_AUDIO_OK;
    }
    
    sink_reset_runtime(true);

    ret = bt_audio_stream_lifecycle_init(&g_sink_ctx.lifecycle);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to create lifecycle guard: %d", ret);
        goto exit;
    }
    
    /* 注册蓝牙适配器事件回调 */
    ret = bt_audio_adapter_registeer(&g_sink_adapter_event_ops);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to register adapter event ops: %d", ret);
        goto exit;
    }
    
    /* 获取默认音频接口（由应用层注册，如 lisa_audio_interface）*/
    if (!g_sink_ctx.audio_interface) {
        g_sink_ctx.audio_interface = bt_audio_interface_get_default();
    }
    if (!g_sink_ctx.audio_interface) {
        LISA_LOGE(TAG, "No audio interface registered, bt_sink cannot work without hardware interface");
        ret = BT_AUDIO_ERR_NOT_FOUND;
        goto exit;
    }
    
    g_sink_ctx.initialized = true;
    LISA_LOGI(TAG, "bt_sink initialized successfully with interface: %s", 
              g_sink_ctx.audio_interface->name);
    
    return BT_AUDIO_OK;

exit:
    bt_audio_stream_lifecycle_deinit(&g_sink_ctx.lifecycle);
    sink_reset_runtime(true);
    return ret;
}

bt_audio_error_t bt_sink_set_audio_interface(const bt_audio_interface_ops_t *ops)
{
    bool active = false;

    if (!ops) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }

    if (g_sink_ctx.initialized) {
        if (!bt_audio_stream_lifecycle_lock(&g_sink_ctx.lifecycle)) {
            return BT_AUDIO_ERR_INVALID_STATE;
        }
        active = g_sink_ctx.lifecycle.state != BT_AUDIO_STREAM_IDLE ||
                 g_sink_ctx.lifecycle.sessions[BT_AUDIO_STREAM_SESSION_DOWNLINK] ||
                 g_sink_ctx.lifecycle.sessions[BT_AUDIO_STREAM_SESSION_UPLINK];
        bt_audio_stream_lifecycle_unlock(&g_sink_ctx.lifecycle);
        if (active) {
            return BT_AUDIO_ERR_INVALID_STATE;
        }
    }

    g_sink_ctx.audio_interface = ops;
    g_sink_ctx.audio_interface_explicit = true;
    return BT_AUDIO_OK;
}
