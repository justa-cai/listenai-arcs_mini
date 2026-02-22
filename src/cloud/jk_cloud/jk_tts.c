#define TAG "jk_tts"

#include "jk_tts.h"
#include "jk_tts_parser.h"
#include "jk_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_kv.h"
#include "kv/kv_user.h"
#include "cJSON.h"
#include "FreeRTOS.h"
#include "task.h"
#include "evs_utils.h"
#include <stdio.h>
#include <string.h>

static void generate_uuid(char *buf, size_t buf_len) {
    static uint32_t counter = 0;
    snprintf(buf, buf_len, "req-%08lx-%04x",
             (unsigned long)xTaskGetTickCount(), ++counter);
}

// Forward declaration
static jk_tts_t *g_tts_instance = NULL;

// Deferred function to send pending TTS request
static int send_pending_request_runnable(void *arg) {
    jk_tts_t *tts = (jk_tts_t *)arg;
    if (!tts || !tts->has_pending) {
        return 0;
    }

    LISA_LOGI(TAG, "Sending pending TTS request: %s", tts->pending_text);

    // Copy the pending text since it will be cleared by jk_tts_request
    char pending_copy[JK_TTS_PENDING_TEXT_LEN];
    strncpy(pending_copy, tts->pending_text, JK_TTS_PENDING_TEXT_LEN - 1);
    pending_copy[JK_TTS_PENDING_TEXT_LEN - 1] = '\0';

    // Clear the pending flag before sending the request
    tts->has_pending = false;
    tts->pending_text[0] = '\0';

    // Send the pending request (directly, not through jk_tts_request to avoid re-queueing)
    if (tts->ws) {
        tts->data_complete = false;
        tts->state = JK_TTS_STATE_PLAYING;  // Set state to PLAYING before sending
        generate_uuid(tts->current_request_id, sizeof(tts->current_request_id));

        cJSON *req = cJSON_CreateObject();
        cJSON_AddStringToObject(req, "type", "tts_request");
        cJSON_AddStringToObject(req, "request_id", tts->current_request_id);

        cJSON *params = cJSON_CreateObject();
        cJSON_AddStringToObject(params, "text", pending_copy);
        cJSON_AddStringToObject(params, "mode", "streaming");
        cJSON_AddStringToObject(params, "voice_id", tts->voice_id);
        cJSON_AddItemToObject(req, "params", params);

        char *json_str = cJSON_PrintUnformatted(req);
        LISA_LOGI(TAG, "Pending TTS request sent: request_id=%s", tts->current_request_id);
        jk_ws_send_text(tts->ws, json_str);
        lisa_mem_free(json_str);
        cJSON_Delete(req);
    }

    return 0;
}

static void tts_on_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    jk_tts_t *tts = (jk_tts_t *)user;
    if (!tts) return;

    switch (event) {
    case JK_WS_EVENT_CONNECTED:
        LISA_LOGI(TAG, "TTS CONNECTED");
        tts->state = JK_TTS_STATE_CONNECTED;
        if (tts->cbs.on_connected) {
            tts->cbs.on_connected(tts);
        }
        break;
    case JK_WS_EVENT_DISCONNECTED:
        LISA_LOGI(TAG, "TTS DISCONNECTED");
        tts->state = JK_TTS_STATE_DISCONNECTED;
        if (tts->cbs.on_disconnected) {
            tts->cbs.on_disconnected(tts);
        }
        break;
    case JK_WS_EVENT_ERROR:
        LISA_LOGE(TAG, "TTS ERROR");
        tts->state = JK_TTS_STATE_DISCONNECTED;
        if (tts->cbs.on_error) {
            tts->cbs.on_error(tts, "WebSocket error");
        }
        break;
    }
}

