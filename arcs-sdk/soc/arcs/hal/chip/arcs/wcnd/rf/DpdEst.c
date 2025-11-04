
#include <math.h>
#include <stdio.h>
#include "DpdEst.h"
#include "log_print.h"
#include "rf_fxp.h"

const int PREDSEL[3][5] = { {1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1} };
#if 0 //dig_gain=64
const complexint32 cInitAutoMatrix[((PREDLEN * PREDLEN - PREDLEN) >> 1) + PREDLEN] =
{
    {       526,         0},
    {     -1117,        -5},
    {     16680,         0},
    {      1222,        90},
    {    -32640,       220},
    {    143071,         0},
    {      -847,         2},
    {       886,       -14},
    {       118,       159},
    {      1548,         0},
    {       904,       -31},
    {    -10452,        45},
    {      3091,       121},
    {     -1843,        42},
    {     24512,         0},
    {       -68,        56},
    {      3114,        14},
    {    -22530,       988},
    {      1240,       -88},
    {    -31600,        17},
    {    146913,         0},
    {       445,        -1},
    {      -415,        12},
    {      -174,       -84},
    {      -844,        -2},
    {       906,        17},
    {       -66,       -41},
    {       523,         0},
    {      -407,       -12},
    {      4591,       -89},
    {       838,        25},
    {       876,        22},
    {    -10418,        -2},
    {      3096,        27},
    {     -1113,       -11},
    {     16658,         0},
    {      -113,        34},
    {      1003,      -176},
    {      1390,        46},
    {         1,       -21},
    {      2978,       291},
    {    -22573,      1028},
    {      1283,         1},
    {    -32597,      -300},
    {    143086,         0},
};
#endif
#if 1 //dig_gain = 74
const complexint32 cInitAutoMatrix[((PREDLEN * PREDLEN - PREDLEN) >> 1) + PREDLEN] =
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

const int32_t uLamda[2] = {1843, 1024};
const int32_t uDcLamda[2] = {1024, 512};

