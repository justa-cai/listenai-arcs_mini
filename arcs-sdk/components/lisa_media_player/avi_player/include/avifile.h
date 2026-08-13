/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAKEFOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define RIFF_ID MAKEFOURCC('R', 'I', 'F', 'F')
#define AVI_ID  MAKEFOURCC('A', 'V', 'I', ' ')
#define LIST_ID MAKEFOURCC('L', 'I', 'S', 'T')
#define HDRL_ID MAKEFOURCC('h', 'd', 'r', 'l')
#define AVIH_ID MAKEFOURCC('a', 'v', 'i', 'h')
#define STRL_ID MAKEFOURCC('s', 't', 'r', 'l')
#define STRH_ID MAKEFOURCC('s', 't', 'r', 'h')
#define STRF_ID MAKEFOURCC('s', 't', 'r', 'f')
#define MOVI_ID MAKEFOURCC('m', 'o', 'v', 'i')
#define MJPG_ID MAKEFOURCC('M', 'J', 'P', 'G')
#define VIDS_ID MAKEFOURCC('v', 'i', 'd', 's')
#define AUDS_ID MAKEFOURCC('a', 'u', 'd', 's')

typedef enum {
    AVI_VIDEO_FORMAT_UNKNOWN = 0,
    AVI_VIDEO_FORMAT_MJPEG,
} avi_video_format_t;

#pragma pack(push, 1)

typedef struct {
    uint32_t fourcc;
    uint32_t size;
} avi_chunk_header_t;

typedef struct {
    uint32_t list;
    uint32_t size;
    uint32_t fourcc;
} avi_list_header_t;

typedef struct {
    uint32_t fourcc;
    uint32_t size;
    uint32_t us_per_frame;
    uint32_t max_bytes_per_sec;
    uint32_t padding;
    uint32_t flags;
    uint32_t total_frames;
    uint32_t init_frames;
    uint32_t streams;
    uint32_t suggest_buff_size;
    uint32_t width;
    uint32_t height;
    uint32_t reserved[4];
} avi_main_header_t;

typedef struct {
    uint32_t fourcc;
    uint32_t size;
    uint32_t fourcc_type;
    uint32_t fourcc_codec;
    uint32_t flags;
    uint16_t priority;
    uint16_t language;
    uint32_t init_frames;
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
    uint32_t suggest_buff_size;
    uint32_t quality;
    uint32_t sample_size;
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } rc_frame;
} avi_stream_header_t;

typedef struct {
    uint32_t fourcc;
    uint32_t size;
    uint32_t size1;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bitcount;
    uint32_t fourcc_compression;
    uint32_t image_size;
    uint32_t x_pixels_per_meter;
    uint32_t y_pixels_per_meter;
    uint32_t num_colors;
    uint32_t imp_colors;
} avi_video_format_desc_t;

typedef struct {
    uint32_t fourcc;
    uint32_t size;
    uint16_t format_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
} avi_audio_format_desc_t;

#pragma pack(pop)

typedef struct {
    uint32_t movi_start;
    uint32_t movi_size;

    /* Main AVI header timing */
    uint32_t avih_us_per_frame;

    /* Video stream header timing (rate/scale) */
    uint32_t vids_strh_scale;
    uint32_t vids_strh_rate;

    uint16_t vids_fps;
    uint16_t vids_width;
    uint16_t vids_height;
    avi_video_format_t vids_format;

    uint16_t auds_channels;
    uint32_t auds_sample_rate;
    uint16_t auds_bits;

    /* Extra WAVEFORMAT fields for diagnostics / robustness */
    uint16_t auds_format_tag;
    uint16_t auds_block_align;
    uint32_t auds_avg_bytes_per_sec;

    /* AVI stream header fields for audio (often reliable) */
    uint32_t auds_strh_scale;
    uint32_t auds_strh_rate;

    /* Total frames from AVI main header */
    uint32_t total_frames;
} avi_file_info_t;

int avi_search_fourcc(uint32_t fourcc, const uint8_t *buffer, uint32_t length);
int avi_parse(avi_file_info_t *avi_file, const uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif
