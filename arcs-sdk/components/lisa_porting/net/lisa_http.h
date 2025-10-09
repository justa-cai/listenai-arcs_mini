#ifndef __LISA_HAL_HTTP__
#define __LISA_HAL_HTTP__

#include "lisa_err.h"
#include "lisa_mem.h"
#include <string.h>

#define URL_MAX_LENGTH (1024)
#define BODY_MAX_LENGTH (1024)
#define HEADER_MAX_LENGTH (1024)

typedef enum {
	LISA_HTTP_OK = 0,
	LISA_HTTP_COMMON_ERR,
	LISA_HTTP_PARAM_ERROR,
} lisa_http_err_e;

typedef enum {
	LISA_HTTP_GET,
	LISA_HTTP_POST,
	LISA_HTTP_PUT,
	LISA_HTTP_PATCH,
	LISA_HTTP_DELETE,
} lisa_http_method_e;

typedef struct {
	const void *buf;
	int32_t len;
	void *user;
} lisa_http_data_t;

typedef struct {
	lisa_http_method_e method;
	uint8_t url[URL_MAX_LENGTH];
	uint8_t body[BODY_MAX_LENGTH];
	void *header_addr;
	int32_t body_len;
	uint32_t timeout;
	void *user;
	void (*inter_on_data)(lisa_http_data_t *data);
} lisa_http_t;

typedef struct {
	lisa_http_method_e method;
	uint8_t *url;
	uint8_t *headers;
	void *body;
	int32_t body_len;
	uint32_t timeout;
	void *user;
	void (*on_data)(lisa_http_data_t *data);
} lisa_http_request_t;

/**
 * @brief 创建http实例
 * 
 * @param req 
 * @return lisa_http_t* 
 */
lisa_http_t *lisa_http_init(lisa_http_request_t *req);

/**
 * @brief 执行http请求
 * 
 * @param ins 
 * @return lisa_http_err_e 
 */
lisa_http_err_e lisa_http_perform(lisa_http_t *ins);

/**
 * @brief download
 * @param  ins              
 * @return lisa_http_err_e 
 */
lisa_http_err_e lisa_http_download(lisa_http_t *ins);

/**
 * @brief 释放http实例
 * 
 * @param ins 
 * @return lisa_http_err_e 
 */
lisa_http_err_e lisa_http_cleanup(lisa_http_t *ins);

/**
 * @brief 以单个chunk的方式回调数据，单个chunk最大支持4096个字节
 *
 * @param ins
 * @return lisa_http_err_e
 */
lisa_http_err_e lisa_http_perform_chunked(lisa_http_t *ins);

/**
 * @brief 以单个chunk的方式回调数据，单个chunk最大支持4096个字节
 *
 * @param ins
 * @param on_chunk 数据回调地址，如果回调接口返回非0,将退出http主动请求
 * @return lisa_http_err_e
 */
lisa_http_err_e lisa_http_perform_chunked_with_cb(lisa_http_t *ins,
						  int (*on_chunk)(lisa_http_data_t *));

#endif  //__LISA_HAL_HTTP__
