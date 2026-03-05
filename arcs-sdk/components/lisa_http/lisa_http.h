/**
 * @file lisa_http.h
 * @brief LISA HTTP 客户端 API
 *
 * 此文件提供 LISA HTTP 客户端接口，支持多种 HTTP 方法（GET、POST、PUT、PATCH、DELETE），
 * 提供同步和异步数据传输、分块传输、文件下载等功能，为 ARCS 平台提供完整的 HTTP 通信能力。
 */

#ifndef __LISA_HAL_HTTP__
#define __LISA_HAL_HTTP__

#include <stdint.h>

/**
 * @brief HTTP 相关常量定义
 *
 * 定义了 HTTP 组件中使用的最大长度限制常量。
 */
#define URL_MAX_LENGTH    (1024)   /**< URL 最大长度 */
#define BODY_MAX_LENGTH   (1024)   /**< 请求体最大长度 */
#define HEADER_MAX_LENGTH (1024)   /**< HTTP 头部最大长度 */

/**
 * @brief HTTP 操作错误代码
 *
 * HTTP 操作可能的返回值枚举，用于表示操作结果状态。
 */
typedef enum {
	LISA_HTTP_OK = 0,         /**< 操作成功 */
	LISA_HTTP_COMMON_ERR,     /**< 一般错误 */
	LISA_HTTP_PARAM_ERROR,    /**< 参数错误 */
	LISA_HTTP_NET_ERR = 7,    /**< 网络错误 */
} lisa_http_err_e;

/**
 * @brief HTTP 请求方法
 *
 * 支持的 HTTP 请求方法枚举。
 */
typedef enum {
	LISA_HTTP_GET,     /**< HTTP GET 方法，用于获取资源 */
	LISA_HTTP_POST,    /**< HTTP POST 方法，用于提交数据 */
	LISA_HTTP_PUT,     /**< HTTP PUT 方法，用于更新资源 */
	LISA_HTTP_PATCH,   /**< HTTP PATCH 方法，用于部分更新资源 */
	LISA_HTTP_DELETE,  /**< HTTP DELETE 方法，用于删除资源 */
} lisa_http_method_e;

/**
 * @brief HTTP 数据结构
 *
 * 用于传输 HTTP 数据的结构体，包含数据缓冲区、长度和用户信息。
 */
typedef struct {
	const void *buf;   /**< 数据缓冲区指针 */
	int32_t len;       /**< 数据长度（字节） */
	void *user;        /**< 用户自定义数据指针 */
} lisa_http_data_t;

/**
 * @brief HTTP 实例结构
 *
 * HTTP 请求实例的内部结构，包含请求方法、URL、请求体、回调函数等信息。
 */
typedef struct {
	lisa_http_method_e method;          /**< HTTP 请求方法 */
	uint8_t url[URL_MAX_LENGTH];       /**< 请求 URL 字符串 */
	uint8_t body[BODY_MAX_LENGTH];     /**< 请求体数据 */
	void *header_addr;                  /**< HTTP 头部地址 */
	int32_t body_len;                   /**< 请求体长度 */
	uint32_t timeout;                   /**< 请求超时时间（毫秒） */
	void *user;                         /**< 用户自定义数据 */
	void (*inter_on_data)(lisa_http_data_t *data); /**< 内部数据回调函数 */
} lisa_http_t;

/**
 * @brief HTTP 请求配置结构
 *
 * HTTP 请求的配置参数，包含请求方法、URL、头部、请求体等信息。
 */
typedef struct {
	lisa_http_method_e method;  /**< HTTP 请求方法 */
	uint8_t *url;               /**< 请求 URL 字符串指针 */
	uint8_t *headers;           /**< HTTP 头部字符串指针 */
	void *body;                 /**< 请求体数据指针 */
	int32_t body_len;           /**< 请求体长度 */
	uint32_t timeout;           /**< 请求超时时间（毫秒） */
	void *user;                 /**< 用户自定义数据 */
	void (*on_data)(lisa_http_data_t *data); /**< 数据接收回调函数 */
} lisa_http_request_t;

/**
 * @brief 创建 HTTP 实例
 *
 * 根据提供的请求配置创建一个新的 HTTP 实例。
 * 实例创建后可以用于执行 HTTP 请求。
 *
 * @param req HTTP 请求配置结构体指针
 * @return HTTP 实例指针，失败时返回 NULL
 */
lisa_http_t *lisa_http_init(lisa_http_request_t *req);

/**
 * @brief 执行 HTTP 请求
 *
 * 执行同步 HTTP 请求，等待请求完成后返回。
 * 适用于需要等待完整响应的场景。
 *
 * @param ins HTTP 实例指针
 * @return 操作结果：LISA_HTTP_OK 表示成功，其他值表示对应错误
 */
lisa_http_err_e lisa_http_perform(lisa_http_t *ins);

/**
 * @brief 下载文件
 *
 * 执行 HTTP 下载请求，专门用于下载文件资源。
 * 支持大文件下载，通过回调函数返回下载的数据块。
 *
 * @param ins HTTP 实例指针
 * @return 操作结果：LISA_HTTP_OK 表示成功，其他值表示对应错误
 */
lisa_http_err_e lisa_http_download(lisa_http_t *ins);

/**
 * @brief 释放 HTTP 实例
 *
 * 清理 HTTP 实例占用的资源，包括内存、网络连接等。
 * 实例释放后不能再使用。
 *
 * @param ins HTTP 实例指针
 * @return 操作结果：LISA_HTTP_OK 表示成功，其他值表示对应错误
 */
lisa_http_err_e lisa_http_cleanup(lisa_http_t *ins);

/**
 * @brief 执行分块传输 HTTP 请求
 *
 * 以分块（chunked）方式执行 HTTP 请求，单个数据块最大支持 4096 字节。
 * 适用于大文件传输或流式数据处理，数据通过内部回调函数返回。
 *
 * @param ins HTTP 实例指针
 * @return 操作结果：LISA_HTTP_OK 表示成功，其他值表示对应错误
 */
lisa_http_err_e lisa_http_perform_chunked(lisa_http_t *ins);

/**
 * @brief 执行带自定义回调的分块传输 HTTP 请求
 *
 * 以分块（chunked）方式执行 HTTP 请求，使用用户提供的回调函数处理每个数据块。
 * 单个数据块最大支持 4096 字节。
 *
 * @param ins HTTP 实例指针
 * @param on_chunk 数据块回调函数，如果回调函数返回非 0，将主动退出 HTTP 请求
 * @return 操作结果：LISA_HTTP_OK 表示成功，其他值表示对应错误
 */
lisa_http_err_e lisa_http_perform_chunked_with_cb(lisa_http_t *ins,
						  int (*on_chunk)(lisa_http_data_t *));

#endif //__LISA_HAL_HTTP__
