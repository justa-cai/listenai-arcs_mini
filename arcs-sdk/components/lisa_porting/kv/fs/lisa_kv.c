#include "lisa_kv.h"
#include "base64.h"

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_typedef.h"
#include "lisa_mutex.h"

#include "sysheap.h"

#include "lsfs.h"
#include "cJSON.h"

#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "lisa_kv_utils.h"

struct lisa_kv_obj {
    struct lsfs_file_t fp;
    cJSON *root;
    lisa_mutex_t *mutex;
};

#define LISA_KV_FILE_NAME "/NAND:/lisa_kv.ini"

static struct lisa_kv_obj lisa_kv_obj = {0};


static char *lisa_kv_get_all(struct lisa_kv_obj *obj)
{
    int r;
    char *datas = NULL;
    r = lsfs_open(&lisa_kv_obj.fp, LISA_KV_FILE_NAME, LSFS_O_READ);
    if (r < 0) {
        LOGE("lisa_kv_init fail, error: %d", r);
        return NULL;
    }

    r = lsfs_seek(&obj->fp, 0, LSFS_SEEK_END);
    if (r) {
        LOGE("lisa_kv_get_all, seek fail, error: %d", r);
        return NULL;
    }

    int size = lsfs_tell(&obj->fp);
    lsfs_seek(&obj->fp, 0, SEEK_SET);
    LOGI("lisa_kv_get_all, size: %d", size);
    if (size) {
        datas = (char *)LISA_KV_MALLOC(size + 1);
        if (!datas) {
            LOGE("lisa_kv_get_all, tell fail, LISA_KV_MALLOC fail, size: %d", size);
            return NULL;
        }
        memset(datas, 0, size + 1);
        r = lsfs_read(&obj->fp, datas, size);
        if (r < 0) {
            LOGE("lisa_kv_get_all, read fail, error: %d", r);
            LISA_KV_FREE(datas);
            return NULL;
        }
    }

    lsfs_close(&obj->fp);

    return datas;
}

static int lisa_kv_save(struct lisa_kv_obj *obj)
{
    int r;

    r = lsfs_open(&obj->fp, LISA_KV_FILE_NAME, LSFS_O_WRITE | LSFS_O_CREATE | LSFS_O_TRUNC);
    if (r < 0) {
        LOGE("lisa_kv_save fail, lsfs_open fail, error: %d", r);
        return -1;
    }
    char *all = cJSON_PrintUnformatted(obj->root);
    if (!all) {
        LOGE("lisa_kv_save fail, cJSON_Print fail");
        return -1;
    }
    lsfs_truncate(&obj->fp, 0);
    lsfs_seek(&obj->fp, 0, SEEK_SET);
    r = lsfs_write(&obj->fp, all, strlen(all));
    if (r < 0) {
        LOGE("lisa_kv_save fail, lsfs_write fail, error: %d", r);
        LISA_KV_FREE(all);
        return -1;
    }

    lsfs_close(&obj->fp);

    LISA_KV_FREE(all);

    return 0;
}

static int lisa_kv_load(struct lisa_kv_obj *obj)
{
    obj->root = cJSON_Parse(lisa_kv_get_all(obj));

    if (obj->root == NULL) {
        LOGE("lisa_kv_load fail, cJSON_Parse fail");
        obj->root = cJSON_CreateObject();
#ifndef CONFIG_LISA_KV_POWEROFF_SAVE
        lisa_kv_save(obj);
#endif
    }

    return 0;
}

int lisa_kv_init(void)
{
    int r;
    lisa_kv_obj.mutex = lisa_mutex_create();
    assert(lisa_kv_obj.mutex != NULL);
    lisa_kv_load(&lisa_kv_obj);

    return 0;
}

int lisa_kv_set(const char *key, const char *value)
{
    int r;

    lisa_mutex_lock(lisa_kv_obj.mutex, LISA_OS_WAIT_FOREVER);
    cJSON_DeleteItemFromObject(lisa_kv_obj.root, key);
    cJSON_AddStringToObject(lisa_kv_obj.root, key, value);
#ifndef CONFIG_LISA_KV_POWEROFF_SAVE
    lisa_kv_save(&lisa_kv_obj);
#endif
    lisa_mutex_unlock(lisa_kv_obj.mutex);

    return 0;
}

int lisa_kv_get(const char *key, char **value)
{
    *value = NULL;
    cJSON *item = cJSON_GetObjectItem(lisa_kv_obj.root, key);
    if (!item) {
        return -1;
    }
    *value = LISA_KV_MALLOC(strlen(item->valuestring) + 1);
    if (!*value) {
        LOGE("lisa_kv_get fail, LISA_KV_MALLOC fail");
        return -1;
    }
    memcpy(*value, item->valuestring, strlen(item->valuestring) + 1);

    return 0;
}