static void tts_on_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    jk_tts_t *tts = (jk_tts_t *)user;
    if (!tts || !data) return;

    // Debug: Log all data received
    static int data_count = 0;
    if (data_count++ < 3) {
        LISA_LOGI(TAG, "TTS data received: type=%d, len=%u, state=%d", type, len, tts->state);
    }

    if (type == JK_WS_DATA_BINARY) {
        jk_tts_parse_result_t result;
        int ret = jk_tts_parse_frame((uint8_t *)data, len, &result);

        if (ret == 0 && result.audio_len > 0) {
            // Check if request_id matches current request
            // If no request_id in frame, accept it as current request (server compatibility)
            if (result.metadata.request_id[0] != '\0' &&
                strcmp(result.metadata.request_id, tts->current_request_id) != 0) {
                // Discard old request data
                LISA_LOGD(TAG, "Discarding audio for old request_id=%s (current: %s)",
                          result.metadata.request_id, tts->current_request_id);
                return;
            }

            // Log first few audio frames for debugging
            static int audio_frame_count = 0;
            if (audio_frame_count++ < 5) {
                LISA_LOGI(TAG, "TTS audio frame #%d: %u samples, request_id=%s, is_final=%d",
                          audio_frame_count, result.audio_len,
                          result.metadata.request_id[0] ? result.metadata.request_id : "(none)",
                          result.metadata.is_final);
            }

            // Log request_id match for debugging (only first few times)
            static int req_match_count = 0;
            if (result.metadata.request_id[0] != '\0' && req_match_count++ < 3) {
                LISA_LOGI(TAG, "TTS audio matched request_id=%s", result.metadata.request_id);
            }

            // Copy audio data to aligned buffer ONLY for current request to avoid alignment issues
            // and use-after-free when frame buffer is freed/reused
            // Max TTS frame is typically < 4KB, use static buffer for efficiency
            #define TTS_AUDIO_BUFFER_SAMPLES 4096
            static int16_t s_audio_buffer[TTS_AUDIO_BUFFER_SAMPLES];
            int16_t *aligned_audio = result.audio_data;

            if (result.audio_len <= TTS_AUDIO_BUFFER_SAMPLES) {
                memcpy(s_audio_buffer, result.audio_data, result.audio_len * sizeof(int16_t));
                aligned_audio = s_audio_buffer;
            }

            tts->state = JK_TTS_STATE_PLAYING;
            if (tts->cbs.on_audio_data) {
                tts->cbs.on_audio_data(tts, aligned_audio, result.audio_len, &result.metadata);
            }
        }

        if (result.metadata.is_final) {
            // Only mark complete if request_id matches
            if (result.metadata.request_id[0] == '\0' ||
                strcmp(result.metadata.request_id, tts->current_request_id) == 0) {
                tts->state = JK_TTS_STATE_CONNECTED;
                tts->data_complete = true;
                LISA_LOGI(TAG, "TTS data complete, waiting for playback to finish");
                // Check if there's a pending request to send
                jk_tts_check_pending(tts);
            } else {
                LISA_LOGW(TAG, "TTS final from old request %s (current: %s), discarding",
                          result.metadata.request_id, tts->current_request_id);
            }
        }
    } else if (type == JK_WS_DATA_TEXT) {
        LISA_LOGI(TAG, "TTS text data: %.*s", len > 100 ? 100 : (int)len, (const char*)data);
        cJSON *json = cJSON_ParseWithLength((const char *)data, len);
        if (!json) return;

        cJSON *type_item = cJSON_GetObjectItem(json, "type");
        cJSON *request_id_item = cJSON_GetObjectItem(json, "request_id");

        if (type_item && cJSON_IsString(type_item)) {
            const char *type_str = type_item->valuestring;

            if (strcmp(type_str, "progress") == 0) {
                LISA_LOGI(TAG, "TTS progress");
            } else if (strcmp(type_str, "complete") == 0) {
                // Check request_id for complete message
                if (request_id_item && cJSON_IsString(request_id_item)) {
                    if (strcmp(request_id_item->valuestring, tts->current_request_id) == 0) {
                        LISA_LOGI(TAG, "TTS complete for current request");
                        tts->state = JK_TTS_STATE_CONNECTED;
                        tts->data_complete = true;
                        // Check if there's a pending request to send
                        jk_tts_check_pending(tts);
                    } else {
                        LISA_LOGW(TAG, "TTS complete from old request %s (current: %s), discarding",
                                  request_id_item->valuestring, tts->current_request_id);
                    }
                } else {
                    // No request_id in complete message, accept it anyway
                    LISA_LOGI(TAG, "TTS complete (no request_id)");
                    tts->state = JK_TTS_STATE_CONNECTED;
                    tts->data_complete = true;
                    // Check if there's a pending request to send
                    jk_tts_check_pending(tts);
                }
            } else if (strcmp(type_str, "error") == 0) {
                cJSON *error_obj = cJSON_GetObjectItem(json, "error");
                if (error_obj) {
                    cJSON *msg = cJSON_GetObjectItem(error_obj, "message");
                    if (msg && tts->cbs.on_error) {
                        tts->cbs.on_error(tts, msg->valuestring);
                    }
                }
            }
        } else {
            cJSON *is_final = cJSON_GetObjectItem(json, "is_final");
            cJSON *sample_rate = cJSON_GetObjectItem(json, "sample_rate");

            if (request_id_item) {
                LISA_LOGI(TAG, "TTS audio metadata: request_id=%s, is_final=%d",
                          request_id_item->valuestring, is_final ? cJSON_IsTrue(is_final) : false);
                if (is_final && cJSON_IsTrue(is_final)) {
                    // Check request_id match
                    if (strcmp(request_id_item->valuestring, tts->current_request_id) == 0) {
                        tts->state = JK_TTS_STATE_CONNECTED;
                        tts->data_complete = true;
                        LISA_LOGI(TAG, "TTS data complete, waiting for playback to finish");
                    } else {
                        LISA_LOGW(TAG, "TTS final from old request %s (current: %s), discarding",
                                  request_id_item->valuestring, tts->current_request_id);
                    }
                }
            }
        }
        cJSON_Delete(json);
    }
}

