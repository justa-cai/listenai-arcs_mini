#ifndef __LSC_STREAM_TEXT_H__
#define __LSC_STREAM_TEXT_H__

#include "lisa_semaphore.h"

/**
 * @addtogroup lsc_stream_text 流式文本数据请求
 * @{
 */

typedef enum {
	SSE_EVT_DATA,  /**< 流式数据 */
	SSE_EVT_DONE,  /**< 流式数据接收完毕 */
	SSE_EVT_ABORT, /**< 流式数据接收过程中被中断 */
} sse_evt_e;

/**
 * @brief 流式数据回调
 *
 * @param evt 流式数据事件类型 @see sse_evt_e
 * @param data 流式数据事件的具体数据，当事件为SSE_EVT_DONE时，该参数为NULL
 * @param user 用户参数
 */
typedef void (*sse_evt_cb_t)(int evt, const char *data, void *user);

struct lsc_stream_text_request_ctx {
	char *url;                  /**< 流式数据请求的url地址 */
	sse_evt_cb_t cb;            /**< 流式数据回调的地址，@see sse_evt_cb_t */
	void *user;                 /**< 用户参数 */
	lisa_semaphore_t *exit_sem; /**< 用于打断流式数据请求，该参数为NULL时，表示不需要打断 */
};

/**
 * @brief 创建一个流式数据请求会话
 *
 * @param url 请求的地址
 * @param cb 数据回调的地址
 * @param user 用户参数
 * @return struct lsc_stream_text_request_ctx
 */
struct lsc_stream_text_request_ctx *lsc_stream_text_request_new(const char *url, sse_evt_cb_t cb, void *user);

/**
 * @brief 开始一次流式文本数据的请求
 *
 * @param ctx 会话实体
 * @param timeout 超时时间，单位：秒
 * @return int 0表示成功，非0表示失败
 */
int lsc_stream_text_request_start(struct lsc_stream_text_request_ctx *ctx, uint32_t timeout);

/**
 * @brief 用于主动打断流式数据请求过程
 *
 * @param ctx 会话实体
 * @note 需要使用打断机制的话，必须初始化lsc_stream_text_request_ctx中的exit_sem
 *
 * @return int 0表示成功，非0表示失败
 */
int lsc_stream_text_request_abort(struct lsc_stream_text_request_ctx *ctx);

/**
 * @brief 刪除会话
 *
 * @param ctx
 * @return int
 */
void lsc_stream_text_request_delete(struct lsc_stream_text_request_ctx *ctx);

/**
 * @}
 */

#endif
