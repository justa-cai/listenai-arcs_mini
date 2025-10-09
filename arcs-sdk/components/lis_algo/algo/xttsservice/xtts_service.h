#ifndef __XTTSSERVICE_H
#define __XTTSSERVICE_H

#include <stddef.h>
#include <stdint.h>
// #include <stdbool.h>

#include "xtts_stream.h"

/****** XttsService specific error codes *****/
#define XTTSSERVICE_OK                       (0)            ///< 成功
#define XTTSSERVICE_ERROR_GENERAL            (-1)           ///< 一般错误

/****** XttsService Event *****/
///< TTS成功, 其param (session | 字节数)
// #define XTTSSERVICE_EVENT_TTS_COMPLETE       (1UL << 0)

/**
  \brief       服务初始化, 内置核间通信初始化(一次性)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_initialize(void);

/**
  \brief       服务销毁
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_uninitialize(void);

/**
  \brief       用户提供的事件回调
  \param[in]   event            事件
  \param[in]   param            事件参数
  \param[in]   user_data        用户自定义参数
*/
typedef uint32_t (*XttsService_SignalEvent_t) (uint32_t event, uint32_t param, uint32_t user_data);

/**
  \brief       注册事件的监听回调
  \param[in]   cb_event         用户提供的事件回调
  \param[in]   user_data        事件回调的用户自定义参数
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_register(XttsService_SignalEvent_t cb_event, uint32_t user_data);

/**
  \brief       注销事件的监听回调
  \param[in]   cb_event         用户提供的事件回调
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_unregister(XttsService_SignalEvent_t cb_event);

/**
  \brief       获取内部对象引用, close 状态
  \param[in]
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_getXttsStream(XttsStream **out_xtts_stream);

/**
  \brief       启动服务, 在使能调度器之后, task中调用.
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_start(void);

/**
  \brief       停止服务, 停止内置的外设
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsService_stop(void);

int xtts_app_task(void);

void XttsService_remote_sync(void);

#endif /* __XTTSSERVICE_H */
