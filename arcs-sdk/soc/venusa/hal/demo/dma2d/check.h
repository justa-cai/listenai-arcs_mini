#ifndef __JPEG_CHECK_H__
#define __JPEG_CHECK_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
// #include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SUCCESS     0
#define FAILURE     1

typedef enum {
    FALSE = 0,
    TRUE  = 1,
} bool_t;

#define CHECK_POINT_NOT_NULL(point)\
do{\
    if (NULL == (point))\
    {\
        VIDEO_LOG("point %s is NULL at %s: LINE: %d", #point, __FUNCTION__, __LINE__);\
        return FAILURE;\
    }\
}while(0)


#define CHECK_POINT_NOT_NULL_EXIT(point, errExit)\
do{\
    if (NULL == (point))\
    {\
        VIDEO_LOG("point %s is NULL at %s: LINE: %d", #point, __FUNCTION__, __LINE__);\
        goto errExit;\
    }\
}while(0)


#define CHECK_RET(express)\
do{\
    if (SUCCESS != (express))\
    {\
        VIDEO_LOG("failed at %s: LINE: %d with %#x!", __FUNCTION__, __LINE__, (express));\
        return (express);\
    }\
}while(0)


#define CHECK_RET_EQ(Ret, express)\
do{\
    if ((express) != (Ret))\
    {\
        VIDEO_LOG("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
        return (Ret);\
    }\
}while(0)


#define CHECK_RET_EQ_EXIT(Ret, express, errExit)\
do{\
    if ((express) != (Ret))\
    {\
        VIDEO_LOG("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
        goto errExit;\
    }\
}while(0)


#define TEST_TRACE(fmt...)   \
do {\
    VIDEO_LOG("[%s]-%d: ", __FUNCTION__, __LINE__);\
    VIDEO_LOG((char*)fmt);\
}while(0)


#define CHECK_FUNC_EXIT(func, errExit)\
do{\
    ret = func;\
    if (SUCCESS != ret)\
    {\
        VIDEO_LOG("failed at %s: LINE: %d with func!", __FUNCTION__, __LINE__);\
        goto errExit;\
    }\
}while(0)


#define CHECK_EQ_TIMEOUT_EXIT(value0, value1, timeout, errExit)\
do{\
    if ((value0) != (value1))\
    {\
        break;\
    }\
    DELAY_US(1); \
    if(timeout-- == 0) \
    { \
        VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__); \
        ret = FAILURE; \
        goto errExit; \
    } \
}while(1)


#define DVP_IMAGE_WIDTH   320
#define DVP_IMAGE_HEIGHT  240
#define DVP_MCLK_OUT      12000000

#ifdef __cplusplus
}
#endif

#endif /* __JPEG_CHECK_H__ */
