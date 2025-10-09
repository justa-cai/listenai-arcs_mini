#ifndef __DPD_EST_LUNA_H__
#define __DPD_EST_LUNA_H__

#include "DpdEst.h"
#include "luna_basic_math.h"
#include "luna_complex_math.h"

void DcEstLuna(int32_t* in_re, int32_t* in_im, int32_t* dc_re, int32_t* dc_im);
void LunaVectorSumI32O32(int32_t* in, int32_t* sum_value, int16_t size, int16_t shift);
void SumComplex32Luna(int32_t* in_re, int32_t* in_im, int32_t* sum_value_re, int32_t* sum_value_im, int16_t size, int16_t shift);
void LunaDotProdI32O32(int32_t* in1, int32_t* in2, int32_t* dot_value, int16_t size, int16_t shift);
void LunaDotProdI32O64(int32_t* in1, int32_t* in2, int64_t* dot_value, int16_t size, int16_t shift);
void CalculateDcEstLuna(int32_t* in_re, int32_t* in_im, uint32_t* uPower, complexint32* cDcEstRx);
void DotComplex32Luna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, int32_t* o_re, int32_t* o_im, int8_t y_conj_en, int16_t size, int16_t shift);
void CalculateTimeEstLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t uTimeEstStartTx, uint8_t uTimeEstStartFb, uint16_t uTimeEstWinLen, uint8_t uTimeEstLen, int16_t* iIntDelay, int16_t* iFracDelay);
void FbCompTimeLuna(int32_t* x_re, int32_t* x_im, int16_t iFracDelay);
void CalculateGainEstLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t uTimeEstStart, uint16_t uTimeEstLen, complexint16* cGain);
void FbCompGainLuna(int32_t* x_re, int32_t* x_im, complexint16 cGain);
void TxIqEstLuna(int32_t* x_re, int32_t* x_im, int16_t iIntdelay, int16_t* iC21, int16_t* iC22);
int16_t CalculateDpdParaLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t iOrder, uint8_t iMem, complexint16* cLegcyPara, complexint16* cParaEst, uint16_t iCalLen, uint8_t iIterNum, uint16_t uGainOffset);
uint32_t CalculateParaLuna(complexint32* cCrossMartix, complexint16* cLegcyPara, complexint16* cParaEst, uint8_t iOrder, uint8_t iMem, uint8_t uIterNum, uint16_t uGainOffset);
void CalculateCrossMatrixLuna(int32_t* x_re, int32_t* x_im, int32_t* y_re, int32_t* y_im, uint8_t iOrder, uint8_t iMem, uint16_t iCalLen, uint16_t iStartIdx, complexint32* cCrossMartix);
#endif
