#define TAG "jk_asr"

#include "jk_asr.h"
#include "jk_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#ifdef CONFIG_SDK_MODULE_OPUS_DECODER
#include "opus/opus.h"
#define HAS_OPUS 1
#else
#define HAS_OPUS 0
#warning "Opus decoder module not enabled, Opus encoding support will be disabled"
#endif

/* Opus encoder configuration */
#define OPUS_FRAME_SIZE (JK_ASR_SAMPLE_RATE / 50)  /* 20ms at 16kHz = 320 samples */
#define OPUS_BITRATE 24000  /* 24 kbps */
#define OPUS_MAX_PACKET_SIZE (1276)  /* Max Opus frame size */

/* Static counter for Opus frame debugging */
#if HAS_OPUS
static uint32_t s_opus_encode_count = 0;
static uint32_t s_opus_send_fail_count = 0;
#endif

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

    cJSON *json = cJSON_ParseWithLength((const char *)data, len);
    if (!json) {
        LISA_LOGE(TAG, "Failed to parse ASR JSON: %.*s", len < 128 ? (int)len : 128, (const char *)data);
        return;
    }

    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        LISA_LOGW(TAG, "ASR JSON missing 'type' field");
        goto cleanup;
    }

    const char *msg_type = type_item->valuestring;

    // Handle VAD events
    if (strcmp(msg_type, "vad") == 0) {
        cJSON *event_item = cJSON_GetObjectItem(json, "event");
        if (event_item && cJSON_IsString(event_item)) {
            const char *event = event_item->valuestring;
            float duration = 0.0f;

            // Get duration for speech_end events
            if (strcmp(event, "speech_end") == 0) {
                cJSON *duration_item = cJSON_GetObjectItem(json, "duration");
                if (duration_item && cJSON_IsNumber(duration_item)) {
                    duration = (float)duration_item->valuedouble;
                }
            }

            LISA_LOGI(TAG, "ASR VAD: event=%s, duration=%.3f", event, duration);

            if (asr->cbs.on_vad_event) {
                asr->cbs.on_vad_event(asr, event, duration);
            }
        }
        goto cleanup;
    }

    // Handle recognition results
    if (strcmp(msg_type, "result") == 0) {
        LISA_LOGI(TAG, "ASR JSON received (%u bytes): %.*s", len, len < 256 ? (int)len : 256, (const char *)data);

        cJSON *text_item = cJSON_GetObjectItem(json, "text");
        cJSON *is_final_item = cJSON_GetObjectItem(json, "is_final");

        if (text_item && cJSON_IsString(text_item)) {
            const char *text = text_item->valuestring;
            bool is_final = is_final_item && cJSON_IsTrue(is_final_item);

            LISA_LOG(TAG, "ASR: [%s] (final=%d)", text, is_final);

            if (asr->cbs.on_text_result && strlen(text) > 0) {
                asr->cbs.on_text_result(asr, text, is_final);
            }
        } else {
            LISA_LOGW(TAG, "ASR JSON missing 'text' field");
        }
        goto cleanup;
    }

    // Handle audio format set acknowledgment (for set_audio_format command)
    if (strcmp(msg_type, "audio_format_set") == 0) {
        cJSON *format_item = cJSON_GetObjectItem(json, "format");
        if (format_item && cJSON_IsString(format_item)) {
            const char *format = format_item->valuestring;
            LISA_LOGI(TAG, "=== SERVER CONFIRMED: audio_format_set to '%s' ===", format);
            if (strcmp(format, "opus") == 0) {
                LISA_LOGI(TAG, "Opus mode is now ACTIVE on server, sending encoded audio...");
            }
        }
        goto cleanup;
    }

    // Handle error messages
    if (strcmp(msg_type, "error") == 0) {
        cJSON *msg_item = cJSON_GetObjectItem(json, "message");
        const char *error_msg = msg_item && cJSON_IsString(msg_item) ? msg_item->valuestring : "Unknown error";

        // Check if server doesn't support Opus (error code 17), fallback to PCM
        cJSON *code_item = cJSON_GetObjectItem(json, "code");
        int error_code = code_item && cJSON_IsNumber(code_item) ? code_item->valueint : 0;

        if (asr->opus_enabled && error_code == 17) {
            LISA_LOGW(TAG, "Server doesn't support Opus, falling back to PCM mode");

            /* Cleanup Opus encoder */
            if (asr->opus_encoder) {
                opus_encoder_destroy(asr->opus_encoder);
                asr->opus_encoder = NULL;
            }
            if (asr->pcm_buffer) {
                lisa_mem_free(asr->pcm_buffer);
                asr->pcm_buffer = NULL;
            }
            asr->pcm_buffer_pos = 0;
            asr->codec_mode = JK_ASR_CODEC_PCM;
            asr->opus_enabled = false;

            // Don't report this error to upper layer, just fallback
            goto cleanup;
        }

        LISA_LOGE(TAG, "ASR error: %s", error_msg);

        if (asr->cbs.on_error) {
            asr->cbs.on_error(asr, error_msg);
        }
        goto cleanup;
    }

    // Handle pong response
    if (strcmp(msg_type, "pong") == 0) {
        LISA_LOGD(TAG, "ASR pong received");
        goto cleanup;
    }

    LISA_LOGW(TAG, "ASR unknown message type: %s", msg_type);

