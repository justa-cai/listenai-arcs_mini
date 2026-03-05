
#include <math.h>
#include <stdio.h>
#include "DpdEstLuna.h"
//#include "TxSignalRe.h"
//#include "TxSignalIm.h"
#include "log_print.h"
const int8_t PREDSEL1[MAX_MEMORY][MAX_ORDER] = {
    {1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1}
};

extern int32_t *pApiTemp32;
extern int64_t *pApiTemp64;
extern int32_t *pAmpSig;

#if 1 //dig_gain = 74
const complexint32 cInitAutoMatrix1[((PREDLEN * PREDLEN - PREDLEN) >> 1) + PREDLEN] =
{
    {       412,         0},
    {      -916,       -16},
    {     12071,         0},
    {      1326,        77},
    {    -25428,       135},
    {     81512,         0},
    {      -647,         3},
    {       647,       -31},
    {      -288,       148},
    {      1188,         0},
    {       681,       -24},
    {     -7060,        46},
    {      8461,      -129},
    {     -1421,        32},
    {     16465,         0},
    {      -439,        47},
    {      8480,      -212},
    {    -29979,      1207},
    {      1513,       -58},
    {    -27771,       -13},
    {     92013,         0},
    {       338,        -1},
    {      -276,        19},
    {       -14,       -78},
    {      -646,        -1},
    {       681,        13},
    {      -438,       -14},
    {       411,         0},
    {      -282,        -9},
    {      2699,       -45},
    {     -1708,       122},
    {       659,        11},
    {     -7032,         9},
    {      8488,      -242},
    {      -923,        -3},
    {     12049,         0},
    {        39,        23},
    {     -1590,       -35},
    {      7296,      -396},
    {      -391,        -2},
    {      8380,         9},
    {    -30014,      1229},
    {      1381,        -2},
    {    -25397,      -205},
    {     81531,         0}
};
#endif

const int32_t uLamda1[2] = { 1843, 1024 };
const int32_t uDcLamda1[2] = { 1024, 512 };

void DcEstLuna(int32_t* in_re, int32_t* in_im, int32_t* dc_re, int32_t* dc_im)
{
    SumComplex32Luna(in_re, in_im, dc_re, dc_im, SEL_LEN, SEL_SHIFT);
    return;
}


void LunaVectorSumI32O32(int32_t* in, int32_t* sum_value, int16_t size, int16_t shift)
{
    //extern int32_t *pApiTemp32;
    LUNA_API_SIM(luna_vector_sum_i32o32)(in, pApiTemp32, size, shift);
    *sum_value = *pApiTemp32;
}

void SumComplex32Luna(int32_t* in_re, int32_t* in_im, int32_t* sum_value_re, int32_t* sum_value_im, int16_t size, int16_t shift)
{
    LunaVectorSumI32O32(in_re, sum_value_re, size, shift);
    LunaVectorSumI32O32(in_im, sum_value_im, size, shift);
    return;
}

void LunaDotProdI32O32(int32_t* in1, int32_t* in2, int32_t* dot_value, int16_t size, int16_t shift)
{
    //extern int32_t *pApiTemp32;
    LUNA_API_SIM(luna_dot_prod_i32i32o32)(in1, in2, pApiTemp32, size, shift);
    *dot_value = *pApiTemp32;
}

void LunaOffsetI32O32(int32_t* in, int32_t scale, int32_t* out, int16_t size, int16_t shift)
{
    //extern int32_t *pApiTemp32;
    *pApiTemp32 = scale;
    LUNA_API_SIM(luna_offset_i32i32o32)(in, *pApiTemp32, out, size, shift);
}

void CalculateDcEstLuna(int32_t* in_re, int32_t* in_im, complexint32* cDcEstRx)
{
    DcEstLuna(in_re, in_im, &(cDcEstRx->re), &(cDcEstRx->im));
    LunaOffsetI32O32(in_re, -1 * cDcEstRx->re, in_re, SEL_LEN, 0);
    LunaOffsetI32O32(in_im, -1 * cDcEstRx->im, in_im, SEL_LEN, 0);
    //CLOGI("LUNA cDcEstRx.re=%d,cDcEstRx.im = %d\n",cDcEstRx->re,cDcEstRx->im);
    return;
}

