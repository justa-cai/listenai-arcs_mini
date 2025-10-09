#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lisa_log.h"
#include "sysheap.h"
#include "easyflash.h"
#include "lisa_kv_utils.h"

static int lisa_kv_set_small_data(const char *key, const void *data, int len)
{
    assert(key);
    assert(data);
    assert(len);

    return ef_set_env_blob(key, data, len);
}

static int lisa_kv_get_small_data(const char *key, void *data, int len)
{
    assert(key);
    assert(data);
    assert(len);

    int old_len;
    ef_get_env_blob(key, NULL, 0, &old_len);

    if (old_len == 0) {
        LOGE("lisa_kv_get_small_data(key: %s) fail, old_len is 0", key);
        return -1;
    }

    ef_get_env_blob(key, data, len, &len);
    if(len == 0){
        LOGW("lisa_kv_get_small_data fail(key: %s), size is 0", key);
        return -1;
    }

    return 0;
}

int lisa_kv_init(void)
{
    static bool inited = false;
    if (inited) {
        return 0;
    }
    inited = true;
    int r = easyflash_init();
    if (r != 0) {
        LOGE("lisa_kv_init fail, easyflash_init fail, error: %d", r);
        return -1;
    }

    return 0;
}

int lisa_kv_del(const char *key)
{
    return ef_del_env(key);
}

void lisa_kv_free(void *value)
{
    LISA_KV_FREE(value);
}

void lisa_kv_dump(void)
{
    ef_print_env();
}

int lisa_kv_set_int(const char *key, int value)
{
    return lisa_kv_set_small_data(key, &value, sizeof(int));
}

int lisa_kv_get_int(const char *key, int *value)
{
    return lisa_kv_get_small_data(key, value, sizeof(int));
}

int lisa_kv_set_bool(const char *key, bool value)
{
    return lisa_kv_set_small_data(key, &value, sizeof(bool));
}

int lisa_kv_get_bool(const char *key, bool *value)
{
    return lisa_kv_get_small_data(key, value, sizeof(bool));
}

int lisa_kv_get_blob(const char *key, uint8_t **data, int *len)
{
    assert(key);
    assert(data);
    assert(len);

    int old_len;
    ef_get_env_blob(key, NULL, 0, &old_len);
    if (old_len == 0) {
        LOGE("lisa_kv_get_blob fail(key: %s), old_len is 0", key);
        return -1;
    }

    uint8_t *p = LISA_KV_MALLOC(old_len);
    if (!p) {
        LOGE("lisa_kv_get_blob LISA_KV_MALLOC fail");
        return -1;
    }

    *len = ef_get_env_blob(key, p, old_len, NULL);
    if (*len == 0) {
        LOGE("lisa_kv_get_blob fail(key: %s), len is 0", key);
        LISA_KV_FREE(p);
        return -1;
    }
    *data = p;

    return 0;
}

int lisa_kv_set_blob(const char *key, uint8_t *data, int len)
{
    assert(key);
    assert(data);
    assert(len);
    int r = ef_set_env_blob(key, data, len);
    if (r != 0) {
        LOGE("lisa_kv_set_blob fail(key: %s), ef_set_env_blob fail, error: %d", key, r);
        return -1;
    }

    return 0;
}

int lisa_kv_set_string(const char *key, const char *value)
{
    assert(key);
    assert(value);

    return lisa_kv_set_blob(key, (uint8_t *)value, strlen(value) + 1);
}

int lisa_kv_get_string(const char *key, char **value)
{
    int len;
    assert(value);
    assert(key);

    return lisa_kv_get_blob(key, (uint8_t **)value, &len);
}

int lisa_kv_clear(void)
{
    int r = ef_env_set_default();
    if (r != 0) {
        LOGE("lisa_kv_clear fail, ef_env_set_default fail, error: %d", r);
        return -1;
    }

    return 0;
}

int lisa_kv_save(void)
{
    int r = ef_save_env();

    if (r != 0) {
        LOGE("lisa_kv_save fail, ef_save_env fail, error: %d", r);
        return -1;
    }

    return 0;
}
