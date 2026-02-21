#define TAG "jk_tts_parser"

#include "jk_tts_parser.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>

static inline uint16_t be16toh(uint16_t val) {
    return (val >> 8) | (val << 8);
}

static inline uint32_t be32toh(uint32_t val) {
    return ((val >> 24) & 0xff) |
           ((val >> 8) & 0xff00) |
           ((val << 8) & 0xff0000) |
           ((val << 24) & 0xff000000);
}

int jk_tts_parse_metadata_json(const char *json_str, uint32_t len, jk_tts_metadata_t *meta) {
    cJSON *json = cJSON_ParseWithLength(json_str, len);
    if (!json) {
        LISA_LOGE(TAG, "Failed to parse metadata JSON");
        return -1;
    }

    memset(meta, 0, sizeof(jk_tts_metadata_t));

    cJSON *request_id = cJSON_GetObjectItem(json, "request_id");
    if (request_id && cJSON_IsString(request_id)) {
        strncpy(meta->request_id, request_id->valuestring, sizeof(meta->request_id) - 1);
    }

    cJSON *is_final = cJSON_GetObjectItem(json, "is_final");
    meta->is_final = is_final && cJSON_IsTrue(is_final);

    cJSON *sample_rate = cJSON_GetObjectItem(json, "sample_rate");
    if (sample_rate && cJSON_IsNumber(sample_rate)) {
        meta->sample_rate = sample_rate->valueint;
    } else {
        meta->sample_rate = 16000;
    }

    cJSON *sequence = cJSON_GetObjectItem(json, "sequence");
    if (sequence && cJSON_IsNumber(sequence)) {
        meta->sequence = sequence->valueint;
    }

    cJSON_Delete(json);
    return 0;
}

int jk_tts_parse_frame(uint8_t *data, uint32_t len, jk_tts_parse_result_t *result) {
    if (!data || !result) {
        return -1;
    }

    memset(result, 0, sizeof(jk_tts_parse_result_t));

    if (len < sizeof(jk_tts_frame_header_t)) {
        LISA_LOGD(TAG, "Frame too short for header, treating as raw PCM: len=%u", len);
        result->is_raw_pcm = true;
        result->audio_data = (int16_t *)data;
        result->audio_len = len / sizeof(int16_t);
        return 0;
    }

    jk_tts_frame_header_t *header = (jk_tts_frame_header_t *)data;
    uint16_t magic = be16toh(header->magic);

    if (magic == JK_TTS_FRAME_MAGIC) {
        LISA_LOGD(TAG, "Detected standard frame format with magic: 0x%04X", magic);

        uint32_t meta_len = be32toh(header->metadata_len);
        uint32_t header_size = sizeof(jk_tts_frame_header_t);

        // Handle case where metadata_len = 0 (no JSON metadata)
        if (meta_len == 0) {
            LISA_LOGD(TAG, "No metadata in frame, treating as raw audio data");
            result->is_raw_pcm = true;
            result->audio_data = (int16_t *)(data + header_size);
            result->audio_len = (len - header_size) / sizeof(int16_t);
            result->metadata.request_id[0] = '\0';  // Empty request_id means accept as current
            return 0;
        }

        if (len < header_size + meta_len + 4) {
            LISA_LOGE(TAG, "Incomplete frame: len=%u, expected>=%u",
                      len, header_size + meta_len + 4);
            result->parse_error = -3;
            return -3;
        }

        char *meta_json = (char *)(data + header_size);
        if (jk_tts_parse_metadata_json(meta_json, meta_len, &result->metadata) != 0) {
            result->parse_error = -4;
            return -4;
        }

        uint32_t *payload_len_ptr = (uint32_t *)(data + header_size + meta_len);
        uint32_t payload_len = be32toh(*payload_len_ptr);

        if (len < header_size + meta_len + 4 + payload_len) {
            LISA_LOGE(TAG, "Incomplete audio payload");
            result->parse_error = -5;
            return -5;
        }

        if (payload_len > 0) {
            result->audio_data = (int16_t *)(data + header_size + meta_len + 4);
            result->audio_len = payload_len / sizeof(int16_t);
        }

        LISA_LOGD(TAG, "Frame parsed: request_id=%s, is_final=%d, audio_len=%u",
                  result->metadata.request_id, result->metadata.is_final, result->audio_len);
        return 0;
    } else {
        LISA_LOGD(TAG, "No valid magic (0x%04X), treating as raw PCM: len=%u", magic, len);
        result->is_raw_pcm = true;
        result->audio_data = (int16_t *)data;
        result->audio_len = len / sizeof(int16_t);
        return 0;
    }
}