void CalculatePowerWithDcLuna(int32_t* in_re, int32_t* in_im, uint32_t* uPower)
{
    int32_t uPowerRe = 0;
    int32_t uPowerIm = 0;
    uint32_t uPowerRI = 0;
    LunaDotProdI32O32(in_re, in_re, &uPowerRe, SEL_LEN, 12);
    LunaDotProdI32O32(in_im, in_im, &uPowerIm, SEL_LEN, 12);
    uPowerRI = (uint32_t)uPowerRe + (uint32_t)uPowerIm;
    *uPower = uPowerRI;
    return;
}

void LunaDotProdI32O64(int32_t* in1, int32_t* in2, int64_t* dot_value, int16_t size, int16_t shift)
{
    //extern int64_t *pApiTemp64;
    LUNA_API_SIM(luna_dot_prod_i32i32o64)(in1, in2, pApiTemp64, size, shift);
    *dot_value = *pApiTemp64;
}

void DotComplex32Luna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, int32_t* o_re, int32_t* o_im,int8_t y_conj_en, int16_t size, int16_t shift)
{
    int64_t ac;
    int64_t bd;
    int64_t ad;
    int64_t bc;
    int32_t out_re;
    int32_t out_im;
    LunaDotProdI32O64(x_re, y_re, &ac, size, 0);
    LunaDotProdI32O64(x_im, y_im, &bd, size, 0);
    LunaDotProdI32O64(x_re, y_im, &ad, size, 0);
    LunaDotProdI32O64(x_im, y_re, &bc, size, 0);
    if (y_conj_en)
    {
        *o_re = (int32_t)((ac + bd) >> shift);
        *o_im = (int32_t)((bc - ad) >> shift);
    }
    else
    {
        *o_re = (int32_t)((ac - bd) >> shift);
        *o_im = (int32_t)((ad + bc) >> shift);
    }
}


void CalculateTimeEstLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t uTimeEstStartTx, uint8_t uTimeEstStartFb, uint16_t uTimeEstWinLen, uint8_t uTimeEstLen, int16_t* iIntDelay, int16_t* iFracDelay)
{
    complexint32 cCrossVal[10] = { 0 };
    uint32_t uCrossAmp[10] = { 0 };
    uint8_t uMaxIdx = 0;
    uint32_t uMaxCrossAmp = 0;
    int32_t a, b, c;
    uint32_t uCrossSyncAmp;
    int16_t iFracTmp;
    for (int idx = 0; idx < uTimeEstLen; idx++)
    {
        DotComplex32Luna(x_re+ uTimeEstStartTx, x_im+ uTimeEstStartTx, y_re + idx, y_im + idx, &cCrossVal[idx].re, &cCrossVal[idx].im, 1, uTimeEstWinLen, 11);
        uCrossAmp[idx] = CalculateAmp(cCrossVal[idx], 0);
        if (uCrossAmp[idx] > uMaxCrossAmp)
        {
            uMaxCrossAmp = uCrossAmp[idx];
            uMaxIdx = idx;
        }
    }
    if (uMaxIdx < 1)
    {
        uMaxIdx = 1;
    }
    if (uMaxIdx > 9)
    {
        uMaxIdx = 9;
    }
    b = uCrossAmp[uMaxIdx + 1] - uCrossAmp[uMaxIdx - 1];
    a = uCrossAmp[uMaxIdx + 1] - (b >> 1) - uCrossAmp[uMaxIdx];
    iFracTmp = -1 * (b << 3) / a;
    if (iFracTmp < 0)
    {
        *iIntDelay = uMaxIdx - uTimeEstStartTx + uTimeEstStartFb - 1;
        *iFracDelay = (int16_t)(1 << 5) + iFracTmp;
    }
    else
    {
        *iIntDelay = uMaxIdx - uTimeEstStartTx + uTimeEstStartFb;
        *iFracDelay = iFracTmp;
    }
    return;
}


void FbCompTimeLuna(int32_t* x_re, int32_t* x_im, int16_t iFracDelay)
{
    complexint32 cTmp1, cTmp2;
    int16_t iCompFracDelay = (int16_t)(1 << 5) - iFracDelay;
    for (int idx = 0;idx < SEL_LEN - 1; idx++)
    {
        cTmp1.re = ((x_re[idx] * iCompFracDelay) + (1 << 4)) >> 5;
        cTmp1.im = ((x_im[idx] * iCompFracDelay) + (1 << 4)) >> 5;
        cTmp2.re = ((x_re[idx+1] * iFracDelay) + (1 << 4)) >> 5;
        cTmp2.im = ((x_im[idx+1] * iFracDelay) + (1 << 4)) >> 5;
        x_re[idx] = cTmp1.re + cTmp2.re;
        x_im[idx] = cTmp1.im + cTmp2.im;
    }
    return;
}

void CalculateGainEstLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t uTimeEstStart, uint16_t uTimeEstLen, complexint16* cGain)
{
    complexint32 cTrFb = { 0 };
    complexint16 cConjXSignal = { 0 };
    complexint32 cTrFbSum = { 0 };
    uint64_t sumGainPower = 0;
    uint64_t sumGainPowerRe = 0;
    uint64_t sumGainPowerIm = 0;
    uint32_t gainPower = 0;
    complexint32 gainTmp = { 512 };

    //complexint32 cXYSignal[SEL_LEN] = { 0 };
    //LUNA_API_SIM(luna_clx_conj_mul_i32i32o32)(&cYSignal[uTimeEstStart], &cXSignal[uTimeEstStart], &cXYSignal[0], uTimeEstLen, 3);
    //SumComplex32Luna(cXYSignal, &cTrFbSum, uTimeEstLen, 0);

    DotComplex32Luna(y_re, y_im,x_re, x_im, &cTrFbSum.re, &cTrFbSum.im, 1, uTimeEstLen, 3);

    sumGainPowerRe = ((uint64_t)((uint64_t)(cTrFbSum.re) * (uint64_t)(cTrFbSum.re)) + (1 << 26)) >> 27;
    sumGainPowerIm = ((uint64_t)((uint64_t)(cTrFbSum.im) * (uint64_t)(cTrFbSum.im)) + (1 << 26)) >> 27;
    sumGainPower = sumGainPowerRe + sumGainPowerIm;
    gainPower = TXPOWER / sumGainPower;
    cTrFbSum.im = cTrFbSum.im * (-1);
    gainTmp.re = (int32_t)(((int64_t)cTrFbSum.re * (int64_t)gainPower) >> 23);
    gainTmp.im = (int32_t)(((int64_t)cTrFbSum.im * (int64_t)gainPower) >> 23);
    cGain->re = (int16_t)gainTmp.re;
    cGain->im = (int16_t)gainTmp.im;
}

void FbCompGainLuna(int32_t* x_re, int32_t* x_im, complexint16 cGain)
{
    //LUNA_API_SIM(luna_scale_i32i32o32)()
    int32_t ac;
    int32_t bd;
    int32_t ad;
    int32_t bc;
    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        ac = x_re[idx] * cGain.re;
        bd = x_im[idx] * cGain.im;
        ad = x_re[idx] * cGain.im;
        bc = x_im[idx] * cGain.re;
        x_re[idx] = (ac - bd + (1 << 10)) >> 11;
        x_im[idx] = (ad + bc + (1 << 10)) >> 11;
    }
}

void TxIqEstLuna(int32_t* x_re, int32_t* x_im, int16_t iIntdelay, int16_t* iC21, int16_t* iC22)
{
    int64_t iI2Sum = 0;
    int64_t iQ2Sum = 0;
    int64_t iIQ2Sum = 0;
    int32_t g21 = 0;
    int32_t g22 = 2048;

    LunaDotProdI32O64(x_re+ iIntdelay, x_re + iIntdelay, &iI2Sum, EST_LEN, 0);
    LunaDotProdI32O64(x_im + iIntdelay, x_im + iIntdelay, &iQ2Sum, EST_LEN, 0);
    LunaDotProdI32O64(x_re + iIntdelay, x_im + iIntdelay, &iIQ2Sum, EST_LEN, 0);

    /*for (int idx = iIntdelay;idx < EST_LEN + 1; idx++)
      {
      iI2Sum += (cYSignal + idx)->re * (cYSignal + idx)->re;
      iQ2Sum += (cYSignal + idx)->im * (cYSignal + idx)->im;
      iIQ2Sum += (cYSignal + idx)->re * (cYSignal + idx)->im;
      }*/
    g21 = (iIQ2Sum << 15) / (iI2Sum);
    g22 = (iQ2Sum << 15) / (iI2Sum);
    g22 = fix_sqrt(g22 << 15);
    g21 = (g21 + ((g22 * -82) >> 11)) >> 4;
    g22 = (g22 * 2102) >> 15;
    *iC21 = (int16_t)(-1 * g21);
    *iC22 = (int16_t)(((int32_t)1 << 22) / g22);
    CLOGI("[luna]g21=%d g22=%d iC21=%d iC22=%d\n", g21, g22, *iC21, *iC22);
}


