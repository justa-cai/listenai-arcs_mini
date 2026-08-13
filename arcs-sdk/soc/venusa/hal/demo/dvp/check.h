#ifndef __CHECK_H__
#define __CHECK_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SUCCESS
#define SUCCESS     0
#endif

#define FAILURE     1

typedef enum {
    FALSE = 0,
    TRUE  = 1,
} bool_t;

#define CHECK_POINT_NOT_NULL(point)\
do{\
    if (NULL == (point))\
    {\
        CLOG("point %s is NULL at %s: LINE: %d", #point, __FUNCTION__, __LINE__);\
        return FAILURE;\
    }\
}while(0)


#define CHECK_POINT_NOT_NULL_EXIT(point, errExit)\
do{\
    if (NULL == (point))\
    {\
        CLOG("point %s is NULL at %s: LINE: %d", #point, __FUNCTION__, __LINE__);\
        goto errExit;\
    }\
}while(0)


#define CHECK_RET(express)\
do{\
    if (SUCCESS != (express))\
    {\
        CLOG("failed at %s: LINE: %d with %#x!", __FUNCTION__, __LINE__, (express));\
        return (express);\
    }\
}while(0)


#define CHECK_RET_EQ(Ret, express)\
do{\
    if ((express) != (Ret))\
    {\
        CLOG("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
        return (Ret);\
    }\
}while(0)


#define CHECK_RET_EQ_EXIT(Ret, express, errExit)\
do{\
    if ((express) != (Ret))\
    {\
        CLOG("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
        goto errExit;\
    }\
}while(0)


#define TEST_TRACE(fmt...)   \
do {\
    CLOG("[%s]-%d: ", __FUNCTION__, __LINE__);\
    CLOG((char*)fmt);\
}while(0)


#define CHECK_FUNC_EXIT(func, errExit)\
do{\
    ret = func;\
    if (SUCCESS != ret)\
    {\
        CLOG("failed at %s: LINE: %d with func!", __FUNCTION__, __LINE__);\
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
        CLOG("[%s:%d] wait timeout", __func__, __LINE__); \
        ret = FAILURE; \
        goto errExit; \
    } \
}while(1)


#ifdef __cplusplus
}
#endif

#endif /* __CHECK_H__ */
