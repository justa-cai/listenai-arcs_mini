/**
 * @file bt_audio_interface_virtual.c
 * @brief 蓝牙虚拟音频接口实现 - 用于Source模式（发送音频到蓝牙）
 * 
 * 架构说明：
 * 
 * 【编码模式 - 发送音频】
 * 应用 → bt_vsnd_play_write(PCM) → 本接口playback_write 
 *      → session.capture_write_pcm → [编码线程] → 帧队列 
 *      → adapter读取 → 蓝牙发送
 * 
 * 【透传模式 - 发送音频】
 * 应用 → bt_vsnd_play_write(已编码) → 本接口playback_write 
 *      → session.capture_write_frame → 帧队列 
 *      → adapter读取 → 蓝牙发送
 * 
 * 设计要点：
 * - Source模式使用CAPTURE方向的session（录音→编码→发送）
 * - playback_write实际是写入capture session（名称对应虚拟声卡的playback）
 * - 编码和透传模式使用不同的session接口
 * - Adapter通过session读取编码帧并发送到蓝牙
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_interface_virtual.h"
#include "bt_audio_adapter.h"
#include "bt_audio_session.h"
#include "lisa_log.h"
#include "lisa_device.h"
#include "lisa_hwtimer.h"
#include "bt_avrcp.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>
#include <stdlib.h>

#define TAG "BT_VINTF"

#define HWTIMER_DEVICE "dual_timer"
#define TIMER_FREQ_HZ  16000  /* 16KHz, 每个计数 = 62.5μs */

#define AUDIO_EVENT_START    (1 << 0)  /* 启动拉取 */
#define AUDIO_EVENT_STOP     (1 << 1)  /* 停止拉取 */
#define AUDIO_EVENT_EXIT     (1 << 2)  /* 退出线程 */

#define MAX_FRAMES_PER_SEND   8     /* 每次最多发送的帧数 */
#define MAX_SEND_BUFFER_SIZE  1024  /* 发送缓冲区大小（MTU限制）*/
#define SEND_BUFFER_POOL_SIZE 2     /* 发送buffer池大小（支持并发） */
#define MAX_HFP_DATA_SIZE   512

/* mSBC over HFP 封装格式 */
#define MSBC_HFP_PACKET_SIZE     60   /* mSBC HFP包总大小 */
#define MSBC_FRAME_SIZE          57   /* 纯mSBC帧大小 */
#define MSBC_HFP_SYNC_WORD       0x01 /* Sync header第一字节 */
#define MSBC_HFP_PADDING         0x00 /* Padding字节 */

/* 虚拟接口状态 */
typedef enum {
    VINTF_STATE_IDLE,      /* 未打开 */
    VINTF_STATE_OPENING,   /* 正在打开（等待蓝牙启动回调）*/
    VINTF_STATE_OPENED,    /* 已打开并就绪 */
    VINTF_STATE_CLOSING,   /* 正在关闭（等待蓝牙停止回调）*/
} vintf_state_t;

typedef struct {
    bool initialized;
    bool session_ready;
    vintf_state_t state;                     /* 接口状态 */
    SemaphoreHandle_t close_sem;             /* 关闭同步信号量 */
    bt_audio_format_t format;
    uint8_t a2dp_conidx;
    uint8_t hfp_conidx;
    bool playback_is_passthrough;
    bool capture_is_passthrough;
    bt_audio_session_handle_t uplink_session;
    bt_audio_session_handle_t downlink_session;
    vintf_open_complete_callback_t open_complete_callback;
    bt_audio_capture_callback_t capture_callback;
    void *capture_user_data;
    lisa_device_t *hw_timer;                 /* 硬件定时器设备 */
    uint8_t timer_channel;                   /* 硬件定时器通道 */
    size_t mtu_size;                         /* 当前MTU大小 */
    size_t frames_per_send;                  /* 每次发送的帧数 */
    uint32_t frame_duration_us;              /* 单帧时长（微秒）*/
    size_t frame_size_bytes;                 /* 单帧大小（字节）*/
    
    /* 发送buffer池（避免数据被覆盖）*/
    uint8_t send_buffer_pool[SEND_BUFFER_POOL_SIZE][MAX_SEND_BUFFER_SIZE];
    uint8_t send_buffer_index;               /* 当前buffer索引 */
    uint8_t msbc_seq_index;                  /* mSBC HFP序列号索引(0-3) */
    uint8_t current_volume;                  /* 当前音量 (0-100) */
} bt_vintf_context_t;

static bt_vintf_context_t g_vintf_ctx = {0};


static void vintf_on_bt_start(aud_codec_info_t codec_info);
static void vintf_on_bt_stop(uint8_t conidx, uint8_t status);
static void vintf_on_bt_rcv_data(const uint8_t *data, size_t size);
static void vintf_on_bt_pause(void);
static void vintf_on_bt_resume(aud_codec_info_t codec_info);
static void vintf_on_bt_audio_send_complete(void);

/* 下行数据回调（HFP全双工：接收蓝牙数据 → session解码 → 回调给应用）*/
static int vintf_downlink_data_callback(const void *pcm_data, size_t size, void *user_data);

static bt_audio_adapter_event_ops_t g_virtual_adapter_event_ops = {
    .bt_event_start = vintf_on_bt_start,
    .bt_event_stop = vintf_on_bt_stop,
    .bt_event_rcv_data = vintf_on_bt_rcv_data,
    .bt_event_pause = vintf_on_bt_pause,
    .bt_event_resume = vintf_on_bt_resume,
    .bt_audio_send_complete = vintf_on_bt_audio_send_complete,
};

static inline bt_audio_codec_type_t aud_type_to_codec_type(uint8_t aud_type)
{
    switch (aud_type) {
#ifdef CONFIG_AUDIO_CODEC_SBC
        case AUD_TYPE_SBC:
            return BT_CODEC_SBC;
#endif
#ifdef CONFIG_AUDIO_CODEC_LC3
        case AUD_TYPE_LC3:
            return BT_CODEC_LC3;
#endif
#ifdef CONFIG_AUDIO_CODEC_MSBC
        case AUD_TYPE_MSBC:
            return BT_CODEC_MSBC;
#endif

        case AUD_TYPE_CVSD:
            return BT_CODEC_CVSD;

        default:
            LISA_LOGE(TAG, "Unknown or unsupported aud_type: %d", aud_type);
            return BT_CODEC_NONE;
    }
}
/**
 * @brief 音频发送硬件定时器回调函数
 * @param user_data 用户数据
 */
static void bt_audio_send_timer_callback(void *user_data)
{
    size_t accumulated_bytes = 0;
    size_t frame_count = 0;
    
    if (!g_vintf_ctx.session_ready || !g_vintf_ctx.uplink_session) {
        return;
    }
    
    /* 使用环形索引获取下一个可用buffer */
    uint8_t *current_buffer = g_vintf_ctx.send_buffer_pool[g_vintf_ctx.send_buffer_index];
    g_vintf_ctx.send_buffer_index = (g_vintf_ctx.send_buffer_index + 1) % SEND_BUFFER_POOL_SIZE;
    
    /* 根据 MTU 和单帧大小计算实际发送帧数 */
    size_t max_send_size = (g_vintf_ctx.mtu_size > 0 && g_vintf_ctx.mtu_size < MAX_SEND_BUFFER_SIZE) ? 
                           g_vintf_ctx.mtu_size : MAX_SEND_BUFFER_SIZE;
    max_send_size -= 32;  /* 预留协议头等开销 */

    if (!g_vintf_ctx.session_ready || !g_vintf_ctx.uplink_session) {
        return;
    }

    /* 读取并累积编码帧 */
    while (frame_count < g_vintf_ctx.frames_per_send) {
        size_t read_size = 0;
        
        /* 检查缓冲区剩余空间是否足够（预留一帧的最大可能大小）*/
        if (accumulated_bytes + g_vintf_ctx.frame_size_bytes > max_send_size) {
            /* 缓冲区已满，停止读取 */
            break;
        }
        
        bt_audio_error_t ret = bt_audio_session_capture_read_frame(
            g_vintf_ctx.uplink_session, 
            &current_buffer[accumulated_bytes], 
            MAX_SEND_BUFFER_SIZE - accumulated_bytes, 
            &read_size);
        
        if (ret == BT_AUDIO_OK && read_size > 0) {
            accumulated_bytes += read_size;
            frame_count++;
        } else if (ret == BT_AUDIO_OK && read_size == 0) {
            /* 队列为空，暂无数据 */
            LISA_LOGW(TAG, "No more frames available to read frame_count=%zu", frame_count);
            break;
        } else {
            /* 其他错误（参数错误等）*/
            LISA_LOGW(TAG, "Read frame failed: %d", ret);
            break;
        }
    }

    /* 发送累积的帧 */
    if (frame_count > 0 && accumulated_bytes > 0) {
        bt_audio_adapter_send_frames(g_vintf_ctx.a2dp_conidx, current_buffer, frame_count, accumulated_bytes);
    } else {
        LISA_LOGW(TAG, "No frames to send in this period");
    }
}

static bt_audio_error_t bt_vintf_create_uplink_session(bt_audio_codec_type_t codec, 
                                                  const bt_audio_format_t format)
{
    bt_audio_error_t ret;
    
    if (!g_vintf_ctx.initialized) {
        LISA_LOGE(TAG, "Virtual interface not ready");
        ret = BT_AUDIO_ERR_INVALID_STATE;
        goto _exit;
    }
    
    if (g_vintf_ctx.uplink_session) {
        LISA_LOGW(TAG, "Uplink session already exists");
        ret = BT_AUDIO_ERR_INVALID_STATE;
        goto _exit;
    }

    // 在link层调用CVSD的编解码，应用层不需要进行处理
    if (codec == BT_CODEC_CVSD)
        g_vintf_ctx.playback_is_passthrough = true;
    
    /* 创建上行session（CAPTURE方向：录音→编码→发送）*/
    bt_audio_session_config_t session_config = {
        .codec_type = codec,
        .direction = BT_AUDIO_DIR_CAPTURE,
        .passthrough_mode = g_vintf_ctx.playback_is_passthrough,
        .event_callback = NULL,
        .playback_data_callback = NULL,  /* 上行不需要，数据由应用主动写入 */
        .capture_data_callback = NULL,   /* 上行不需要，数据由应用主动写入 */
        .user_data = NULL,
    };
    
    ret = bt_audio_session_create(&session_config, &g_vintf_ctx.uplink_session);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to create uplink session: %d", ret);
        goto _exit;
    }
    
    /* 配置codec */
    bt_audio_codec_config_t codec_config = {
        .format = format,
    };
    
    ret = bt_audio_session_start(g_vintf_ctx.uplink_session, &codec_config);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to start uplink session: %d", ret);
        bt_audio_session_destroy(g_vintf_ctx.uplink_session);
        g_vintf_ctx.uplink_session = NULL;
        goto _exit;
    }
    
    /* 编码模式：通过接口获取codec帧参数 */
    /* 透传模式：使用之前通过 bt_vintf_set_frame_params 设置的值 */
    if (!g_vintf_ctx.playback_is_passthrough) {
        ret = bt_audio_session_get_codec_params(g_vintf_ctx.uplink_session,
                                                &g_vintf_ctx.frame_duration_us,
                                                &g_vintf_ctx.frame_size_bytes);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGW(TAG, "Failed to get codec params: %d", ret);
        }
    }
    
    LISA_LOGI(TAG, "Uplink session created: codec=%d, frame_duration=%u us, frame_size=%zu bytes",
             codec, g_vintf_ctx.frame_duration_us, g_vintf_ctx.frame_size_bytes);

    return BT_AUDIO_OK;

_exit:
    return ret;
}

static bt_audio_error_t bt_vintf_create_downlink_session(bt_audio_codec_type_t codec,
                                                    const bt_audio_format_t format)
{
    bt_audio_error_t ret;
    
    if (!g_vintf_ctx.initialized) {
        LISA_LOGE(TAG, "Virtual interface not ready");
        ret = BT_AUDIO_ERR_INVALID_STATE;
        goto _exit;
    }
    
    if (g_vintf_ctx.downlink_session) {
        LISA_LOGW(TAG, "Downlink session already exists");
        ret = BT_AUDIO_ERR_INVALID_STATE;
        goto _exit;
    }

    // 在link层调用CVSD的编解码，应用层不需要进行处理
    if (codec == BT_CODEC_CVSD)
        g_vintf_ctx.capture_is_passthrough = true;
    
    /* 创建下行session（PLAYBACK方向：接收→解码→回调）*/
    bt_audio_session_config_t session_config = {
        .codec_type = codec,
        .direction = BT_AUDIO_DIR_PLAYBACK,
        .passthrough_mode = g_vintf_ctx.capture_is_passthrough,
        .event_callback = NULL,
        .playback_data_callback = vintf_downlink_data_callback,  /* 接收解码后的PCM */
        .capture_data_callback = NULL,
        .user_data = NULL,
    };

    ret = bt_audio_session_create(&session_config, &g_vintf_ctx.downlink_session);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to create downlink session: %d", ret);
        goto _exit;
    }

    /* 配置codec */
    bt_audio_codec_config_t codec_config = {
        .format = format,
    };

    ret = bt_audio_session_start(g_vintf_ctx.downlink_session, &codec_config);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to start downlink session: %d", ret);
        bt_audio_session_destroy(g_vintf_ctx.downlink_session);
        g_vintf_ctx.downlink_session = NULL;
        goto _exit;
    }

    LISA_LOGI(TAG, "Downlink session created with codec=%d", codec);

    return BT_AUDIO_OK;

_exit:
    return ret;
}

/**
 * @brief 下行数据回调 - 从 session 接收解码后的 PCM 数据
 * @note 用于 HFP 全双工模式，将接收到的语音数据传递给应用
 */
static int vintf_downlink_data_callback(const void *pcm_data, size_t size, void *user_data)
{
    (void)user_data;
    
    if (!g_vintf_ctx.session_ready) {
        return -1;
    }
    
    /* 通过应用注册的 capture_callback 传递数据 */
    if (g_vintf_ctx.capture_callback) {
        g_vintf_ctx.capture_callback(pcm_data, size, g_vintf_ctx.capture_user_data);
        return size;
    }
    
    return 0;
}

