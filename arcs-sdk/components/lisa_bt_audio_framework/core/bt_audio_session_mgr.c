/**
 * @file bt_audio_session_mgr.c
 * @brief 蓝牙音频会话管理器 - 处理蓝牙与音频设备之间的数据转换
 * 
 * 架构设计：
 * 
 * 【编码模式 - 下行播放 (Sink场景)】
 * 蓝牙 → playback_write → 队列 → [downlink_decode_task] → PCM ringbuf → [playback_task] → 硬件
 * 
 * 【透传模式 - 下行播放 (Sink场景)】
 * 蓝牙 → playback_write → PCM ringbuf → [playback_task] → 硬件
 * 
 * 【编码模式 - 下行无自动播放 (Source场景)】
 * 蓝牙 → playback_write → 队列 → [downlink_decode_task] → PCM ringbuf → playback_read_pcm → 应用
 * 
 * 【编码模式 - 上行录音】
 * 硬件 → [capture_callback_encode] → PCM ringbuf → [uplink_encode_task] → 帧队列 → 蓝牙
 * 
 * 【透传模式 - 上行录音】
 * 应用 → capture_write_frame → 帧队列 → 蓝牙
 * 
 * 优化特性：
 * - 编码和透传模式完全分离（零耦合）
 * - 不缓存编码数据，立即解码降低延迟
 * - 使用队列保证编码帧对齐
 * - 透传模式资源按需分配
 * - 支持Source场景下解码但不自动播放
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_session.h"
#include "ring_buffer.h"
#include <string.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define TAG "BT_AUDIO_SESSION"

/* 会话配置 */
#define MAX_SESSION_COUNT CONFIG_LISA_BT_AUDIO_MAX_SESSIONS

/* 时间配置（毫秒）- 用于动态计算缓冲区大小 */
#define DECODE_WORK_BUFFER_MS CONFIG_LISA_BT_AUDIO_WORK_BUFFER_TIME_MS
#define PCM_BUFFER_MS_DOWNLINK CONFIG_LISA_BT_AUDIO_PCM_RINGBUF_TIME_MS_DOWNLINK
#define PCM_BUFFER_MS_UPLINK CONFIG_LISA_BT_AUDIO_PCM_RINGBUF_TIME_MS_UPLINK
#define PCM_PREFILL_MS CONFIG_LISA_BT_AUDIO_PCM_PREFILL_TIME_MS

/* 编码帧队列配置 */
#define ENCODED_FRAME_QUEUE_LENGTH 30   /* 队列深度：可存放30帧编码数据 */
#define MAX_ENCODED_FRAME_SIZE 1024     /* 单帧最大大小 */

/* 线程配置 */
#define DOWNLINK_TASK_STACK_SIZE CONFIG_LISA_BT_AUDIO_DECODE_TASK_STACK_SIZE
#define DOWNLINK_TASK_PRIORITY CONFIG_LISA_BT_AUDIO_DECODE_TASK_PRIORITY
#define PLAYBACK_TASK_STACK_SIZE CONFIG_LISA_BT_AUDIO_PLAYBACK_TASK_STACK_SIZE
#define PLAYBACK_TASK_PRIORITY CONFIG_LISA_BT_AUDIO_PLAYBACK_TASK_PRIORITY
#define UPLINK_TASK_STACK_SIZE CONFIG_LISA_BT_AUDIO_ENCODE_TASK_STACK_SIZE
#define UPLINK_TASK_PRIORITY CONFIG_LISA_BT_AUDIO_ENCODE_TASK_PRIORITY
#define UPLINK_QUEUE_SEND_WAIT_MS 50
#define UPLINK_QUEUE_SEND_RETRY_COUNT 20

/**
 * @brief 根据音频格式计算缓冲区大小
 * @param format 音频格式
 * @param time_ms 时间长度（毫秒）
 * @return 缓冲区大小（字节）
 */
static inline size_t calc_pcm_buffer_size(const bt_audio_format_t *format, uint32_t time_ms)
{
    if (!format) {
        return 0;
    }
    /* 计算公式：sample_rate × channels × bytes_per_sample × time_ms / 1000 */
    size_t bytes_per_sample = format->bits_per_sample / 8;
    size_t size = (size_t)format->sample_rate * format->channels * bytes_per_sample * time_ms / 1000;
    
    /* 对齐4字节边界以提高效率 */
    size = (size + 3) & ~3;
    
    return size;
}

/* 编码帧数据结构（用于队列） */
typedef struct {
    uint8_t data[MAX_ENCODED_FRAME_SIZE];  /* 帧数据 */
    size_t length;                          /* 实际长度 */
} encoded_frame_t;

/* 会话状态 */
typedef enum {
    SESSION_STATE_IDLE,          /* 空闲 */
    SESSION_STATE_STARTED,       /* 已启动 */
    SESSION_STATE_PAUSED,        /* 已暂停 */
} session_state_t;