int16_t CalculateDpdPara(complexint16* cXSignal, complexint16* cYSignal, uint8_t iOrder, uint8_t iMem, complexint16* cLegcyPara, complexint16* cParaEst, uint16_t iCalLen, uint8_t iIterNum, uint16_t uGainOffset)
{
    int iParaLen = PREDLEN;
    complexint32 cCrossMartix[PREDLEN] = {0};
    for (int idx = 0;idx < iCalLen; idx++)
    {
        *(cYSignal + idx) = ComplexSub16(cXSignal + idx, cYSignal + idx);
        (cYSignal + idx)->re = ((cYSignal + idx)->re) << ERROR_SHIFT;
        (cYSignal + idx)->im = ((cYSignal + idx)->im) << ERROR_SHIFT;
    }
#if DPD_LUNA_DEBUG
    CLOGI("=========================================\n");
    for(int i_d=0;i_d<5;i_d++)
    {
        CLOGI("R x_re[i].re=%d,x_re[i].im = %d\n", (cXSignal+i_d)->re, (cXSignal+i_d)->im);
        CLOGI("R y_re[i].re=%d,y_re[i].im = %d\n", (cYSignal+i_d)->re, (cYSignal+i_d)->im);
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
    CalculateCrossMatrix(cXSignal, cYSignal, iOrder, iMem, EST_LEN,2, cCrossMartix);
#if DPD_LUNA_DEBUG
    for(int i_cc = 0; i_cc < PREDLEN;i_cc ++)
    {
        CLOGI("cCrossMartix[%d].re=%d,cCrossMartix[i].im = %d\n",i_cc,cCrossMartix[i_cc].re,cCrossMartix[i_cc].im);
    }
#endif

    CalculatePara(cCrossMartix, cLegcyPara, cParaEst, iOrder,iMem, iIterNum, uGainOffset);
    return 0;
};

inline uint16_t CalculateAmp16(complexint16 cSignal, uint8_t iShift)//S(16,14)*2^14
{
    uint16_t iMinValue = 0;
    uint16_t iMaxValue = 0;
    uint16_t iAmpValue = 0;
    iMinValue = abs(cSignal.re) > abs(cSignal.im) ? abs(cSignal.im) : abs(cSignal.re);
    iMaxValue = abs(cSignal.re) > abs(cSignal.im) ? abs(cSignal.re) : abs(cSignal.im);
    if (iMaxValue > (iMinValue << 1))
    {
        //iAmpValue = iMaxValue + (iMinValue >> 2);
        iAmpValue = (iMaxValue << 3) + (iMinValue << 1);
    }
    else
    {
        //iAmpValue = ((7 * iMaxValue) >> 3) + (iMinValue >> 1);
        iAmpValue = (iMaxValue << 3) - iMaxValue + (iMinValue << 2);
    }
    return iAmpValue >> iShift;
};

inline uint32_t CalculateAmp(complexint32 cSignal, uint8_t iShift)//S(16,14)*2^14
{
    uint32_t iMinValue = 0;
    uint32_t iMaxValue = 0;
    uint32_t iAmpValue = 0;
    iMinValue = abs(cSignal.re) > abs(cSignal.im) ? abs(cSignal.im) : abs(cSignal.re);
    iMaxValue = abs(cSignal.re) > abs(cSignal.im) ? abs(cSignal.re) : abs(cSignal.im);
    if (iMaxValue > (iMinValue << 1))
    {
        //iAmpValue = iMaxValue + (iMinValue >> 2);
        iAmpValue = (iMaxValue << 3) + (iMinValue << 1);
    }
    else
    {
        //iAmpValue = ((7 * iMaxValue) >> 3) + (iMinValue >> 1);
        iAmpValue = (iMaxValue << 3) - iMaxValue + (iMinValue << 2);
    }
    return iAmpValue >> iShift;
};

void CalculateCrossMatrix(complexint16* cXSignal, complexint16* cYSignal, uint8_t iOrder, uint8_t iMem, uint16_t iCalLen, uint16_t iStartIdx, complexint32* cCrossMartix)
{
    complexint32 cBufferData[MAX_ORDER][MAX_MEMORY] = {0};
    uint16_t uAmpTmp;
    complexint32 cXAbsTmp;
    complexint32 cXAbsSel;
    complexint32 cYSignalTemp;
    complexint64 cCrossMartixBuffer[MAX_ORDER][MAX_MEMORY] = {0};
#if PRINTDATA
    FILE* fp0;
    fp0 = fopen("XAbs.txt", "w");
    FILE* fp1;
    fp1 = fopen("Abs.txt", "w");
#endif
    for (int idxT = 0;idxT < iCalLen + iStartIdx; idxT++)
    {
        uAmpTmp = CalculateAmp16(cXSignal[idxT],0);
        cXAbsTmp.re = cXSignal[idxT].re << 19;
        cXAbsTmp.im = cXSignal[idxT].im << 19;
#if PRINTDATA
        fprintf(fp1, "%5d\n", uAmpTmp);
#endif
        for (int idxO = 0;idxO < iOrder; idxO++)
        {
            for (int idxMShift = MAX_MEMORY - 1;idxMShift > 0 ; idxMShift-- )
            {
                cBufferData[idxO][idxMShift] = cBufferData[idxO][idxMShift -1];
            }
            cBufferData[idxO][0] = ComplexConj(&cXAbsTmp,0);

            for (int idxM = 0;idxM < iMem; idxM++)
            {
                if (idxT >= iStartIdx)
                {
                    //int PREDSEL[1][2];
                    if(PREDSEL[idxM][idxO])
                    {
                        cYSignalTemp.re = (int32_t)(cYSignal[idxT].re);
                        cYSignalTemp.im = (int32_t)(cYSignal[idxT].im);
                        cXAbsSel = ComplexMulti(&cBufferData[idxO][idxM], &cYSignalTemp,13);
                        cCrossMartixBuffer[idxO][idxM] = ComplexAddInt64(&cCrossMartixBuffer[idxO][idxM], &cXAbsSel);
                    }
                }
            }
#if PRINTDATA
            fprintf(fp0, "%10d  %10d  ", cXAbsTmp.re, cXAbsTmp.im);
#endif
            cXAbsTmp = ComplexMultiReal(&cXAbsTmp, uAmpTmp, 14);
        }
#if PRINTDATA
        fprintf(fp0, "\n");
#endif
    }
#if PRINTDATA
    fclose(fp0);
    fclose(fp1);
#endif

    int cCrossIdx = 0;
    for (int idxM = 0;idxM < MAX_MEMORY;idxM++)
    {
        for (int idxO = 0;idxO < MAX_ORDER;idxO++)
        {
            //printf("%d\n", (cCrossMartix+1)->re);
            if (PREDSEL[idxM][idxO])
            {
                (cCrossMartix + cCrossIdx)->re = (cCrossMartixBuffer[idxO][idxM].re + (1 << (ERROR_SHIFT - 1 + CROSS_SHIFT))) >> (ERROR_SHIFT+CROSS_SHIFT);
                (cCrossMartix + cCrossIdx)->im = (cCrossMartixBuffer[idxO][idxM].im + (1 << (ERROR_SHIFT - 1 + CROSS_SHIFT))) >> (ERROR_SHIFT+CROSS_SHIFT);
                cCrossIdx = cCrossIdx + 1;
            #if DPD_LUNA_DEBUG
                CLOGI("cCrossMartixBufferR[%d][%d].re=%d,im = %d\n",idxO,idxM,cCrossMartixBuffer[idxO][idxM].re,cCrossMartixBuffer[idxO][idxM].im);
            #endif
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

    return ;
}

inline complexint32 ComplexAddInt32(complexint32* cPara0, complexint32* cPara1)
{
    complexint32 cTmp;
    cTmp.re = cPara0->re + cPara1->re;
    cTmp.im = cPara0->im + cPara1->im;
    return cTmp;
}

inline complexint16 ComplexAddInt16(complexint16* cPara0, complexint16* cPara1)
{
    complexint16 cTmp;
    cTmp.re = cPara0->re + cPara1->re;
    cTmp.im = cPara0->im + cPara1->im;
    return cTmp;
}


inline complexint64 ComplexAddInt64(complexint64* cPara0, complexint32* cPara1)
{
    complexint64 cTmp;
    cTmp.re = cPara0->re + cPara1->re;
    cTmp.im = cPara0->im + cPara1->im;
    return cTmp;
}

complexint16 ComplexSub16(complexint16* cPara0, complexint16* cPara1)
{
    complexint16 cTmp;
    cTmp.re = cPara0->re - cPara1->re;
    cTmp.im = cPara0->im - cPara1->im;
    return cTmp;
}

complexint32 ComplexMulti(complexint32* cPara0, complexint32* cPara1, uint16_t iShift)
{
    //complexdata cTmp;
    //cTmp.re = ((cPara0->re * cPara1->re) - (cPara0->im * cPara1->im)) >> iShift;
    //cTmp.im = ((cPara0->re * cPara1->im) + (cPara0->im * cPara1->re)) >> iShift;
    complexint32 cTmp;
    int64_t iMulTmp;
    if (iShift != 0)
    {
        iMulTmp = ((int64_t)((int64_t)((int64_t)cPara0->re * (int64_t)cPara1->re) - (int64_t)((int64_t)cPara0->im * (int64_t)cPara1->im)) + ((int64_t)1 << (iShift - 1))) >> iShift;
        cTmp.re = iMulTmp;
        iMulTmp = ((int64_t)((int64_t)((int64_t)cPara0->re * (int64_t)cPara1->im) + (int64_t)((int64_t)cPara0->im * (int64_t)cPara1->re)) + ((int64_t)1 << (iShift - 1))) >> iShift;
        cTmp.im = iMulTmp;
    }
    else
    {
        iMulTmp = (int64_t)((int64_t)((int64_t)cPara0->re * (int64_t)cPara1->re) - (int64_t)((int64_t)cPara0->im * (int64_t)cPara1->im));
        cTmp.re = iMulTmp;
        iMulTmp = (int64_t)((int64_t)((int64_t)cPara0->re * (int64_t)cPara1->im) + (int64_t)((int64_t)cPara0->im * (int64_t)cPara1->re));
        cTmp.im = iMulTmp;

    }
    return cTmp;
}


complexint16 ComplexMulti16(complexint16* cPara0, complexint16* cPara1, uint16_t iShift)
{
    //complexdata cTmp;
    //cTmp.re = ((cPara0->re * cPara1->re) - (cPara0->im * cPara1->im)) >> iShift;
    //cTmp.im = ((cPara0->re * cPara1->im) + (cPara0->im * cPara1->re)) >> iShift;
    complexint16 cTmp;
    int32_t iMulTmp;
    if (iShift != 0)
    {
        iMulTmp = ((int32_t)((int32_t)((int32_t)cPara0->re * (int32_t)cPara1->re) - (int32_t)((int32_t)cPara0->im * (int32_t)cPara1->im)) + (int32_t)(1 << (iShift - 1))) >> iShift;
        cTmp.re = (int16_t)iMulTmp;
        iMulTmp = ((int32_t)((int32_t)((int32_t)cPara0->re * (int32_t)cPara1->im) + (int32_t)((int32_t)cPara0->im * (int32_t)cPara1->re)) + (int32_t)(1 << (iShift - 1))) >> iShift;
        cTmp.im = (int16_t)iMulTmp;
    }
    else
    {
        iMulTmp = (int32_t)((int32_t)((int32_t)cPara0->re * (int32_t)cPara1->re) - (int32_t)((int32_t)cPara0->im * (int32_t)cPara1->im));
        cTmp.re = (int16_t)iMulTmp;
        iMulTmp = (int32_t)((int32_t)((int32_t)cPara0->re * (int32_t)cPara1->im) + (int32_t)((int32_t)cPara0->im * (int32_t)cPara1->re));
        cTmp.im = (int16_t)iMulTmp;

    }
    return cTmp;
}




complexint32 ComplexMulti16_32(complexint16* cPara0, complexint16* cPara1,uint8_t iShift)
{
    //complexdata cTmp;
    //cTmp.re = ((cPara0->re * cPara1->re) - (cPara0->im * cPara1->im)) >> iShift;
    //cTmp.im = ((cPara0->re * cPara1->im) + (cPara0->im * cPara1->re)) >> iShift;
    complexint32 cTmp;
    cTmp.re = (int32_t)((int32_t)((int32_t)cPara0->re * (int32_t)cPara1->re) - (int32_t)((int32_t)cPara0->im * (int32_t)cPara1->im) + (1 << (iShift - 1))) >> iShift;
    cTmp.im = (int32_t)((int32_t)((int32_t)cPara0->re * (int32_t)cPara1->im) + (int32_t)((int32_t)cPara0->im * (int32_t)cPara1->re) + (1 << (iShift - 1))) >> iShift;
    return cTmp;
}



inline complexint32 ComplexMultiReal(complexint32* cPara0, int32_t cPara1, uint16_t iShift)
{
    //complexdata cTmp;
    //cTmp.re = (cPara0->re * cPara1) >> iShift;
    //cTmp.im = (cPara0->im * cPara1) >> iShift;
    complexint32 cTmp;
    int64_t iMulTmp;
    if (iShift != 0)
    {
        iMulTmp = ((int64_t)((int64_t)cPara0->re * (int64_t)cPara1) + (int64_t)(1 << (iShift - 1))) >> iShift;
        cTmp.re = (int32_t)iMulTmp;
        iMulTmp = ((int64_t)((int64_t)cPara0->im * (int64_t)cPara1) + (int64_t)(1 << (iShift - 1))) >> iShift;
        cTmp.im = (int32_t)iMulTmp;
    }
    else
    {
        iMulTmp = (int64_t)((int64_t)cPara0->re * (int64_t)cPara1);
        cTmp.re = (int32_t)iMulTmp;
        iMulTmp = (int64_t)((int64_t)cPara0->im * (int64_t)cPara1);
        cTmp.im = (int32_t)iMulTmp;
    }
    return cTmp;
}

inline complexint16 ComplexMultiReal16(complexint16* cPara0, int16_t cPara1, uint16_t iShift)
{
    //complexdata cTmp;
    //cTmp.re = (cPara0->re * cPara1) >> iShift;
    //cTmp.im = (cPara0->im * cPara1) >> iShift;
    complexint16 cTmp;
    int32_t iMulTmp;
    if (iShift != 0)
    {
        iMulTmp = ((int32_t)((int32_t)cPara0->re * (int32_t)cPara1) + (int32_t)(1 << (iShift - 1))) >> iShift;
        cTmp.re = (int16_t)iMulTmp;
        iMulTmp = ((int32_t)((int32_t)cPara0->im * (int32_t)cPara1) + (int32_t)(1 << (iShift - 1))) >> iShift;
        cTmp.im = (int16_t)iMulTmp;
    }
    else
    {
        iMulTmp = (int32_t)((int32_t)cPara0->re * (int32_t)cPara1);
        cTmp.re = (int16_t)iMulTmp;
        iMulTmp = (int32_t)((int32_t)cPara0->im * (int32_t)cPara1);
        cTmp.im = (int16_t)iMulTmp;
    }
    return cTmp;
}



inline complexint32 ComplexConj(complexint32* cPara0,int8_t iShift)
{
    complexint32 cTmp;
    if (iShift >= 0)
    {
        cTmp.re = (cPara0->re) >> iShift;
        cTmp.im = (-1 * cPara0->im) >> iShift;
    }
    else
    {
        cTmp.re = (cPara0->re) << -iShift;
        cTmp.im = (-1 * cPara0->im) << -iShift;
    }
    return cTmp;
}


static inline complexint16 ComplexConj16(complexint16* cPara0)
{
    complexint16 cTmp;
    cTmp.re = (cPara0->re);
    cTmp.im = (-1 * cPara0->im);
    return cTmp;
}


uint32_t CalculatePara(complexint32* cCrossMartix, complexint16* cLegcyPara, complexint16* cParaEst, uint8_t iOrder, uint8_t iMem, uint8_t uIterNum, uint16_t uGainOffset)
{
    complexint32 cParaUpdate[MAX_PARALEN] = {0};
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
    for (int idx = 1; idx < MAX_ORDER-1; idx++)
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
            if (PREDSEL[idxM][idxO])
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
                cMultiTmp1 = cInitAutoMatrix[uAddrMap];
                cMultiTmp1 = ComplexConj(&cMultiTmp1, 0);
            }
            else
            {
                uAddrMap = ((idxP0 * idxP0 + idxP0) >> 1) + idxP1;
                cMultiTmp1 = cInitAutoMatrix[uAddrMap];
            }

            cMultiTmp = ComplexMulti(&cMultiTmp1, cCrossMartix + idxP1, 25 - CROSS_SHIFT);
            cParaEstTmp[idxP0] = ComplexAddInt32(&cParaEstTmp[idxP0], &cMultiTmp);

        }
        if (uIterNum < 3)
        {
            cParaEstTmp[idxP0] = ComplexMultiReal(&cParaEstTmp[idxP0], uLamda[0], 11);
        }
        else
        {
            cParaEstTmp[idxP0] = ComplexMultiReal(&cParaEstTmp[idxP0], uLamda[1], 11);
        }

    }
    iParaCounter = 0;
    for (int idxM = 0;idxM < 3; idxM++)
    {
        for (int idxO = 0;idxO < 5; idxO++)
        {
            //int PREDSEL[1][2];
            if (PREDSEL[idxM][idxO])
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
        cParaUpdate[idxP].re = (cParaUpdate[idxP].re + (1 << 4))>> 5;
        cParaUpdate[idxP].im = (cParaUpdate[idxP].im + (1 << 4))>> 5;
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

void CalculateTimeEst(complexint16* cXSignal, complexint16* cYSignal, uint8_t uTimeEstStartTx, uint8_t uTimeEstStartFb, uint16_t uTimeEstWinLen, uint8_t uTimeEstLen, int16_t* iIntDelay, int16_t* iFracDelay)
{
    complexint32 cCrossVal[10] = { 0 };
    uint32_t uCrossAmp[10] = { 0 };
    uint8_t uMaxIdx = 0;
    uint32_t uMaxCrossAmp = 0;
    complexint32 cTrFbTemp;
    complexint16 cConjFbTemp;
    complexint32 cCrossSync, cCrossSyncTmp0, cCrossSyncTmp1;
    int32_t a,b,c;
    uint32_t uCrossSyncAmp;
    int16_t iFracTmp;
    //uint32_t uTxFbGain;

    for(int idx = 0; idx < uTimeEstLen; idx++)
    {
        for (int idxWin = 0; idxWin < uTimeEstWinLen; idxWin++)
        {
            cConjFbTemp = ComplexConj16(&cYSignal[idx + idxWin + uTimeEstStartFb]);
            cTrFbTemp = ComplexMulti16_32(&cXSignal[idxWin + uTimeEstStartTx], &cConjFbTemp, 11);
            cCrossVal[idx] = ComplexAddInt32(&cCrossVal[idx], &cTrFbTemp);
            //cCrossVal
        }
        uCrossAmp[idx] = CalculateAmp(cCrossVal[idx], 0);
        if (uCrossAmp[idx] > uMaxCrossAmp)
        {
            uMaxCrossAmp = uCrossAmp[idx];
            uMaxIdx = idx;
        }
    }

    b = uCrossAmp[uMaxIdx + 1] - uCrossAmp[uMaxIdx - 1];
    a = uCrossAmp[uMaxIdx + 1] - (b >> 1) - uCrossAmp[uMaxIdx];
    iFracTmp = -1*(b << 3) / a;
    if (iFracTmp < 0)
    {
        *iIntDelay = uMaxIdx - uTimeEstStartTx + uTimeEstStartFb - 1;
        *iFracDelay = (int16_t)(1<<5) + iFracTmp;
    }
    else
    {
        *iIntDelay = uMaxIdx - uTimeEstStartTx + uTimeEstStartFb;
        *iFracDelay = iFracTmp;
    }

    //CLOGD("uMaxIdx=%d\n", uMaxIdx);

    //uTxPower = uTxPower >> 11;
    return;
}

void FbCompTime(complexint16* cYSignal, int16_t iFracDelay)
{
    complexint16 cTmp1, cTmp2;
    for (int idx = 0;idx < SEL_LEN - 1; idx++)
    {
        cTmp1 = ComplexMultiReal16(cYSignal + idx, (int16_t)(1<<5) - iFracDelay, 5);
        cTmp2 = ComplexMultiReal16(cYSignal + idx+1, iFracDelay, 5);

        *(cYSignal + idx) = ComplexAddInt16(&cTmp1, &cTmp2);
    }
    return;
}

void CalculateDcEst(complexint16* cYSignal,uint32_t* uPower, complexint16* cDcEstRx)
{
    complexint32 cDcEst = { 0 };
    complexint16 cDcEst16 = { 0 };
    complexint32 sigTmp = { 0 };
    uint32_t uPowerSum = 0;
    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        uPowerSum += ((cYSignal[idx].re * cYSignal[idx].re) + (cYSignal[idx].im * cYSignal[idx].im)) >> 3;
    }
    *uPower = uPowerSum >> 9;
    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        sigTmp.re = (int32_t)(cYSignal + idx)->re;
        sigTmp.im = (int32_t)(cYSignal + idx)->im;
        cDcEst = ComplexAddInt32(&cDcEst, &sigTmp);
    }
    cDcEst.re = (cDcEst.re + (1 << 11)) >> 12;
    cDcEst.im = (cDcEst.im + (1 << 11)) >> 12;
    cDcEst16.re = (int16_t)(cDcEst.re);
    cDcEst16.im = (int16_t)(cDcEst.im);
    cDcEstRx->re = cDcEst.re;
    cDcEstRx->im = cDcEst.im;
    //CLOGD("cDcEst.re=%d im=%d\n", cDcEst.re, cDcEst.im);
    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        *(cYSignal + idx) = ComplexSub16(cYSignal+idx, &cDcEst16);
    }
    return;
}

inline void CalculatePowerEst(complexint16* cYSignal, uint32_t* uPower)
{
    complexint32 cDcEst = { 0 };
    complexint16 cDcEst16 = { 0 };
    complexint32 sigTmp = { 0 };
    uint32_t uPowerSum = 0;

    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        sigTmp.re = (int32_t)(cYSignal + idx)->re;
        sigTmp.im = (int32_t)(cYSignal + idx)->im;
        cDcEst = ComplexAddInt32(&cDcEst, &sigTmp);
    }
    cDcEst.re = (cDcEst.re + (1 << 11)) >> 12;
    cDcEst.im = (cDcEst.im + (1 << 11)) >> 12;
    cDcEst16.re = (int16_t)(cDcEst.re);
    cDcEst16.im = (int16_t)(cDcEst.im);
    //CLOGD("cDcEst.re=%d im=%d\n", cDcEst.re, cDcEst.im);
    for (int idx = 0; idx < SEL_LEN; idx++)
    {
        *(cYSignal + idx) = ComplexSub16(cYSignal+idx, &cDcEst16);
    }

    for (int idx = 0; idx < SEL_LEN; idx++)
    {
        uPowerSum += ((cYSignal[idx].re * cYSignal[idx].re) + (cYSignal[idx].im * cYSignal[idx].im)) >> 3;
    }
    *uPower = uPowerSum >> 9;

    return;
}

