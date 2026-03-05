/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lisa_audio_arcs"
#include "lisa_log.h"

#include <string.h>
#include "lisa_audio.h"
#include "lisa_audio_internal.h"

#define MAX_AUDIO_OBSERVERS 4
#define WAIT_DATA_TIMEOUT    (20)
#define DISPATCH_QUEUE_SIZE (CONFIG_LISA_AUDIO_RECORD_BUFFER_COUNT - 1)


// Observer for the unified callback
typedef struct {
    lisa_audio_callback_t callback;
    void *user_data;
} audio_observer_t;

typedef struct {
    /* Driver sub-modules */
    lisa_audio_record_priv_t record;
    lisa_audio_play_priv_t play;

    /* Unified Dispatcher State */
    lisa_mutex_t *mutex;
    TaskHandle_t dispatch_task;

    /* Queues for pairing record and echo events */
    QueueHandle_t record_queue;
    QueueHandle_t echo_queue;

    /* Phase compensation (samples to skip) */
    lisa_audio_phase_compensation_t phase_comp;

    /* Unified Callback Observers */
    audio_observer_t observers[MAX_AUDIO_OBSERVERS];
    uint8_t observer_count;

    /* Track which streams are active */
    bool record_running;
    bool play_running;
    uint32_t record_drop_samples;
    uint32_t echo_drop_samples;

    volatile uint32_t pending_record_drops;  // Number of record frames to drop for sync
    volatile uint32_t pending_echo_drops;    // Number of echo frames to drop for
} lisa_audio_priv_t;

static int audio_record_config(lisa_device_t *dev, const lisa_audio_record_config_t *config)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;

    return arcs_audio_record_config(&priv->record, config);
}

static int audio_record_control(lisa_device_t *dev, uint32_t cmd, void *arg)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    int ret = arcs_audio_record_control(&priv->record, cmd, arg);
    
    /* Track record running state */
    if (ret == LISA_DEVICE_OK) {
        lisa_mutex_lock(priv->mutex, -1);
        if (cmd == LISA_AUDIO_IOCTL_RECORD_START) {
            xQueueReset(priv->record_queue);
            priv->record_running = true;
            priv->record_drop_samples = 0;
        } else if (cmd == LISA_AUDIO_IOCTL_RECORD_STOP) {
            priv->record_running = false;
            xQueueReset(priv->record_queue);
        }
        lisa_mutex_unlock(priv->mutex);
    }
    
    return ret;
}

static int audio_play_config(lisa_device_t *dev, const lisa_audio_play_config_t *config)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    return arcs_audio_play_config(&priv->play, config);
}

static int audio_play_write(lisa_device_t *dev, const void *buffer, uint32_t samples)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    return arcs_audio_play_write(&priv->play, buffer, samples);
}

static int audio_play_get_buffer(lisa_device_t *dev, void **buffer, uint32_t timeout_ms)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    return arcs_audio_play_get_buffer(&priv->play, buffer, timeout_ms);
}

static int audio_play_control(lisa_device_t *dev, uint32_t cmd, void *arg)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    int ret = arcs_audio_play_control(&priv->play, cmd, arg);
    
    /* Track play running state */
    if (ret == LISA_DEVICE_OK) {
        lisa_mutex_lock(priv->mutex, -1);
        if (cmd == LISA_AUDIO_IOCTL_PLAY_START) {
            xQueueReset(priv->echo_queue);
            priv->play_running = true;
            priv->echo_drop_samples = 0;
        } else if (cmd == LISA_AUDIO_IOCTL_PLAY_STOP) {
            priv->play_running = false;
            xQueueReset(priv->echo_queue);
        }
        lisa_mutex_unlock(priv->mutex);
    }
    
    return ret;
}

static int audio_ioctl(lisa_device_t *dev, uint8_t cmd, void *arg)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;

    switch (cmd) {
        case LISA_AUDIO_IOCTL_SET_PHASE_COMPENSATION:
            if (!arg) {
                return LISA_DEVICE_ERR_INVALID;
            }
            lisa_mutex_lock(priv->mutex, -1);
            priv->phase_comp = *(lisa_audio_phase_compensation_t *)arg;
            lisa_mutex_unlock(priv->mutex);
            return LISA_DEVICE_OK;

        case LISA_AUDIO_IOCTL_GET_PHASE_COMPENSATION:
            if (!arg) {
                return LISA_DEVICE_ERR_INVALID;
            }
            lisa_mutex_lock(priv->mutex, -1);
            *(lisa_audio_phase_compensation_t *)arg = priv->phase_comp;
            lisa_mutex_unlock(priv->mutex);
            return LISA_DEVICE_OK;

        default:
            return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
}

static void audio_dispatch_thread(void *arg);

static int audio_register_callback(lisa_device_t *dev, lisa_audio_callback_t callback, void *user_data)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    lisa_mutex_lock(priv->mutex, -1);

    if (priv->observer_count >= MAX_AUDIO_OBSERVERS) {
        lisa_mutex_unlock(priv->mutex);
        return LISA_DEVICE_ERR_NO_MEM;
    }

    for (int i = 0; i < priv->observer_count; i++) {
        if (priv->observers[i].callback == callback) {
            lisa_mutex_unlock(priv->mutex);
            return LISA_DEVICE_OK;
        }
    }

    priv->observers[priv->observer_count].callback = callback;
    priv->observers[priv->observer_count].user_data = user_data;
    priv->observer_count++;

    lisa_mutex_unlock(priv->mutex);
    return LISA_DEVICE_OK;
}

