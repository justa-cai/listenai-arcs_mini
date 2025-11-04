/**
 ****************************************************************************************
 *
 * @file msbc.c
 *
 * @brief msbc dec&enc
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */



////////////////////////////////////////////////////////////////////////////////
//                                                                            //
/// @file msbc.c                                                               //
/// That file implementes the SBC service.                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
//#include <errno.h>
//#include <string.h>
//#include <stdlib.h>
//#include <stdbool.h>
//#include <sys/types.h>
//#include <limits.h>

#include "msbc_math.h"
#include "msbc_tables.h"

#include "msbc.h"
#include "msbc_private.h"
#include "msbc_primitives.h"

#include "aud_common.h"

#define USE_NEW_PROCESS

//JUST FOR DEBUG ,ADD BY QUANWZ
//#define  CALCULATE_STEP_DEBUG

#define SBC_SYNCWORD    0x9C

#define MSBC_SYNCWORD    0xAD
#define MSBC_BLOCKS    15

#define A2DP_SAMPLING_FREQ_16000        (1 << 3)
#define A2DP_SAMPLING_FREQ_32000        (1 << 2)
#define A2DP_SAMPLING_FREQ_44100        (1 << 1)
#define A2DP_SAMPLING_FREQ_48000        (1 << 0)

#define A2DP_CHANNEL_MODE_MONO            (1 << 3)
#define A2DP_CHANNEL_MODE_DUAL_CHANNEL        (1 << 2)
#define A2DP_CHANNEL_MODE_STEREO        (1 << 1)
#define A2DP_CHANNEL_MODE_JOINT_STEREO        (1 << 0)

#define A2DP_BLOCK_LENGTH_4            (1 << 3)
#define A2DP_BLOCK_LENGTH_8            (1 << 2)
#define A2DP_BLOCK_LENGTH_12            (1 << 1)
#define A2DP_BLOCK_LENGTH_16            (1 << 0)

#define A2DP_SUBBANDS_4                (1 << 1)
#define A2DP_SUBBANDS_8                (1 << 0)

#define A2DP_ALLOCATION_SNR            (1 << 1)
#define A2DP_ALLOCATION_LOUDNESS        (1 << 0)

#if __BYTE_ORDER == __LITTLE_ENDIAN

struct a2dp_msbc {
    uint8_t channel_mode:4;
    uint8_t frequency:4;
    uint8_t allocation_method:2;
    uint8_t subbands:2;
    uint8_t block_length:4;
    uint8_t min_bitpool;
    uint8_t max_bitpool;
};

#elif __BYTE_ORDER == __BIG_ENDIAN

struct a2dp_msbc {
    uint8_t frequency:4;
    uint8_t channel_mode:4;
    uint8_t block_length:4;
    uint8_t subbands:2;
    uint8_t allocation_method:2;
    uint8_t min_bitpool;
    uint8_t max_bitpool;
};

#else
#error "Unknown byte order"
#endif

/* This structure contains an unpacked SBC frame.
   Yes, there is probably quite some unused space herein */
struct msbc_frame {
//    uint8_t frequency;
//    uint8_t block_mode;
//    uint8_t blocks;
//    enum {
//        MONO        = SBC_MODE_MONO,
//        DUAL_CHANNEL    = SBC_MODE_DUAL_CHANNEL,
//        STEREO        = SBC_MODE_STEREO,
//        JOINT_STEREO    = SBC_MODE_JOINT_STEREO
//    } mode;
//    uint8_t channels;
//    enum {
//        LOUDNESS    = SBC_AM_LOUDNESS,
//        SNR        = SBC_AM_SNR
//    } allocation;
//    uint8_t subband_mode;
//    uint8_t subbands;
//    uint8_t bitpool;
    uint16_t codesize;
    uint8_t length;
//
//    /* bit number x set means joint stereo has been used in subband x */
//    uint8_t joint;

    /* only the lower 4 bits of every element are to be used */
    uint32_t SBC_ALIGNED scale_factor[8]; //[2][8];

#ifdef MSBC_ENCODE
    /* only the lower 4 bits of every element are to be used */
    uint32_t SBC_ALIGNED scale_factor_f[8]; //[2][8];
#endif //MSBC_ENCODE

#ifdef MSBC_ENCODE
    /* raw integer subband samples in the frame */
    int32_t SBC_ALIGNED sb_sample_f[16][8]; //[2][16][8];
#endif //MSBC_ENCODE

    /* modified subband samples */
    int32_t SBC_ALIGNED sb_sample[16][8]; //[2][16][8];

#ifdef MSBC_DECODE
    /* original pcm audio samples */
    int16_t SBC_ALIGNED pcm_sample/*[2]*/[16*8];
#endif //MSBC_DECODE
};


#ifdef USE_NEW_PROCESS
#define SBC_V_BUFFER_SIZE  128
#endif

struct msbc_decoder_state {
    int subbands;
#ifndef USE_NEW_PROCESS
    int V/*[2]*/[170];
    int offset/*[2]*/[16];
#else
    int16_t V/*[2]*/[SBC_V_BUFFER_SIZE];
    int offset/*[2]*/;
#endif
};


#ifdef USE_NEW_PROCESS

#define AAN_C4_FIX (759250125)   /* 0.707107  S1.30 */
#define AAN_C6_FIX (410903207)   /* 0.382683  S1.30 */
#define AAN_Q0_FIX (581104888)   /* 0.541196  S1.30 */
#define AAN_Q1_FIX (1402911301)  /* 1.306563  S1.30 */

#define DCTII_4_K06_FIX ( 11585) /* 0.707107  S1.14 */
#define DCTII_4_K08_FIX ( 21407) /* 1.306563  S1.14 */
#define DCTII_4_K09_FIX (-15137) /* -0.923880 S1.14 */
#define DCTII_4_K10_FIX ( -8867) /* -0.541196 S1.14 */

#define SCALE(x, y) (((x) + (1 <<((y)-1))) >> (y))
#define MUL_32S_32S_HI(_x, _y) msbc_default_mul_32s_32s_hi(_x, _y)
#define BUTTERFLY(x,y) x += y; y = x - (y<<1);
#define FIX_MULT_DCT(K, x) (MUL_32S_32S_HI(K,x)<<2)

#define DCTII_8_SHIFT_IN 0
#define DCTII_8_SHIFT_OUT 16-DCTII_8_SHIFT_IN

#define DCTII_8_SHIFT_0 (DCTII_8_SHIFT_OUT)
#define DCTII_8_SHIFT_1 (DCTII_8_SHIFT_OUT)
#define DCTII_8_SHIFT_2 (DCTII_8_SHIFT_OUT)
#define DCTII_8_SHIFT_3 (DCTII_8_SHIFT_OUT)
#define DCTII_8_SHIFT_4 (DCTII_8_SHIFT_OUT)
#define DCTII_8_SHIFT_5 (DCTII_8_SHIFT_OUT)
#define DCTII_8_SHIFT_6 (DCTII_8_SHIFT_OUT-1)
#define DCTII_8_SHIFT_7 (DCTII_8_SHIFT_OUT-2)

#define DCT_SHIFT 15

