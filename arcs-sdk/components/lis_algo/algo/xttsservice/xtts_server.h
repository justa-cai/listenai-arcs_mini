#ifndef __XTTSSERVER_H
#define __XTTSSERVER_H

#include <stddef.h>
#include <stdint.h>
// #include <stdbool.h>

#include "xtts_stream.h"

/****** XttsServer specific error codes *****/
#define XTTSSERVER_OK                       (0)            ///< 成功
#define XTTSSERVER_ERROR_GENERAL            (-1)           ///< 一般错误

/****** XttsServer Event *****/
///< TTS成功, 其param (session | 字节数)
// #define XTTSSERVICE_EVENT_TTS_COMPLETE       (1UL << 0)

/**
  \brief       初始化
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_initialize(XttsStream * xttsStream);

/**
  \brief       销毁
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_uninitialize(void);

/**
  \brief       用户提供的事件回调
  \param[in]   event            事件
  \param[in]   param            事件参数
  \param[in]   user_data        用户自定义参数
*/
typedef uint32_t (*XttsServer_SignalEvent_t) (uint32_t event, uint32_t param, uint32_t user_data);

/**
  \brief       注册 事件的监听回调
  \param[in]   cb_event         用户提供的事件回调
  \param[in]   user_data        事件回调的用户自定义参数
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_register(XttsServer_SignalEvent_t cb_event, uint32_t user_data);

/**
  \brief       注销扫描事件的监听回调
  \param[in]   cb_event         用户提供的事件回调
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_unregister(XttsServer_SignalEvent_t cb_event);

/**
  \brief       启动, 在使能调度器之后, task中调用.
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_start(void);

/**
  \brief       停止服务, 停止内置的外设
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_stop(void);

/**
  \brief       暂时挂起服务, 内置的外设挂起
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_suspend(void);

/**
  \brief       继续一个已经挂起的服务, 快速继续外设
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
extern int32_t XttsServer_resume(void);

#endif /* __XTTSSERVER_H */