void CalculateGainEst(complexint16* cXSignal, complexint16* cYSignal, uint8_t uTimeEstStart, uint16_t uTimeEstLen, complexint16* cGain)
{
    complexint32 cTrFb = { 0 };
    complexint16 cConjXSignal = { 0 };
    complexint32 cTrFbSum = { 0 };
    uint64_t sumGainPower = 0;
    uint64_t sumGainPowerRe = 0;
    uint64_t sumGainPowerIm = 0;
    uint32_t gainPower = 0;
    complexint32 gainTmp = { 512 };
    for (int idx = uTimeEstStart;idx < uTimeEstStart + uTimeEstLen; idx++)
    {
        cConjXSignal = ComplexConj16(cXSignal + idx);
        cTrFb = ComplexMulti16_32(&cConjXSignal, cYSignal + idx, 3);
        cTrFbSum = ComplexAddInt32(&cTrFbSum, &cTrFb);
    }
    sumGainPowerRe = ((uint64_t)((uint64_t)(cTrFbSum.re) * (uint64_t)(cTrFbSum.re)) + (1 << 26)) >> 27;
    sumGainPowerIm = ((uint64_t)((uint64_t)(cTrFbSum.im) * (uint64_t)(cTrFbSum.im)) + (1 << 26)) >> 27;
    sumGainPower = sumGainPowerRe + sumGainPowerIm;
    gainPower = TXPOWER / sumGainPower;
    cTrFbSum.im = cTrFbSum.im * (-1);
    gainTmp.re = (int32_t)(((int64_t)cTrFbSum.re * (int64_t)gainPower) >> 23);
    gainTmp.im = (int32_t)(((int64_t)cTrFbSum.im * (int64_t)gainPower) >> 23);
    cGain->re = (int16_t)gainTmp.re;
    cGain->im = (int16_t)gainTmp.im;
    //(,gainPower)
}