#define int16_t_MIN  ((int16_t)0x8000)       /**< decimal value: -32768 */
#define int16_t_MAX  ((int16_t)0x7FFF)       /**< decimal value: 32767 */

#define MUL_16S_16S(_x, _y) ((_x) * (_y))

int32_t msbc_default_mul_16s_32s_hi(int16_t u, int32_t v);
int32_t msbc_default_mul_32s_32s_hi(int32_t u, int32_t v);

void msbc_dct2_8(short *out, int32_t const *in);
void msbc_synth_window80_generated(int16_t *pcm, int16_t const *buffer);
void cosineModulateSynth4(int16_t *out, const int32_t *in);
void SynthWindow40_int32_int32_symmetry_with_sum(int16_t *pcm, int16_t* buffer);

#define MUL_16S_32S_HI(_x, _y) msbc_default_mul_16s_32s_hi(_x, _y)
#define LONG_MULT_DCT(K, sample) (MUL_16S_32S_HI(K, sample)<<2)

#define CLIP_INT16(x) do { if (x > int16_t_MAX) { x = int16_t_MAX; } else if (x < int16_t_MIN) { x = int16_t_MIN; } } while (0)

#define DIV_DATA 32768
#define SHFIT_DATA 15

const int32_t msbc_dec_window_4[21] = {
    0,        /* +0.00000000E+00 */
    97,        /* +5.36548976E-04 */
    270,        /* +1.49188357E-03 */
    495,        /* +2.73370904E-03 */
    694,        /* +3.83720193E-03 */
    704,        /* +3.89205149E-03 */
    338,        /* +1.86581691E-03 */
    -554,        /* -3.06012286E-03 */
    1974,        /* +1.09137620E-02 */
    3697,        /* +2.04385087E-02 */
    5224,        /* +2.88757392E-02 */
    5824,        /* +3.21939290E-02 */
    4681,        /* +2.58767811E-02 */
    1109,        /* +6.13245186E-03 */
    -5214,        /* -2.88217274E-02 */
    -14047,        /* -7.76463494E-02 */
    24529,        /* +1.35593274E-01 */
    35274,        /* +1.94987841E-01 */
    44618,        /* +2.46636662E-01 */
    50984,        /* +2.81828203E-01 */
    53243,        /* +2.94315332E-01 */
};

#define SBC_DEQUANT_LONG_SCALED_OFFSET 1555931970

const uint32_t msbc_dequant_long_scaled[17] = {
    0x00000000,
    0x00000000,
    0x1ee9e116,  /* bits=2  0.24151243  1/3 * (1/1.38019122262781) */
    0x0d3fa99c,  /* bits=3  0.10350533  1/7 * (1/1.38019122262781) */
    0x062ec69e,  /* bits=4  0.04830249  1/15 * (1/1.38019122262781) */
    0x02fddbfa,  /* bits=5  0.02337217  1/31 * (1/1.38019122262781) */
    0x0178d9f5,  /* bits=6  0.01150059  1/63 * (1/1.38019122262781) */
    0x00baf129,  /* bits=7  0.00570502  1/127 * (1/1.38019122262781) */
    0x005d1abe,  /* bits=8  0.00284132  1/255 * (1/1.38019122262781) */
    0x002e760d,  /* bits=9  0.00141788  1/511 * (1/1.38019122262781) */
    0x00173536,  /* bits=10 0.00070825  1/1023 * (1/1.38019122262781) */
    0x000b9928,  /* bits=11 0.00035395  1/2047 * (1/1.38019122262781) */
    0x0005cc37,  /* bits=12 0.00017693  1/4095 * (1/1.38019122262781) */
    0x0002e604,  /* bits=13 0.00008846  1/8191 * (1/1.38019122262781) */
    0x000172fc,  /* bits=14 0.00004422  1/16383 * (1/1.38019122262781) */
    0x0000b97d,  /* bits=15 0.00002211  1/32767 * (1/1.38019122262781) */
    0x00005cbe,  /* bits=16 0.00001106  1/65535 * (1/1.38019122262781) */
};

#endif

/*
 * Calculates the CRC-8 of the first len bits in data
 */
static const uint8_t crc_table[256] = {
    0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53,
    0xE8, 0xF5, 0xD2, 0xCF, 0x9C, 0x81, 0xA6, 0xBB,
    0xCD, 0xD0, 0xF7, 0xEA, 0xB9, 0xA4, 0x83, 0x9E,
    0x25, 0x38, 0x1F, 0x02, 0x51, 0x4C, 0x6B, 0x76,
    0x87, 0x9A, 0xBD, 0xA0, 0xF3, 0xEE, 0xC9, 0xD4,
    0x6F, 0x72, 0x55, 0x48, 0x1B, 0x06, 0x21, 0x3C,
    0x4A, 0x57, 0x70, 0x6D, 0x3E, 0x23, 0x04, 0x19,
    0xA2, 0xBF, 0x98, 0x85, 0xD6, 0xCB, 0xEC, 0xF1,
    0x13, 0x0E, 0x29, 0x34, 0x67, 0x7A, 0x5D, 0x40,
    0xFB, 0xE6, 0xC1, 0xDC, 0x8F, 0x92, 0xB5, 0xA8,
    0xDE, 0xC3, 0xE4, 0xF9, 0xAA, 0xB7, 0x90, 0x8D,
    0x36, 0x2B, 0x0C, 0x11, 0x42, 0x5F, 0x78, 0x65,
    0x94, 0x89, 0xAE, 0xB3, 0xE0, 0xFD, 0xDA, 0xC7,
    0x7C, 0x61, 0x46, 0x5B, 0x08, 0x15, 0x32, 0x2F,
    0x59, 0x44, 0x63, 0x7E, 0x2D, 0x30, 0x17, 0x0A,
    0xB1, 0xAC, 0x8B, 0x96, 0xC5, 0xD8, 0xFF, 0xE2,
    0x26, 0x3B, 0x1C, 0x01, 0x52, 0x4F, 0x68, 0x75,
    0xCE, 0xD3, 0xF4, 0xE9, 0xBA, 0xA7, 0x80, 0x9D,
    0xEB, 0xF6, 0xD1, 0xCC, 0x9F, 0x82, 0xA5, 0xB8,
    0x03, 0x1E, 0x39, 0x24, 0x77, 0x6A, 0x4D, 0x50,
    0xA1, 0xBC, 0x9B, 0x86, 0xD5, 0xC8, 0xEF, 0xF2,
    0x49, 0x54, 0x73, 0x6E, 0x3D, 0x20, 0x07, 0x1A,
    0x6C, 0x71, 0x56, 0x4B, 0x18, 0x05, 0x22, 0x3F,
    0x84, 0x99, 0xBE, 0xA3, 0xF0, 0xED, 0xCA, 0xD7,
    0x35, 0x28, 0x0F, 0x12, 0x41, 0x5C, 0x7B, 0x66,
    0xDD, 0xC0, 0xE7, 0xFA, 0xA9, 0xB4, 0x93, 0x8E,
    0xF8, 0xE5, 0xC2, 0xDF, 0x8C, 0x91, 0xB6, 0xAB,
    0x10, 0x0D, 0x2A, 0x37, 0x64, 0x79, 0x5E, 0x43,
    0xB2, 0xAF, 0x88, 0x95, 0xC6, 0xDB, 0xFC, 0xE1,
    0x5A, 0x47, 0x60, 0x7D, 0x2E, 0x33, 0x14, 0x09,
    0x7F, 0x62, 0x45, 0x58, 0x0B, 0x16, 0x31, 0x2C,
    0x97, 0x8A, 0xAD, 0xB0, 0xE3, 0xFE, 0xD9, 0xC4
};

