#ifndef __LIS_TRANS_H__
#define __LIS_TRANS_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus/*需要被.c文件使用的函数声明*/
#include "lis_algo.h"

// ret:小于0，超时，大于0，代表结果的长度
int lis_trans_wait_result(char *result, TickType_t xTicksToWait);

/*******************************************/
typedef enum
{
    LIS_TRANS_AUTO = 0,
    LIS_TRANS_CN2EN,
    LIS_TRANS_EN2CN
} lis_trans_type;

typedef enum
{
    LIS_TRANS_STATE_ING = 0,    //进行中
    LIS_TRANS_STATE_ERR,        //内存出错
    LIS_TRANS_STATE_RESULT,     //有翻译结果，可能有多次结果，如按句输出译文等
    LIS_TRANS_STATE_OVER,       //正常结束
    LIS_TRANS_STATE_EARLY_OVER, //提前结束，如调用了lis_trans_stop
} lis_trans_status;

//
void lis_trans_init(void);
//
void lis_trans_deinit(void);
//启动翻译,支持的文本长度至少512字节
lis_err_t lis_trans_start(char *txt, uint32_t txt_size, lis_trans_type type);

//查询翻译状态，在lis_trans_start后会轮询调用该函数获取状态
lis_trans_status lis_trans_get_status();

//当lis_trans_get_status获取到TRANS_STATE_RESULT后调用该接口获取译文
char *lis_trans_get_result();

//强制结束，阻塞式
lis_err_t lis_trans_stop();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __LIS_TRANS_H__
