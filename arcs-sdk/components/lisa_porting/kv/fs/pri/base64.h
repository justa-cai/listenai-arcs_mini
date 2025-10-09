#ifndef BASE64__H
#define BASE64__H

#ifdef __cplusplus

#include <stdio.h>
#include "stdint.h"

extern "C" {
#endif

/**
 * decode base64 encoded data
 *
 * @param str the source data (zero terminated)
 * @return pointer to the target buffer
 */
unsigned char *lisa_kv_base64_encode(uint8_t *str, int str_len);

/**
 * decode base64 encoded data
 *
 * @param indata the encoded data (zero terminated)
 * @param inlen length of the target buffer
 * @param target pointer to the target buffer
 * @param outlen length of the target buffer
 * @return length of converted data on success, -1 otherwise
 */
int lisa_kv_base64_decode(const char *indata, int inlen, char *outdata, int *outlen);

#ifdef __cplusplus
}
#endif

#endif