/* 会话结构 */
typedef struct bt_audio_session {
    uint8_t session_id;
    session_state_t state;
    bt_audio_session_config_t config;
    
    const bt_audio_codec_ops_t *codec_ops;
    
    bt_audio_codec_config_t codec_config;
    bt_audio_stats_t stats;
    
    /* 模式标志 */
    bool is_passthrough;                     /* 是否为透传模式（不编解码） */
    
    /* 动态计算的缓冲区大小 */
    size_t decode_work_buffer_size;          /* 解码工作缓冲区大小 */
    size_t encode_work_buffer_size;          /* 编码工作缓冲区大小 */
    size_t pcm_buffer_size;                  /* PCM 环形缓冲区大小 */
    size_t pcm_prefill_size;                 /* PCM 预填充大小 */
    
    /* 下行播放路径：优化架构
     * 编码模式: [蓝牙] → [下行处理线程：即时解码] → PCM ringbuf → [播放线程(可选)] → 硬件
     * 透传模式: [蓝牙] → PCM ringbuf → [播放线程(可选)] → 硬件
     */
    struct {
        struct ring_buf pcm_ringbuf;         /* PCM数据缓冲区（唯一缓冲）*/
        TaskHandle_t downlink_task;          /* 下行处理线程（接收+解码，仅编码模式）*/
        TaskHandle_t playback_task;          /* 播放线程（所有模式）*/
        QueueHandle_t encoded_data_queue;    /* 编码数据队列（仅编码模式，用于传递给downlink_task）*/
        bool downlink_running;               /* 下行处理线程运行标志 */
        bool playback_running;               /* 播放线程运行标志 */
        bool pcm_prefilled;                  /* PCM是否完成预填充 */
    } downlink;
    
    /* 上行录音路径：优化架构
     * 编码模式: 硬件 → [录音回调] → PCM ringbuf → [上行处理线程：编码] → 帧队列 → [蓝牙读取]
     * 透传模式: 硬件 → [录音回调] → PCM ringbuf → [蓝牙读取]
     */
    struct {
        struct ring_buf pcm_ringbuf;         /* PCM数据缓冲区 */
        QueueHandle_t encoded_frame_queue;   /* 编码帧队列（仅编码模式，存储完整帧）*/
        TaskHandle_t uplink_task;            /* 上行处理线程（编码，仅编码模式）*/
        bool uplink_running;                 /* 上行处理线程运行标志 */
    } uplink;
    
    bool in_use;
} bt_audio_session_t;

static struct {
    bt_audio_session_t sessions[MAX_SESSION_COUNT];
    bool initialized;
} g_session_mgr;

/* 内部函数声明 */
static bt_audio_session_t* find_free_session(void);
static bt_audio_session_t* get_session(bt_audio_session_handle_t handle);
static void notify_event(bt_audio_session_t *session, bt_audio_event_type_t event_type);

/* 下行播放线程函数 - 解码模式 */
static void downlink_decode_task(void *param);   /* 解码模式：接收编码数据 → 解码 → PCM buffer */
static void playback_task_func(void *param);     /* 播放线程：PCM ringbuf → 播放器硬件 */

/* 上行录音线程函数 - 编码模式 */
static void uplink_encode_task(void *param);     /* 编码模式：PCM ringbuf → 编码 → 帧队列 */



/* ========================================================================
 * 会话管理器初始化/去初始化
 * ======================================================================== */

