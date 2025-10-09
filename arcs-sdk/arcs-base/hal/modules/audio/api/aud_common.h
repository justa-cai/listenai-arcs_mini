/**
 ****************************************************************************************
 *
 * @file aud_mgr.h
 *
 * @brief audio modules
 *
 * Copyright (C) Listenai.com 2023
 *
 *
 ****************************************************************************************
 */

#ifndef AUD_COMMON_H_
#define AUD_COMMON_H_

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 * @name audio manager api
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include <string.h>
#include "log_print.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
extern void *btos_malloc(uint32_t size);
extern void btos_free(void *ptr);

#ifdef WIN32
#define AUD_MALLOC(x)  malloc(x) 
#define AUD_FREE(x)    free(x) 
#else
#define AUD_MALLOC(x)  btos_malloc(x)
#define AUD_FREE(x)    btos_free(x)
#endif
#define AUD_STATE_SET(x, y) (x=y>=0x10 ? ((x&0x0f)|y) : ((x&0xf0)|y))
#define AUD_STATE_GET(x, y) (x&y)

#define RESAMPLE_CONFIG

/*
 * DEFINES
 ****************************************************************************************
 */
enum aud_error
{
    AUD_ERROR_NO = 0,
    AUD_ERROR_TYPE_NOT_SUPPORT,
    AUD_ERROR_PARAM_UNAVAILE,

    AUD_ERROR_BUF_NO_RESOURCE,

    AUD_ERROR_BUF_NO_ENOUGH,

    AUD_ERROR_PARAM_NULL,

    AUD_ERROR_MSG_ID_NO,


    AUD_ERROR_NOT_ENOUGH_DATA,
};

enum aud_type
{
    AUD_TYPE_NONE           = 0,
    AUD_TYPE_AMR            = 1,
    AUD_TYPE_AMR_WB         = 2,
    AUD_TYPE_MP3            = 3,
    AUD_TYPE_AAC            = 4,
    AUD_TYPE_PCM_8K         = 5,
    AUD_TYPE_PCM_16K        = 6,
    AUD_TYPE_G711_ALAW      = 7,
    AUD_TYPE_G711_ULAW      = 8,
    AUD_TYPE_WAV            = 9,
    AUD_TYPE_WAV_ALAW       = 10,
    AUD_TYPE_WAV_ULAW       = 11,
    AUD_TYPE_WAV_DVI_ADPCM  = 12,
    AUD_TYPE_M4A            = 13,
    AUD_TYPE_WMA            = 14,
    AUD_TYPE_MIDI           = 15,
    AUD_TYPE_SBC            = 16,
    AUD_TYPE_MSBC           = 17,
    AUD_TYPE_CVSD           = 18,
    AUD_TYPE_TONE           = 19,
    AUD_TYPE_TS             = 20,
    AUD_TYPE_LC3            = 21,
    AUD_TYPE_VENDOR         = 50,

};

 enum aud_sample
 {
    AUD_SAMPLE_8000HZ             = 8000,
    AUD_SAMPLE_11025HZ            = 11025,
    AUD_SAMPLE_16000HZ            = 16000,
    AUD_SAMPLE_22050HZ            = 22050,
    AUD_SAMPLE_24000HZ            = 24000,
    AUD_SAMPLE_32000HZ            = 32000,
    AUD_SAMPLE_44100HZ            = 44100,
    AUD_SAMPLE_48000HZ            = 48000,
    AUD_SAMPLE_88200HZ            = 88200,
    AUD_SAMPLE_96000HZ            = 96000,
    AUD_SAMPLE_176400HZ           = 176400,
    AUD_SAMPLE_192000HZ           = 192000,
    AUD_SAMPLE_384000HZ           = 384000,
 };

 typedef struct aud_codec_info
{
    /// Index
    uint8_t              conidx;
    /// Aud type  @see enum aud_type
    uint8_t              aud_type;
    /// Aud sample
    uint16_t             aud_sample;
    /// Aud ch
    uint8_t              aud_ch;
    /// Aud bits wide, 16 or 24
    uint8_t              aud_bits_wide;
    /// one frame duration ms 1: 0.1ms
    uint16_t             frame_dur;
    /// one frame length
    uint16_t             frame_len;

    /// channel alloc
    uint32_t             ch_alloc;
}aud_codec_info_t;

 typedef struct bt_aud_pkt_info
{
    /// Bt connect index.
    uint8_t              conidx;
    /// Aud frame number.
    uint8_t              frame_num;
    /// Aud sequence.
    uint16_t             seq;
    /// Aud packet length.
    uint16_t             len;
    /// Aud data ptr.
    uint8_t *            data;
}bt_aud_pkt_info_t;

typedef struct aud_cb
{
    /**
    ****************************************************************************************
    * @brief Reception of aud start complete
    ****************************************************************************************
    */
    void (*cb_aud_start_ind)(uint8_t type, uint16_t status);

    /**
    ****************************************************************************************
    * @brief Handles audio stop indicate event from the AUD MGR
    *
    * @param[in] type              audio type, @see enum app_aud_type
    * @param[in] status            status of the stop report
    ****************************************************************************************
    */
    void (*cb_aud_stop_ind)(uint8_t type, uint16_t status);
}aud_cb_t;

/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// @} audio manager api
/// @} AUDIO

#endif // AUD_COMMON_H_