static void vintf_on_bt_start(aud_codec_info_t codec_info)
{
    bt_audio_codec_type_t codec;
    bt_audio_profile_e profile;
    bt_audio_format_t format;
    int ret;
    
    LISA_LOGI(TAG, "[vintf_on_bt_start]: conidx=%d, aud_type=%d, ch=%d, sample=%d",
            codec_info.conidx, codec_info.aud_type, 
            codec_info.aud_ch, codec_info.aud_sample);

    codec = aud_type_to_codec_type(codec_info.aud_type);
    if (codec == BT_CODEC_NONE) {
        LISA_LOGE(TAG, "Unsupported codec type: %d", codec_info.aud_type);
        return;
    }

    profile = bt_audio_adapter_get_profile();
    if (profile == BT_PROFILE_A2DP) {
        g_vintf_ctx.a2dp_conidx = codec_info.conidx;
    } else if (profile == BT_PROFILE_HFP) {
        g_vintf_ctx.hfp_conidx = codec_info.conidx;
    }

    format.sample_rate = codec_info.aud_sample;
    format.channels = codec_info.aud_ch;
    format.bits_per_sample = 16;

    /* 创建上行 session（麦克风 → 编码 → 蓝牙发送）*/
    ret = bt_vintf_create_uplink_session(codec, format);
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to create uplink session: %d", ret);
        return;
    }

    /* HFP 需要创建双向 session（全双工通话）*/
    if (profile == BT_PROFILE_HFP) {
        ret = bt_vintf_create_downlink_session(codec, format);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGE(TAG, "Failed to create downlink session: %d", ret);
            return;
        }
        LISA_LOGI(TAG, "Downlink session created for HFP full-duplex");
    }

    LISA_LOGI(TAG, "Audio streaming started for profile=%d, conidx=%d", 
            profile, codec_info.conidx);

    /* 保存MTU信息 */
    if (profile == BT_PROFILE_A2DP) {
        g_vintf_ctx.mtu_size = app_a2dp_get_media_peer_mtu(g_vintf_ctx.a2dp_conidx);
        LISA_LOGI(TAG, "MTU size: %zu", g_vintf_ctx.mtu_size);
        
        /* 启动硬件定时器发送 */
        if (g_vintf_ctx.hw_timer && g_vintf_ctx.frame_duration_us > 0) {
            /* 计算实际发送帧数：根据 MTU 和帧大小 */
            g_vintf_ctx.frames_per_send = MAX_FRAMES_PER_SEND;
            if (g_vintf_ctx.frame_size_bytes > 0 && g_vintf_ctx.mtu_size > 0) {
                g_vintf_ctx.frames_per_send = (g_vintf_ctx.mtu_size - 32) / g_vintf_ctx.frame_size_bytes;
                if (g_vintf_ctx.frames_per_send == 0) g_vintf_ctx.frames_per_send = 1;
                if (g_vintf_ctx.frames_per_send > MAX_FRAMES_PER_SEND) g_vintf_ctx.frames_per_send = MAX_FRAMES_PER_SEND;
            }
            
            /* 计算定时器周期（微秒）= 帧时长 * 实际发送帧数 */
            uint32_t period_us = g_vintf_ctx.frame_duration_us * g_vintf_ctx.frames_per_send;
            if (period_us == 0) period_us = 1000;  /* 至少1ms */
            
            /* dual_timer 支持16KHz频率，每个计数 = 62.5μs，精度足够 */
            lisa_hwtimer_set_frequency(g_vintf_ctx.hw_timer, g_vintf_ctx.timer_channel, TIMER_FREQ_HZ);
            
            /* 计算计数值：count = period_us * freq_hz / 1000000 */
            uint32_t count = (uint64_t)period_us * TIMER_FREQ_HZ / 1000000;
            if (count == 0) count = 1;
            
            /* 启动周期模式定时器 */
            int timer_ret = lisa_hwtimer_start(g_vintf_ctx.hw_timer, g_vintf_ctx.timer_channel, 
                                              count, LISA_HWTIMER_MODE_PERIODIC);
            if (timer_ret == 0) {
                LISA_LOGI(TAG, "Hardware timer started: period=%u us, count=%u (freq=%u Hz, frame_duration=%u us, frames=%zu, frame_size=%zu)",
                         period_us, count, TIMER_FREQ_HZ, g_vintf_ctx.frame_duration_us, g_vintf_ctx.frames_per_send, g_vintf_ctx.frame_size_bytes);
            } else {
                LISA_LOGE(TAG, "Failed to start hardware timer: %d", timer_ret);
            }
        } else {
            LISA_LOGW(TAG, "Timer not started: hw_timer=%p, frame_duration=%u us",
                     g_vintf_ctx.hw_timer, g_vintf_ctx.frame_duration_us);
        }
    }
    
    if (g_vintf_ctx.open_complete_callback)
        g_vintf_ctx.open_complete_callback(format);

    /* 设置为OPENED状态 */
    g_vintf_ctx.state = VINTF_STATE_OPENED;
    g_vintf_ctx.session_ready = true;
    LISA_LOGI(TAG, "Profile opened: state=OPENED");
}

