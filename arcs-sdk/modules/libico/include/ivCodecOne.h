#if !defined(SE_TEAM__AP__2015_04_15__H)
#define SE_TEAM__AP__2015_04_15__H

#include "ivErrorCode.h"
#include "ivDebug.h"

#define ICO_ENCODER		(0)
#define ICO_DECODER		(1)


typedef struct  tagICOInitParam{
	ivPointer   pBuffer;
	ivInt32		nBufferSize;
	ivInt32		nBitRate;
	ivInt16     nBandWidth;
	ivInt16		nCoderFlag;
	ivPointer   pReserved;
}TICOInitParam,ivPtr PICOInitParam;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	/* Create CAE object */
	ivStatus									/* Returned Error Info */
		ivCall ICOCreate(
		ivHandle ivPtr		hICOObj,			/*[In/Out] To Receive the ICO object handle */
		PICOInitParam		pInitParam			/*[In] Input parameter struct */
		);

	/* Start encoder */
	ivStatus									/* Returned Error Info */
		ivCall ICOEncoder(
		ivHandle		hICOObj,				/* The ICO object handle */
		ivPointer		pInData,
		ivInt16			nInSize,
		ivPointer		pOutData,
		ivPInt16		pOutSize
		);

	ivStatus									/* Returned Error Info */
		ivCall ICODecoder(
		ivHandle		hICOObj,				/* The ICO object handle */
		ivPointer		pInData,
		ivInt16			nInSize,
		ivPointer		pOutData,
		ivPInt16		pOutSize
		);


	ivStatus 
		ivCall ICOReset(ivHandle hICOObj);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
