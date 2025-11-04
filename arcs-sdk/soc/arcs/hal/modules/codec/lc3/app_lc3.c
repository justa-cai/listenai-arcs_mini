/******************************************************************************
*                        ETSI TS 103 634 V1.3.1                               *
*              Low Complexity Communication Codec Plus (LC3plus)              *
*                                                                             *
* Copyright licence is solely granted through ETSI Intellectual Property      *
* Rights Policy, 3rd April 2019. No patent licence is granted by implication, *
* estoppel or otherwise.                                                      *
******************************************************************************/
                                                                               


#include "functions.h" /* needed for basop instrumentation */
#include "lc3.h"
#include "aud_common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* struct to hold command line arguments */
typedef struct lc3_cfg
{

    uint16_t   bitrate;
    uint16_t   sample_rate;
    uint8_t    channels;
    /// 16:16 bits depth, 24:24 bits depth.
    uint8_t    bips_out;
    uint8_t    formatG192;
    /// 100, 50, 25
    uint8_t    frame_ms;
    uint8_t    verbose;
    uint8_t    plc_meth;
    uint8_t    epmode;
    uint8_t    ept;
    uint8_t    hrmode;
    uint16_t   dc;
    uint8_t    *bandwidth;
    uint8_t    *channel_coder_vars_file;
    
    uint16_t   out_samples;
    int32_t    startFrame;
    int32_t    stopFrame;
    int32_t    lfe;
} lc3_cfg_t;

LC3PLUS_Enc   *p_encoder = NULL;
LC3PLUS_Dec   *p_decoder = NULL;
uint8_t       *p_scratch = NULL;
lc3_cfg_t     *p_lc3_cfg = NULL;

uint8_t app_lc3_start(uint16_t sample_rate, uint8_t channels, uint8_t frame_ms, uint8_t bips_out, uint8_t hrmode)
{
    LC3PLUS_Error status = LC3PLUS_OK;

    uint16_t decoder_size, scratch_size, delay;

#ifdef DYNMEM_COUNT
    Dyn_Mem_Init();
#endif

    CLOGD("app lc3 start, sample_rate:%d, channels:0x%x, frame_ms:0x%x, bips_out:0x%x, hrmode:%d",\
          sample_rate, channels, frame_ms, bips_out, hrmode);

    p_lc3_cfg = (lc3_cfg_t *)AUD_MALLOC(sizeof(lc3_cfg_t));
    memset(p_lc3_cfg, 0x00, sizeof(lc3_cfg_t));
    p_lc3_cfg->sample_rate = sample_rate;
    p_lc3_cfg->channels = channels;
    p_lc3_cfg->frame_ms = frame_ms;
    p_lc3_cfg->bips_out = bips_out;
    p_lc3_cfg->hrmode = hrmode;
    p_lc3_cfg->plc_meth = 1;
    /* Setup Decoder */
    decoder_size = lc3plus_dec_get_size(p_lc3_cfg->sample_rate, p_lc3_cfg->channels, (LC3PLUS_PlcMode)p_lc3_cfg->plc_meth);
    p_decoder    = (LC3PLUS_Dec *)AUD_MALLOC(decoder_size);
    status = lc3plus_dec_init(p_decoder, p_lc3_cfg->sample_rate, p_lc3_cfg->channels, (LC3PLUS_PlcMode)p_lc3_cfg->plc_meth
#ifdef ENABLE_HR_MODE
                     , p_lc3_cfg->hrmode
#endif
        );
    status = lc3plus_dec_set_frame_dms(p_decoder, (int) (p_lc3_cfg->frame_ms * 10));
    status = lc3plus_dec_set_ep_enabled(p_decoder, p_lc3_cfg->epmode != 0);
    delay    = p_lc3_cfg->dc ? lc3plus_dec_get_delay(p_decoder) / p_lc3_cfg->dc : 0;
    p_lc3_cfg->out_samples = lc3plus_dec_get_output_samples(p_decoder);
    scratch_size = MAX(lc3plus_dec_get_scratch_size(p_decoder), lc3plus_enc_get_scratch_size(p_encoder));
    p_scratch      = (uint8_t *)AUD_MALLOC(scratch_size);

    CLOGD("status:%d, cfg:0x%x, p_decr:0x%x, p_scra:0x%x, size:%d",status, p_lc3_cfg, p_decoder, p_scratch, scratch_size);
    return status;
}

uint8_t app_lc3_dec(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, uint8_t bfi)
{
    LC3PLUS_Error status = LC3PLUS_OK;

    if(p_lc3_cfg->bips_out == 16)
    {
        status = lc3plus_dec16(p_decoder, in_data, in_len, (int16_t **)&out_data, p_scratch, bfi);
    }
    else if(p_lc3_cfg->bips_out == 24)
    {
        status = lc3plus_dec24(p_decoder, in_data, in_len, (int32_t **)&out_data, p_scratch, bfi);
    }
    *out_len = p_lc3_cfg->out_samples;
    return status;
}

uint8_t app_lc3_stop(void)
{
    if(p_lc3_cfg)
    {
        AUD_FREE(p_lc3_cfg);
        p_lc3_cfg = NULL;
    }
    if(p_scratch)
    {
        AUD_FREE(p_scratch);
        p_scratch = NULL;
    }
    if(p_decoder)
    {
        AUD_FREE(p_decoder);
        p_decoder = NULL;
    }
    return 0;
}