bt_audio_error_t bt_audio_session_manager_init(void)
{
    if (g_session_mgr.initialized) {
        return BT_AUDIO_OK;
    }
    
    memset(&g_session_mgr, 0, sizeof(g_session_mgr));
    
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        g_session_mgr.sessions[i].session_id = i;
        g_session_mgr.sessions[i].state = SESSION_STATE_IDLE;
        g_session_mgr.sessions[i].in_use = false;
    }
    
    g_session_mgr.initialized = true;
    LISA_LOGI(TAG, "Audio session manager initialized");
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_manager_deinit(void)
{
    if (!g_session_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 销毁所有活动会话 */
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if (g_session_mgr.sessions[i].in_use) {
            bt_audio_session_destroy(&g_session_mgr.sessions[i]);
        }
    }
    
    g_session_mgr.initialized = false;
    LISA_LOGI(TAG, "Audio session manager deinitialized");
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 会话生命周期管理
 * ======================================================================== */

bt_audio_error_t bt_audio_session_create(const bt_audio_session_config_t *config,
                                          bt_audio_session_handle_t *session)
{
    if (!g_session_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!config || !session) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 查找空闲会话 */
    bt_audio_session_t *sess = find_free_session();
    if (!sess) {
        return BT_AUDIO_ERR_NO_MEMORY;
    }
    
    /* codec_ops 仅在非透传模式下才需要，将在 start 时获取 */
    sess->codec_ops = NULL;
    
    /* 初始化会话 */
    memcpy(&sess->config, config, sizeof(bt_audio_session_config_t));
    sess->state = SESSION_STATE_IDLE;
    sess->in_use = true;
    sess->is_passthrough = config->passthrough_mode;
    memset(&sess->stats, 0, sizeof(bt_audio_stats_t));
    
    /* 初始化状态标志 */
    if (config->direction == BT_AUDIO_DIR_PLAYBACK) {
        sess->downlink.downlink_task = NULL;
        sess->downlink.playback_task = NULL;
        sess->downlink.encoded_data_queue = NULL;
        sess->downlink.downlink_running = false;
        sess->downlink.playback_running = false;
        sess->downlink.pcm_prefilled = false;
    } else if (config->direction == BT_AUDIO_DIR_CAPTURE) {
        sess->uplink.uplink_task = NULL;
        sess->uplink.encoded_frame_queue = NULL;
        sess->uplink.uplink_running = false;
        sess->uplink.pcm_ringbuf.buffer = NULL;
    }
    
    *session = sess;
    LISA_LOGI(TAG, "Session created: direction=%d, codec=%d, passthrough=%d", 
              config->direction, config->codec_type, sess->is_passthrough);
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_destroy(bt_audio_session_handle_t session)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 如果会话正在运行，先停止（stop 会清理 ringbuf） */
    if (sess->state == SESSION_STATE_STARTED || sess->state == SESSION_STATE_PAUSED) {
        bt_audio_session_stop(session);
    }
    
    sess->in_use = false;
    sess->state = SESSION_STATE_IDLE;
    sess->codec_ops = NULL;
    
    LISA_LOGI(TAG, "Session destroyed");
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_start(bt_audio_session_handle_t session,
                                         const bt_audio_codec_config_t *codec_config)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (!codec_config) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state == SESSION_STATE_STARTED) {
        return BT_AUDIO_OK;
    }
    
    /* 保存codec配置 */
    memcpy(&sess->codec_config, codec_config, sizeof(bt_audio_codec_config_t));
    
    /* 根据音频格式动态计算缓冲区大小 */
    sess->decode_work_buffer_size = calc_pcm_buffer_size(&codec_config->format, DECODE_WORK_BUFFER_MS);
    sess->encode_work_buffer_size = calc_pcm_buffer_size(&codec_config->format, DECODE_WORK_BUFFER_MS);
    sess->pcm_prefill_size = calc_pcm_buffer_size(&codec_config->format, PCM_PREFILL_MS);
    
    if (sess->config.direction == BT_AUDIO_DIR_PLAYBACK) {
        sess->pcm_buffer_size = calc_pcm_buffer_size(&codec_config->format, PCM_BUFFER_MS_DOWNLINK);
    } else {
        sess->pcm_buffer_size = calc_pcm_buffer_size(&codec_config->format, PCM_BUFFER_MS_UPLINK);
    }
    
    LISA_LOGI(TAG, "Buffer sizes: decode_work=%u, encode_work=%u, pcm=%u, prefill=%u bytes",
              sess->decode_work_buffer_size, sess->encode_work_buffer_size, 
              sess->pcm_buffer_size, sess->pcm_prefill_size);
    
    /* 根据计算的大小初始化缓冲区 */
    bt_audio_error_t ret = BT_AUDIO_OK;
    
    if (sess->config.direction == BT_AUDIO_DIR_PLAYBACK) {
        /* ===== 下行播放路径 ===== */
        
        /* 创建PCM ringbuf（所有模式都需要） */
        uint8_t *pcm_buf = (uint8_t *)psram_malloc(sess->pcm_buffer_size);
        if (!pcm_buf) {
            LISA_LOGE(TAG, "Failed to alloc PCM ringbuf");
            return BT_AUDIO_ERR_NO_MEMORY;
        }
        ring_buf_init(&sess->downlink.pcm_ringbuf, sess->pcm_buffer_size, pcm_buf);
        
        /* 编码模式：创建编码数据队列 */
        if (!sess->is_passthrough) {
            sess->downlink.encoded_data_queue = xQueueCreate(10, sizeof(encoded_frame_t));
            if (!sess->downlink.encoded_data_queue) {
                LISA_LOGE(TAG, "Failed to create encoded data queue");
                psram_free(pcm_buf);
                return BT_AUDIO_ERR_NO_MEMORY;
            }
        }
        
    } else if (sess->config.direction == BT_AUDIO_DIR_CAPTURE) {
        /* ===== 上行录音路径 ===== */
        
        /* 编码模式：创建PCM ringbuf（透传模式不需要）*/
        if (!sess->is_passthrough) {
            uint8_t *pcm_buf = (uint8_t *)psram_malloc(sess->pcm_buffer_size);
            if (!pcm_buf) {
                LISA_LOGE(TAG, "Failed to alloc capture PCM ringbuf, size: %d", sess->pcm_buffer_size);
                return BT_AUDIO_ERR_NO_MEMORY;
            }
            ring_buf_init(&sess->uplink.pcm_ringbuf, sess->pcm_buffer_size, pcm_buf);
        }
        
        /* 创建编码帧队列（所有模式都需要，用于存储完整帧）*/
        sess->uplink.encoded_frame_queue = xQueueCreate(ENCODED_FRAME_QUEUE_LENGTH, 
                                                        sizeof(encoded_frame_t));
        if (!sess->uplink.encoded_frame_queue) {
            LISA_LOGE(TAG, "Failed to create encoded frame queue");
            if (!sess->is_passthrough && sess->uplink.pcm_ringbuf.buffer) {
                psram_free(sess->uplink.pcm_ringbuf.buffer);
            }
            return BT_AUDIO_ERR_NO_MEMORY;
        }
        LISA_LOGI(TAG, "Created encoded frame queue: depth=%d, frame_size=%d", 
                  ENCODED_FRAME_QUEUE_LENGTH, sizeof(encoded_frame_t));
    }
    
    /* 初始化codec（透传模式跳过） */
    if (!sess->is_passthrough) {
        /* 非透传模式：获取并初始化 codec */
        sess->codec_ops = bt_audio_codec_get(sess->config.codec_type);
        if (!sess->codec_ops) {
            LISA_LOGE(TAG, "Codec not found for type %d", sess->config.codec_type);
            ret = BT_AUDIO_ERR_NOT_FOUND;
            goto cleanup;
        }
        
        ret = sess->codec_ops->init(&sess->codec_config);
        if (ret != BT_AUDIO_OK) {
            LISA_LOGE(TAG, "Codec init failed");
            sess->codec_ops = NULL;
            goto cleanup;
        }
        
        /* 自动从codec获取帧参数并保存到session（如果codec支持）*/
        if (sess->codec_ops->get_frame_duration_us) {
            sess->codec_config.frame_duration_us = sess->codec_ops->get_frame_duration_us();
        } else {
            sess->codec_config.frame_duration_us = 0;
        }
        
        if (sess->codec_ops->get_frame_size) {
            sess->codec_config.frame_size_bytes = sess->codec_ops->get_frame_size();
        } else {
            sess->codec_config.frame_size_bytes = 0;
        }

        LISA_LOGI(TAG, "Codec initialized: type=%d, name=%s, duration=%u us, frame_size=%zu bytes",
                  sess->config.codec_type, 
                  sess->codec_ops->name ? sess->codec_ops->name : "unknown",
                  sess->codec_config.frame_duration_us,
                  sess->codec_config.frame_size_bytes);
    }
    
    /* 验证必要的回调函数 */
    if (sess->config.direction == BT_AUDIO_DIR_PLAYBACK) {
        if (!sess->config.playback_data_callback) {
            LISA_LOGE(TAG, "Playback direction requires playback_data_callback");
            ret = BT_AUDIO_ERR_INVALID_PARAM;
            goto cleanup;
        }
    }
    
    /* 根据方向和模式创建处理线程 */
    if (sess->config.direction == BT_AUDIO_DIR_PLAYBACK) {
        /* ===== 下行播放：创建线程 ===== */
        sess->downlink.pcm_prefilled = false;
        
        if (!sess->is_passthrough) {
            /* === 解码模式：创建下行处理线程（接收+解码）=== */
            sess->downlink.downlink_running = true;
            BaseType_t xRet = xTaskCreate(
                downlink_decode_task,
                "bt_dl_dec",
                DOWNLINK_TASK_STACK_SIZE,
                sess,
                DOWNLINK_TASK_PRIORITY,
                &sess->downlink.downlink_task
            );
            if (xRet != pdPASS || !sess->downlink.downlink_task) {
                LISA_LOGE(TAG, "Create downlink decode task failed");
                ret = BT_AUDIO_ERR_NO_MEMORY;
                goto cleanup;
            }
            LISA_LOGI(TAG, "Playback decode mode: created decode task");
        } else {
            /* === 透传模式：无需解码线程 === */
            LISA_LOGI(TAG, "Playback passthrough mode: no decode task needed");
        }
        
        /* 创建播放线程 */
        sess->downlink.playback_running = true;
        BaseType_t xRet = xTaskCreate(
            playback_task_func,
            "bt_play",
            PLAYBACK_TASK_STACK_SIZE,
            sess,
            PLAYBACK_TASK_PRIORITY,
            &sess->downlink.playback_task
        );
        if (xRet != pdPASS || !sess->downlink.playback_task) {
            LISA_LOGE(TAG, "Create playback task failed");
            ret = BT_AUDIO_ERR_NO_MEMORY;
            goto cleanup;
        }
        LISA_LOGI(TAG, "Created playback task");
        
    } else {
        /* ===== 上行录音：创建线程 ===== */
        if (!sess->is_passthrough) {
            /* === 编码模式：创建上行处理线程（PCM编码）=== */
            sess->uplink.uplink_running = true;
            BaseType_t xRet = xTaskCreate(
                uplink_encode_task,
                "bt_ul_enc",
                UPLINK_TASK_STACK_SIZE,
                sess,
                UPLINK_TASK_PRIORITY,
                &sess->uplink.uplink_task
            );
            if (xRet != pdPASS || !sess->uplink.uplink_task) {
                LISA_LOGE(TAG, "Create uplink encode task failed");
                ret = BT_AUDIO_ERR_NO_MEMORY;
                goto cleanup;
            }
            LISA_LOGI(TAG, "Capture encode mode: created encode task");
        } else {
            /* === 透传模式：无需编码线程 === */
            LISA_LOGI(TAG, "Capture passthrough mode: no encode task needed");
        }
        /* 透传模式：不需要处理线程，应用层直接写入已编码帧 */
        sess->uplink.uplink_running = true;
    }
    
    sess->state = SESSION_STATE_STARTED;
    notify_event(sess, BT_AUDIO_EVENT_STARTED);
    
    return BT_AUDIO_OK;

cleanup:
    /* 清理已分配的资源 */
    if (!sess->is_passthrough && sess->codec_ops && sess->codec_ops->deinit) {
        sess->codec_ops->deinit();
    }
    
    /* 先停止所有任务，等待完全退出后再清理资源 */
    if (sess->config.direction == BT_AUDIO_DIR_PLAYBACK) {
        /* 停止下行处理线程 */
        if (sess->downlink.downlink_task) {
            sess->downlink.downlink_running = false;
        }
        /* 停止播放线程 */
        if (sess->downlink.playback_task) {
            sess->downlink.playback_running = false;
        }
        
        /* 等待线程完全退出（最多500ms） */
        int wait_count = 0;
        while ((sess->downlink.downlink_task || sess->downlink.playback_task) && wait_count < 50) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
        
        if (sess->downlink.downlink_task || sess->downlink.playback_task) {
            LISA_LOGW(TAG, "Playback tasks did not exit cleanly, force cleanup");
        }
        
        /* 任务已退出，安全清理资源 */
        if (sess->downlink.encoded_data_queue) {
            vQueueDelete(sess->downlink.encoded_data_queue);
            sess->downlink.encoded_data_queue = NULL;
        }
        if (sess->downlink.pcm_ringbuf.buffer) {
            psram_free(sess->downlink.pcm_ringbuf.buffer);
            sess->downlink.pcm_ringbuf.buffer = NULL;
        }
    } else {
        /* 停止上行处理线程 */
        if (sess->uplink.uplink_task) {
            sess->uplink.uplink_running = false;
            
            /* 等待线程完全退出（最多500ms） */
            int wait_count = 0;
            while (sess->uplink.uplink_task && wait_count < 50) {
                vTaskDelay(pdMS_TO_TICKS(10));
                wait_count++;
            }
            
            if (sess->uplink.uplink_task) {
                LISA_LOGW(TAG, "Uplink task did not exit cleanly, force cleanup");
            }
        }
        
        /* 仅在线程已退出后清理资源，避免删除仍在等待的queue。 */
        if (!sess->uplink.uplink_task && sess->uplink.encoded_frame_queue) {
            vQueueDelete(sess->uplink.encoded_frame_queue);
            sess->uplink.encoded_frame_queue = NULL;
        }
        if (!sess->uplink.uplink_task && sess->uplink.pcm_ringbuf.buffer) {
            psram_free(sess->uplink.pcm_ringbuf.buffer);
            sess->uplink.pcm_ringbuf.buffer = NULL;
        }
    }
    
    return ret;
}

