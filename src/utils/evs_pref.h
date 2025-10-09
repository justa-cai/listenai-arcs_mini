/**
 * @brief 
 * @version 0.1
 * @date 2022-08-17
 * 
 * Copyright (C) 2022 ANHUI LISTENAI Co., LTD All Rights Reserved
 */
#ifndef __LISTENAI_EVS_PREF_H__
#define __LISTENAI_EVS_PREF_H__

#include <stdbool.h>
#include "evs_err.h"

#define PREF_KEY_VOLUME "volume"
#define PREF_KEY_LOG_LEV "log_lev"
#define PREF_KEY_PLAYER_LOG_LEV "player_loglev"
#define PREF_KEY_OFFLINE "aiui_offline"

evs_err_t evs_pref_put_string(const char *k, const char *v);
evs_err_t evs_pref_get_string(const char *k, char **out_v); /* 使用evs_memp_free回收 */

evs_err_t evs_pref_put_int(const char *k, int v);
evs_err_t evs_pref_get_int(const char *k, int *out_v);

evs_err_t evs_pref_put_bool(const char *k, bool v);
evs_err_t evs_pref_get_bool(const char *k, bool *out_v);

evs_err_t evs_pref_put_blob(const char *k, const char *v, int len);
evs_err_t evs_pref_get_blob(const char *k, char **out_v, int *out_len); /* 使用evs_memp_free回收 */

evs_err_t evs_pref_delete(const char *k);

bool evs_pref_has_key(const char *k);

void evs_pref_clear();
void evs_pref_dump();

#endif