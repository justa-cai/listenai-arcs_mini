/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @addtogroup session_objrec 图像识别会话
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 物体识别会话
 */
typedef void *session_objrec_t;

#define SESSION_OBJREC_URL_MAX_SIZE (128)

/**
 * @brief 物体识别结果
 * @details 目前版本中，云端只下发识别结果的url
 * 	        需要通过http请求获取真实的识别结果
 */
struct session_objrec_result {
	char text_url[SESSION_OBJREC_URL_MAX_SIZE]; /**< 存放流式文本的URL, 需要通过HTTP进行获取具体的文本结果 */
	char tts_url[SESSION_OBJREC_URL_MAX_SIZE];  /**< 存放TTS音频的URL */
};

typedef enum {
	SESSION_OBJREC_EVT_TEXT_URL, /**< 收到识别结果文本的URL事件 */
	SESSION_OBJREC_EVT_TTS_URL,  /**< 收到识别结果TTS的URL事件 */
} session_objrec_event_t;

/**
 * @brief 创建物体识别会话
 *
 * @return session_objrec_t NULL表示创建失败，非NULL表示成功
 */
session_objrec_t session_objrec_new();

/**
 * @brief 删除物体识别会话
 * @param s 物体识别会话实例
 *
 */
void session_objrec_delete(session_objrec_t s);

/**
 * @brief 图像识别回调函数
 *
 * @param s 物体识别会话实例
 * @param evt 回调事件类型
 * @param data 回调数据地址
 * @param data_len 回调数据长度
 * @param user 用户数据
 */
typedef void (*session_objrec_evt_cb_t)(session_objrec_t s, int evt, void *data, uint32_t data_len, void *user);

/**
 * @brief 同步的方式进行一次图像识别
 * @details 同步的方式进行一次图像识别，并等待识别结果下发
 * @param s 物体识别会话实例
 * @param jpg_img JPG图像的地址
 * @param size JPG图像的大小
 * @param result 图像识别结果将存放在此结构体中@see struct session_objrec_result
 * @param timeout 超时时间，单位ms
 * @return int 0表示成功，非0表示失败, 错误码见@see lsc_err_e
 */
int session_objrec_run(session_objrec_t s, const void *jpg_img, uint32_t size, struct session_objrec_result *result,
		       int timeout);

/**
 * @brief 异步方式进行一次图像识别
 * @details 异步方式进行一次图像识别，识别结果通过回调函数的形式进行报告
 * @note 切勿在回调函数中进行耗时的操作，应该通过线程间通信的方式交给其他线程处理
 * @param s 物体识别会话实例
 * @param jpg_img JPG图像的地址
 * @param size JPG图像的大小
 * @param cb 事件回调地址，@see session_objrec_evt_cb_t
 * @param user 用户数据
 * @return int 0表示成功，非0表示失败, 错误码见@see lsc_err_e
 */
int session_objrec_run_async(session_objrec_t s, const void *jpg_img, uint32_t size, session_objrec_evt_cb_t cb,
			     void *user);

#ifdef __cplusplus
}

#endif

/**
 * @}
 */