static uint8_t msbc_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x0f;
    size_t i;
    uint8_t octet;

    for (i = 0; i < len / 8; i++)
        crc = crc_table[crc ^ data[i]];

    octet = data[i];
    for (i = 0; i < len % 8; i++) {
        char bit = ((octet ^ crc) & 0x80) >> 7;

        crc = ((crc & 0x7f) << 1) ^ (bit ? 0x1d : 0);

        octet = octet << 1;
    }

    return crc;
}


/*
 * Code straight from the spec to calculate the bits array
 * Takes a pointer to the frame in question, a pointer to the bits array and
 * the sampling frequency (as 2 bit integer)
 */
static SBC_ALWAYS_INLINE void msbc_calculate_bits_internal(
        const struct msbc_frame *frame, int (*bits)/*[8]*/, int subbands, UINT8 dec_enc_sel)
{
    uint8_t sf = SBC_FREQ_16000; //frame->frequency;
       uint32_t *p_sacle_factor = NULL;

      if(dec_enc_sel)
      {
        p_sacle_factor = (uint32_t *)frame->scale_factor;
      }
#ifdef MSBC_ENCODE
      else
      {
        p_sacle_factor = (uint32_t *)frame->scale_factor_f;
      }
#endif
        int bitneed/*[2]*/[8], loudness, max_bitneed, bitcount, slicecount, bitslice;
        int /*ch,*/ sb;
        


              
//        for (ch = 0; ch < frame->channels; ch++) {
            max_bitneed = 0;
            {
                for (sb = 0; sb < subbands; sb++) {
                    if (p_sacle_factor/*[ch]*/[sb] == 0)
                        bitneed/*[ch]*/[sb] = -5;
                    else {
                        if (subbands == 4)
                            loudness = p_sacle_factor/*[ch]*/[sb] - msbc_offset4[sf][sb];
                        else
                            loudness = p_sacle_factor/*[ch]*/[sb] - msbc_offset8[sf][sb];
                        if (loudness > 0)
                            bitneed/*[ch]*/[sb] = loudness / 2;
                        else
                            bitneed/*[ch]*/[sb] = loudness;
                    }
                    if (bitneed/*[ch]*/[sb] > max_bitneed)
                        max_bitneed = bitneed/*[ch]*/[sb];
                }
            }

            bitcount = 0;
            slicecount = 0;
            bitslice = max_bitneed + 1;
            do {
                bitslice--;
                bitcount += slicecount;
                slicecount = 0;
                for (sb = 0; sb < subbands; sb++) {
                    if ((bitneed/*[ch]*/[sb] > bitslice + 1) && (bitneed/*[ch]*/[sb] < bitslice + 16))
                        slicecount++;
                    else if (bitneed/*[ch]*/[sb] == bitslice + 1)
                        slicecount += 2;
                }
            } while (bitcount + slicecount < 26/*frame->bitpool*/);

            if (bitcount + slicecount == 26/*frame->bitpool*/) {
                bitcount += slicecount;
                bitslice--;
            }

            for (sb = 0; sb < subbands; sb++) {
                if (bitneed/*[ch]*/[sb] < bitslice + 2)
                    bits/*[ch]*/[sb] = 0;
                else {
                    bits/*[ch]*/[sb] = bitneed/*[ch]*/[sb] - bitslice;
                    if (bits/*[ch]*/[sb] > 16)
                        bits/*[ch]*/[sb] = 16;
                }
            }

            for (sb = 0; bitcount < 26/*frame->bitpool*/ &&
                            sb < subbands; sb++) {
                if ((bits/*[ch]*/[sb] >= 2) && (bits/*[ch]*/[sb] < 16)) {
                    bits/*[ch]*/[sb]++;
                    bitcount++;
                } else if ((bitneed/*[ch]*/[sb] == bitslice + 1) && (26/*frame->bitpool*/ > bitcount + 1)) {
                    bits/*[ch]*/[sb] = 2;
                    bitcount += 2;
                }
            }

            for (sb = 0; bitcount < 26/*frame->bitpool*/ &&
                            sb < subbands; sb++) {
                if (bits/*[ch]*/[sb] < 16) {
                    bits/*[ch]*/[sb]++;
                    bitcount++;
                }
            }

//        }
}

static void msbc_calculate_bits(const struct msbc_frame *frame, int (*bits)/*[8]*/, UINT8 dec_enc_sel)
{
        msbc_calculate_bits_internal(frame, bits, 8, dec_enc_sel);
}

#ifdef MSBC_DECODE
/*
 * Unpacks a SBC frame at the beginning of the stream in data,
 * which has at most len bytes into frame.
 * Returns the length in bytes of the packed frame, or a negative
 * value on error. The error codes are:
 *
 *  -1   Data stream too short
 *  -2   Sync byte incorrect
 *  -3   CRC8 incorrect
 *  -4   Bitpool value out of bounds
 */
