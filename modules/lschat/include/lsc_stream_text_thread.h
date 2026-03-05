#ifndef __LSC_STREAM_TEXT_THREAD_H__
#define __LSC_STREAM_TEXT_THREAD_H__

#include "lsc_stream_text.h"

/**
 * @addtogroup lsc_stream_text_request_thread 在专用的线程中进行流式文本数据请求
 * @{
 */

/**
 * @brief 初始化线程
 *
 * @return int
 */
int lsc_stream_text_request_thread_init(void);

/**
 * @brief 在专用的线程中进行文本请求
 *
 * @param url 文本地址
 * @param cb 回调函数
 * @param user 用户参数
 * @param abort_all 是否需要打断目前已经存在的请求
 * @return int 0表示成功，非0表示失败
 */
int lsc_stream_text_request_thread_async(const char *url, sse_evt_cb_t cb, void *user, bool abort_all);

/**
 * @brief 打断目前已经存在的请求
 *
 */
void lsc_stream_text_request_thread_abort_all();

/**
 * @brief 反初始化线程
 *
 */
void lsc_stream_text_request_thread_deinit();

/**
 * @}
 */
#endif
