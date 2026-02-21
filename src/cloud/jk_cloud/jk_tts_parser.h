#ifndef __JK_TTS_PARSER_H__
#define __JK_TTS_PARSER_H__

#include <stdint.h>
#include <stdbool.h>

#define JK_TTS_FRAME_MAGIC 0xAA55

typedef struct {
    char request_id[64];
    bool is_final;
    int sample_rate;
    int sequence;
} jk_tts_metadata_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t msg_type;
    uint8_t reserved;
    uint32_t metadata_len;
} jk_tts_frame_header_t;

typedef struct {
    jk_tts_metadata_t metadata;
    int16_t *audio_data;
    uint32_t audio_len;
    int parse_error;
    bool is_raw_pcm;
} jk_tts_parse_result_t;

int jk_tts_parse_frame(uint8_t *data, uint32_t len, jk_tts_parse_result_t *result);
int jk_tts_parse_metadata_json(const char *json_str, uint32_t len, jk_tts_metadata_t *meta);

#endif
