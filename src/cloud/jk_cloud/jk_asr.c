#define TAG "jk_asr"

#include "jk_asr.h"
#include "jk_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static void asr_on_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    jk_asr_t *asr = (jk_asr_t *)user;
    if (!asr) return;

    switch (event) {
    case JK_WS_EVENT_CONNECTED:
        LISA_LOGI(TAG, "ASR CONNECTED");
        asr->state = JK_ASR_STATE_CONNECTED;
        if (asr->cbs.on_connected) {
            asr->cbs.on_connected(asr);
        }
        break;
    case JK_WS_EVENT_DISCONNECTED:
        LISA_LOGI(TAG, "ASR DISCONNECTED");
        asr->state = JK_ASR_STATE_DISCONNECTED;
        if (asr->cbs.on_disconnected) {
            asr->cbs.on_disconnected(asr);
        }
        break;
    case JK_WS_EVENT_ERROR:
        LISA_LOGE(TAG, "ASR ERROR");
        asr->state = JK_ASR_STATE_DISCONNECTED;
        if (asr->cbs.on_error) {
            asr->cbs.on_error(asr, "WebSocket error");
        }
        break;
    }
}

static void asr_on_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    jk_asr_t *asr = (jk_asr_t *)user;
    if (!asr || !data) return;
    
    LISA_LOGD(TAG, "ASR data received: type=%d, len=%u", type, len);
    
    if (type != JK_WS_DATA_TEXT) {
        LISA_LOGW(TAG, "ASR: non-text data received, type=%d", type);
        return;
    }
    
    LISA_LOGI(TAG, "ASR JSON received (%u bytes): %.*s", len, len < 256 ? (int)len : 256, (const char *)data);
    
    cJSON *json = cJSON_ParseWithLength((const char *)data, len);
    if (!json) {
        LISA_LOGE(TAG, "Failed to parse ASR JSON: %.*s", len < 128 ? (int)len : 128, (const char *)data);
        return;
    }
    
    cJSON *text_item = cJSON_GetObjectItem(json, "text");
    cJSON *is_final_item = cJSON_GetObjectItem(json, "is_final");
    
    if (text_item && cJSON_IsString(text_item)) {
        const char *text = text_item->valuestring;
        bool is_final = is_final_item && cJSON_IsTrue(is_final_item);
        
        LISA_LOGI(TAG, "ASR: [%s] (final=%d)", text, is_final);
        
        if (asr->cbs.on_text_result && strlen(text) > 0) {
            asr->cbs.on_text_result(asr, text, is_final);
        }
    } else {
        LISA_LOGW(TAG, "ASR JSON missing 'text' field");
        char *json_str = cJSON_PrintUnformatted(json);
        if (json_str) {
            LISA_LOGW(TAG, "JSON content: %s", json_str);
            free(json_str);
        }
    }
    
    cJSON_Delete(json);
}

jk_asr_t *jk_asr_create(const char *host, const char *port, jk_asr_callbacks_t *cbs) {
    jk_asr_t *asr = lisa_mem_calloc(1, sizeof(jk_asr_t));
    if (!asr) {
        LISA_LOGE(TAG, "Failed to allocate");
        return NULL;
    }

    asr->host = host ? strdup(host) : strdup(JK_ASR_DEFAULT_HOST);
    asr->port = port ? strdup(port) : strdup(JK_ASR_DEFAULT_PORT);
    asr->state = JK_ASR_STATE_DISCONNECTED;

    if (cbs) {
        memcpy(&asr->cbs, cbs, sizeof(jk_asr_callbacks_t));
    }

    jk_ws_config_t config = {
        .host = asr->host,
        .port = asr->port,
        .path = "/",
        .timeout_ms = 30000,
        .user = asr,
        .on_event = asr_on_event,
        .on_data = asr_on_data,
    };

    asr->ws = jk_ws_create(&config);
    if (!asr->ws) {
        LISA_LOGE(TAG, "Failed to create WebSocket");
        lisa_mem_free(asr->host);
        lisa_mem_free(asr->port);
        lisa_mem_free(asr);
        return NULL;
    }

    return asr;
}

void jk_asr_destroy(jk_asr_t *asr) {
    if (!asr) return;

    if (asr->ws) {
        jk_ws_destroy(asr->ws);
    }

    if (asr->host) lisa_mem_free(asr->host);
    if (asr->port) lisa_mem_free(asr->port);
    lisa_mem_free(asr);
}

int jk_asr_connect(jk_asr_t *asr) {
    if (!asr || !asr->ws) return -1;

    if (asr->state == JK_ASR_STATE_CONNECTED) {
        return 0;
    }

    LISA_LOGI(TAG, "Connecting to ws://%s:%s/", asr->host, asr->port);
    asr->state = JK_ASR_STATE_CONNECTING;

    int ret = jk_ws_connect(asr->ws);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to connect: %d", ret);
        asr->state = JK_ASR_STATE_DISCONNECTED;
        return -1;
    }

    return 0;
}

int jk_asr_disconnect(jk_asr_t *asr) {
    if (!asr || !asr->ws) return -1;

    jk_ws_disconnect(asr->ws);
    asr->state = JK_ASR_STATE_DISCONNECTED;
    return 0;
}

int jk_asr_send_audio(jk_asr_t *asr, const int16_t *samples, uint32_t count) {
    if (!asr || !asr->ws || !samples || count == 0) return -1;
    if (asr->state != JK_ASR_STATE_CONNECTED) return -1;

    return jk_ws_send_binary(asr->ws, samples, count * sizeof(int16_t));
}

bool jk_asr_is_connected(jk_asr_t *asr) {
    return asr && asr->state == JK_ASR_STATE_CONNECTED;
}
