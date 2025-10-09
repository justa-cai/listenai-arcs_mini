/**
 ****************************************************************************************
 *
 * @file src.c
 *
 * @brief resample 8k to 16k and 16k to 8k.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */



////////////////////////////////////////////////////////////////////////////////
//                                                                            //
/// @file src.c                                                               //
/// That file implementes the MSBC service.                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
#include "msbc_typedef.h"
#include "string.h"

#define ROUND 16384  // 1<<14
#define PCM_VALUE_MAX 32767
#define PCM_VALUE_MIN -32768

//#define DEBUG_FLOAT
//#define DEBUG_TONE

short coef1[3] = {1976, 13520, 25320};
short coef2[3] = {7077, 19804, 30274};

static short historyUp1[4];
static short historyUp2[4];
static short historyDown1[4];
static short historyDown2[4];

#ifdef DEBUG_FLOAT
float float_coef1[3] = {1976.0/32768, 13520.0/32768, 25320.0/32768};
float float_coef2[3] = {7077.0/32768, 19804.0/32768, 30274.0/32768};

static float float_historyUp1[4];
static float float_historyUp2[4];
static float float_historyDown1[4];
static float float_historyDown2[4];
#endif

#ifdef DEBUG_TONE
short debug_data_1k_8ksample[] = {0, 23170, 32767, 23170, 0, -23170, -32767, -23170};
short debug_data_1k_16ksample[] = {0, 23170, 32767, 23170, 0, -23170, -32767, -23170, 0, 23170, 32767, 23170, 0, -23170, -32767, -23170};
//{0, 12540, 23170, 30274, 32767, 30274, 23170, 12540, 0, -12540, -23170, -30274, -32767, -30274, -23170, -12540};
#endif

void SrcForMsbcInit(void)
{
    memset(historyUp1, 0, sizeof(historyUp1));
    memset(historyUp2, 0, sizeof(historyUp2));
    memset(historyDown1, 0, sizeof(historyDown1));
    memset(historyDown2, 0, sizeof(historyDown2));
}

void SrcSample8kTo16k(short *in_8k, short *out_16k, int sample_size)
{
    char state = 0;
    short i = 0;
    int temp1 = 0;
    int temp2 = 0;
    int temp3 = 0;

    short in_data = 0;

    short *pcoef = NULL;
    for(i = 0; i < sample_size; i ++)
    {

        if(state == 0)
        {
            pcoef = coef1;
        }
        else
        {
            pcoef = coef2;
        }

        in_data = in_8k[i>>1] ;
        // 1
        temp1 = in_data - historyUp2[1];
        temp1 = temp1 * pcoef[0];
        temp1 += ROUND;
        temp1 =  temp1 >> 15;
        temp1 =  temp1 + historyUp2[0];

        if(temp1 > PCM_VALUE_MAX)
        {
            temp1 = PCM_VALUE_MAX;
        }
        else if(temp1 < PCM_VALUE_MIN)
        {
            temp1 = PCM_VALUE_MIN;
        }

        // 2
        temp2 = temp1 - historyUp2[2];
        temp2 = temp2 * pcoef[1];
        temp2 += ROUND;
        temp2 = temp2 >> 15;
              temp2 = temp2 + historyUp2[1];

        if(temp2 > PCM_VALUE_MAX)
        {
        temp2 = PCM_VALUE_MAX;
        }
        else if(temp2 < PCM_VALUE_MIN)
        {
        temp2 = PCM_VALUE_MIN;
        }

        // 3
        temp3 = temp2 - historyUp2[3];
        temp3 = temp3 * pcoef[2];
        temp3 += ROUND;
        temp3 = temp3 >> 15;
              temp3 = temp3 + historyUp2[2];

        if(temp3 > PCM_VALUE_MAX)
        {
        temp3 = PCM_VALUE_MAX;
        }
        else if(temp3 < PCM_VALUE_MIN)
        {
        temp3 = PCM_VALUE_MIN;
        }

        out_16k[i] = temp3;

        historyUp2[0] = historyUp1[0];
        historyUp2[1] = historyUp1[1];
        historyUp2[2] = historyUp1[2];
        historyUp2[3] = historyUp1[3];
        
        historyUp1[0] = in_data;
        historyUp1[1] = (short)temp1;
        historyUp1[2] = (short)temp2;
        historyUp1[3] = (short)temp3;
        state = ~state;
    }
}

