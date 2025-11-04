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

#ifndef AUD_DAC_H_
#define AUD_DAC_H_

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
/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
typedef void *
(*dac_play_isr_cb)(uint8_t status);

typedef struct aud_dac_out_cfg
{
    uint8_t            ch;
    uint8_t            out_bits;
    uint16_t           sample_rate;
    /// play pcm date bytes
    uint16_t           play_len;
    uint8_t            *play_buf;
    dac_play_isr_cb    play_cb;
}aud_dac_out_cfg_t;

typedef struct aud_dac_out_env
{
    aud_dac_out_cfg_t  dac_cfg;
    void *             dac_handle;
    uint16_t           sample_len;
    uint16_t           pi_po_handle;
    uint8_t            *buf_pi;
    uint8_t            *buf_po;
}aud_dac_out_env_t;

/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint8_t app_dac_out_init(aud_dac_out_cfg_t *dac_cfg);
uint8_t app_dac_out_play(aud_dac_out_cfg_t *dac_cfg);
uint8_t app_dac_out_stop(void);
uint8_t dac_oneshot_play(aud_dac_out_cfg_t *dac_cfg);

/// @} audio manager api
/// @} AUDIO

#endif // AUD_DAC_H_