static void vintf_on_bt_stop(uint8_t conidx, uint8_t status)
{
    g_vintf_ctx.session_ready = false;
    
    /* 停止硬件定时器 */
    if (g_vintf_ctx.hw_timer) {
        lisa_hwtimer_stop(g_vintf_ctx.hw_timer, g_vintf_ctx.timer_channel);
        LISA_LOGI(TAG, "Hardware timer stopped");
    }

    if (g_vintf_ctx.uplink_session) {
        bt_audio_session_stop(g_vintf_ctx.uplink_session);
        bt_audio_session_destroy(g_vintf_ctx.uplink_session);
        g_vintf_ctx.uplink_session = NULL;
        LISA_LOGI(TAG, "Uplink session stopped and destroyed");
    }

    if (g_vintf_ctx.downlink_session) {
        bt_audio_session_stop(g_vintf_ctx.downlink_session);
        bt_audio_session_destroy(g_vintf_ctx.downlink_session);
        g_vintf_ctx.downlink_session = NULL;
        LISA_LOGI(TAG, "Downlink session stopped and destroyed");
    }
    
    /* 设置为IDLE状态并释放关闭信号量 */
    g_vintf_ctx.state = VINTF_STATE_IDLE;
    if (g_vintf_ctx.close_sem) {
        xSemaphoreGive(g_vintf_ctx.close_sem);
    }
    LISA_LOGI(TAG, "Stop callback completed: state=IDLE");
}

static void vintf_on_bt_rcv_data(const uint8_t *data, size_t size)
{
    bt_audio_error_t err = BT_AUDIO_OK;
    uint8_t buffer[MAX_HFP_DATA_SIZE];
    size_t read_size = 0;

    if (!g_vintf_ctx.session_ready) {
        LISA_LOGW(TAG, "vintf_on_bt_rcv_data session is not ready");
        return;
    }

    if (size > MAX_HFP_DATA_SIZE) {
        LISA_LOGE(TAG, "HFP recv buf size overflow: %zu > %d", size, MAX_HFP_DATA_SIZE);
        return;
    }
    
    /* 从uplink session读取编码数据 */
    err = bt_audio_session_capture_read_frame(
        g_vintf_ctx.uplink_session,
        buffer,
        MAX_HFP_DATA_SIZE,
        &read_size
    );

    if (err == BT_AUDIO_OK && read_size > 0) {
        /* 发送时需要封装mSBC HFP格式（仅mSBC） */
        if (!g_vintf_ctx.playback_is_passthrough && 
            g_vintf_ctx.uplink_session && 
            read_size == MSBC_FRAME_SIZE) {
            /* mSBC: 封装为HFP格式 [sync_header(2)] + [mSBC(57)] + [padding(1)] */
            static const uint8_t seq_table[] = {0x08, 0x38, 0xC8, 0xF8};
            uint8_t hfp_packet[MSBC_HFP_PACKET_SIZE];
            
            hfp_packet[0] = MSBC_HFP_SYNC_WORD;
            hfp_packet[1] = seq_table[g_vintf_ctx.msbc_seq_index];
            memcpy(&hfp_packet[2], buffer, MSBC_FRAME_SIZE);
            hfp_packet[59] = MSBC_HFP_PADDING;
            
            g_vintf_ctx.msbc_seq_index = (g_vintf_ctx.msbc_seq_index + 1) % 4;
            
            bt_audio_adapter_send_frames(g_vintf_ctx.hfp_conidx, hfp_packet, 1, MSBC_HFP_PACKET_SIZE);
        } else {
            /* CVSD或其他：直接发送 */
            bt_audio_adapter_send_frames(g_vintf_ctx.hfp_conidx, buffer, 1, read_size);
        }
    } else {
        LISA_LOGW(TAG, "HFP read failed: err=%d, read_size=%zu", err, read_size);
    }

    /* 处理下行数据（蓝牙 → 应用）*/
    const uint8_t *decode_data = data;
    size_t decode_size = size;
    
    /* 接收时需要解封装mSBC HFP格式（仅mSBC非透传模式） */
    if (!g_vintf_ctx.capture_is_passthrough && g_vintf_ctx.downlink_session) {
        /* 检测是否为静音包（全0） */
        bool is_silence = true;
        for (size_t i = 0; i < size && i < 16; i++) {
            if (data[i] != 0x00) {
                is_silence = false;
                break;
            }
        }
        
        if (is_silence) {
            /* 静音包：丢弃不解码 */
            return;
        }
        
        /* HFP格式：60字节包固定取位置2-58（57字节mSBC） */
        if (size == MSBC_HFP_PACKET_SIZE) {
            /* 格式：[header(2)] + [mSBC(57)] + [padding(1)] */
            decode_data = data + 2;  /* 跳过2字节header */
            decode_size = MSBC_FRAME_SIZE;  /* 取57字节 */
        } else if (size == MSBC_FRAME_SIZE) {
            /* 直接57字节mSBC（无封装） */
            decode_data = data;
            decode_size = size;
        } else {
            /* 其他长度：记录警告 */
            static uint8_t warn_count = 0;
            if (warn_count++ % 50 == 0) {
                LISA_LOGW(TAG, "Unexpected packet size: %zu bytes (expected 60 or 57)", size);
            }
            return;  /* 丢弃非标准长度的包 */
        }
    }
    
    err = bt_audio_session_playback_write(
        g_vintf_ctx.downlink_session, 
        decode_data, 
        decode_size
    );
    if (err != BT_AUDIO_OK) {
        LISA_LOGW(TAG, "Failed to feed encoded data: %d", err);
    }
}
static void vintf_on_bt_pause(void)
{

}

static void vintf_on_bt_resume(aud_codec_info_t codec_info)
{

}

static void vintf_on_bt_audio_send_complete(void)
{

}

/**
 * @brief 虚拟声卡初始化
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_vintf_init(void)
{
    int ret;
    BaseType_t xReturn = pdFAIL;
    
    if (g_vintf_ctx.initialized) {
        return BT_AUDIO_ERR_INVALID_STATE;
    }

    memset(&g_vintf_ctx, 0, sizeof(g_vintf_ctx));
    g_vintf_ctx.a2dp_conidx = 0xFF;
    g_vintf_ctx.hfp_conidx = 0xFF;
    g_vintf_ctx.playback_is_passthrough = false;
    g_vintf_ctx.capture_is_passthrough = false;
    g_vintf_ctx.frame_duration_us = 0;
    g_vintf_ctx.frame_size_bytes = 0;
    g_vintf_ctx.send_buffer_index = 0;
    g_vintf_ctx.msbc_seq_index = 0;
    g_vintf_ctx.current_volume = 80;  /* 默认音量80% */
    g_vintf_ctx.state = VINTF_STATE_IDLE;
    
    /* 创建关闭同步信号量 */
    g_vintf_ctx.close_sem = xSemaphoreCreateBinary();
    if (!g_vintf_ctx.close_sem) {
        LISA_LOGE(TAG, "Failed to create close semaphore");
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    ret = bt_audio_framework_init();
    if (ret != BT_AUDIO_OK) {
        LISA_LOGE(TAG, "Failed to init bt audio framework: %d", ret);
        goto __exit;
    }
    
    // 注册虚拟adapter事件ops
    ret = bt_audio_adapter_registeer(&g_virtual_adapter_event_ops);
    if (ret != 0) {
        LISA_LOGE(TAG, "bt_audio_adapter_registeer failed");
        goto __exit;
    }

    /* 获取硬件定时器设备 */
    g_vintf_ctx.hw_timer = lisa_device_get(HWTIMER_DEVICE);
    if (!g_vintf_ctx.hw_timer) {
        LISA_LOGE(TAG, "Failed to get hardware timer device");
        goto __exit;
    }
    
    /* 选择使用通道 0 */
    g_vintf_ctx.timer_channel = 0;
    
    /* 设置定时器回调 */
    ret = lisa_hwtimer_set_callback(g_vintf_ctx.hw_timer, g_vintf_ctx.timer_channel,
                                   bt_audio_send_timer_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to set hardware timer callback: %d", ret);
        goto __exit;
    }
    
    g_vintf_ctx.initialized = true;
    return BT_AUDIO_OK;

__exit:

    if (g_vintf_ctx.hw_timer) {
        g_vintf_ctx.hw_timer = NULL;
    }

    return BT_AUDIO_ERR_NOT_INITIALIZED;
}

bt_audio_error_t bt_vintf_deinit(void)
{
    if (!g_vintf_ctx.initialized) {
        return BT_AUDIO_OK;
    }
    
    /* 确保已经关闭 */
    if (g_vintf_ctx.state != VINTF_STATE_IDLE) {
        LISA_LOGW(TAG, "Deinit called while not in IDLE state, closing first");
        vintf_profile_close();
    }
    
    g_vintf_ctx.initialized = false;

    /* 停止硬件定时器 */
    if (g_vintf_ctx.hw_timer) {
        lisa_hwtimer_stop(g_vintf_ctx.hw_timer, g_vintf_ctx.timer_channel);
        g_vintf_ctx.hw_timer = NULL;
        LISA_LOGI(TAG, "Hardware timer stopped and released");
    }
    
    /* 删除信号量 */
    if (g_vintf_ctx.close_sem) {
        vSemaphoreDelete(g_vintf_ctx.close_sem);
        g_vintf_ctx.close_sem = NULL;
    }

    LISA_LOGI(TAG, "Virtual interface deinitialized");
    return BT_AUDIO_OK;
}