static int msbc_unpack_frame_internal(const uint8_t *data,
        struct msbc_frame *frame, size_t len)
{
    unsigned int consumed;
//    /* Will copy the parts of the header that are relevant to crc
//     * calculation here */
    uint8_t crc_header[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int crc_pos = 0;
//    int32_t temp;

    uint32_t audio_sample;
    int /*ch,*/ sb, blk, bit;    /* channel, subband, block and bit standard
                   counters */
    int bits/*[2]*/[8];        /* bits distribution */
    uint32_t levels/*[2]*/[8];    /* levels derived from that */

    uint32_t d;
    int32_t result;

    consumed = 32;

    crc_header[0] = data[1];
    crc_header[1] = data[2];
    crc_pos = 16;

    if (len * 8 < consumed + (4 * 8/*frame->subbands*/ * 1/*frame->channels*/))
        return -1;

//    for (ch = 0; ch < frame->channels; ch++) {
        for (sb = 0; sb < 8/*frame->subbands*/; sb++) {
            /* FIXME assert(consumed % 4 == 0); */
            frame->scale_factor/*[ch]*/[sb] =
                (data[consumed >> 3] >> (4 - (consumed & 0x7))) & 0x0F;
            crc_header[crc_pos >> 3] |=
                frame->scale_factor/*[ch]*/[sb] << (4 - (crc_pos & 0x7));

            consumed += 4;
            crc_pos += 4;
        }
//    }

    if (data[3] != msbc_crc8(crc_header, crc_pos))
        return -3;

    msbc_calculate_bits(frame, bits, 1);

//    for (ch = 0; ch < frame->channels; ch++) {
        for (sb = 0; sb < 8/*frame->subbands*/; sb++)
            levels/*[ch]*/[sb] = (1 << bits/*[ch]*/[sb]) - 1;
//    }

#ifndef USE_NEW_PROCESS
    for (blk = 0; blk < MSBC_BLOCKS/*frame->blocks*/; blk++) {
//        for (ch = 0; ch < frame->channels; ch++) {
            for (sb = 0; sb < 8/*frame->subbands*/; sb++) {
                uint32_t shift;

                if (levels/*[ch]*/[sb] == 0) {
                    frame->sb_sample[blk]/*[ch]*/[sb] = 0;
                    continue;
                }

                shift = frame->scale_factor/*[ch]*/[sb] +
                        1 + SBCDEC_FIXED_EXTRA_BITS;

                audio_sample = 0;
                for (bit = 0; bit < bits/*[ch]*/[sb]; bit++) {
                    if (consumed > len * 8)
                        return -1;

                    if ((data[consumed >> 3] >> (7 - (consumed & 0x7))) & 0x01)
                        audio_sample |= 1 << (bits/*[ch]*/[sb] - bit - 1);

                    consumed++;
                }

                frame->sb_sample[blk]/*[ch]*/[sb] = (int32_t)
                    (((((uint64_t) audio_sample << 1) | 1) << shift) /
                    levels/*[ch]*/[sb]) - (1 << shift);
            }
//        }
    }
#else
        for (blk = 0; blk < MSBC_BLOCKS/*frame->blocks*/; blk++) 
        {
            //for (ch = 0; ch < frame->channels; ch++)
            //{
                for (sb = 0; sb < 8/*frame->subbands*/; sb++)
                {
                    if (bits/*[ch]*/[sb] > 0) 
                    {
                        audio_sample = 0;
                        for (bit = 0; bit < bits/*[ch]*/[sb]; bit++) 
                        {
                            if (consumed > len * 8)
                                return -1; 

                            if ((data[consumed >> 3] >> (7 - (consumed & 0x7))) & 0x01)
                                audio_sample |= 1 << (bits/*[ch]*/[sb] - bit - 1);

                            consumed++;
                        }

                        if (bits/*[ch]*/[sb] <= 1)
                        {
                            frame->sb_sample[blk]/*[ch]*/[sb] = 0;
                        }
                        else
                        {
                            d = (audio_sample * 2) + 1;
                            d *= msbc_dequant_long_scaled[bits/*[ch]*/[sb]];
                            result = d - SBC_DEQUANT_LONG_SCALED_OFFSET;
                            frame->sb_sample[blk]/*[ch]*/[sb] = result >> (15 - frame->scale_factor/*[ch]*/[sb]);
                        }
                    }
                    else
                        frame->sb_sample[blk]/*[ch]*/[sb] = 0;
                }
            //}
        }

#endif

    if ((consumed & 0x7) != 0)
        consumed += 8 - (consumed & 0x7);

    return consumed >> 3;
}

static int msbc_unpack_frame(const uint8_t *data,
        struct msbc_frame *frame, size_t len)
{
    if (len < 4)
        return -1;

    if (data[0] != MSBC_SYNCWORD)
        return -2;
    if (data[1] != 0)
        return -2;
    if (data[2] != 0)
        return -2;

    return msbc_unpack_frame_internal(data, frame, len);
}

static void msbc_decoder_init(struct msbc_decoder_state *state,
                    const struct msbc_frame *frame)
{
    int i/*, ch*/;

    memset(state->V, 0, sizeof(state->V));
    state->subbands = 8/*frame->subbands*/;

#ifndef USE_NEW_PROCESS
//    for (ch = 0; ch < 2; ch++)
        for (i = 0; i < 8/*priv->frame.subbands*/ * 2; i++)
            state->offset/*[ch]*/[i] = (10 * i + 10);
#else
    //for (ch = 0; ch < 2; ch++)
        state->offset/*[ch]*/ = 0;
#endif
}


#ifndef USE_NEW_PROCESS
static SBC_ALWAYS_INLINE int16_t msbc_clip16(int32_t s)
{
    if (s > 0x7FFF)
        return 0x7FFF;
    else if (s < -0x8000)
        return -0x8000;
    else
        return s;
}

static SBC_ALWAYS_INLINE void msbc_synthesize_eight(struct msbc_decoder_state *state,
                struct msbc_frame *frame,/* int ch,*/ int blk)
{
    int i, j, k, idx;
    int *offset = state->offset/*[ch]*/;
#ifdef CALCULATE_STEP_DEBUG
    int result;
#endif

    for (i = 0; i < 16; i++) {
        /* Shifting */
        offset[i]--;
        if (offset[i] < 0) {
            offset[i] = 159;
            for (j = 0; j < 9; j++)
                state->V/*[ch]*/[j + 160] = state->V/*[ch]*/[j];
        }

        /* Distribute the new matrix value to the shifted position */
#ifndef CALCULATE_STEP_DEBUG
        state->V/*[ch]*/[offset[i]] = SCALE8_STAGED1(
            MULA(synmatrix8[i][0], frame->sb_sample[blk]/*[ch]*/[0],
            MULA(synmatrix8[i][1], frame->sb_sample[blk]/*[ch]*/[1],
            MULA(synmatrix8[i][2], frame->sb_sample[blk]/*[ch]*/[2],
            MULA(synmatrix8[i][3], frame->sb_sample[blk]/*[ch]*/[3],
            MULA(synmatrix8[i][4], frame->sb_sample[blk]/*[ch]*/[4],
            MULA(synmatrix8[i][5], frame->sb_sample[blk]/*[ch]*/[5],
            MULA(synmatrix8[i][6], frame->sb_sample[blk]/*[ch]*/[6],
            MUL( synmatrix8[i][7], frame->sb_sample[blk]/*[ch]*/[7])))))))));
#else
        result = MUL(synmatrix8[i][7], frame->sb_sample[blk]/*[ch]*/[7]);
        result = MULA(synmatrix8[i][6], frame->sb_sample[blk]/*[ch]*/[6], result);
        result = MULA(synmatrix8[i][5], frame->sb_sample[blk]/*[ch]*/[5], result);
        result = MULA(synmatrix8[i][4], frame->sb_sample[blk]/*[ch]*/[4], result);
        result = MULA(synmatrix8[i][3], frame->sb_sample[blk]/*[ch]*/[3], result);
        result = MULA(synmatrix8[i][2], frame->sb_sample[blk]/*[ch]*/[2], result);
        result = MULA(synmatrix8[i][1], frame->sb_sample[blk]/*[ch]*/[1], result);
        result = MULA(synmatrix8[i][0], frame->sb_sample[blk]/*[ch]*/[0], result);    
        state->V/*[ch]*/[offset[i]] = SCALE8_STAGED1(result);
        
#endif
    }