int16_t CalculateDpdParaLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t iOrder, uint8_t iMem, complexint16* cLegcyPara, complexint16* cParaEst, uint16_t iCalLen, uint8_t iIterNum, uint16_t uGainOffset)
{
    int iParaLen = PREDLEN;
    complexint32 cCrossMartix[PREDLEN] = { 0 };
#if 0
    CLOGI("=====================\n");
    for(int i=0;i<5;i++)
    {
        CLOGI("LUNA x_re[i].re=%d,x_re[i].im = %d\n",*(x_re+i),*(x_im+i));
        CLOGI("LUNA y_re[i].re=%d,y_re[i].im = %d\n",*(y_re+i),*(y_im+i));
    }
#endif
    LUNA_API_SIM(luna_sub_i32i32o32)(x_re, y_re, y_re, iCalLen, 0);
    LUNA_API_SIM(luna_sub_i32i32o32)(x_im, y_im, y_im, iCalLen, 0);
    LUNA_API_SIM(luna_scale_i32i32o32)(y_re, 1 << ERROR_SHIFT, y_re, iCalLen, 0);
    LUNA_API_SIM(luna_scale_i32i32o32)(y_im, 1 << ERROR_SHIFT, y_im, iCalLen, 0);
#if 0
    CLOGI("SUB=====================\n");
    for(int i=0;i<5;i++)
    {
        CLOGI("LUNA x_re[i].re=%d,x_re[i].im = %d\n",*(x_re+i),*(x_im+i));
        CLOGI("LUNA y_re[i].re=%d,y_re[i].im = %d\n",*(y_re+i),*(y_im+i));
    }
#endif
#if PRINTDATA
    FILE* fp0;
    fp0 = fopen("Error.txt", "w");
    for (int idx = 0;idx < iCalLen; idx++)
    {
        fprintf(fp0, "%10d  %10d\n", (cYSignal + idx)->re, (cYSignal + idx)->im);
    }
    fclose(fp0);
#endif
    CalculateCrossMatrixLuna(x_re, x_im, y_re, y_im, iOrder, iMem, EST_LEN, 2, cCrossMartix);
#if 0
    for(int i_cc = 0; i_cc < PREDLEN;i_cc ++)
    {
        CLOGI("LUNA cCrossMartix[%d].re=%d,cCrossMartix[i].im = %d\n",i_cc,cCrossMartix[i_cc].re,cCrossMartix[i_cc].im);
    }
#endif
    CalculateParaLuna(cCrossMartix, cLegcyPara, cParaEst, iOrder, iMem, iIterNum, uGainOffset);
    //CalculateCrossMatrix(cXSignal, cYSignal, iOrder, iMem, EST_LEN, 2, cCrossMartix);
    //CalculatePara(cCrossMartix, cLegcyPara, cParaEst, iOrder, iMem, iIterNum, uGainOffset);

    return 0;
};