cleanup:
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
    asr->codec_mode = JK_ASR_CODEC_PCM;
    asr->opus_encoder = NULL;
    asr->pcm_buffer = NULL;
    asr->pcm_buffer_pos = 0;
    asr->opus_enabled = false;

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

    /* Disable Opus if enabled */
    if (asr->opus_enabled) {
        jk_asr_opus_disable(asr);
    }

    if (asr->ws) {
        jk_ws_destroy(asr->ws);
    }

    if (asr->host) lisa_mem_free(asr->host);
    if (asr->port) lisa_mem_free(asr->port);
    if (asr->pcm_buffer) lisa_mem_free(asr->pcm_buffer);
    lisa_mem_free(asr);
}

int jk_asr_connect(jk_asr_t *asr) {
    if (!asr || !asr->ws) return -1;

    if (asr->state == JK_ASR_STATE_CONNECTED) {
        return 0;
    }

    LISA_LOG(TAG, "Connecting to ws://%s:%s/", asr->host, asr->port);
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

int jk_asr_opus_enable(jk_asr_t *asr) {
    LISA_LOGI(TAG, "jk_asr_opus_enable called: asr=%p, ws=%p, state=%d, HAS_OPUS=%d",
              asr, asr ? asr->ws : NULL, asr ? asr->state : -1, HAS_OPUS);

    if (!asr || !asr->ws) return -1;

#if HAS_OPUS
    if (asr->opus_enabled) {
        LISA_LOGW(TAG, "Opus already enabled");
        return 0;
    }

    if (asr->state != JK_ASR_STATE_CONNECTED) {
        LISA_LOGE(TAG, "Cannot enable Opus: not connected (state=%d)", asr->state);
        return -1;
    }

    /* Create Opus encoder */
    int opus_err;
    asr->opus_encoder = opus_encoder_create(
        JK_ASR_SAMPLE_RATE,
        1,
        OPUS_APPLICATION_VOIP,
        &opus_err
    );

    if (!asr->opus_encoder || opus_err != OPUS_OK) {
        LISA_LOGE(TAG, "Failed to create Opus encoder: %d", opus_err);
        return -1;
    }

    /* Set bitrate */
    opus_encoder_ctl(asr->opus_encoder, OPUS_SET_BITRATE(OPUS_BITRATE));

    /* Allocate PCM buffer for accumulating samples */
    asr->pcm_buffer = lisa_mem_calloc(OPUS_FRAME_SIZE, sizeof(int16_t));
    if (!asr->pcm_buffer) {
        LISA_LOGE(TAG, "Failed to allocate PCM buffer");
        opus_encoder_destroy(asr->opus_encoder);
        asr->opus_encoder = NULL;
        return -1;
    }
    asr->pcm_buffer_pos = 0;

    /* Send set_audio_format command to server */
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        LISA_LOGE(TAG, "Failed to create JSON");
        opus_encoder_destroy(asr->opus_encoder);
        lisa_mem_free(asr->pcm_buffer);
        asr->opus_encoder = NULL;
        asr->pcm_buffer = NULL;
        return -1;
    }

    cJSON_AddStringToObject(json, "command", "set_audio_format");
    cJSON_AddStringToObject(json, "format", "opus");
    char *json_str = cJSON_PrintUnformatted(json);

    LISA_LOGI(TAG, "Sending set_audio_format command: %s", json_str);

    int ret = jk_ws_send_text(asr->ws, json_str);

    cJSON_Delete(json);
    free(json_str);

    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to send set_audio_format command: %d", ret);
        opus_encoder_destroy(asr->opus_encoder);
        lisa_mem_free(asr->pcm_buffer);
        asr->opus_encoder = NULL;
        asr->pcm_buffer = NULL;
        return -1;
    }

    LISA_LOGI(TAG, "set_audio_format command sent successfully, waiting for server response...");

    asr->codec_mode = JK_ASR_CODEC_OPUS;
    asr->opus_enabled = true;

    /* Reset debug counters */
    s_opus_encode_count = 0;
    s_opus_send_fail_count = 0;

    LISA_LOGI(TAG, "Opus encoding enabled (16kHz, mono, %d kbps), frame_size=%d samples",
              OPUS_BITRATE / 1000, OPUS_FRAME_SIZE);
    return 0;
#else
    LISA_LOGE(TAG, "Opus support not compiled in");
    return -1;
#endif
}

int jk_asr_opus_disable(jk_asr_t *asr) {
    if (!asr || !asr->ws) return -1;

#if HAS_OPUS
    if (!asr->opus_enabled) {
        return 0;
    }

    /* Flush any remaining PCM data */
    if (asr->pcm_buffer_pos > 0) {
        uint8_t opus_packet[OPUS_MAX_PACKET_SIZE];

        /* Zero-pad the remaining buffer and encode */
        memset(&asr->pcm_buffer[asr->pcm_buffer_pos], 0,
               (OPUS_FRAME_SIZE - asr->pcm_buffer_pos) * sizeof(int16_t));

        int encoded_bytes = opus_encode(
            asr->opus_encoder,
            asr->pcm_buffer,
            OPUS_FRAME_SIZE,
            opus_packet,
            OPUS_MAX_PACKET_SIZE
        );

        if (encoded_bytes > 0) {
            jk_ws_send_binary(asr->ws, opus_packet, encoded_bytes);
        }
    }

    /* Send set_audio_format command to switch back to PCM */
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "command", "set_audio_format");
        cJSON_AddStringToObject(json, "format", "pcm");
        char *json_str = cJSON_PrintUnformatted(json);
        jk_ws_send_text(asr->ws, json_str);
        cJSON_Delete(json);
        free(json_str);
    }

    /* Cleanup encoder */
    if (asr->opus_encoder) {
        opus_encoder_destroy(asr->opus_encoder);
        asr->opus_encoder = NULL;
    }

    if (asr->pcm_buffer) {
        lisa_mem_free(asr->pcm_buffer);
        asr->pcm_buffer = NULL;
    }
    asr->pcm_buffer_pos = 0;

    asr->codec_mode = JK_ASR_CODEC_PCM;
    asr->opus_enabled = false;

    LISA_LOGI(TAG, "Opus encoding disabled");
    return 0;
#else
    return 0;
#endif
}

bool jk_asr_is_opus_enabled(jk_asr_t *asr) {
    return asr && asr->opus_enabled;
}

int jk_asr_send_audio(jk_asr_t *asr, const int16_t *samples, uint32_t count) {
    if (!asr || !asr->ws || !samples || count == 0) return -1;
    if (asr->state != JK_ASR_STATE_CONNECTED) return -1;

#if HAS_OPUS
    if (asr->opus_enabled && asr->opus_encoder) {
        /* Opus encoding mode */
        uint32_t samples_processed = 0;
        uint8_t opus_packet[OPUS_MAX_PACKET_SIZE];
        static uint32_t log_counter = 0;

        while (samples_processed < count) {
            /* Fill PCM buffer */
            uint32_t remaining = count - samples_processed;
            uint32_t space = OPUS_FRAME_SIZE - asr->pcm_buffer_pos;
            uint32_t to_copy = (remaining < space) ? remaining : space;

            memcpy(&asr->pcm_buffer[asr->pcm_buffer_pos],
                   &samples[samples_processed],
                   to_copy * sizeof(int16_t));
            asr->pcm_buffer_pos += to_copy;
            samples_processed += to_copy;

            /* Encode when buffer is full */
            if (asr->pcm_buffer_pos >= OPUS_FRAME_SIZE) {
                int encoded_bytes = opus_encode(
                    asr->opus_encoder,
                    asr->pcm_buffer,
                    OPUS_FRAME_SIZE,
                    opus_packet,
                    OPUS_MAX_PACKET_SIZE
                );

                if (encoded_bytes > 0) {
                    s_opus_encode_count++;
                    int ret = jk_ws_send_binary(asr->ws, opus_packet, encoded_bytes);
                    if (ret != 0) {
                        s_opus_send_fail_count++;
                    }

                    /* Log every 50 frames to avoid spam */
                    if (++log_counter >= 50) {
                        LISA_LOGI(TAG, "Opus: encoded %u frames, %d bytes/frame, sent_fail=%u",
                                  s_opus_encode_count, encoded_bytes, s_opus_send_fail_count);
                        log_counter = 0;
                    }
                } else {
                    LISA_LOGE(TAG, "Opus: encode failed, ret=%d", encoded_bytes);
                }

                asr->pcm_buffer_pos = 0;
            }
        }

        return 0;
    }
#endif

    /* PCM mode - send directly */
    return jk_ws_send_binary(asr->ws, samples, count * sizeof(int16_t));
}

bool jk_asr_is_connected(jk_asr_t *asr) {
    return asr && asr->state == JK_ASR_STATE_CONNECTED;
}