    /* Compute the samples */
    for (idx = 0, i = 0; i < 8; i++, idx += 5) {
        k = (i + 8) & 0xf;

        /* Store in output, Q0 */
#ifndef CALCULATE_STEP_DEBUG
        frame->pcm_sample/*[ch]*/[blk * 8 + i] = msbc_clip16(SCALE8_STAGED1(
            MULA(state->V/*[ch]*/[offset[i] + 0], msbc_proto_8_80m0[idx + 0],
            MULA(state->V/*[ch]*/[offset[k] + 1], msbc_proto_8_80m1[idx + 0],
            MULA(state->V/*[ch]*/[offset[i] + 2], msbc_proto_8_80m0[idx + 1],
            MULA(state->V/*[ch]*/[offset[k] + 3], msbc_proto_8_80m1[idx + 1],
            MULA(state->V/*[ch]*/[offset[i] + 4], msbc_proto_8_80m0[idx + 2],
            MULA(state->V/*[ch]*/[offset[k] + 5], msbc_proto_8_80m1[idx + 2],
            MULA(state->V/*[ch]*/[offset[i] + 6], msbc_proto_8_80m0[idx + 3],
            MULA(state->V/*[ch]*/[offset[k] + 7], msbc_proto_8_80m1[idx + 3],
            MULA(state->V/*[ch]*/[offset[i] + 8], msbc_proto_8_80m0[idx + 4],
            MUL( state->V/*[ch]*/[offset[k] + 9], msbc_proto_8_80m1[idx + 4]))))))))))));
#else
        result = MUL(state->V/*[ch]*/[offset[k] + 9], msbc_proto_8_80m1[idx + 4]);
        result = MULA(state->V/*[ch]*/[offset[i] + 8], msbc_proto_8_80m0[idx + 4], result);
        result = MULA(state->V/*[ch]*/[offset[k] + 7], msbc_proto_8_80m1[idx + 3], result);
        result = MULA(state->V/*[ch]*/[offset[i] + 6], msbc_proto_8_80m0[idx + 3], result);
        result = MULA(state->V/*[ch]*/[offset[k] + 5], msbc_proto_8_80m1[idx + 2], result);
        result = MULA(state->V/*[ch]*/[offset[i] + 4], msbc_proto_8_80m0[idx + 2], result);
        result = MULA(state->V/*[ch]*/[offset[k] + 3], msbc_proto_8_80m1[idx + 1], result);
        result = MULA(state->V/*[ch]*/[offset[i] + 2], msbc_proto_8_80m0[idx + 1], result);
        result = MULA(state->V/*[ch]*/[offset[k] + 1], msbc_proto_8_80m1[idx + 0], result);
        result = MULA(state->V/*[ch]*/[offset[i] + 0], msbc_proto_8_80m0[idx + 0], result);
        frame->pcm_sample/*[ch]*/[blk * 8 + i] = msbc_clip16(SCALE8_STAGED1(result));
#endif
    }
}

#else

int32_t msbc_default_mul_16s_32s_hi(int16_t u, int32_t v)
{
    uint16_t v0;
    int16_t v1;

    int32_t w, x;

    v0 = (uint16_t)(v & 0xffff);
    v1 = (int16_t)(v >> 16);

    w = v1 * u;
    x = u * v0;

    w = w + (x >> 16);

    return w;
}

int32_t msbc_default_mul_32s_32s_hi(int32_t u, int32_t v)
{
    uint32_t u0, v0;
    int32_t u1, v1, w1, w2, t;

    u0 = u & 0xFFFF; u1 = u >> 16;
    v0 = v & 0xFFFF; v1 = v >> 16;
    t = u0*v0;
    t = u1*v0 + ((uint32_t)t >> 16);
    w1 = t & 0xFFFF;
    w2 = t >> 16;
    w1 = u0*v1 + w1;
    w2 = u1*v1 + w2 + (w1 >> 16);

    return w2;
}

void msbc_dct2_8(int16_t *out, const int32_t *in)
{
    int32_t L00, L01, L02, L03, L04, L05, L06, L07;
    int32_t L25;

    int32_t in0, in1, in2, in3;
    int32_t in4, in5, in6, in7;


    in0 = in[0];
    in1 = in[1];
    in2 = in[2];
    in3 = in[3];
    in4 = in[4];
    in5 = in[5];
    in6 = in[6];
    in7 = in[7];

    L00 = in0 + in7;
    L01 = in1 + in6;
    L02 = in2 + in5;
    L03 = in3 + in4;

    L04 = in3 - in4;
    L05 = in2 - in5;
    L06 = in1 - in6;
    L07 = in0 - in7;

    BUTTERFLY(L00, L03);
    BUTTERFLY(L01, L02);

    L02 += L03;

    L02 = FIX_MULT_DCT(AAN_C4_FIX, L02);

    BUTTERFLY(L00, L01);
    out[0] = (int32_t)SCALE(L00, DCTII_8_SHIFT_0);
    out[4] = (int32_t)SCALE(L01, DCTII_8_SHIFT_4);

    BUTTERFLY(L03, L02);
    out[6] = (int32_t)SCALE(L02, DCTII_8_SHIFT_6);
    out[2] = (int32_t)SCALE(L03, DCTII_8_SHIFT_2);

    L04 += L05;
    L05 += L06;
    L06 += L07;

    L04 /= 2;
    L05 /= 2;
    L06 /= 2;
    L07 /= 2;

    L05 = FIX_MULT_DCT(AAN_C4_FIX, L05);

    L25 = L06 - L04;
    L25 = FIX_MULT_DCT(AAN_C6_FIX, L25);

    L04 = FIX_MULT_DCT(AAN_Q0_FIX, L04);
    L04 -= L25;

    L06 = FIX_MULT_DCT(AAN_Q1_FIX, L06);
    L06 -= L25;

    BUTTERFLY(L07, L05);
    BUTTERFLY(L05, L04);

    out[3] = (int32_t)SCALE(L04, DCTII_8_SHIFT_3 - 1);
    out[5] = (int32_t)SCALE(L05, DCTII_8_SHIFT_5 - 1);

    BUTTERFLY(L07, L06);
    out[7] = (int32_t)SCALE(L06, DCTII_8_SHIFT_7 - 1);
    out[1] = (int32_t)SCALE(L07, DCTII_8_SHIFT_1 - 1);

#undef BUTTERFLY
}

void msbc_synth_window80_generated(int16_t *pcm, int16_t const *buffer)
{
    int32_t pcm_a, pcm_b;

    /* 1 - stage 0 */ pcm_b = 0;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(8235, buffer[12])) >> 3;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(-23167, buffer[20])) >> 3;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(26479, buffer[28])) >> 2;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(-17397, buffer[36])) << 1;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(9399, buffer[44])) << 3;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(17397, buffer[52])) << 1;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(26479, buffer[60])) >> 2;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(23167, buffer[68])) >> 3;
    /* 1 - stage 0 */ pcm_b += (MUL_16S_16S(8235, buffer[76])) >> 3;

    /* 1 - stage 0 */ pcm_b /= DIV_DATA; CLIP_INT16(pcm_b); pcm[0] = (int16_t)pcm_b;

    /* 1 - stage 1 */ pcm_a = 0;
    /* 1 - stage 1 */ pcm_b = 0;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(-3263, buffer[5])) >> 5;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(9293, buffer[5])) >> 3;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(29293, buffer[11])) >> 5;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(-6087, buffer[11])) >> 2;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(-5229, buffer[21]));
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(1247, buffer[21])) << 3;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(30835, buffer[27])) >> 3;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(-2893, buffer[27])) << 3;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(-27021, buffer[37])) << 1;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(23671, buffer[37])) << 2;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(31633, buffer[43])) << 1;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(18055, buffer[43])) << 1;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(17319, buffer[53])) << 1;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(11537, buffer[53])) >> 1;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(26663, buffer[59])) >> 2;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(1747, buffer[59])) << 1;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(4555, buffer[69])) >> 1;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(685, buffer[69])) << 1;
    /* 1 - stage 1 */ pcm_a += (MUL_16S_16S(12419, buffer[75])) >> 4;
    /* 1 - stage 1 */ pcm_b += (MUL_16S_16S(8721, buffer[75])) >> 7;

    /* 1 - stage 1 */ pcm_a /= DIV_DATA; CLIP_INT16(pcm_a); pcm[1] = (int16_t)pcm_a;
    /* 1 - stage 1 */ pcm_b /= DIV_DATA; CLIP_INT16(pcm_b); pcm[7] = (int16_t)pcm_b;

    /* 1 - stage 2 */ pcm_a = 0;
    /* 1 - stage 2 */ pcm_b = 0;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(-10385, buffer[6])) >> 6;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(11167, buffer[6])) >> 4;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(24995, buffer[10])) >> 5;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(-10337, buffer[10])) >> 4;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(-309, buffer[22])) << 4;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(1917, buffer[22])) << 2;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(9161, buffer[26])) >> 3;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(-30605, buffer[26])) >> 1;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(-23063, buffer[38])) << 1;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(8317, buffer[38])) << 3;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(27561, buffer[42])) << 1;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(9553, buffer[42])) << 2;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(2309, buffer[54])) << 3;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(22117, buffer[54])) >> 4;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(12705, buffer[58])) >> 1;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(16383, buffer[58])) >> 2;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(6239, buffer[70])) >> 3;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(7543, buffer[70])) >> 3;
    /* 1 - stage 2 */ pcm_a += (MUL_16S_16S(9251, buffer[74])) >> 4;
    /* 1 - stage 2 */ pcm_b += (MUL_16S_16S(8603, buffer[74])) >> 6;

    /* 1 - stage 2 */ pcm_a /= DIV_DATA; CLIP_INT16(pcm_a); pcm[2] = (int16_t)pcm_a;
    /* 1 - stage 2 */ pcm_b /= DIV_DATA; CLIP_INT16(pcm_b); pcm[6] = (int16_t)pcm_b;

    /* 1 - stage 3 */ pcm_a = 0;
    /* 1 - stage 3 */ pcm_b = 0;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(-16457, buffer[7])) >> 6;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(16913, buffer[7])) >> 5;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(19083, buffer[9])) >> 5;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(-8443, buffer[9])) >> 7;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(-23641, buffer[23])) >> 2;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(3687, buffer[23])) << 1;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(-29015, buffer[25])) >> 4;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(-301, buffer[25])) << 5;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(-12889, buffer[39])) << 2;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(15447, buffer[39])) << 2;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(6145, buffer[41])) << 3;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(10255, buffer[41])) << 2;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(24211, buffer[55])) >> 1;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(-18233, buffer[55])) >> 3;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(23469, buffer[57])) >> 2;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(9405, buffer[57])) >> 1;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(21223, buffer[71])) >> 8;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(1499, buffer[71])) >> 1;
    /* 1 - stage 3 */ pcm_a += (MUL_16S_16S(26913, buffer[73])) >> 6;
    /* 1 - stage 3 */ pcm_b += (MUL_16S_16S(26189, buffer[73])) >> 7;

    /* 1 - stage 3 */ pcm_a /= DIV_DATA; CLIP_INT16(pcm_a); pcm[3] = (int16_t)pcm_a;
    /* 1 - stage 3 */ pcm_b /= DIV_DATA; CLIP_INT16(pcm_b); pcm[5] = (int16_t)pcm_b;


    /* 1 - stage 4 */ pcm_a = 0;
    pcm_b = 0;
    /* 1 - stage 4 */ pcm_a += (MUL_16S_16S(10445, buffer[8])) >> 4;
    /* 1 - stage 4 */ pcm_b += (MUL_16S_16S(-5297, buffer[24])) << 1;
    /* 1 - stage 4 */ pcm_a += (MUL_16S_16S(22299, buffer[40])) << 2;
    /* 1 - stage 4 */ pcm_b += (MUL_16S_16S(10603, buffer[56]));
    /* 1 - stage 4 */ pcm_a += (MUL_16S_16S(9539, buffer[72])) >> 4;
    pcm_a += pcm_b;

    /* 1 - stage 4 */ pcm_a /= DIV_DATA; CLIP_INT16(pcm_a); pcm[4] = (int16_t)pcm_a;
}