int lisa_kv_del(const char *key)
{
    lisa_mutex_lock(lisa_kv_obj.mutex, LISA_OS_WAIT_FOREVER);
    cJSON_DeleteItemFromObject(lisa_kv_obj.root, key);
#ifndef CONFIG_LISA_KV_POWEROFF_SAVE
    lisa_kv_save(&lisa_kv_obj);
#endif
    lisa_mutex_unlock(lisa_kv_obj.mutex);

    return 0;
}

void lisa_kv_free(void *value)
{
    if (value) {
        LISA_KV_FREE(value);
    }
}

int lisa_kv_deinit(void)
{
    return 0;
}

void lisa_kv_dump(void)
{
    char *all = cJSON_PrintUnformatted(lisa_kv_obj.root);
    if (!all) {
        LOGE("lisa_kv_save fail, cJSON_Print fail");
        return;
    }
    LOGI("lisa_kv_save, all: %s", all);
    LISA_KV_FREE(all);
}

int lisa_kv_get_int(const char *key, int *value)
{
    char *str_value = NULL;
    if (lisa_kv_get(key, &str_value) == 0) {
        *value = atoi(str_value);
        LISA_KV_FREE(str_value);
        return 0;
    }
    return -1;
}

int lisa_kv_get_bool(const char *key, bool *value)
{
    char *str_value = NULL;
    if (lisa_kv_get(key, &str_value) == 0) {
        *value = !!atoi(str_value);
        LISA_KV_FREE(str_value);
        return 0;
    }
    return -1;
}

int lisa_kv_get_string(const char *key, char **value)
{
    return lisa_kv_get(key, value);
}

int lisa_kv_set_string(const char *key, const char *value)
{
    return lisa_kv_set(key, value);
}

int lisa_kv_set_int(const char *key, int value)
{
    char str_value[32];
    snprintf(str_value, sizeof(str_value), "%d", value);
    return lisa_kv_set(key, str_value);
}

int lisa_kv_set_bool(const char *key, bool value)
{
    return lisa_kv_set_int(key, value);
}

int lisa_kv_clear(void)
{
    lisa_mutex_lock(lisa_kv_obj.mutex, LISA_OS_WAIT_FOREVER);
    cJSON_Delete(lisa_kv_obj.root);
    lisa_kv_obj.root = cJSON_CreateObject();
#ifndef CONFIG_LISA_KV_POWEROFF_SAVE
    lisa_kv_save(&lisa_kv_obj);
#endif
    lisa_mutex_unlock(lisa_kv_obj.mutex);

    return 0;
}

int lisa_kv_set_blob(const char *key, uint8_t *data, int len)
{
    char *str = lisa_kv_base64_encode(data, len);
    if (str == NULL) {
        return -1;
    }

    int r = lisa_kv_set(key, str);

    LISA_KV_FREE(str);

    return r;
}

int lisa_kv_get_blob(const char *key, uint8_t **data, int *len)
{
    char *str = NULL;
    *data = NULL;

    int r = lisa_kv_get(key, &str);
    if (r != 0) {
        return r;
    }

    int decLen = strlen(str) / 4.0 * 3 + 8;
    uint8_t *_data = LISA_KV_MALLOC(decLen);
    if (_data == NULL) {
        LOGE("lisa_kv_get_blob LISA_KV_MALLOC fail");
        LISA_KV_FREE(str);
        return -1;
    }

    r = lisa_kv_base64_decode(str, strlen(str), _data, len);
    LISA_KV_FREE(str);
    *data = _data;

    return r;
}

#ifdef CONFIG_LISA_KV_POWEROFF_SAVE
/**
 * @brief 关机时保存KV数据到文件
 * 
 * @return int 0表示成功，-1表示失败
 */
int lisa_kv_poweroff_save(void)
{
    int r;
    
    if (lisa_kv_obj.mutex == NULL) {
        LOGE("lisa_kv_poweroff_save: KV not initialized");
        return -1;
    }
    
    lisa_mutex_lock(lisa_kv_obj.mutex, LISA_OS_WAIT_FOREVER);
    r = lisa_kv_save(&lisa_kv_obj);
    lisa_mutex_unlock(lisa_kv_obj.mutex);
    
    if (r == 0) {
        LOGI("lisa_kv_poweroff_save: KV data saved successfully");
    } else {
        LOGE("lisa_kv_poweroff_save: Failed to save KV data, error: %d", r);
    }
    
    return r;
}
#endif