bt_audio_error_t vintf_profile_open(vintf_open_info_t *info)
{
    int ret = 0;

    if (info == NULL)
        return BT_AUDIO_ERR_INVALID_PARAM;
    
    if (!g_vintf_ctx.initialized) 
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    
    /* 状态检查：只有IDLE状态才能打开 */
    if (g_vintf_ctx.state != VINTF_STATE_IDLE) {
        LISA_LOGE(TAG, "Cannot open: current state=%d (expected IDLE)", g_vintf_ctx.state);
        return BT_AUDIO_ERR_INVALID_STATE;
    }
    
    if (g_vintf_ctx.uplink_session || g_vintf_ctx.downlink_session) {
        LISA_LOGE(TAG, "Cannot open: sessions already exist");
        return BT_AUDIO_ERR_INVALID_STATE;
    }

    if (info->type == VINTF_PROFILE_CAPTURE) {
        g_vintf_ctx.capture_callback = info->audio_data_callback;
        g_vintf_ctx.capture_user_data = info->user_data;
    }

    g_vintf_ctx.open_complete_callback = info->open_complete_callback;
    
    g_vintf_ctx.uplink_session = NULL;
    g_vintf_ctx.downlink_session = NULL;
    
    /* 设置为OPENING状态 */
    g_vintf_ctx.state = VINTF_STATE_OPENING;
    LISA_LOGI(TAG, "Profile opening: type=%d, state=OPENING", info->type);

    if (info->type == VINTF_PROFILE_PLAYBACK) {
        ret = bt_audio_adapter_start_audio_stream(BT_PROFILE_A2DP);
    } else if (info->type == VINTF_PROFILE_CAPTURE) {
        ret = bt_audio_adapter_start_audio_stream(BT_PROFILE_HFP);
    }
    if (ret != 0) {
        g_vintf_ctx.state = VINTF_STATE_IDLE;  /* 失败则恢复IDLE状态 */
        LISA_LOGE(TAG, "Failed to start audio stream: %d, state=IDLE", ret);
        return BT_AUDIO_ERR_DEVICE_OPEN;
    }

    return BT_AUDIO_OK;
}

bt_audio_error_t vintf_profile_close(void)
{
    bt_audio_profile_e profile;
    BaseType_t ret;

    if (!g_vintf_ctx.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 状态检查：只有OPENED状态才能关闭 */
    if (g_vintf_ctx.state != VINTF_STATE_OPENED) {
        LISA_LOGW(TAG, "Cannot close: current state=%d (expected OPENED)", g_vintf_ctx.state);
        /* 如果已经是IDLE或CLOSING，视为成功 */
        if (g_vintf_ctx.state == VINTF_STATE_IDLE || g_vintf_ctx.state == VINTF_STATE_CLOSING) {
            return BT_AUDIO_OK;
        }
        return BT_AUDIO_ERR_INVALID_STATE;
    }
    
    /* 设置为CLOSING状态 */
    g_vintf_ctx.state = VINTF_STATE_CLOSING;
    LISA_LOGI(TAG, "Profile closing: state=CLOSING");

    profile = bt_audio_adapter_get_profile();
    bt_audio_adapter_stop_audio_stream(profile);
    
    /* 等待关闭完成（最多等待5秒） */
    ret = xSemaphoreTake(g_vintf_ctx.close_sem, pdMS_TO_TICKS(5000));
    if (ret != pdTRUE) {
        LISA_LOGE(TAG, "Close timeout: no stop callback received");
        /* 超时也要清理资源并恢复状态 */
        g_vintf_ctx.state = VINTF_STATE_IDLE;
        g_vintf_ctx.open_complete_callback = NULL;
        g_vintf_ctx.capture_callback = NULL;
        g_vintf_ctx.capture_user_data = NULL;
        return BT_AUDIO_ERR_TIMEOUT;
    }
    
    g_vintf_ctx.open_complete_callback = NULL;
    g_vintf_ctx.capture_callback = NULL;
    g_vintf_ctx.capture_user_data = NULL;
    
    LISA_LOGI(TAG, "Profile closed: state=IDLE");
    return BT_AUDIO_OK;
}

int vintf_playback_write(const void *buffer, size_t size)
{
    bt_audio_error_t ret;
    
    if (!g_vintf_ctx.initialized || !g_vintf_ctx.session_ready) {
        return -BT_AUDIO_ERR_INVALID_STATE;
    }
    
    if (!buffer || size == 0) {
        return -BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 根据模式选择不同的session接口 */
    if (g_vintf_ctx.playback_is_passthrough) {
        /* 透传模式：写入已编码的帧数据（要求帧对齐） */
        ret = bt_audio_session_capture_write_frame(g_vintf_ctx.uplink_session, buffer, size);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGE(TAG, "Failed to write frame (passthrough): %d", ret);
            return -BT_AUDIO_ERR_CODEC_FAILED;
        }
        LISA_LOGD(TAG, "Playback write (passthrough): %zu bytes", size);
    } else {
        /* 编码模式：写入PCM数据，由session编码 */
        ret = bt_audio_session_capture_write_pcm(g_vintf_ctx.uplink_session, buffer, size);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGE(TAG, "Failed to write PCM (encode): %d", ret);
            return -BT_AUDIO_ERR_CODEC_FAILED;
        }
        LISA_LOGD(TAG, "Playback write (encode): %zu bytes PCM", size);
    }
    
    return size;
}