static  void msbc_synthesize_eight(struct msbc_decoder_state *state,
struct msbc_frame *frame, /*int ch,*/ int blk)
{

    if (state->offset/*[ch]*/ == 0)
    {
        memmove(state->V/*[ch]*/ + SBC_V_BUFFER_SIZE - 72, state->V/*[ch]*/, 72 * sizeof(short));
        state->offset/*[ch]*/ = SBC_V_BUFFER_SIZE - 80;
    }
    else
    {
        state->offset/*[ch]*/ -= 1 * 8;
    }

    msbc_dct2_8(state->V/*[ch]*/ + state->offset/*[ch]*/, &frame->sb_sample[blk]/*[ch]*/[0]);
    msbc_synth_window80_generated(&frame->pcm_sample/*[ch]*/[blk * 8], state->V/*[ch]*/ + state->offset/*[ch]*/);
}

#endif


static int msbc_synthesize_audio(struct msbc_decoder_state *state,
                        struct msbc_frame *frame)
{
    int /*ch,*/ blk;

//        for (ch = 0; ch < frame->channels; ch++) {
            for (blk = 0; blk < MSBC_BLOCKS/*frame->blocks*/; blk++)
                msbc_synthesize_eight(state, frame,/* ch,*/ blk);
//        }
        return MSBC_BLOCKS/*frame->blocks*/ * 8;
}
#endif //MSBC_DECODE

#ifdef MSBC_ENCODE
static int msbc_analyze_audio(struct msbc_encoder_state *state,
                        struct msbc_frame *frame)
{
    int /*ch,*/ blk;
    int16_t *x;

//        for (ch = 0; ch < 1/*frame->channels*/; ch++) {
            x = &state->X/*[ch]*/[state->position - 8 *
                    state->increment + MSBC_BLOCKS/*frame->blocks*/ * 8];
            for (blk = 0; blk < MSBC_BLOCKS/*frame->blocks*/;
                        blk += state->increment) {
                state->msbc_analyze_8s(
                    state, x,
                    frame->sb_sample_f[blk]/*[ch]*/,
                    frame->sb_sample_f[blk + 1]/*[ch]*/ -
                    frame->sb_sample_f[blk]/*[ch]*/);
                x -= 8 * state->increment;
            }
//        }
        return MSBC_BLOCKS/*frame->blocks*/ * 8;

}
#endif //MSBC_ENCODE

/* Supplementary bitstream writing macros for 'msbc_pack_frame' */

