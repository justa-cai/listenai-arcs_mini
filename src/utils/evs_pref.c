// #include <string.h>
// #include "evs_pref.h"
// #include "lisa_log.h"
// #include "lisa_mem.h"
// #include "ef_types.h"
// #include "easyflash.h"

// #define TAG "evs_pref"

// static char *__buf_bak(const char *src, int src_len)
// {
// 	char *dst = lisa_mem_alloc(src_len);
// 	memcpy(dst, src, src_len * sizeof(char));
// 	return dst;
// }

// static char *__string_bak(const char *src)
// {
// 	int src_len = strlen(src) + 1;
// 	return __buf_bak(src, src_len);
// }

// #define free_string(k) lisa_mem_free(k)

// evs_err_t evs_pref_put_string(const char *k, const char *v)
// {
// 	char *k_bak = __string_bak(k);
// 	char *v_bak = __string_bak(v);
// 	if (EF_NO_ERR == ef_set_env_blob(k_bak, v_bak, strlen(v_bak))) {
// 		free_string(k_bak);
// 		free_string(v_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	free_string(v_bak);
// 	return EVS_FAIL;
// }

// // need free(use lisa_mem_free) out_v after call this fucntion
// evs_err_t evs_pref_get_string(const char *k, char **out_v)
// {
// 	size_t len = 0;
// 	char *k_bak = __string_bak(k);
// 	int ret1 = ef_get_env_blob(k_bak, NULL, 0, &len);
// 	// printk("len=%d, ret1=%d\n", len, ret1);
// 	if (len == 0) {
// 		free_string(k_bak);
// 		return EVS_FAIL;
// 	}
// 	(*out_v) = (char *)lisa_mem_alloc(len + 1);
// 	if (*out_v == NULL) return EVS_FAIL;
// 	int ret2 = ef_get_env_blob(k_bak, *out_v, len, NULL);
// 	if (ret2 > 0) {
// 		(*out_v)[len] = '\0';
// 		free_string(k_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	lisa_mem_free(*out_v);
// 	*out_v = NULL;
// 	return EVS_FAIL;
// }

// evs_err_t evs_pref_put_blob(const char *k, const char *v, int len)
// {
// 	char *k_bak = __string_bak(k);
// 	char *v_bak = __buf_bak(v, len);
// 	if (EF_NO_ERR == ef_set_env_blob(k_bak, v_bak, len)) {
// 		free_string(k_bak);
// 		free_string(v_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	free_string(v_bak);
// 	return EVS_FAIL;
// }

// evs_err_t evs_pref_get_blob(const char *k, char **out_v, int *out_len)
// {
// 	size_t len = 0;
// 	char *k_bak = __string_bak(k);
// 	int ret1 = ef_get_env_blob(k_bak, NULL, 0, &len);
// 	*out_len = len;
// 	if (len == 0) {
// 		free_string(k_bak);
// 		return EVS_FAIL;
// 	}
// 	(*out_v) = (char *)lisa_mem_alloc(len);
// 	if (*out_v == NULL) {
// 		free_string(k_bak);
// 		return EVS_FAIL;
// 	}
// 	int ret2 = ef_get_env_blob(k_bak, *out_v, len, NULL);
// 	if (ret2 > 0) {
// 		free_string(k_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	lisa_mem_free(*out_v);
// 	*out_v = NULL;
// 	return EVS_FAIL;
// }

// evs_err_t evs_pref_put_int(const char *k, int v)
// {
// 	char *k_bak = __string_bak(k);
// 	if (EF_NO_ERR == ef_set_int(k_bak, v)) {
// 		free_string(k_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	return EVS_FAIL;
// }

// evs_err_t evs_pref_get_int(const char *k, int *out_v)
// {
// 	size_t len = 0;
// 	char *k_bak = __string_bak(k);
// 	int ret1 = ef_get_env_blob(k_bak, NULL, 0, &len);
// 	if (len == 0) {
// 		free_string(k_bak);
// 		return EVS_FAIL;
// 	}
// 	*out_v = ef_get_int(k_bak);
// 	free_string(k_bak);
// 	return EVS_OK;
// }

// evs_err_t evs_pref_put_bool(const char *k, bool v)
// {
// 	char *k_bak = __string_bak(k);
// 	if (EF_NO_ERR == ef_set_bool(k_bak, v)) {
// 		free_string(k_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	return EVS_FAIL;
// }

// evs_err_t evs_pref_get_bool(const char *k, bool *out_v)
// {
// 	size_t len = 0;
// 	char *k_bak = __string_bak(k);
// 	int ret1 = ef_get_env_blob(k_bak, NULL, 0, &len);
// 	if (len == 0) {
// 		free_string(k_bak);
// 		return EVS_FAIL;
// 	}
// 	*out_v = ef_get_bool(k_bak);
// 	free_string(k_bak);
// 	return EVS_OK;
// }

// bool evs_pref_has_key(const char *k)
// {
// 	char *k_bak = __string_bak(k);

// 	bool find = ef_has_env(k_bak);

// 	free_string(k_bak);
// 	return find;
// }

// evs_err_t evs_pref_delete(const char *k)
// {
// 	size_t len = 0;
// 	char *k_bak = __string_bak(k);
// 	int ret1 = ef_get_env_blob(k_bak, NULL, 0, &len);
// 	if (len == 0) {
// 		free_string(k_bak);
// 		return EVS_FAIL;
// 	}
// 	if (EF_NO_ERR == ef_del_env(k_bak)) {
// 		free_string(k_bak);
// 		return EVS_OK;
// 	}
// 	free_string(k_bak);
// 	return EVS_FAIL;
// }

// void evs_pref_clear()
// {
// 	ef_env_set_default();
// }

// void evs_pref_dump()
// {
// 	ef_log_enable(true);
// 	ef_print_env();
// 	ef_log_enable(false);
// }