void SrcSample16kTo8k(short *in_16k, short *out_8k, int sample_size)
{
    char state = 0;
    short i = 0;
    int temp1 = 0;
    int temp2 = 0;
    int temp3 = 0;

    short in_data = 0;
    static short out_data = 0;

    short *pcoef = NULL;
    for(i = 0; i < sample_size; i ++)
    {

        if(state == 0)
        {
            pcoef = coef1;
        }
        else
        {
            pcoef = coef2;
        }

        in_data = in_16k[i] ;
        // 1
        temp1 = in_data - historyDown2[1];
        temp1 = temp1 * pcoef[0];
        temp1 += ROUND;
        temp1 = (temp1 >> 15) + historyDown2[0];

        if(temp1 > PCM_VALUE_MAX)
        {
            temp1 = PCM_VALUE_MAX;
        }
        else if(temp1 < PCM_VALUE_MIN)
        {
            temp1 = PCM_VALUE_MIN;
        }

        // 2
        temp2 = temp1 - historyDown2[2];
        temp2 = temp2 * pcoef[1];
        temp2 += ROUND;
        temp2 = (temp2>>15) + historyDown2[1];

        if(temp2 > PCM_VALUE_MAX)
        {
            temp2 = PCM_VALUE_MAX;
        }
        else if(temp2 < PCM_VALUE_MIN)
        {
            temp2 = PCM_VALUE_MIN;
        }

        // 3
        temp3 = temp2 - historyDown2[3];
        temp3 = temp3 * pcoef[2];
        temp3 += ROUND;
        temp3 = (temp3 >> 15) + historyDown2[2];

        if(temp3 > PCM_VALUE_MAX)
        {
            temp3 = PCM_VALUE_MAX;
        }
        else if(temp3 < PCM_VALUE_MIN)
        {
            temp3 = PCM_VALUE_MIN;
        }

        historyDown2[0] = historyDown1[0];
        historyDown2[1] = historyDown1[1];
        historyDown2[2] = historyDown1[2];
        historyDown2[3] = historyDown1[3];

        historyDown1[0] = in_data;
        historyDown1[1] = (short)temp1;
        historyDown1[2] = (short)temp2;
        historyDown1[3] = (short)temp3;

        if(state == 0)
        {
            out_8k[i/2]  = (short)((out_data + temp3)>>1);
            out_data = 0;
        }
        else
        {
            out_data = temp3;
        }

        state = ~state;
    }
}

#ifdef DEBUG_FLOAT
void SrcSample8kTo16kFloat(short *in_8k, short *out_16k, int sample_size)
{
    char state = 0;
    short i = 0;
    double temp1 = 0;
    double temp2 = 0;
    double temp3 = 0;

    float in_data = 0;

       int temp_out = 0;
    float *pcoef = NULL;
    for(i = 0; i < sample_size; i ++)
    {

        if(state == 0)
        {
            pcoef = float_coef1;
        }
        else
        {
            pcoef = float_coef2;
        }

        in_data =(float) in_8k[i>>1] /32768;
        // 1
        temp1 = in_data - float_historyUp2[1];
        temp1 = temp1 * pcoef[0];
        temp1 =  temp1 + float_historyUp2[0];

        // 2
        temp2 = temp1 - float_historyUp2[2];
        temp2 = temp2 * pcoef[1];
            temp2 = temp2 + float_historyUp2[1];

        // 3
        temp3 = temp2 - float_historyUp2[3];
        temp3 = temp3 * pcoef[2];
        temp3 = temp3 + float_historyUp2[2];

        temp_out = (int)(temp3*32768);
        if(temp_out > PCM_VALUE_MAX)
        {
            temp_out = PCM_VALUE_MAX;
        }
        else if(temp_out <= PCM_VALUE_MIN)
        {
            temp_out = PCM_VALUE_MIN;
        }
        out_16k[i] = (short)(temp_out);

        float_historyUp2[0] = float_historyUp1[0];
        float_historyUp2[1] = float_historyUp1[1];
        float_historyUp2[2] = float_historyUp1[2];
        float_historyUp2[3] = float_historyUp1[3];
        
        float_historyUp1[0] = (float)in_data;
        float_historyUp1[1] = (float)temp1;
        float_historyUp1[2] = (float)temp2;
        float_historyUp1[3] = (float)temp3;


        state = ~state;
    }
}

void SrcSample16kTo8kFloat(short *in_16k, short *out_8k, int sample_size)
{
    char state = 0;
    short i = 0;
    double temp1 = 0;
    double temp2 = 0;
    double temp3 = 0;
    static double temp_out = 0;

    float in_data = 0;
    
    float *pcoef = NULL;
    for(i = 0; i < sample_size; i ++)
    {

        if(state == 0)
        {
            pcoef = float_coef1;
        }
        else
        {
            pcoef = float_coef2;
        }

        in_data =(float) in_16k[i] /32768;
        // 1
        temp1 = in_data - float_historyDown2[1];
        temp1 = temp1 * pcoef[0];
        temp1 =  temp1 + float_historyDown2[0];

        // 2
        temp2 = temp1 - float_historyDown2[2];
        temp2 = temp2 * pcoef[1];
        temp2 = temp2 + float_historyDown2[1];

        // 3
        temp3 = temp2 - float_historyDown2[3];
        temp3 = temp3 * pcoef[2];
        temp3 = temp3 + float_historyDown2[2];

        float_historyDown2[0] = float_historyDown1[0];
        float_historyDown2[1] = float_historyDown1[1];
        float_historyDown2[2] = float_historyDown1[2];
        float_historyDown2[3] = float_historyDown1[3];

        float_historyDown1[0] = (float)in_data;
        float_historyDown1[1] = (float)temp1;
        float_historyDown1[2] = (float)temp2;
        float_historyDown1[3] = (float)temp3;

        if(state == 0)
        {
            int out_sat;
            temp3 = (temp_out + temp3)/2;
            out_sat = (int)(temp3*32768);
            if(out_sat > PCM_VALUE_MAX)
            {
                out_sat = PCM_VALUE_MAX;
            }
            else if(out_sat <= PCM_VALUE_MIN)
            {
                out_sat = PCM_VALUE_MIN;
            }
            temp_out = 0;
            out_8k[i/2] = (short)out_sat;
        }
        else
        {
            temp_out = temp3;
        }
        state = ~state;
    }
}
#endif