static int audio_unregister_callback(lisa_device_t *dev, lisa_audio_callback_t callback)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)dev->priv_data;
    lisa_mutex_lock(priv->mutex, -1);

    int pos = -1;
    for (int i = 0; i < priv->observer_count; i++) {
        if (priv->observers[i].callback == callback) {
            pos = i;
            break;
        }
    }

    if (pos != -1) {
        for (int i = pos; i < priv->observer_count - 1; i++) {
            priv->observers[i] = priv->observers[i + 1];
        }
        priv->observer_count--;
    }

    lisa_mutex_unlock(priv->mutex);
    return LISA_DEVICE_OK;
}


static const lisa_audio_api_t audio_api = {
    .register_callback = audio_register_callback,
    .unregister_callback = audio_unregister_callback,
    .record_config = audio_record_config,
    .record_control = audio_record_control,
    .play_config = audio_play_config,
    .play_write = audio_play_write,
    .play_get_buffer = audio_play_get_buffer,
    .play_control = audio_play_control,
    .ioctl = audio_ioctl,
};

static lisa_audio_priv_t audio_priv = { 0 };

int audio_submit_event_from_isr(internal_audio_event_t *event)
{
    int ret = 0;
    if (!event) return LISA_DEVICE_ERR_INVALID;

    BaseType_t yield = pdFALSE;
    /* Select target queue and skip counter based on event type */
    if (event->type == AUDIO_EVENT_TYPE_RECORD) {
        
        if(audio_priv.pending_record_drops > 0){
            audio_priv.pending_record_drops--;
            return 0;
        }

        if(audio_priv.phase_comp.record_skip_samples > audio_priv.record_drop_samples){
            audio_priv.record_drop_samples += event->samples;
            return 0;
        }
            /* Send to target queue */
        if (xQueueSendFromISR(audio_priv.record_queue, event, &yield) != pdPASS) {
            /* Queue full - frame dropped (logged in dispatch thread) */
            audio_priv.pending_echo_drops++;
            LOGE("Record queue full, frame dropped");
            ret = LISA_DEVICE_ERR_NO_MEM;
        }
    } else if (event->type == AUDIO_EVENT_TYPE_ECHO) {

        if(audio_priv.pending_echo_drops > 0){
            audio_priv.pending_echo_drops--;
            return 0;
        }

        if(audio_priv.phase_comp.echo_skip_samples > audio_priv.echo_drop_samples){
            audio_priv.echo_drop_samples += event->samples;
            return 0;
        }

        /* Send to target queue */
        if (xQueueSendFromISR(audio_priv.echo_queue, event, &yield) != pdPASS) {
            /* Queue full - frame dropped (logged in dispatch thread) */
            audio_priv.pending_record_drops++;
            LOGE("Echo queue full, frame dropped");
            ret = LISA_DEVICE_ERR_NO_MEM;
            
        }
    }

    portYIELD_FROM_ISR(yield);

    return ret;
}


static int audio_init(void)
{
    int ret;

    memset(&audio_priv, 0, sizeof(audio_priv));

    audio_priv.mutex = lisa_mutex_create();
    if (!audio_priv.mutex) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* Create queues for record and echo events */
    audio_priv.record_queue = xQueueCreate(DISPATCH_QUEUE_SIZE, sizeof(internal_audio_event_t));
    audio_priv.echo_queue = xQueueCreate(DISPATCH_QUEUE_SIZE, sizeof(internal_audio_event_t));



    if (!audio_priv.record_queue || !audio_priv.echo_queue) {
        if (audio_priv.record_queue) vQueueDelete(audio_priv.record_queue);
        if (audio_priv.echo_queue) vQueueDelete(audio_priv.echo_queue);
        lisa_mutex_delete(audio_priv.mutex);
        return LISA_DEVICE_ERR_NO_MEM;
    }

    if (xTaskCreate(audio_dispatch_thread, "audio_dispatch",
                    CONFIG_LISA_AUDIO_DISPATCH_TASK_STACK_SIZE / sizeof(StackType_t),
                    &audio_priv,
                    configMAX_PRIORITIES - CONFIG_LISA_AUDIO_DISPATCH_TASK_PRIORITY - 1,
                    &audio_priv.dispatch_task) != pdPASS) {
        vQueueDelete(audio_priv.record_queue);
        vQueueDelete(audio_priv.echo_queue);
        lisa_mutex_delete(audio_priv.mutex);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = arcs_audio_record_init(&audio_priv.record);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_audio_play_init(&audio_priv.play);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return LISA_DEVICE_OK;
}

static void audio_dispatch_thread(void *arg)
{
    lisa_audio_priv_t *priv = (lisa_audio_priv_t *)arg;
    bool has_data;

    while (1) {
        internal_audio_event_t rec_evt = {0}, echo_evt = {0};

        has_data = false;
        if ((priv->record_running) || (priv->play_running)) {

            if (priv->record_running) {
                if(pdPASS != xQueueReceive(priv->record_queue, &rec_evt,  pdMS_TO_TICKS(WAIT_DATA_TIMEOUT))){
                    continue;
                }  
            }

            if (priv->play_running) {
                if(pdPASS != xQueueReceive(priv->echo_queue, &echo_evt,  pdMS_TO_TICKS(WAIT_DATA_TIMEOUT))){
                    continue;
                }
            }


            lisa_audio_event_t out_event = {
                .record_buffer = rec_evt.buffer,
                .record_samples = rec_evt.samples,
                .echo_buffer = echo_evt.buffer,
                .echo_samples = echo_evt.samples,
            };

            /* Dispatch the event (paired or single) */
            for (int i = 0; i < priv->observer_count; i++) {
                priv->observers[i].callback(&out_event, priv->observers[i].user_data);
            }
            

        }
        /*waiting for running*/
        else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

LISA_DEVICE_REGISTER(audio0,
                     &audio_api,
                     &audio_priv,
                     NULL,
                     audio_init,
                     CONFIG_LISA_AUDIO_INIT_PRIORITY);