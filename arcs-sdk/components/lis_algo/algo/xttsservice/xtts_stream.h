#ifndef __XTTSSTREAM_H
#define __XTTSSTREAM_H

#include <stddef.h>
#include <stdint.h>
// #include <stdbool.h>
#include "ic_message.h"
#include "lis_algo.h"

#define DEF_TASK_STACK 1024

/****** XttsStream specific error codes *****/
#define XTTSSTREAM_OK                       (0)            ///< 成功
#define XTTSSTREAM_ERROR                    (-1)           ///< 一般错误
// TODO: 任何连接异常都应该引起错误

/****** XttsStream Event *****/
#define XTTSSTREAM_EVENT_CONSUMER_OPEN       (1UL << 0)     ///< consumer端打开
#define XTTSSTREAM_EVENT_CONSUMER_CLOSE       (1UL << 0)     ///< consumer端关闭

/****** XttsStream Frame Type *****/
enum {
    XTTSSTREAM_FRAMETYPE_ERROR = 0,
    XTTSSTREAM_FRAMETYPE_BEGIN_OF_STREAM,
    XTTSSTREAM_FRAMETYPE_NORMAL,
    XTTSSTREAM_FRAMETYPE_END_OF_STREAM,
};

typedef struct _tag_XttsStream {
    int dummy;
} XttsStream;

//------------------------------------------------
/**
  \brief       初始化, 传入已分配的内存
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern XttsStream * XttsStream_ctor(void *self_mem);

/**
  \brief       销毁
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsStream_dtor(XttsStream *self);

/**
  \brief       用户提供的事件回调
  \param[in]   event            事件
  \param[in]   param            事件参数
  \param[in]   user_data        用户自定义参数
*/
typedef uint32_t (*XttsStream_SignalEvent_t) (uint32_t event, uint32_t param, uint32_t user_data);

extern int32_t XttsStream_register(XttsStream_SignalEvent_t cb_event, uint32_t user_data);

/**
  \brief       打开流
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsStream_open(XttsStream *self);

/**
  \brief       关闭流
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsStream_close(XttsStream *self);

// 阻塞式接口, 非copy, 用完归还 空frame.
// TODO: 接口待改进. 结尾帧时, 不是有效数据帧, 无视其中数据.
extern int32_t XttsStream_Consumer_acquireFrame(XttsStream *self, void **out_frame, int *out_frameType);

// 阻塞式接口, 用完归还 frame.
extern int32_t XttsStream_Consumer_releaseFrame(XttsStream *self, void *frame);

extern int32_t XttsStream_Producer_acquireFrame(XttsStream *self, void **out_frame, int frameType);

extern int32_t XttsStream_Producer_releaseFrame(XttsStream *self, void *frame, int frameType);

#endif /* __XTTSSTREAM_H */
