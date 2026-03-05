#ifndef __LISA_KV_H__
#define __LISA_KV_H__

#include <stdbool.h>
#include "stdint.h"

int lisa_kv_init(void);
int lisa_kv_del(const char *key);
void lisa_kv_free(void *value);
void lisa_kv_dump(void);

int lisa_kv_set_int(const char *key, int value);
int lisa_kv_get_int(const char *key, int *value);
int lisa_kv_set_string(const char *key, const char *value);
int lisa_kv_get_string(const char *key, char **value);
int lisa_kv_set_bool(const char *key, bool value);
int lisa_kv_get_bool(const char *key, bool *value);
int lisa_kv_clear(void);
int lisa_kv_get_blob(const char *key, uint8_t **data, int *len);
int lisa_kv_set_blob(const char *key, uint8_t *data, int len);

#ifdef CONFIG_LISA_KV_POWEROFF_SAVE
int lisa_kv_poweroff_save(void);
#endif

#endif