bt_audio_error_t vintf_set_volume(uint8_t volume)
{
    if (volume > 100) {
        LISA_LOGE(TAG, "Invalid volume: %d (must be 0-100)", volume);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    g_vintf_ctx.current_volume = volume;
    LISA_LOGI(TAG, "Set volume: %d", volume);
    
    /* 通过AVRCP发送绝对音量到蓝牙设备 */
    /* AVRCP绝对音量范围是0-127，需要转换 */
    uint8_t abs_volume = (uint8_t)((volume * 127) / 100);
    
    /* 检查是否已连接A2DP */
    if (g_vintf_ctx.a2dp_conidx != 0xFF) {
        /* 发送绝对音量设置命令 (需要底层支持AVRCP) */
        app_avrcp_press_req(g_vintf_ctx.a2dp_conidx, 
                              BT_AVRCP_KEY_PRESS_SINGLE,
                              BT_AVRCP_PRESS_ID_SET_ABSOLUTE_VOLUME,
                              abs_volume);
        LISA_LOGI(TAG, "AVRCP absolute volume: %d (0x%02x)", abs_volume, abs_volume);
    } else {
        LISA_LOGW(TAG, "No A2DP connection, volume not sent to device");
    }
    
    return BT_AUDIO_OK;
}

bt_audio_error_t vintf_get_volume(uint8_t *volume)
{
    if (!volume) {
        LISA_LOGE(TAG, "volume pointer is NULL");
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    *volume = g_vintf_ctx.current_volume;
    LISA_LOGI(TAG, "Get volume: %d", *volume);
    
    return BT_AUDIO_OK;
}

/**
 * @brief 设置上行编码模式
 * @param encode_pcm true=编码模式(接收PCM), false=透传模式(接收已编码数据)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 必须在playback_open之前调用
 */
bt_audio_error_t bt_vintf_set_encode_mode(bool encode_pcm)
{
    if (!g_vintf_ctx.initialized) return BT_AUDIO_ERR_NOT_INITIALIZED;
    g_vintf_ctx.playback_is_passthrough = !encode_pcm;
    return BT_AUDIO_OK;
}

/**
 * @brief 设置下行解码模式
 * @param decode_to_pcm true=解码模式(解码到PCM), false=透传模式(直接传递编码数据)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 必须在capture_open之前调用
 */
bt_audio_error_t bt_vintf_set_decode_mode(bool decode_to_pcm)
{
    if (!g_vintf_ctx.initialized) return BT_AUDIO_ERR_NOT_INITIALIZED;
    g_vintf_ctx.capture_is_passthrough = !decode_to_pcm;
    return BT_AUDIO_OK;
}

/**
 * @brief 设置音频帧参数（为透传模式使用）
 * @param frame_duration_us 单帧时长，单位微秒
 * @param frame_size_bytes 单帧大小，单位字节
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 此接口主要为透传模式设计，编码模式会在初始化编码器时自动设置这些参数
 *       建议在vintf_init之后、vintf_profile_open之前调用
 */
bt_audio_error_t bt_vintf_set_frame_params(uint32_t frame_duration_us, size_t frame_size_bytes)
{
    if (!g_vintf_ctx.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (frame_duration_us == 0 || frame_size_bytes == 0) {
        LISA_LOGE(TAG, "Invalid frame params: duration=%u us, size=%zu bytes", 
                 frame_duration_us, frame_size_bytes);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    g_vintf_ctx.frame_duration_us = frame_duration_us;
    g_vintf_ctx.frame_size_bytes = frame_size_bytes;
    LISA_LOGI(TAG, "Frame params set: duration=%u us, size=%zu bytes", 
             frame_duration_us, frame_size_bytes);
    
    return BT_AUDIO_OK;
}
