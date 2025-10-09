/**
 * @brief   算法相关处理
 * @version 0.1
 * @date    2022-11-11
 * 
 * Copyright (C) 2022 ANHUI LISTENAI Co., LTD All Rights Reserved
 */

#ifndef __LISTENAI_ALGO_PROC_H__
#define __LISTENAI_ALGO_PROC_H__

#include "stdint.h"
#include "stdbool.h"

/**
 * 从算法的Param1中解析出 keyword 和 kid
 * @param in 	            算法JSON串
 * @param out	            解析后的字符串
 * @param max_len	        解析后字符串最大长度
 * @return  0       解析成功
 * @return  -1      解析失败
 */
int app_algo_keyword_and_kid_extract(const uint8_t *const in, uint8_t *out, int max_len);

/**
 * 从算法的Param1中解析出 keyword 转化成tone对应的 ID
 * @param keyword 	    算法中得到的keyword
 * @return  >=0     解析成功，返回提示音id
 * @return  -1      解析失败
 */
int local_tone_get(const uint8_t *keyword);

#endif