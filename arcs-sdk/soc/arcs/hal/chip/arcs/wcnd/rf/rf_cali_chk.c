/**
 ****************************************************************************************
 *
 * @file rf_cali_chk.c
 *
 * @brief functions of RF calibraion check results
 *
 * Copyright (C) ListenAI 2020-2025
 *
 * Created on: Jun 26, 2025
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */

#include "rf_cali_chk.h"
#include "log_print.h"

#ifndef abs
#define abs(x)   ((x)>=0?(x):-(x))
#endif

 int8_t rf_cali_check_dc_result(int16_t dc_i, int16_t dc_q, int16_t dc_i_thd, int16_t dc_q_thd)
 {
     if (abs(dc_i) > dc_i_thd || abs(dc_q) > dc_q_thd) {
         return -1;  // invalid DC offset
     }
     return 0;  // valid DC offset
 }

 int8_t rf_cali_check_iq_result(int16_t *c21, int16_t *c22, int16_t c21_thd, int16_t c22_thd)
 {
    int8_t ret = 0;
    int16_t c21_tmp = *c21;
    int16_t c22_tmp = *c22;

    if (*c21 > c21_thd) {
        *c21 = c21_thd;
        ret = -1;
    }
    else if (*c21 < -c21_thd) {
        *c21 = -c21_thd;
        ret = -1;
    }
    if (*c22 > 2048 + c22_thd) {
        *c22 = 2048 + c22_thd;
        ret = -1;
    }
    else if (*c22 < 2048 - c22_thd) {
        *c22 = 2048 - c22_thd;
        ret = -1;
    }
    if (ret != 0) {
        CLOGW("RF cali check IQ out of range, c21(%d->%d) c22(%d->%d)\n", c21_tmp, *c21, c22_tmp, *c22);
    }
    return ret;
 }

 int8_t rf_cali_check_dpd_result(complexint16* cParaEst)
{
    complexint16 cSumTmp = {0};
    for (int idx = 0; idx < MAX_PARALEN; idx++)
    {
        if (abs((cParaEst + idx)->re) > 8190)
            return -1; // invalid DPD parameters
        if (abs((cParaEst + idx)->im) > 8190)
            return -1; // invalid DPD parameters
    }
    if ((CalculateAmp16(*cParaEst, 3) < 384) || (CalculateAmp16(*cParaEst, 3) > 665))
        return -1; // invalid DPD parameters
    cSumTmp = ComplexAddInt16(&cSumTmp, cParaEst);
    cSumTmp = ComplexAddInt16(&cSumTmp, cParaEst+5);
    cSumTmp = ComplexAddInt16(&cSumTmp, cParaEst+10);
    if ((CalculateAmp16(cSumTmp, 3) < 384) || (CalculateAmp16(cSumTmp, 3) > 665))
        return -1; // invalid DPD parameters
    return 0; // valid DPD parameters
}