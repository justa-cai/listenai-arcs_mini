#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "app_tone.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "config_parser.h"
#include "romfs/romfs.h"
#include "tone.h"
#define TAG "tone"

static tone_hdr_t *s_tone_hdr = NULL;
// 如果是多套音频打包成一个的情况
// 第二套音频需要跳过的数量
static uint32_t s_skip_count = 0;

int app_tone_default_init()
{
    const Config *config = config_get();
    uint32_t tone_addr = 0;
    uint32_t tone_size = 0;

    if (config && config->resources.count > 0) {
        for (size_t i = 0; i < config->resources.count; i++) {
            const ResourceConfig *res = &config->resources.items[i];
            if (strcmp(res->name, "tone") == 0) {
                tone_addr = res->address;
                tone_size = res->size;
            }
        }
    }

    int ret = app_tone_init(tone_addr, tone_size);
    if (ret != 0) {
        LISA_LOGE(TAG, "tone default init fail");
        return -1;
    }

    return 0;
}

static void _app_tone_uninit()
{
    if (s_tone_hdr) {
        if (s_tone_hdr->item) {
            for (int i = 0; i < s_tone_hdr->total_cnt; i++) {
                if (s_tone_hdr->item[i].url) {
                    lisa_mem_free(s_tone_hdr->item[i].url);
                }
            }
            lisa_mem_free(s_tone_hdr->item);
        }

        lisa_mem_free(s_tone_hdr);
        s_tone_hdr = NULL;
    }
}

/**
 * 从 romfs 加载音频文件到 s_tone_hdr 映射表
 * 扫描指定目录，将符合 /XXX 格式（XXX为3位数字）的文件加载为 tone_id
 * @param fs: romfs 文件系统指针
 * @param dir_path: 目录路径（如 "/"）
 * @return 成功加载的 tone 数量，或 <0 表示错误
 */
static inline int _app_tone_load_from_romfs(struct romfs *fs, const char *dir_path)
{
    if (!fs || !dir_path || !s_tone_hdr || !s_tone_hdr->item) {
        LISA_LOGE(TAG, "Invalid param for load_from_romfs");
        return -1;
    }


    struct romfs_dir_iter iter;
    if (romfs_dir_iter_start(fs, dir_path, &iter) != 0) {
        LISA_LOGE(TAG, "romfs dir iter start fail for path: %s", dir_path);
        return -1;
    }

    char path[ROMFS_PATH_MAX];
    uint8_t *data = NULL;
    uint32_t size = 0;
    char tone_url[MAX_URL_LEN];
    int tone_id = 0; 
    bool is_dir = false;
    int loaded_mem_size = 0;

    while (romfs_dir_iter_next(&iter, path, sizeof(path), &data, &size, &is_dir) == 0) {
        LISA_LOGD(TAG, "tone romfs file: %s, addr: %p, size: %u", path, data, size);

        if (is_dir || data == NULL || size == 0 || path[0] != '/' || strlen(path) < 5) {
            continue;
        }

        if (sscanf(path + 1, "%03d", &tone_id) != 1) {
            LISA_LOGI(TAG, "ignore invalid tone file: %s", path);
            continue;
        }

        if (tone_id >= s_tone_hdr->total_cnt) {
            LISA_LOGW(TAG, "tone id %d exceed max %d", tone_id, s_tone_hdr->total_cnt);
            continue;
        }

        memset(tone_url, 0, MAX_URL_LEN);
        snprintf(tone_url, MAX_URL_LEN, "mem://addr=%usize=%u", (unsigned int)(uintptr_t)data, size);

        uint32_t url_len = strlen(tone_url) + 1;

        s_tone_hdr->item[tone_id].tone_id = tone_id;
        s_tone_hdr->item[tone_id].url = lisa_mem_alloc(url_len);
        if (!s_tone_hdr->item[tone_id].url) {
            LISA_LOGE(TAG, "tone url alloc fail for id %d", tone_id);
            return -1;
        }
        strcpy(s_tone_hdr->item[tone_id].url, tone_url);

        loaded_mem_size+=url_len;

        LISA_LOGI(TAG, "Loaded tone[%d] from romfs: %s, size: %u", tone_id, path, size);

    }

    LISA_LOGI(TAG, "Loaded %d tones from romfs", loaded_mem_size);

    return loaded_mem_size;
}