void FbCompGain(complexint16* cYSignal, complexint16 cGain)
{
    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        *(cYSignal + idx) = ComplexMulti16(cYSignal + idx, &cGain,11);
    }
}

void TxCompGain(complexint16* cYSignal, int16_t uGain)
{
	for (int idx = 0;idx < SEL_LEN; idx++)
	{
		*(cYSignal + idx) = ComplexMultiReal16(cYSignal + idx, uGain, 11);
	}
}

void CalculateTxDcEst(complexint16* cTxDc, complexint16* cTxLegacyDc, complexint16* cDcEstRx, complexint16* cGainEst, complexint16* cTxDcEst, uint8_t iter)
{
    complexint16 cTxDcRmRx = { 0 };
    cTxDcRmRx.re = cTxDc->re - cDcEstRx->re;
    cTxDcRmRx.im = cTxDc->im - cDcEstRx->im;
    cTxDcRmRx = ComplexMulti16(&cTxDcRmRx, cGainEst, 10);
    if (iter < 3)
    {
        cTxDcRmRx = ComplexMultiReal16(&cTxDcRmRx, uDcLamda[0], 11);
    }
    else
    {
        cTxDcRmRx = ComplexMultiReal16(&cTxDcRmRx, uDcLamda[1], 11);
    }
    cTxDcEst->re = cTxDcRmRx.re + cTxLegacyDc->re;
    cTxDcEst->im = cTxDcRmRx.im + cTxLegacyDc->im;
}

