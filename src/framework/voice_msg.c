#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "sys_init.h"
#include "lisa_log.h"
#include "ebus/ebus.h"
#include "voice_msg.h"
#include "sysheap.h"

#define TAG "voice.msg"

static ebus_chn_t *voice_default_chn = NULL;

struct voice_invoke {
    voice_invoke_worker_t worker;
    uint32_t data_len;
    uint8_t data[0];
};

struct voice_invoke_sync {
    voice_invoke_sync_worker_t worker;
    struct voice_invoke_rsp *rsp;
    uint32_t data_len;
    uint8_t data[0];
};

static SemaphoreHandle_t voice_invoke_sync_resp_sem = NULL;
static SemaphoreHandle_t voice_invoke_sync_lock = NULL;

static void voice_invoke_sumbit_msg_received(void *unused, uint32_t evt, void *data, uint32_t len, void *user_data)
{
    struct voice_invoke *invoke = (struct voice_invoke *)data;
    if (invoke == NULL) {
        return;
    }

    LOGI("voice invoke, worker: %p, data: %p, len: %d", invoke->worker, invoke->data, invoke->data_len);

    if (invoke->worker) {
        invoke->worker(invoke->data, invoke->data_len);
    }
}

static void voice_invoke_sync_sumbit_msg_received(void *unused, uint32_t evt, void *data, uint32_t len, void *user_data)
{
    struct voice_invoke_sync *invoke = (struct voice_invoke_sync *)data;
    if (invoke == NULL) {
        return;
    }

    LOGI("voice invoke, worker: %p, data: %p, len: %d", invoke->worker, invoke->data, invoke->data_len);

    if (invoke->worker) {
        invoke->worker(invoke->data, invoke->data_len, invoke->rsp);
    }

    xSemaphoreGive(voice_invoke_sync_resp_sem);
}

int voice_invoke(voice_invoke_worker_t worker, void *data, uint32_t len)
{
    if (worker == NULL) {
        return -1;
    }

    struct voice_invoke *invoke = psram_malloc(sizeof(*invoke) + len);
    if (invoke == NULL) {
        return -1;
    }

    invoke->worker = worker;
    invoke->data_len = len;

    if (len) {
        memcpy(invoke->data, data, len);
    }

    if (voice_msg_pub(VOICE_MSG_INVOKE_SUBMIT, invoke, sizeof(*invoke) + len) != 0) {
        psram_free(invoke);
        return -1;
    }

    return 0;
}

int voice_invoke_sync(voice_invoke_sync_worker_t worker, void *data, uint32_t len, struct voice_invoke_rsp *rsp,
                      uint32_t timeout)
{
    if (worker == NULL || rsp == NULL) {
        return -1;
    }

    struct voice_invoke_sync *invoke_sync = psram_malloc(sizeof(*invoke_sync) + len);
    if (invoke_sync == NULL) {
        return -1;
    }

    invoke_sync->worker = worker;
    invoke_sync->data_len = len;
    invoke_sync->rsp = rsp;

    if (len) {
        memcpy(invoke_sync->data, data, len);
    }

    BaseType_t r = xSemaphoreTake(voice_invoke_sync_lock, timeout);
    if (r != pdPASS) {
        psram_free(invoke_sync);
        return -1;
    }

    xSemaphoreTake(voice_invoke_sync_resp_sem, 0);
    voice_msg_pub(VOICE_MSG_INVOKE_SYNC_SUBMIT, invoke_sync, sizeof(*invoke_sync) + len);
    r = xSemaphoreTake(voice_invoke_sync_resp_sem, timeout);

    xSemaphoreGive(voice_invoke_sync_lock);

    return r == pdPASS ? 0 : -1;
}

int voice_msg_init(void)
{
    int r;
    ebus_handle_t *bus;
    ebus_chn_t *chn;

    r = ebus_init();
    LOGI("voice_msg_init, ebus_init ret:%d", r);
    assert(r == 0);

    bus = ebus_create_async("voice.ebus", 128, 32 * 1024, 6);
    LOGI("voice_msg_init, ebus_create ret:%p", bus);
    assert(bus);

    voice_default_chn = ebus_chn_create_attach(bus, "voice.ebus.default");
    LOGI("voice_msg_init, ebus_chn_create_attach ret:%p", voice_default_chn);
    assert(voice_default_chn);

    voice_invoke_sync_lock = xSemaphoreCreateMutex();
    assert(voice_invoke_sync_lock);

    voice_invoke_sync_resp_sem = xSemaphoreCreateBinary();
    assert(voice_invoke_sync_resp_sem);


    voice_msg_sub(VOICE_MSG_INVOKE_SUBMIT, voice_invoke_sumbit_msg_received, NULL);
    voice_msg_sub(VOICE_MSG_INVOKE_SYNC_SUBMIT, voice_invoke_sync_sumbit_msg_received, NULL);

    return 0;
}

int voice_msg_pub(uint32_t evt, void *data, uint32_t len)
{
    ebus_chn_t *chn;
    int r;

    chn = voice_default_chn;
    if (chn == NULL) {
        LOGE("voice msg chn is null, evt:%d", evt);
        return -1;
    }

    r = ebus_message_pub_async(chn, evt, data, len);
    if (r != 0) {
        LOGE("voice msg pub failed, evt:%d", evt);
        return -1;
    }

    return 0;
}

int voice_msg_sub(uint32_t evt, voice_msg_cb_t cb, void *user_data)
{
    ebus_chn_t *chn;
    int r;

    chn = voice_default_chn;
    if (chn == NULL) {
        LOGE("voice msg chn is null, evt:%d", evt);
        return -1;
    }

    r = ebus_message_subscribe(chn, EBUS_SUBSCRIBER_TYPE_ASYNC, evt, (ebus_chn_cb_t)cb, user_data);
    if (r != 0) {
        LOGE("voice msg sub failed, evt:%d", evt);
        return -1;
    }

    return 0;
}

int voice_msg_unsub(uint32_t evt, voice_msg_cb_t cb)
{
    ebus_chn_t *chn;
    int r;

    chn = voice_default_chn;
    if (chn == NULL) {
        LOGE("voice msg chn is null, evt:%d", evt);
        return -1;
    }

    r = ebus_message_unsubscribe(chn, (ebus_chn_cb_t)cb);
    if (r != 0) {
        LOGE("voice msg unsub failed, evt:%d", evt);
        return -1;
    }

    return 0;
}

SYS_INIT(voice_msg_init, SYS_INIT_LEVEL_PRE_APPLICATION, 30);