bt_audio_error_t bt_audio_session_stop(bt_audio_session_handle_t session)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state == SESSION_STATE_IDLE) {
        return BT_AUDIO_OK;
    }
    
    /* 停止处理线程 */
    if (sess->config.direction == BT_AUDIO_DIR_PLAYBACK) {
        /* 停止下行处理线程 */
        if (sess->downlink.downlink_task) {
            sess->downlink.downlink_running = false;
            LISA_LOGI(TAG, "Stopping downlink task...");
        }
        
        /* 停止播放线程 */
        if (sess->downlink.playback_task) {
            sess->downlink.playback_running = false;
            LISA_LOGI(TAG, "Stopping playback task...");
        }
        
        /* 等待线程完全退出（最多500ms） */
        int wait_count = 0;
        while ((sess->downlink.downlink_task || sess->downlink.playback_task) && wait_count < 50) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
        
        if (sess->downlink.downlink_task || sess->downlink.playback_task) {
            LISA_LOGW(TAG, "Tasks did not exit cleanly: downlink=%p, playback=%p",
                      sess->downlink.downlink_task, sess->downlink.playback_task);
        } else {
            LISA_LOGI(TAG, "All playback tasks stopped");
        }
        
        /* 清空缓冲区和队列（任务已停止，安全操作）*/
        if (sess->downlink.encoded_data_queue) {
            vQueueDelete(sess->downlink.encoded_data_queue);
            sess->downlink.encoded_data_queue = NULL;
        }
        if (sess->downlink.pcm_ringbuf.buffer) {
            psram_free(sess->downlink.pcm_ringbuf.buffer);
            sess->downlink.pcm_ringbuf.buffer = NULL;
        }
        sess->downlink.pcm_prefilled = false;
        
    } else {
        /* 停止上行处理线程 */
        if (sess->uplink.uplink_task) {
            sess->uplink.uplink_running = false;
            LISA_LOGI(TAG, "Stopping uplink task...");
            
            /* 等待线程完全退出（最多500ms） */
            int wait_count = 0;
            while (sess->uplink.uplink_task && wait_count < 50) {
                vTaskDelay(pdMS_TO_TICKS(10));
                wait_count++;
            }
            
            if (sess->uplink.uplink_task) {
                LISA_LOGW(TAG, "Uplink task did not exit cleanly");
            } else {
                LISA_LOGI(TAG, "Uplink task stopped");
            }
        }
        
        /* 仅在线程已退出后清理资源，避免删除仍在等待的queue。 */
        if (!sess->uplink.uplink_task && sess->uplink.encoded_frame_queue) {
            vQueueDelete(sess->uplink.encoded_frame_queue);
            sess->uplink.encoded_frame_queue = NULL;
        }
        if (!sess->uplink.uplink_task && sess->uplink.pcm_ringbuf.buffer) {
            psram_free(sess->uplink.pcm_ringbuf.buffer);
            sess->uplink.pcm_ringbuf.buffer = NULL;
        }
    }
    
    /* 去初始化codec */
    if (!sess->is_passthrough && sess->codec_ops && sess->codec_ops->deinit) {
        sess->codec_ops->deinit();
    }
    
    /* 注意：资源清理（队列、缓冲区）由 destroy 负责，stop 只负责停止任务和接口 */
    
    sess->state = SESSION_STATE_IDLE;
    notify_event(sess, BT_AUDIO_EVENT_STOPPED);
    
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_pause(bt_audio_session_handle_t session)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state != SESSION_STATE_STARTED) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    sess->state = SESSION_STATE_PAUSED;
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_resume(bt_audio_session_handle_t session)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state != SESSION_STATE_PAUSED) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    sess->state = SESSION_STATE_STARTED;
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 数据接口 - 下行播放（从蓝牙接收 → 播放）
 * ======================================================================== */

