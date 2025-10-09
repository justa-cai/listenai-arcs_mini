/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lisa_kv_free(void *value);

int lisa_kv_get_blob(const char *key, uint8_t **data, int *len);
int lisa_kv_set_blob(const char *key, uint8_t *data, int len);

int lisa_kv_get_int(const char *key, int *value);
int lisa_kv_set_int(const char *key, int value);

int lisa_kv_del(const char *key);


#ifdef __cplusplus
}
#endif