jk_tts_t *jk_tts_create(const char *host, const char *port, jk_tts_callbacks_t *cbs) {
    jk_tts_t *tts = lisa_mem_calloc(1, sizeof(jk_tts_t));
    if (!tts) {
        LISA_LOGE(TAG, "Failed to allocate");
        return NULL;
    }

    tts->host = host ? strdup(host) : strdup(JK_TTS_DEFAULT_HOST);
    tts->port = port ? strdup(port) : strdup(JK_TTS_DEFAULT_PORT);

    // Try to load voice_id from KV storage first
    char *saved_voice_id = NULL;
    if (lisa_kv_get_string(KV_KEY_USER_VOICE_ID, &saved_voice_id) == 0 && saved_voice_id != NULL) {
        tts->voice_id = strdup(saved_voice_id);
        LISA_LOGI(TAG, "Loaded voice_id from KV: %s", saved_voice_id);
    } else {
        // Use default voice_id if not found in KV
        tts->voice_id = strdup(JK_TTS_VOICE_ID);
        LISA_LOGI(TAG, "Using default voice_id: %s", JK_TTS_VOICE_ID);
    }

    tts->state = JK_TTS_STATE_DISCONNECTED;
    tts->data_complete = false;
    tts->has_pending = false;
    tts->pending_text[0] = '\0';

    // Store global instance for deferred pending request handling
    g_tts_instance = tts;

    if (cbs) {
        memcpy(&tts->cbs, cbs, sizeof(jk_tts_callbacks_t));
    }

    jk_ws_config_t config = {
        .host = tts->host,
        .port = tts->port,
        .path = JK_TTS_DEFAULT_PATH,
        .timeout_ms = 60000,
        .user = tts,
        .on_event = tts_on_event,
        .on_data = tts_on_data,
    };

    tts->ws = jk_ws_create(&config);
    if (!tts->ws) {
        LISA_LOGE(TAG, "Failed to create WebSocket");
        lisa_mem_free(tts->host);
        lisa_mem_free(tts->port);
        lisa_mem_free(tts->voice_id);
        lisa_mem_free(tts);
        return NULL;
    }

    return tts;
}

void jk_tts_destroy(jk_tts_t *tts) {
    if (!tts) return;

    if (tts->ws) {
        jk_ws_destroy(tts->ws);
    }

    if (g_tts_instance == tts) {
        g_tts_instance = NULL;
    }

    if (tts->host) lisa_mem_free(tts->host);
    if (tts->port) lisa_mem_free(tts->port);
    if (tts->voice_id) lisa_mem_free(tts->voice_id);
    lisa_mem_free(tts);
}