/**
 * @brief 播放：写入待播放数据
 * @param session 会话句柄
 * @param data 数据指针
 *             - 编码模式：编码数据(SBC/mSBC/CVSD等)
 *             - 透传模式：PCM数据
 * @param size 数据大小
 * @return BT_AUDIO_OK成功，其他失败
 * @note 编码模式：数据通过队列传递，即时解码后缓存PCM
 *       透传模式：PCM数据直接缓存，无需解码
 */
bt_audio_error_t bt_audio_session_playback_write(bt_audio_session_handle_t session,
                                                  const void *data,
                                                  size_t size)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess || !data || size == 0) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state != SESSION_STATE_STARTED) {
        return BT_AUDIO_ERR_INVALID_STATE;
    }
    
    if (sess->config.direction != BT_AUDIO_DIR_PLAYBACK) {
        return BT_AUDIO_ERR_INVALID_STATE;
    }
    
    /* 根据模式选择处理方式 */
    if (!sess->is_passthrough) {
        /* 编码模式：通过队列传递给下行处理线程进行即时解码 */
        if (size > MAX_ENCODED_FRAME_SIZE) {
            LISA_LOGW(TAG, "Encoded data too large: %u > %u", size, MAX_ENCODED_FRAME_SIZE);
            return BT_AUDIO_ERR_INVALID_PARAM;
        }
        
        encoded_frame_t frame;
        memcpy(frame.data, data, size);
        frame.length = size;
        
        /* 发送到队列，不阻塞 */
        if (xQueueSend(sess->downlink.encoded_data_queue, &frame, 0) != pdTRUE) {
            LISA_LOGW(TAG, "Encoded data queue full, dropped %u bytes", size);
            sess->stats.dropped_frames++;
            return BT_AUDIO_ERR_NO_MEMORY;
        }
        
    } else {
        /* 透传模式：直接写入PCM ringbuf */
        uint32_t written = ring_buf_put(&sess->downlink.pcm_ringbuf, data, size);
        
        if (written < size) {
            LISA_LOGW(TAG, "PCM ringbuf full! Dropped %u/%u bytes", size - written, size);
            sess->stats.dropped_frames++;
        }
        
        /* 检查PCM预填充 */
        if (!sess->downlink.pcm_prefilled) {
            uint32_t buffered = ring_buf_size_get(&sess->downlink.pcm_ringbuf);
            if (buffered >= sess->pcm_prefill_size) {
                sess->downlink.pcm_prefilled = true;
                LISA_LOGI(TAG, "PCM prefill done: %u bytes (passthrough)", buffered);
            }
        }
    }
    
    sess->stats.total_frames++;
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 数据接口 - 上行录音（录音 → 发送给蓝牙）
 * ======================================================================== */

/**
 * @brief 录音：写入PCM数据（仅编码模式）
 * @param session 会话句柄
 * @param pcm_data PCM数据指针
 * @param size 数据大小（可以是任意大小）
 * @return BT_AUDIO_OK成功，其他失败
 * @note 仅用于编码模式，PCM数据写入ringbuf后由编码线程处理
 *       透传模式不应调用此接口，应使用 capture_write_frame
 */
bt_audio_error_t bt_audio_session_capture_write_pcm(bt_audio_session_handle_t session,
                                                     const void *pcm_data,
                                                     size_t size)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess || !pcm_data || size == 0) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state != SESSION_STATE_STARTED) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->config.direction != BT_AUDIO_DIR_CAPTURE) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->is_passthrough) {
        LISA_LOGE(TAG, "Passthrough mode should use capture_write_frame");
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 编码模式：写入PCM数据到ringbuf，由上行处理线程编码 */
    const uint8_t *ptr = (const uint8_t *)pcm_data;
        size_t remaining = size;
        int retry_count = 0;
        const int MAX_RETRIES = 25;
        
        while (remaining > 0 && retry_count < MAX_RETRIES) {
            uint32_t written = ring_buf_put(&sess->uplink.pcm_ringbuf, ptr, remaining);
            
            if (written > 0) {
                ptr += written;
                remaining -= written;
                retry_count = 0;
            } else {
                /* Buffer满，短暂等待后重试 */
                retry_count++;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        
        if (remaining > 0) {
            LISA_LOGW(TAG, "Failed to write all PCM data, dropped %zu bytes", remaining);
            sess->stats.dropped_frames++;
        }
        
        sess->stats.bytes_processed += (size - remaining);
        sess->stats.total_frames++;
    return BT_AUDIO_OK;
}

/**
 * @brief 录音：写入已编码的完整帧（仅透传模式）
 * @param session 会话句柄
 * @param frame_data 编码帧数据指针（必须是完整帧，帧对齐）
 * @param frame_size 帧大小
 * @return BT_AUDIO_OK成功，其他失败
 * @note 仅用于透传模式，要求传入的数据必须是完整的编码帧
 *       编码模式不应调用此接口，应使用 capture_write_pcm
 */
bt_audio_error_t bt_audio_session_capture_write_frame(bt_audio_session_handle_t session,
                                                       const void *frame_data,
                                                       size_t frame_size)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess || !frame_data || frame_size == 0) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->state != SESSION_STATE_STARTED) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->config.direction != BT_AUDIO_DIR_CAPTURE) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (!sess->is_passthrough) {
        LISA_LOGE(TAG, "Encode mode should use capture_write_pcm");
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 透传模式：写入已编码的完整帧到队列（要求数据已帧对齐）*/
    if (frame_size > MAX_ENCODED_FRAME_SIZE) {
        LISA_LOGW(TAG, "Frame too large: %u > %u", frame_size, MAX_ENCODED_FRAME_SIZE);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    encoded_frame_t frame;
    memcpy(frame.data, frame_data, frame_size);
    frame.length = frame_size;
    
    /* 放入队列（完整帧）*/
    if (xQueueSend(sess->uplink.encoded_frame_queue, &frame, pdMS_TO_TICKS(1000)) != pdTRUE) {
        LISA_LOGW(TAG, "Frame queue full, dropped frame (%u bytes)", frame_size);
        sess->stats.dropped_frames++;
        return BT_AUDIO_ERR_NO_MEMORY;
    }
    
    sess->stats.total_frames++;
    return BT_AUDIO_OK;
}

/**
 * @brief 录音：读取编码后的完整帧（供蓝牙发送）
 * @param session 会话句柄
 * @param buffer 接收缓冲区
 * @param buffer_size 缓冲区大小
 * @param frame_size 实际读取的帧大小（输出）
 * @return BT_AUDIO_OK成功，其他失败
 * @note 总是返回完整帧，保证帧对齐
 *       编码模式和透传模式都使用此接口读取
 */
bt_audio_error_t bt_audio_session_capture_read_frame(bt_audio_session_handle_t session,
                                                      void *buffer,
                                                      size_t buffer_size,
                                                      size_t *frame_size)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess || !buffer || !frame_size) {
        LISA_LOGE(TAG, "Invalid param: sess=%p, buffer=%p, frame_size=%p", sess, buffer, frame_size);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->config.direction != BT_AUDIO_DIR_CAPTURE) {
        LISA_LOGE(TAG, "Session direction error: expect CAPTURE, got %d", sess->config.direction);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    *frame_size = 0;
    
    /* 从帧队列读取完整帧（编码模式和透传模式都使用队列）*/
    encoded_frame_t frame;
    if (xQueueReceive(sess->uplink.encoded_frame_queue, &frame, 0) != pdTRUE) {
        /* 队列为空，暂无数据 - 返回OK但frame_size为0，调用者通过frame_size判断 */
        return BT_AUDIO_OK;
    }
    
    if (frame.length > buffer_size) {
        LISA_LOGW(TAG, "Buffer too small: need %u, have %u", frame.length, buffer_size);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    memcpy(buffer, frame.data, frame.length);
    *frame_size = frame.length;
    
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_capture_is_drained(bt_audio_session_handle_t session,
                                                      bool *drained)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess || !drained) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }

    if (sess->config.direction != BT_AUDIO_DIR_CAPTURE) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }

    if (sess->state != SESSION_STATE_STARTED) {
        return BT_AUDIO_ERR_INVALID_STATE;
    }

    if (!sess->is_passthrough) {
        if (!sess->uplink.pcm_ringbuf.buffer) {
            return BT_AUDIO_ERR_INVALID_STATE;
        }
        *drained = (ring_buf_size_get(&sess->uplink.pcm_ringbuf) == 0);
        return BT_AUDIO_OK;
    }

    if (!sess->uplink.encoded_frame_queue) {
        return BT_AUDIO_ERR_INVALID_STATE;
    }

    *drained = (uxQueueMessagesWaiting(sess->uplink.encoded_frame_queue) == 0);
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 控制和查询
 * ======================================================================== */

bt_audio_error_t bt_audio_session_get_codec_params(bt_audio_session_handle_t session,
                                                    uint32_t *frame_duration_us,
                                                    size_t *frame_size_bytes)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (!frame_duration_us || !frame_size_bytes) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (sess->is_passthrough) {
        LISA_LOGW(TAG, "Passthrough mode, codec params not available");
        return BT_AUDIO_ERR_NOT_SUPPORTED;
    }
    
    *frame_duration_us = sess->codec_config.frame_duration_us;
    *frame_size_bytes = sess->codec_config.frame_size_bytes;
    
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_session_set_volume(bt_audio_session_handle_t session,
                                              uint8_t volume)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (volume > 100) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 音量控制由外部实现，session 不再处理 */
    return BT_AUDIO_ERR_NOT_SUPPORTED;
}

bt_audio_error_t bt_audio_session_get_stats(bt_audio_session_handle_t session,
                                             bt_audio_stats_t *stats)
{
    bt_audio_session_t *sess = get_session(session);
    if (!sess || !stats) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    memcpy(stats, &sess->stats, sizeof(bt_audio_stats_t));
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

static bt_audio_session_t* find_free_session(void)
{
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if (!g_session_mgr.sessions[i].in_use) {
            return &g_session_mgr.sessions[i];
        }
    }
    return NULL;
}

static bt_audio_session_t* get_session(bt_audio_session_handle_t handle)
{
    bt_audio_session_t *sess = (bt_audio_session_t*)handle;
    
    /* 验证会话有效性 */
    if (!sess || !sess->in_use) {
        return NULL;
    }
    
    return sess;
}

static void notify_event(bt_audio_session_t *session, bt_audio_event_type_t event_type)
{
    if (!session || !session->config.event_callback) {
        return;
    }
    
    bt_audio_event_t event = {
        .event = event_type,
        .data = NULL,
        .data_len = 0
    };
    
    session->config.event_callback(session, &event, session->config.user_data);
}

/* ========================================================================
 * 核心处理线程
 * ======================================================================== */

/**
 * @brief 下行编码模式处理线程：从队列接收编码数据 → 即时解码 → PCM缓冲
 * @note 仅用于编码模式，透传模式不启动此线程
 */
