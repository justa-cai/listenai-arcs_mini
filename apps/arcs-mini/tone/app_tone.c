#define TAG "tone"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "app_tone.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "config_parser.h"
#include "romfs/romfs.h"
#include "tone.h"

static tone_hdr_t *s_tone_hdr = NULL;

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

int app_tone_init(uint32_t flash_addr, uint32_t flash_size)
{
    struct romfs *tone_fs = NULL;
    struct romfs_dir_iter iter;
    char path[ROMFS_PATH_MAX];
    uint8_t *data = NULL;
    uint32_t size = 0;
    bool is_dir = false;
    int tone_id = 0;
    int tone_id_max = 0;
    char tone_url[MAX_URL_LEN];
    uint32_t total_mem_size = 0;

    _app_tone_uninit();

    if (flash_addr == 0 || flash_size == 0) {
        LISA_LOGE(TAG, "Invalid tone ROMFS addr/size: addr=%p, size=%u", (void *)flash_addr, flash_size);
        return -1;
    }

    s_tone_hdr = lisa_mem_calloc(1, sizeof(tone_hdr_t));
    if (!s_tone_hdr) {
        return -1;
    }

    total_mem_size += sizeof(tone_hdr_t);

    // 初始化 ROMFS

    if (romfs_init(&tone_fs, (const void *)flash_addr, flash_size) != 0) {
        LISA_LOGE(TAG, "ROMFS init failed for tone addr=%p size=%u", (void *)flash_addr, flash_size);
        goto TONE_INIT_FAIL;
    }

    LISA_LOGI(TAG, "Loading tone with ROMFS addr=%p size=%u", (void *)flash_addr, flash_size);

    // 预扫描一遍，获取最大 tone_id

    if (romfs_dir_iter_start(tone_fs, "/", &iter) != 0) {
        LISA_LOGE(TAG, "ROMFS iterator start fail for tone");
        goto TONE_INIT_FAIL;
    }

    while (romfs_dir_iter_next(&iter, path, sizeof(path), &data, &size, &is_dir) == 0) {
        if (is_dir || data == NULL || size == 0 || path[0] != '/' || strlen(path) < 5) {
            continue;
        }

        if (sscanf(path + 1, "%03d", &tone_id) != 1) {
            continue;
        }

        if (tone_id > tone_id_max) {
            tone_id_max = tone_id;
        }
    }

    s_tone_hdr->total_cnt = tone_id_max + 1;
    LISA_LOGI(TAG, "Found %d tones", s_tone_hdr->total_cnt);

    // 分配 tone 映射表

    total_mem_size += (s_tone_hdr->total_cnt * sizeof(tone_dsc_t));
    s_tone_hdr->item = lisa_mem_calloc(s_tone_hdr->total_cnt, sizeof(tone_dsc_t));
    if (!s_tone_hdr->item) {
        LISA_LOGE(TAG, "Failed to alloc tone item table for %d items", s_tone_hdr->total_cnt);
        goto TONE_INIT_FAIL;
    }

    // 从 ROMFS 创建音频索引

    if (romfs_dir_iter_start(tone_fs, "/", &iter) != 0) {
        LISA_LOGE(TAG, "ROMFS iterator start fail for tone");
        goto TONE_INIT_FAIL;
    }

    while (romfs_dir_iter_next(&iter, path, sizeof(path), &data, &size, &is_dir) == 0) {
        LISA_LOGD(TAG, "Found ROMFS file: %s, addr=%p size=%u", path, data, size);

        if (is_dir || data == NULL || size == 0 || path[0] != '/' || strlen(path) < 5) {
            continue;
        }

        if (sscanf(path + 1, "%03d", &tone_id) != 1) {
            LISA_LOGI(TAG, "Ignore invalid tone file: %s", path);
            continue;
        }

        if (tone_id >= s_tone_hdr->total_cnt) {
            LISA_LOGW(TAG, "Tone id %d exceed max %d", tone_id, s_tone_hdr->total_cnt);
            continue;
        }

        char tone_url[MAX_URL_LEN];
        memset(tone_url, 0, MAX_URL_LEN);
        snprintf(tone_url, MAX_URL_LEN, "mem://addr=%usize=%u", (uint32_t)data, size);

        uint32_t url_len = strlen(tone_url) + 1;

        s_tone_hdr->item[tone_id].tone_id = tone_id;
        s_tone_hdr->item[tone_id].url = lisa_mem_alloc(url_len);
        if (!s_tone_hdr->item[tone_id].url) {
            LISA_LOGE(TAG, "Tone URL alloc fail for id %d", tone_id);
            goto TONE_INIT_FAIL;
        }
        strcpy(s_tone_hdr->item[tone_id].url, tone_url);

        total_mem_size += url_len;

        LISA_LOGI(TAG, "Loaded tone[%d] from ROMFS: %s, size: %u", tone_id, path, size);
    }

    LISA_LOGI(TAG, "Loaded %d tones from ROMFS, total mem size: %u bytes", s_tone_hdr->total_cnt, total_mem_size);

    return 0;

TONE_INIT_FAIL:
    _app_tone_uninit();
    return -1;
}

char *app_tone_get_url(uint16_t tone_id)
{
    tone_dsc_t *item = NULL;

    if (!s_tone_hdr || !s_tone_hdr->item) {
        LISA_LOGE(TAG, "Tone not init or no item");
        return NULL;
    }

    if (tone_id >= s_tone_hdr->total_cnt) {
        LISA_LOGE(TAG, "Invalid tone id %d, exceed max %d", tone_id, s_tone_hdr->total_cnt);
        return NULL;
    }

    item = &s_tone_hdr->item[tone_id];
    if (item->url == NULL) {
        LISA_LOGE(TAG, "Tone id %d url is null", tone_id);
        return NULL;
    }

    return item->url;
}