#define PUT_BITS(data_ptr, bits_cache, bits_count, v, n)        \
    do {                                \
        bits_cache = (v) | (bits_cache << (n));            \
        bits_count += (n);                    \
        if (bits_count >= 16) {                    \
            bits_count -= 8;                \
            *data_ptr++ = (uint8_t)                \
                (bits_cache >> bits_count);        \
            bits_count -= 8;                \
            *data_ptr++ = (uint8_t)                \
                (bits_cache >> bits_count);        \
        }                            \
    } while (0)

#define FLUSH_BITS(data_ptr, bits_cache, bits_count)            \
    do {                                \
        while (bits_count >= 8) {                \
            bits_count -= 8;                \
            *data_ptr++ = (uint8_t)                \
                (bits_cache >> bits_count);        \
        }                            \
        if (bits_count > 0)                    \
            *data_ptr++ = (uint8_t)                \
                (bits_cache << (8 - bits_count));    \
    } while (0)

#ifdef MSBC_ENCODE
/*
 * Packs the SBC frame from frame into the memory at data. At most len
 * bytes will be used, should more memory be needed an appropriate
 * error code will be returned. Returns the length of the packed frame
 * on success or a negative value on error.
 *
 * The error codes are:
 * -1 Not enough memory reserved
 * -2 Unsupported sampling rate
 * -3 Unsupported number of blocks
 * -4 Unsupported number of subbands
 * -5 Bitpool value out of bounds
 * -99 not implemented
 */
