/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "avifile.h"

#include <stdio.h>
#include <string.h>

int avi_search_fourcc(uint32_t fourcc, const uint8_t *buffer, uint32_t length)
{
    if (length < 4) {
        return -1;
    }

    for (uint32_t i = 0; i <= length - 4; i++) {
        uint32_t v;
        memcpy(&v, buffer + i, sizeof(v));
        if (v == fourcc) {
            return (int)i;
        }
    }
    return -1;
}

static int parse_strl(avi_file_info_t *avi_file, const uint8_t *buffer, uint32_t length, uint32_t *list_length)
{
    const uint8_t *pdata = buffer;

    avi_list_header_t strl;
    if (length < sizeof(strl)) {
        return -1;
    }
    memcpy(&strl, pdata, sizeof(strl));
    if (strl.list != LIST_ID || strl.fourcc != STRL_ID) {
        return -1;
    }
    pdata += sizeof(avi_list_header_t);
    *list_length = strl.size + 8;

    avi_stream_header_t strh;
    if (length < sizeof(avi_list_header_t) + sizeof(strh)) {
        return -1;
    }
    memcpy(&strh, pdata, sizeof(strh));
    if (strh.fourcc != STRH_ID) {
        return -1;
    }
    pdata += sizeof(avi_stream_header_t);

    if (strh.fourcc_type == VIDS_ID) {
        avi_video_format_desc_t strf;
        if (length < (uint32_t)(pdata - buffer) + sizeof(strf)) {
            return -1;
        }
        memcpy(&strf, pdata, sizeof(strf));
        if (strf.fourcc != STRF_ID) {
            return -1;
        }

        avi_file->vids_strh_scale = strh.scale;
        avi_file->vids_strh_rate = strh.rate;
        if (strh.scale == 0 || strh.rate == 0) {
            avi_file->vids_fps = 0;
        } else {
            /* Round instead of truncating (e.g. 30000/1001 -> 30) */
            uint32_t fps = (strh.rate + (strh.scale / 2U)) / strh.scale;
            if (fps == 0) {
                fps = 1;
            }
            if (fps > 1000) {
                fps = 1000;
            }
            avi_file->vids_fps = (uint16_t)fps;
        }
        avi_file->vids_width = (uint16_t)strf.width;
        avi_file->vids_height = (uint16_t)strf.height;
        avi_file->vids_format = (strh.fourcc_codec == MJPG_ID) ? AVI_VIDEO_FORMAT_MJPEG : AVI_VIDEO_FORMAT_UNKNOWN;
        return 0;
    }

    if (strh.fourcc_type == AUDS_ID) {
        avi_file->auds_strh_scale = strh.scale;
        avi_file->auds_strh_rate = strh.rate;

        avi_chunk_header_t hdr;
        if (length < (uint32_t)(pdata - buffer) + sizeof(hdr)) {
            return -1;
        }
        memcpy(&hdr, pdata, sizeof(hdr));
        if (hdr.fourcc != STRF_ID) {
            return -1;
        }
        if (length < (uint32_t)(pdata - buffer) + 8 + hdr.size) {
            return -1;
        }

        avi_audio_format_desc_t strf;
        memcpy(&strf, pdata, sizeof(strf));
        avi_file->auds_channels = strf.channels;
        avi_file->auds_sample_rate = strf.samples_per_sec;
        avi_file->auds_bits = strf.bits_per_sample;

        avi_file->auds_format_tag = strf.format_tag;
        avi_file->auds_block_align = strf.block_align;
        avi_file->auds_avg_bytes_per_sec = strf.avg_bytes_per_sec;
        return 0;
    }

    return 0;
}

int avi_parse(avi_file_info_t *avi_file, const uint8_t *buffer, uint32_t length)
{
    if (!avi_file || !buffer || length < 512) {
        return -1;
    }

    memset(avi_file, 0, sizeof(*avi_file));

    const uint8_t *pdata = buffer;

    avi_list_header_t riff;
    memcpy(&riff, pdata, sizeof(riff));
    if (riff.list != RIFF_ID || riff.fourcc != AVI_ID) {
        return -1;
    }
    pdata += sizeof(avi_list_header_t);

    avi_list_header_t hdrl;
    memcpy(&hdrl, pdata, sizeof(hdrl));
    if (hdrl.list != LIST_ID || hdrl.fourcc != HDRL_ID) {
        return -3;
    }
    pdata += sizeof(avi_list_header_t);

    avi_main_header_t avih;
    memcpy(&avih, pdata, sizeof(avih));
    if (avih.fourcc != AVIH_ID) {
        return -5;
    }
    avi_file->avih_us_per_frame = avih.us_per_frame;
    avi_file->total_frames = avih.total_frames;
    pdata += sizeof(avi_main_header_t);

    for (uint32_t i = 0; i < avih.streams; i++) {
        uint32_t strl_size = 0;
        (void)parse_strl(avi_file, pdata, length - (uint32_t)(pdata - buffer), &strl_size);
        if (strl_size == 0) {
            break;
        }
        pdata += strl_size;
        if ((uint32_t)(pdata - buffer) >= length) {
            return -7;
        }
    }

    int movi_offset = avi_search_fourcc(MOVI_ID, pdata, length - (uint32_t)(pdata - buffer));
    if (movi_offset < 0) {
        return -7;
    }

    avi_file->movi_start = (uint32_t)(pdata - buffer) + (uint32_t)movi_offset + 4;

    const uint8_t *movi_list = pdata + movi_offset - 8;
    avi_list_header_t movi;
    memcpy(&movi, movi_list, sizeof(movi));
    if (movi.list != LIST_ID || movi.fourcc != MOVI_ID) {
        return -8;
    }
    avi_file->movi_size = movi.size;
    return 0;
}