static void downlink_decode_task(void *param)
{
    bt_audio_session_t *sess = (bt_audio_session_t *)param;
    
    /* 使用PSRAM动态分配缓冲区 */
    uint8_t *pcm_buffer = (uint8_t *)psram_malloc(sess->decode_work_buffer_size);
    
    if (!pcm_buffer) {
        LISA_LOGE(TAG, "[Encode] Failed to allocate decode buffer");
        sess->downlink.downlink_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    LISA_LOGI(TAG, "[Decode] Downlink decode task started");
    
    encoded_frame_t frame;
    
    while (sess->downlink.downlink_running) {
        /* 1. 从队列接收编码数据 */
        if (xQueueReceive(sess->downlink.encoded_data_queue, &frame, pdMS_TO_TICKS(10)) != pdTRUE) {
            continue;
        }
        
        /* 2. 即时解码 */
        size_t pcm_len = 0;
        bt_audio_error_t ret = sess->codec_ops->decode(
            frame.data, frame.length,
            pcm_buffer, sess->decode_work_buffer_size,
            &pcm_len
        );
        
        if (ret != BT_AUDIO_OK || pcm_len == 0) {
            LISA_LOGE(TAG, "Decode failed: ret=%d, encoded=%u", ret, frame.length);
            sess->stats.error_frames++;
            continue;
        }
        
        /* 3. 写入PCM ringbuf */
        uint32_t written = ring_buf_put(&sess->downlink.pcm_ringbuf, pcm_buffer, pcm_len);
        if (written < pcm_len) {
            LISA_LOGW(TAG, "PCM ringbuf full: written %u/%u", written, pcm_len);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        
        /* 4. 检查是否达到预填充阈值 */
        if (!sess->downlink.pcm_prefilled) {
            uint32_t buffered = ring_buf_size_get(&sess->downlink.pcm_ringbuf);
            if (buffered >= sess->pcm_prefill_size) {
                sess->downlink.pcm_prefilled = true;
                LISA_LOGI(TAG, "PCM prefill done: %u bytes", buffered);
            }
        }
    }
    
    LISA_LOGI(TAG, "[Encode] Downlink decode task exited");
    
    psram_free(pcm_buffer);
    sess->downlink.downlink_task = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief 播放线程：PCM ringbuf → 播放器硬件
 */
static void playback_task_func(void *param)
{
    bt_audio_session_t *sess = (bt_audio_session_t *)param;
    
    /* 动态分配缓冲区 */
    uint8_t *pcm_buffer = (uint8_t *)psram_malloc(sess->decode_work_buffer_size);
    
    if (!pcm_buffer) {
        LISA_LOGE(TAG, "Failed to allocate playback buffer");
        sess->downlink.playback_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    LISA_LOGI(TAG, "Playback task started");
    
    /* 等待PCM预填充完成 */
    while (sess->downlink.playback_running && !sess->downlink.pcm_prefilled) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (!sess->downlink.playback_running) {
        LISA_LOGW(TAG, "Playback task stopped before prefill");
        goto exit;
    }
    
    LISA_LOGI(TAG, "PCM prefilled, start playback");
    
    while (sess->downlink.playback_running) {
        /* 1. 从PCM ringbuf读取数据 */
        uint16_t pcm_len = ring_buf_get(&sess->downlink.pcm_ringbuf,
                                        pcm_buffer, sess->decode_work_buffer_size);
        
        if (pcm_len == 0) {
            // LISA_LOGW(TAG, "playback_task_func get data falied");
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        
        /* 2. 通过回调抛出数据给外部 */
        if (sess->config.playback_data_callback) {
            int written = sess->config.playback_data_callback(pcm_buffer, pcm_len, sess->config.user_data);
            if (written > 0) {
                sess->stats.bytes_processed += written;
            } else if (written < 0) {
                LISA_LOGE(TAG, "Playback callback failed: %d", written);
            }
        }
    }
    
exit:
    LISA_LOGI(TAG, "Playback task exited");
    
    psram_free(pcm_buffer);
    sess->downlink.playback_task = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief 上行编码模式处理线程：PCM ringbuf → 编码 → 完整帧队列
 * @note 仅用于编码模式，透传模式不启动此线程
 */
static void uplink_encode_task(void *param)
{
    bt_audio_session_t *sess = (bt_audio_session_t *)param;
    
    /* 获取单帧PCM大小 */
    size_t pcm_frame_size = sess->codec_ops->get_pcm_frame_size 
        ? sess->codec_ops->get_pcm_frame_size() : 512;
    
    /* 分配单帧PCM buffer */
    uint8_t *pcm_buffer = (uint8_t *)psram_malloc(pcm_frame_size);
    
    if (!pcm_buffer) {
        LISA_LOGE(TAG, "[Encode] Failed to allocate PCM buffer (%zu bytes)", pcm_frame_size);
        sess->uplink.uplink_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    LISA_LOGI(TAG, "[Encode] Uplink encode task started (PCM frame: %zu bytes)", pcm_frame_size);
    
    while (sess->uplink.uplink_running) {
        /* 1. 从PCM ringbuf读取一帧数据 */
        uint16_t pcm_len = ring_buf_get(&sess->uplink.pcm_ringbuf,
                                        pcm_buffer, pcm_frame_size);
        
        if (pcm_len < pcm_frame_size) {
            /* 数据不足一帧，填充静音后继续编码，防止 BT 链路因无数据而断开 */
            memset(pcm_buffer + pcm_len, 0, pcm_frame_size - pcm_len);

        }
        
        /* 2. 编码一帧PCM */
        encoded_frame_t frame;
        size_t enc_len = 0;
        bt_audio_error_t ret = sess->codec_ops->encode(
            pcm_buffer, pcm_frame_size,
            frame.data, MAX_ENCODED_FRAME_SIZE,
            &enc_len
        );
        
        if (ret != BT_AUDIO_OK || enc_len == 0) {
            LISA_LOGE(TAG, "Encode failed: ret=%d", ret);
            sess->stats.error_frames++;
            continue;
        }
        
        /* 3. 将编码帧放入队列，短超时重试兼顾stop响应和满队列丢帧。 */
        frame.length = enc_len;
        bool queued = false;
        uint32_t retries = UPLINK_QUEUE_SEND_RETRY_COUNT;
        while (sess->uplink.uplink_running &&
               sess->uplink.encoded_frame_queue &&
               retries-- > 0) {
            if (xQueueSend(sess->uplink.encoded_frame_queue,
                           &frame,
                           pdMS_TO_TICKS(UPLINK_QUEUE_SEND_WAIT_MS)) == pdTRUE) {
                queued = true;
                break;
            }
        }

        if (!queued && sess->uplink.uplink_running && sess->uplink.encoded_frame_queue) {
            LISA_LOGW(TAG, "Encoded frame queue full, dropped frame (%u bytes)", enc_len);
            sess->stats.dropped_frames++;
        }
    }
    
    LISA_LOGI(TAG, "[Encode] Uplink encode task exited");
    
    psram_free(pcm_buffer);
    sess->uplink.uplink_task = NULL;
    vTaskDelete(NULL);
}

/* 录音数据由外部通过 bt_audio_session_capture_write_pcm 或 capture_write_frame 主动推送 */