int app_tone_init(uint32_t flash_addr, uint32_t flash_size)
{
    // 先逆初始化
    _app_tone_uninit();

    if (flash_addr == 0 || flash_size == 0) {
        LISA_LOGE(TAG, "invalid tone romfs addr/size: addr=%p, size=%u", flash_addr, flash_size);
        return -1;
    }

    uint32_t total_mem_size = 0;

    s_tone_hdr = lisa_mem_calloc(1, sizeof(tone_hdr_t));
    if (!s_tone_hdr) {
        return -1;
    }

    total_mem_size += sizeof(tone_hdr_t);

    struct romfs *tone_fs = NULL;

    // 初始化 romfs
    if (romfs_init(&tone_fs, (const void *)flash_addr, flash_size) != 0) {
        LISA_LOGE(TAG, "romfs init failed for tone addr=%p size=%u", flash_addr, flash_size);
        goto TONE_INIT_FAIL;
    }

    LISA_LOGI(TAG, "tone init romfs addr: %p, size: %u", flash_addr, flash_size);

    s_tone_hdr->total_cnt = TONE_ID_MAX;

    total_mem_size += (s_tone_hdr->total_cnt * sizeof(tone_dsc_t));
    s_tone_hdr->item = lisa_mem_calloc(s_tone_hdr->total_cnt, sizeof(tone_dsc_t));
    if (!s_tone_hdr->item) {
        LISA_LOGE(TAG, "tone item no mem...");
        goto TONE_INIT_FAIL;
    }

    // 从 romfs 中获取每个文件的数据指针和长度，拼出 mem://addr=...size=... 的 url
    int loaded_mem_size = _app_tone_load_from_romfs(tone_fs, "/");
    if (loaded_mem_size < 0)
    {
        LISA_LOGE(TAG, "tone item no mem...");
        goto TONE_INIT_FAIL;
    }
    total_mem_size += loaded_mem_size;


    LISA_LOGD(TAG, "tone init use mem: %d", total_mem_size);

    return 0;

TONE_INIT_FAIL:
    _app_tone_uninit();
    return -1;
}

static tone_dsc_t *__get_tone_by_id(uint16_t tone_id)
{
    tone_dsc_t *item = NULL;
    if (s_tone_hdr && s_tone_hdr->item) {
        if (tone_id >= s_tone_hdr->total_cnt) {
            return NULL;
        }

        item = &s_tone_hdr->item[tone_id];
        if (item->url == NULL) {
            return NULL;
        }
    }
    return item;
}

char *app_tone_get_url(uint16_t tone_id)
{
// 如果是第三种配置方案
#if (VOICEID_TYPE == 3)
    tone_dsc_t *dsc_t = NULL;
    if (s_skip_count > 0) {
        uint16_t next_id = tone_id + s_skip_count;
        LISA_LOGV(TAG, "tone_id: %d, next_id: %d", tone_id, next_id);
        dsc_t = __get_tone_by_id(next_id);
        if (dsc_t) {
            return dsc_t->url;
        } else {
            LISA_LOGV(TAG, "get next_id: %d fail, use tone_id: %d", next_id, tone_id);
            // 如果第二套音频获取失败情况下
            // 从第一套音频获取, 比如公共的音频
            dsc_t = __get_tone_by_id(tone_id);
            if (dsc_t) {
                return dsc_t->url;
            }
        }
    } else {
        // 不需要跳过时, 直接从第一套音频获取
        dsc_t = __get_tone_by_id(tone_id);
        if (dsc_t) {
            return dsc_t->url;
        }
    }

    return NULL;
#else
    tone_dsc_t *dsc_t = __get_tone_by_id(tone_id);
    if (dsc_t) {
        return dsc_t->url;
    }
    return NULL;
#endif
}

int app_tone_reload(uint32_t custom_addr, uint32_t custom_size)
{
    LISA_LOGD(TAG, "app tone reload by 0x%X, size: %u", custom_addr, custom_size);
    int ret = app_tone_init(custom_addr, custom_size);
    if (ret == -1) {
        app_tone_default_init();
        return -1;
    }
    return 0;
}

void app_tone_skip_count(int skip_count)
{
    s_skip_count = skip_count;
}

int app_tone_override(uint16_t tone_id, const void *addr, uint32_t size)
{
    tone_dsc_t *item = __get_tone_by_id(tone_id);
    if (!item) {
        return -1;
    }

    if (item->url) {
        lisa_mem_free(item->url);
        item->url = NULL;
    }

    char tmp_buf[MAX_URL_LEN];
    uint32_t tmp_size = 0;
    memset(tmp_buf, 0, MAX_URL_LEN);
    sprintf(tmp_buf, "mem://addr=%usize=%u", (uint32_t)addr, size);
    tmp_size = strlen(tmp_buf) + 1;
    item->url = lisa_mem_alloc(tmp_size);
    LISA_ASSERT(item->url, "alloc tone url fail");
    strcpy(item->url, tmp_buf);

    return 0;
}

int app_tone_load_from_romfs(struct romfs *fs, const char *dir_path)
{
    if (!s_tone_hdr || !s_tone_hdr->item) {
        LISA_LOGE(TAG, "tone header not initialized, call app_tone_init first");
        return -1;
    }

    return _app_tone_load_from_romfs(fs, dir_path);
}