void DcEst(complexint16* cYSignal, complexint16* cDcEstRx)
{
    complexint32 cDcEst = { 0 };
    complexint16 cDcEst16 = {0 };
    complexint32 sigTmp = { 0 };
    for (int idx = 0;idx < SEL_LEN; idx++)
    {
        sigTmp.re = (int32_t)(cYSignal + idx)->re;
        sigTmp.im = (int32_t)(cYSignal + idx)->im;
        cDcEst = ComplexAddInt32(&cDcEst, &sigTmp);
    }
    cDcEst.re = (cDcEst.re + (1 << 11)) >> 12;
    cDcEst.im = (cDcEst.im + (1 << 11)) >> 12;
    cDcEstRx->re = cDcEst.re;
    cDcEstRx->im = cDcEst.im;
    return;
}

void TxIqEst(complexint16* cYSignal,int16_t iIntdelay, int16_t* iC21, int16_t* iC22)
{
    int64_t iI2Sum = 0;
    int64_t iQ2Sum = 0;
    int64_t iIQ2Sum = 0;
    int32_t g21 = 0;
    int32_t g22 = 2048;
    //int32_t g_fix;
    //int32_t gx2_fix;

    for (int idx = iIntdelay;idx < EST_LEN+1; idx++)
    {
        iI2Sum += (cYSignal + idx)->re * (cYSignal + idx)->re;
        iQ2Sum += (cYSignal + idx)->im * (cYSignal + idx)->im;
        iIQ2Sum += (cYSignal + idx)->re * (cYSignal + idx)->im;
    }
    g21 = (iIQ2Sum<<15) / (iI2Sum);
    g22 = (iQ2Sum<<15)  / (iI2Sum);
    g22 = fix_sqrt(g22 << 15);
    g21 = (g21 + ((g22 * -82) >> 11)) >> 4;
    g22 = (g22 * 2102) >> 15;
    *iC21 = (int16_t) ( - 1 * g21);
    *iC22 = (int16_t)(((int32_t)1 <<22) / g22);
    //gx2_fix = (((uint64_t)iI2Sum << 30) / iQ2Sum); //Q30
    //g_fix = fix_sqrt(gx2_fix); //Q15
    //CLOGW("g_fix=%d g21=%d g22=%d iC21=%d iC22=%d\n", g_fix, g21, g22, *iC21, *iC22);
}