complexint32 cBufferData[MAX_ORDER][MAX_MEMORY] = { 0 };
complexint32 cCrossMartixBuffer[MAX_ORDER][MAX_MEMORY] = { 0 };
void CalculateCrossMatrixLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t iOrder, uint8_t iMem, uint16_t iCalLen, uint16_t iStartIdx, complexint32* cCrossMartix)
{
    uint16_t uAmpTmp;
    //complexint32 cXAbsTmp;
    complexint32 cXAbsSel;
    complexint32 cYSignalTemp;
#if PRINTDATA
    FILE* fp0;
    fp0 = fopen("XAbs.txt", "w");
    FILE* fp1;
    fp1 = fopen("Abs.txt", "w");
#endif
    //int32_t iAmpSig[SEL_LEN] = { 0 };
    complexint16 cXAbsTmp;
    for (int idxT = 0;idxT < iCalLen + iStartIdx; idxT++)
    {
        cXAbsTmp.re = *(x_re + idxT);
        cXAbsTmp.im = *(x_im + idxT);
        //iAmpSig[idxT] = CalculateAmp16(cXAbsTmp, 0);
        *(pAmpSig + idxT) = CalculateAmp16(cXAbsTmp, 0);
    }
    LUNA_API_SIM(luna_scale_i32i32o32)(x_re, 1 << 19, x_re, iCalLen + iStartIdx, 0);
    LUNA_API_SIM(luna_scale_i32i32o32)(x_im, 1 << 19, x_im, iCalLen + iStartIdx, 0);
    for (int idxO = 0;idxO < iOrder; idxO++)
    {
        if (idxO > 0)
        {
            LUNA_API_SIM(luna_mul_i32i32o32)(x_re, pAmpSig, x_re, iCalLen + iStartIdx, 14);
            LUNA_API_SIM(luna_mul_i32i32o32)(x_im, pAmpSig, x_im, iCalLen + iStartIdx, 14);
        }
#if 0
        printf("-----------------------------------\n");
        for (int i_p = 0;i_p < 10; i_p++)
        {
            printf("Order = %d; x_re = %d; x_im = %d;y_re = %d; y_im = %d;\n", idxO,*(x_re + i_p), *(x_im + i_p), *(y_re + i_p), *(y_im + i_p));
        }
#endif
        for (int idxM = 0;idxM < iMem; idxM++)
        {
            if (PREDSEL1[idxM][idxO])
            {
                DotComplex32Luna(y_re + iStartIdx, y_im + iStartIdx, x_re + iStartIdx - idxM, x_im + iStartIdx - idxM, &cCrossMartixBuffer[idxO][idxM].re, &cCrossMartixBuffer[idxO][idxM].im, 1, iCalLen, 13 + ERROR_SHIFT + CROSS_SHIFT);
                //CLOGI("cCrossMartixBuffer[i].re=%d,cCrossMartixBuffer[i].im = %d\n",cCrossMartixBuffer[idxO][idxM].re,cCrossMartixBuffer[idxO][idxM].im);
                //DotComplex32Luna(y_re + iStartIdx, y_im + iStartIdx, x_re + iStartIdx - idxM, x_im + iStartIdx - idxM, &cCrossMartixBuffer[idxO][idxM].re, &cCrossMartixBuffer[idxO][idxM].im, 1, 1, 13);
            }
        }
    }
    int cCrossIdx = 0;
    for (int idxM = 0;idxM < MAX_MEMORY;idxM++)
    {
        for (int idxO = 0;idxO < MAX_ORDER;idxO++)
        {
            //printf("%d\n", (cCrossMartix+1)->re);
            if (PREDSEL1[idxM][idxO])
            {
                (cCrossMartix + cCrossIdx)->re = cCrossMartixBuffer[idxO][idxM].re;
                (cCrossMartix + cCrossIdx)->im = cCrossMartixBuffer[idxO][idxM].im;
                cCrossIdx = cCrossIdx + 1;
            }
        }
    }
#if 0
    CLOGD("cCrossMartix:");
    for (int idxM = 0;idxM < MAX_MEMORY;idxM++)
    {
        for (int idxO = 0;idxO < MAX_ORDER;idxO++)
        {
            CLOGD("%10d  %10d\n", (cCrossMartix + (idxO + MAX_ORDER * idxM))->re, (cCrossMartix + (idxO + MAX_ORDER * idxM))->im);
        }
    }
#endif
    return;
}