int jk_tts_connect(jk_tts_t *tts) {
    if (!tts || !tts->ws) return -1;

    if (tts->state == JK_TTS_STATE_CONNECTED) {
        return 0;
    }

    LISA_LOGI(TAG, "Connecting to ws://%s:%s%s", tts->host, tts->port, JK_TTS_DEFAULT_PATH);
    tts->state = JK_TTS_STATE_CONNECTING;

    int ret = jk_ws_connect(tts->ws);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to connect: %d", ret);
        tts->state = JK_TTS_STATE_DISCONNECTED;
        return -1;
    }

    return 0;
}

int jk_tts_disconnect(jk_tts_t *tts) {
    if (!tts || !tts->ws) return -1;

    jk_ws_disconnect(tts->ws);
    tts->state = JK_TTS_STATE_DISCONNECTED;
    return 0;
}

int jk_tts_request(jk_tts_t *tts, const char *text, const char *voice_id) {
    if (!tts || !tts->ws || !text) {
        LISA_LOGE(TAG, "TTS request: invalid params tts=%p, ws=%p, text=%p", tts, tts ? tts->ws : NULL, text);
        return -1;
    }
    if (tts->state == JK_TTS_STATE_DISCONNECTED) {
        LISA_LOGE(TAG, "TTS request: TTS disconnected, state=%d", tts->state);
        return -1;
    }

    // Check if previous request is still in progress (state is PLAYING)
    // If so, cancel the old request and send the new one immediately (new TTS has priority)
    // Old session data will be discarded by request_id filtering
    if (tts->state == JK_TTS_STATE_PLAYING) {
        LISA_LOGW(TAG, "Previous TTS request still in progress, cancelling and sending new request (priority mode)");
        // Generate a new request_id to distinguish from the old request
        // Old request's audio data will be filtered out by request_id mismatch
    }

    tts->data_complete = false;
    tts->state = JK_TTS_STATE_PLAYING;  // Mark as playing when sending request
    generate_uuid(tts->current_request_id, sizeof(tts->current_request_id));

    LISA_LOGI(TAG, "TTS request prepared: request_id=%s, state=%d", tts->current_request_id, tts->state);

    const char *voice = voice_id ? voice_id : tts->voice_id;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "tts_request");
    cJSON_AddStringToObject(req, "request_id", tts->current_request_id);

    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "text", text);
    cJSON_AddStringToObject(params, "mode", "streaming");
    cJSON_AddStringToObject(params, "voice_id", voice);
    cJSON_AddItemToObject(req, "params", params);

    char *json_str = cJSON_PrintUnformatted(req);
    LISA_LOGI(TAG, "TTS request JSON: %s", json_str);
    int ret = jk_ws_send_text(tts->ws, json_str);
    LISA_LOGI(TAG, "TTS request sent: request_id=%s, ret=%d", tts->current_request_id, ret);
    lisa_mem_free(json_str);
    cJSON_Delete(req);

    return ret;
}

int jk_tts_stop(jk_tts_t *tts) {
    if (!tts) return -1;

    tts->state = JK_TTS_STATE_CONNECTED;
    return 0;
}

void jk_tts_check_pending(jk_tts_t *tts) {
    if (!tts || !tts->has_pending) {
        return;
    }

    // Defer sending pending request to event loop thread to avoid
    // reentrant calls from WebSocket receive thread
    LISA_LOGI(TAG, "Deferring pending TTS request to event loop");
    evs_handler_post_runnable(send_pending_request_runnable, tts);
}

bool jk_tts_is_connected(jk_tts_t *tts) {
    return tts && tts->state >= JK_TTS_STATE_CONNECTED;
}

bool jk_tts_is_playing(jk_tts_t *tts) {
    return tts && tts->state == JK_TTS_STATE_PLAYING;
}

bool jk_tts_is_data_complete(jk_tts_t *tts) {
    return tts && tts->data_complete;
}

void jk_tts_set_voice(jk_tts_t *tts, const char *voice_id) {
    if (!tts || !voice_id) return;

    if (tts->voice_id) {
        lisa_mem_free(tts->voice_id);
    }
    tts->voice_id = strdup(voice_id);
}