static SBC_ALWAYS_INLINE ssize_t msbc_pack_frame_internal(uint8_t *data,
                    struct msbc_frame *frame, size_t len,
                    int frame_subbands, int frame_channels,
                    int joint)
{
    /* Bitstream writer starts from the fourth byte */
    uint8_t *data_ptr = data + 4;
    uint32_t bits_cache = 0;
    uint32_t bits_count = 0;

//    /* Will copy the header parts for CRC-8 calculation here */
    uint8_t crc_header[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int crc_pos = 0;

    uint32_t audio_sample;

    int /*ch,*/ sb, blk;    /* channel, subband, block and bit counters */
    int bits/*[2]*/[8];        /* bits distribution */
    uint32_t levels/*[2]*/[8];    /* levels are derived from that */
    uint32_t sb_sample_delta/*[2]*/[8];

    /* Can't fill in crc yet */

    crc_header[0] = data[1];
    crc_header[1] = data[2];
    crc_pos = 16;

//    for (ch = 0; ch < frame_channels; ch++) {
        for (sb = 0; sb < frame_subbands; sb++) {
            PUT_BITS(data_ptr, bits_cache, bits_count,
                frame->scale_factor_f/*[ch]*/[sb] & 0x0F, 4);
            crc_header[crc_pos >> 3] <<= 4;
            crc_header[crc_pos >> 3] |= frame->scale_factor_f/*[ch]*/[sb] & 0x0F;
            crc_pos += 4;
        }
//    }

//    /* align the last crc byte */
    if (crc_pos % 8)
        crc_header[crc_pos >> 3] <<= 8 - (crc_pos % 8);

    data[3] = msbc_crc8(crc_header, crc_pos);

    msbc_calculate_bits(frame, bits, 0);

//    for (ch = 0; ch < frame_channels; ch++) {
        for (sb = 0; sb < frame_subbands; sb++) {
            levels/*[ch]*/[sb] = ((1 << bits/*[ch]*/[sb]) - 1) <<
                (32 - (frame->scale_factor_f/*[ch]*/[sb] +
                    SCALE_OUT_BITS + 2));
            sb_sample_delta/*[ch]*/[sb] = (uint32_t) 1 <<
                (frame->scale_factor_f/*[ch]*/[sb] +
                    SCALE_OUT_BITS + 1);
        }
//    }

    for (blk = 0; blk < MSBC_BLOCKS/*frame->blocks*/; blk++) {
//        for (ch = 0; ch < frame_channels; ch++) {
            for (sb = 0; sb < frame_subbands; sb++) {

                if (bits/*[ch]*/[sb] == 0)
                    continue;

                audio_sample = ((uint64_t) levels/*[ch]*/[sb] *
                    (sb_sample_delta/*[ch]*/[sb] +
                    frame->sb_sample_f[blk]/*[ch]*/[sb])) >> 32;

                PUT_BITS(data_ptr, bits_cache, bits_count,
                    audio_sample, bits/*[ch]*/[sb]);
            }
//        }
    }

    FLUSH_BITS(data_ptr, bits_cache, bits_count);

    return data_ptr - data;
}
#endif //MSBC_ENCODE

#ifdef MSBC_ENCODE
static ssize_t msbc_pack_frame(uint8_t *data, struct msbc_frame *frame,
                        size_t len, int joint)
{
    data[0] = MSBC_SYNCWORD;
    data[1] = 0;
    data[2] = 0;

    return msbc_pack_frame_internal(data, frame, len, 8, 1, joint);
}

static void msbc_encoder_init(struct msbc_encoder_state *state,
                        const struct msbc_frame *frame)
{
    memset(&state->X, 0, sizeof(state->X));
    state->position = (SBC_X_BUFFER_SIZE - 8/*frame->subbands*/ * 9) & ~7;
    state->increment = 1;

    msbc_init_primitives(state);
}
#endif //MSBC_ENCODE

struct msbc_priv {
    BOOL init;
    BOOL init_dec;
    BOOL init_enc;
    struct SBC_ALIGNED msbc_frame frame;
#ifdef MSBC_DECODE
    struct SBC_ALIGNED msbc_decoder_state dec_state;
#endif //MSBC_DECODE
#ifdef MSBC_ENCODE
    struct SBC_ALIGNED msbc_encoder_state enc_state;
#endif //MSBC_ENCODE
#ifdef MSBC_DECODE
    int (*unpack_frame)(const uint8_t *data, struct msbc_frame *frame,
            size_t len);
#endif //MSBC_DECODE
#ifdef MSBC_ENCODE
    ssize_t (*pack_frame)(uint8_t *data, struct msbc_frame *frame,
            size_t len, int joint);
#endif //MSBC_ENCODE
};

//reduce mem  ,use receive ns bakup ram.
//uint8_t msbc_mem[sizeof(struct msbc_priv) + SBC_ALIGN_MASK];

static void msbc_set_defaults(msbc_t *msbc, unsigned long flags)
{
    struct msbc_priv *priv = msbc->priv;

#ifdef MSBC_ENCODE
        priv->pack_frame = msbc_pack_frame;
#endif //MSBC_ENCODE
#ifdef MSBC_DECODE
        priv->unpack_frame = msbc_unpack_frame;
#endif //MSBC_DECODE
}

SBC_EXPORT int msbc_init(msbc_t *msbc, unsigned long flags)
{
    if (!msbc)
        return -SBC_EIO;

    memset(msbc, 0, sizeof(msbc_t));

    msbc->priv_alloc_base = (void *)AUD_MALLOC(sizeof(struct msbc_priv) + SBC_ALIGN_MASK);
    if (!msbc->priv_alloc_base)
        return -SBC_ENOMEM;

    msbc->priv = (void *) (((uintptr_t) msbc->priv_alloc_base +
            SBC_ALIGN_MASK) & ~((uintptr_t) SBC_ALIGN_MASK));

    memset(msbc->priv, 0, sizeof(struct msbc_priv));

    msbc_set_defaults(msbc, flags);
    

    //SrcForMmsbcInit();

    return 0;
}

#ifdef MSBC_DECODE
SBC_EXPORT ssize_t msbc_parse(msbc_t *msbc, const void *input, size_t input_len)
{
    return msbc_decode(msbc, input, input_len, NULL, 0, NULL);
}

SBC_EXPORT ssize_t msbc_decode(msbc_t *msbc, const void *input, size_t input_len,
            void *output, size_t output_len, size_t *written)
{
    struct msbc_priv *priv;
    short *ptr;
    int i,/* ch,*/ framelen, samples;

    if (!msbc || !input)
        return -SBC_EIO;
    
    priv = msbc->priv;

    framelen = priv->unpack_frame(input, &priv->frame, input_len);

    if (!priv->init_dec && (framelen > 0)) {
        msbc_decoder_init(&priv->dec_state, &priv->frame);
        priv->init_dec = priv->init = TRUE;

        priv->frame.codesize = msbc_get_codesize(msbc);
        priv->frame.length = framelen;
        CLOGI("codesize:%d,frame_len:%d", priv->frame.codesize, priv->frame.length);
    }

    if (written)
        *written = 0;
    if (framelen <= 0)
        return framelen;

    samples = msbc_synthesize_audio(&priv->dec_state, &priv->frame);

    ptr = output;

    if (output_len < (size_t)samples)
        samples = output_len;
        
    for (i = 0; i < samples; i++) 
    {
        *ptr++ = priv->frame.pcm_sample[i];
    }

    if (written)
        *written = samples; //* 1/*priv->frame.channels*/;
    return framelen;
}
#endif //MSBC_DECODE

#ifdef MSBC_ENCODE
SBC_EXPORT ssize_t msbc_encode(msbc_t *msbc, const void *input, size_t input_len,
            void *output, size_t output_len, size_t *written)
{
    struct msbc_priv *priv;
    int samples;
    ssize_t framelen;
    int (*msbc_enc_process_input)(int position,
            const uint8_t *pcm, int16_t X/*[2]*/[SBC_X_BUFFER_SIZE],
            int nsamples, int nchannels);

    if (!msbc || !input)
        return -SBC_EIO;

    priv = msbc->priv;

    if (written)
        *written = 0;

    if (!priv->init_enc) {
        priv->frame.codesize = msbc_get_codesize(msbc);
        priv->frame.length = msbc_get_frame_length(msbc);

        msbc_encoder_init(&priv->enc_state, &priv->frame);
        priv->init_enc = priv->init = TRUE;
    }

    /* input must be large enough to encode a complete frame */
    if (input_len < priv->frame.codesize)
        return -SBC_ENLENINV;

    /* output must be large enough to receive the encoded frame */
    if (!output || output_len < priv->frame.length)
        return -SBC_ENOSPC;

    msbc_enc_process_input = priv->enc_state.msbc_enc_process_input_8s_le;

    priv->enc_state.position = msbc_enc_process_input(
        priv->enc_state.position, (const uint8_t *) input,
        priv->enc_state.X, 8/*priv->frame.subbands*/ * MSBC_BLOCKS/*priv->frame.blocks*/,
        1/*priv->frame.channels*/);

    samples = msbc_analyze_audio(&priv->enc_state, &priv->frame);

    {
        priv->enc_state.msbc_calc_scalefactors(
            priv->frame.sb_sample_f, priv->frame.scale_factor_f,
            MSBC_BLOCKS/*priv->frame.blocks*/, 1/*priv->frame.channels*/,
            8/*priv->frame.subbands*/);
        framelen = priv->pack_frame(output,
                &priv->frame, output_len, 0);
    }

    if (written)
        *written = framelen;

    return samples * 1/*priv->frame.channels*/;
}
#endif //MSBC_ENCODE

SBC_EXPORT void msbc_finish(msbc_t *msbc)
{
    if (!msbc)
        return;

    if (msbc->priv_alloc_base)
        AUD_FREE(msbc->priv_alloc_base);

    memset(msbc, 0, sizeof(msbc_t));
}

SBC_EXPORT size_t msbc_get_frame_length(msbc_t *msbc)
{
    int ret;
    uint8_t subbands, channels, blocks, joint, bitpool;
    struct msbc_priv *priv;

    priv = msbc->priv;
    if (priv->init) // && priv->frame.bitpool == msbc->bitpool)
        return priv->frame.length;

    subbands = 8;
    blocks = MSBC_BLOCKS;
    channels = 1; //msbc->mode == SBC_MODE_MONO ? 1 : 2;
    joint = 0; //msbc->mode == SBC_MODE_JOINT_STEREO ? 1 : 0;
    bitpool = 26; //msbc->bitpool;

    ret = 4 + (4 * subbands * channels) / 8;
    /* This term is not always evenly divide so we round it up */
    if (channels == 1)
        ret += ((blocks * channels * bitpool) + 7) / 8;
    else
        ret += (((joint ? subbands : 0) + blocks * bitpool) + 7) / 8;

    return ret;
}

SBC_EXPORT unsigned msbc_get_frame_duration(msbc_t *msbc)
{
    uint8_t subbands, blocks;
    uint16_t frequency;
    struct msbc_priv *priv;

    priv = msbc->priv;
    if (!priv->init) {
        subbands = 8; //msbc->subbands ? 8 : 4;
            blocks = MSBC_BLOCKS;
    } else {
        subbands = 8/*priv->frame.subbands*/;
        blocks = MSBC_BLOCKS/*priv->frame.blocks*/;
    }

    frequency = 16000;

    return (1000000 * blocks * subbands) / frequency;
}

SBC_EXPORT size_t msbc_get_codesize(msbc_t *msbc)
{
    uint16_t subbands, channels, blocks;
    struct msbc_priv *priv;

    priv = msbc->priv;
    if (!priv->init) {
        subbands = 8; //msbc->subbands ? 8 : 4;
            blocks = MSBC_BLOCKS;
        channels = 1; //msbc->mode == SBC_MODE_MONO ? 1 : 2;
    } else {
        subbands = 8/*priv->frame.subbands*/;
        blocks = MSBC_BLOCKS/*priv->frame.blocks*/;
        channels = 1/*priv->frame.channels*/;
    }

    return subbands * blocks * channels*2;
}

#ifdef MSBC_ENCODE
SBC_EXPORT const char *msbc_get_implementation_info(msbc_t *msbc)
{
    struct msbc_priv *priv;

    if (!msbc)
        return NULL;

    priv = msbc->priv;
    if (!priv)
        return NULL;

    return priv->enc_state.implementation_info;
}
#endif //MSBC_ENCODE

SBC_EXPORT int msbc_reinit(msbc_t *msbc, unsigned long flags)
{
    struct msbc_priv *priv;

    if (!msbc || !msbc->priv)
        return -SBC_EIO;

    priv = msbc->priv;

    if (priv->init)
        memset(msbc->priv, 0, sizeof(struct msbc_priv));

    msbc_set_defaults(msbc, flags);

    return 0;
}