uint32_t CalculateParaLuna(complexint32* cCrossMartix, complexint16* cLegcyPara, complexint16* cParaEst, uint8_t iOrder, uint8_t iMem, uint8_t uIterNum, uint16_t uGainOffset)
{
    complexint32 cParaUpdate[MAX_PARALEN] = { 0 };
    complexint32 cParaEstTmp[PREDLEN] = { 0 };
    complexint32 cLegcyParaInt32;
    uint32_t iParaLen = PREDLEN;
    complexint32 cMultiTmp;
    complexint32 cMultiTmp1;
    uint8_t uAddrMap;
    uint8_t iParaCounter = 0;
    //complexdata* cPtmp0;
    //complexdata* cPtmp1;
    uint32_t uGainOffsetTmp[MAX_ORDER] = { 2048 };
    uGainOffsetTmp[0] = uGainOffset;
    for (int idx = 1; idx < MAX_ORDER - 1; idx++)
    {
        uGainOffsetTmp[idx] = (uGainOffsetTmp[idx - 1] * uGainOffset) >> 11;
    }
    if (uIterNum == 0)
    {
        for (int idx = 0; idx < MAX_PARALEN; idx++)
        {
            cLegcyPara[idx].re = 0;
            cLegcyPara[idx].im = 0;
        }
        cLegcyPara[0].re = 1 << 9;
    }

    iParaCounter = 0;
    for (int idxM = 0;idxM < 3; idxM++)
    {
        //uGainOffsetDiv = (2048 << 11) / uGainOffset;
        //uGainOffsetTmp = 2048;
        for (int idxO = 0;idxO < 5; idxO++)
        {

            //int PREDSEL[1][2];
            if (PREDSEL1[idxM][idxO])
            {
                if (idxO > 0)
                    cCrossMartix[iParaCounter] = ComplexMultiReal(&cCrossMartix[iParaCounter], uGainOffsetTmp[idxO - 1], 11);
                iParaCounter = iParaCounter + 1;
            }
        }
        //uGainOffsetTmp = (((uint32_t)uGainOffsetTmp * (uint32_t)uGainOffsetDiv) >> 11);
    }
    for (int idxP0 = 0; idxP0 < iParaLen; idxP0++)
    {
        for (int idxP1 = 0; idxP1 < iParaLen; idxP1++)
        {
            //cMultiTmp = ComplexMulti(((cAutoMatirx + idxP1)+ iParaLen * idxP0), cCrossMartix + idxP1, 25);
            if (idxP0 < idxP1)
            {
                uAddrMap = ((idxP1 * idxP1 + idxP1) >> 1) + idxP0;
                cMultiTmp1 = cInitAutoMatrix1[uAddrMap];
                cMultiTmp1 = ComplexConj(&cMultiTmp1, 0);
            }
            else
            {
                uAddrMap = ((idxP0 * idxP0 + idxP0) >> 1) + idxP1;
                cMultiTmp1 = cInitAutoMatrix1[uAddrMap];
            }

            cMultiTmp = ComplexMulti(&cMultiTmp1, cCrossMartix + idxP1, 25 - CROSS_SHIFT);
            cParaEstTmp[idxP0] = ComplexAddInt32(&cParaEstTmp[idxP0], &cMultiTmp);

        }
        if (uIterNum < 3)
        {
            cParaEstTmp[idxP0] = ComplexMultiReal(&cParaEstTmp[idxP0], uLamda1[0], 11);
        }
        else
        {
            cParaEstTmp[idxP0] = ComplexMultiReal(&cParaEstTmp[idxP0], uLamda1[1], 11);
        }

    }
    iParaCounter = 0;
    for (int idxM = 0;idxM < 3; idxM++)
    {
        for (int idxO = 0;idxO < 5; idxO++)
        {
            //int PREDSEL[1][2];
            if (PREDSEL1[idxM][idxO])
            {
                if (idxO > 0)
                    cParaUpdate[idxO + idxM * 5] = ComplexMultiReal(&cParaEstTmp[iParaCounter], uGainOffsetTmp[idxO - 1], 11);
                else
                    cParaUpdate[idxO + idxM * 5] = cParaEstTmp[iParaCounter];
                iParaCounter = iParaCounter + 1;
            }
        }
    }



    for (int idxP = 0; idxP < MAX_PARALEN; idxP++)
    {
        cParaUpdate[idxP].re = (cParaUpdate[idxP].re + (1 << 4)) >> 5;
        cParaUpdate[idxP].im = (cParaUpdate[idxP].im + (1 << 4)) >> 5;
        cLegcyParaInt32.re = (int32_t)cLegcyPara[idxP].re;
        cLegcyParaInt32.im = (int32_t)cLegcyPara[idxP].im;
        cParaUpdate[idxP] = ComplexAddInt32(&cLegcyParaInt32, &cParaUpdate[idxP]);
    }
    for (int idxP = 0; idxP < MAX_PARALEN; idxP++)
    {
        (cParaEst + idxP)->re = (int16_t)cParaUpdate[idxP].re;
        (cParaEst + idxP)->im = (int16_t)cParaUpdate[idxP].im;
    }
    return 0;
}
