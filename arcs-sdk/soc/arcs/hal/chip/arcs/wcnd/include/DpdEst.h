#ifndef __DPD_EST_H__
#define __DPD_EST_H__

#include <stdint.h>

#define SEL_LEN  4096
#define SEL_SHIFT log2(SEL_LEN)
#define EST_LEN  4000
#define PREDMODEL 3
/*dig_gain=64*/
//#define TXPOWER 12507495984
/*dig_gain=74*/
#define TXPOWER 16723984272
#if PREDMODEL == 0
#define PREDLEN 2
#define MAX_ORDER 3
#define MAX_MEMORY 1
#elif PREDMODEL == 1
#define PREDLEN 3
#define MAX_ORDER 5
#define MAX_MEMORY 1
#elif PREDMODEL == 2
#define PREDLEN 6
#define MAX_ORDER 5
#define MAX_MEMORY 2
#else
#define PREDLEN 9
#define MAX_ORDER 5
#define MAX_MEMORY 3
#endif

#define AUTO_MATRIX  PREDLEN*PREDLEN
#define MAX_PARALEN  15
#define ERROR_SHIFT 3
#define CROSS_SHIFT 8

#define PRINTDATA 0
#ifndef abs
#define abs(x)   ((x)>=0?(x):-(x))
#endif

extern const int32_t uLamda[2];
extern const int32_t uDcLamda[2];

#if 0
typedef long long int  int64_t;
typedef long long unsigned int  uint64_t;
typedef int int32_t;
typedef unsigned int  uint32_t;
typedef short int16_t;
typedef unsigned short  uint16_t;
typedef signed char int8_t;
typedef unsigned char  uint8_t;
#endif
typedef struct complexint32
{
    int32_t re;
    int32_t im;
}complexint32;

typedef struct complexint16
{
    int16_t re;
    int16_t im;
}complexint16;

typedef struct complexint64
{
    int64_t re;
    int64_t im;
}complexint64;

int16_t CalculateDpdPara(complexint16* cXSignal, complexint16* cYSignal, uint8_t iOrder, uint8_t iMem, complexint16* cLegcyPara, complexint16* cParaEst, uint16_t iCalLen, uint8_t iIterNum, uint16_t uGainOffset);
uint32_t CalculateAmp(complexint32 cSignal, uint8_t iShift);
uint16_t CalculateAmp16(complexint16 cSignal, uint8_t iShift);
void CalculateCrossMatrix(complexint16* cXSignal, complexint16* cYSignal, uint8_t iOrder, uint8_t iMem, uint16_t iCalLen, uint16_t iStartIdx, complexint32* cCrossMartixBuffer);
complexint16 ComplexAddInt16(complexint16* cPara0, complexint16* cPara1);
complexint32 ComplexAddInt32(complexint32* cPara0, complexint32* cPara1);
complexint64 ComplexAddInt64(complexint64* cPara0, complexint32* cPara1);
complexint16 ComplexSub16(complexint16* cPara0, complexint16* cPara1);
complexint32 ComplexMulti(complexint32* cPara0, complexint32* cPara1, uint16_t iShift);
complexint32 ComplexMultiReal(complexint32* cPara0, int32_t cPara1, uint16_t iShift);
complexint16 ComplexMultiReal16(complexint16* cPara0, int16_t cPara1, uint16_t iShift);
complexint32 ComplexConj(complexint32* cPara0, int8_t iShift);
uint32_t CalculatePara(complexint32* cCrossMartix, complexint16* cLegcyPara, complexint16* cParaEst, uint8_t iOrder, uint8_t iMem, uint8_t uIterNum, uint16_t uGainOffset);
void CalculateTimeEst(complexint16* cXSignal, complexint16* cYSignal, uint8_t uTimeEstStartTx, uint8_t uTimeEstStartFb, uint16_t uTimeEstWinLen, uint8_t uTimeEstLen, int16_t* iIntDelay, int16_t* iFracDelay);
void CalculateGainEst(complexint16* cXSignal, complexint16* cYSignal, uint8_t uTimeEstStart, uint16_t uTimeEstLen, complexint16* cGain);
void FbCompTime(complexint16* cYSignal, int16_t iFracDelay);
void CalculateDcEst(complexint16* cYSignal, complexint16* cDcEstRx);
void CalculatePowerEst(complexint16* cYSignal, uint32_t* uPower);
void CalculatePowerWithDC(complexint16* cYSignal, uint32_t* uPower);
void FbCompGain(complexint16* cYSignal, complexint16 cGain);
void TxCompGain(complexint16* cYSignal, int16_t uGain);
void CalculateTxDcEst(complexint16* cTxDc, complexint16* cTxLegacyDc, complexint16* cDcEstRx, complexint16* cGainEst, complexint16* cTxDcEst, uint8_t iter);
void DcEst(complexint16* cYSignal, complexint16* cDcEstRx);
void TxIqEst(complexint16* cYSignal,int16_t iIntdelay, int16_t* iC21, int16_t* iC22);
//uint16_t fix_sqrt(uint32_t x);

#endif
