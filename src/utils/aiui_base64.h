#ifndef BASE64__H
#define BASE64__H

#ifdef __cplusplus

#include <stdio.h>

extern "C" {
#endif

/**
 * decode base64 encoded data
 *
 * @param str the source data (zero terminated)
 * @return pointer to the target buffer
 */
unsigned char *aiui_base64_encode(unsigned char *str);

/**
 * decode base64 encoded data
 *
 * @param indata the encoded data (zero terminated)
 * @param inlen length of the target buffer
 * @param target pointer to the target buffer
 * @param outlen length of the target buffer
 * @return length of converted data on success, -1 otherwise
 */
int aiui_base64_decode(const char *indata, int inlen, char *outdata, int *outlen);

uint8_t *aiui_base64_encodev2(const uint8_t *data, uint32_t data_len);

#ifdef __cplusplus
}
#endif

#endif
