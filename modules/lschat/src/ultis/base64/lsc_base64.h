/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief base64编码
 *
 * @param data 需要被编码数据的地址
 * @param data_len 需要被编码数据的大小
 * @return uint8_t* 编码后的base64数据，需要手动通过lisa_mem_free进行内存释放
 */
uint8_t *lsc_base64_encodev2(const uint8_t *data, uint32_t data_len);

unsigned char *lsc_base64_encode(unsigned char *str);
int lsc_base64_decode(const unsigned char *indata, int inlen, char *outdata, int *outlen);

#ifdef __cplusplus
}
#endif