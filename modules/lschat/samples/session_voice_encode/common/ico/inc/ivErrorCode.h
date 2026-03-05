/*----------------------------------------------+
|												|
|	ivErrorCode.h - Basic Definitions		|
|												|
|		Copyright (c) 1999-2012, iFLYTEK Ltd.	|
|		All rights reserved.					|
|												|
+----------------------------------------------*/

#ifndef IFLYTEK_VOICE__2012_02_27_ERRORCODE__H
#define IFLYTEK_VOICE__2012_02_27_ERRORCODE__H

#include "ivDefine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ivInt32 ivErrID;
typedef ivInt32 ivStatus;
typedef ivInt32 MAEErrID;
typedef ivInt32 ivMAEStatus;

/* General */
#define ivErr_InvArg      ((ivStatus)2)
#define ivErr_Failed      ((ivStatus)3)
#define ivErr_NotSupport  ((ivStatus)4)
#define ivErr_OutOfMemory ((ivStatus)5)

/* For object status */
#define ivErr_InvCall     ((ivStatus)6)
#define ivErr_BufferEmpty ((ivStatus)7)
#define ivErr_BufferFull  ((ivStatus)8)
#define ivErr_Done        ((ivStatus)9) /* ���ݴ������� */

#ifdef __cplusplus
}
#endif

#define ivErr_OK    ((ivStatus)0)
#define ivErr_FALSE ((ivStatus)1)

#define ivSucceeded(hr) ((ivUInt32)(hr) <= 1)

#endif /* !IFLYTEK_VOICE__2008_10_13_ERRORCODE__H */
