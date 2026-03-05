/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_audio.h"
#include "lisa_mutex.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

/* ===== Internal Dispatcher Structures ===== */
typedef enum {
    AUDIO_EVENT_TYPE_RECORD,
    AUDIO_EVENT_TYPE_ECHO,
} audio_event_type_t;

// Event from ISR to dispatcher
typedef struct {
    audio_event_type_t type;
    void *buffer;
    uint32_t samples;
    uint64_t timestamp;
} internal_audio_event_t;


/* ===== Record Internal Structures ===== */

typedef struct {
    lisa_mutex_t *mutex;

    void *hdrv;                           /* ADC_PDM 驱动句柄 */
    lisa_audio_record_config_t config;
    lisa_audio_status_t status;

    /* 缓冲区管理 */
    void *queue;                          /* x_queue_t */
    void **buffers;                       /* 缓冲区指针数组 */
    uint8_t buffer_count;
    uint16_t buffer_samples;
    uint32_t buffer_size;
    int current_index;

    bool is_running;
    bool initialized;
    bool adc_initialized;
} lisa_audio_record_priv_t;

/* ===== Play Internal Structures ===== */

typedef struct {
    void *addr;
    uint32_t size;
} play_item_t;

typedef enum {
    PLAY_STATE_IDLE = 0,
    PLAY_STATE_PLAY_REQ,
    PLAY_STATE_PLAY_RUN,
    PLAY_STATE_STOP_REQ
} play_state_t;

typedef struct {
    lisa_mutex_t *mutex;

    void *hdrv;                           /* DAC 驱动句柄 */
    lisa_audio_play_config_t config;
    lisa_audio_status_t status;

    /* Ping-Pong 缓冲 */
    void *ping_addr;
    void *pong_addr;

    /* 缓冲池 */
    QueueHandle_t play_queue;
    QueueHandle_t free_queue;
    uint8_t *buffer_pool;
    uint8_t buffer_count;
    uint16_t buffer_samples;
    uint32_t buffer_size;

    /* 状态机 */
    play_state_t state;
    EventGroupHandle_t event;

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    /* Echo feature fields */
    void **echo_fifo;
    int echo_xpos;
    uint32_t echo_buffer_size;
    uint16_t echo_buffer_samples;
    uint8_t echo_buffer_count;
#endif

    bool initialized;
} lisa_audio_play_priv_t;

/* ===== Internal Function Prototypes ===== */

/* Record Functions */
int arcs_audio_record_init(lisa_audio_record_priv_t *priv);
int arcs_audio_record_deinit(lisa_audio_record_priv_t *priv);
int arcs_audio_record_config(lisa_audio_record_priv_t *priv, const lisa_audio_record_config_t *config);
int arcs_audio_record_control(lisa_audio_record_priv_t *priv, uint32_t cmd, void *arg);

/* Play Functions */
int arcs_audio_play_init(lisa_audio_play_priv_t *priv);
int arcs_audio_play_deinit(lisa_audio_play_priv_t *priv);
int arcs_audio_play_config(lisa_audio_play_priv_t *priv, const lisa_audio_play_config_t *config);
int arcs_audio_play_write(lisa_audio_play_priv_t *priv, const void *buffer, uint32_t samples);
int arcs_audio_play_get_buffer(lisa_audio_play_priv_t *priv, void **buffer, uint32_t timeout_ms);
int arcs_audio_play_control(lisa_audio_play_priv_t *priv, uint32_t cmd, void *arg);
