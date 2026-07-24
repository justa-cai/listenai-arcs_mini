/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define TAG "voice_music_list"

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lsc.h"

#include "voice_music_list.h"

/**
 * @brief 单个子列表最大容量
 *
 * 每个列表各自独立分配，online/offline 各最多 200 首。
 * 200 × ~776 字节/首 ≈ 155KB，shuffle_order 另加 ~800 字节。
 */
#define MUSIC_LIST_MAX_PER_LIST 200

/**
 * @brief 单列表子列表
 *
 * 独立管理一个列表类型（ONLINE / OFFLINE 等）的所有曲目。
 * items 和 shuffle_order 从 PSRAM 按需分配，clear 时释放。
 */
typedef struct {
    music_item_t *items;          /**< 曲目数组（PSRAM 分配，NULL 表示空） */
    int count;                   /**< 曲目数量 */
    int current_index;           /**< 当前播放索引（-1 表示未播放） */
    int *shuffle_order;          /**< 随机播放顺序（PSRAM 分配） */
    int shuffle_count;           /**< 随机列表中的曲目数量 */
} voice_music_sub_list_t;

/**
 * @brief 全局音乐列表上下文
 *
 * 包含各列表独立子列表，通过 active_list / playing_list 控制播放范围。
 * ALL 模式启用跨列表遍历。
 */
typedef struct {
    voice_music_sub_list_t lists[MUSIC_LIST_COUNT];  /**< 子列表（按列表索引） */
    voice_music_list_id_t active_list;            /**< 播放范围：具体列表或 ALL */
    voice_music_mode_t mode;                       /**< 当前播放模式 */
    lisa_mutex_t *lock;                            /**< 互斥锁 */
} voice_music_list_ctx_t;

/** 全局音乐列表实例（PSRAM 分配） */
static voice_music_list_ctx_t *g_list;

/* SD 扫描等异步调用方可能早于/无视 init 结果访问本模块，所有公开接口先查就绪 */
static bool _list_ready(void)
{
    return g_list != NULL && g_list->lock != NULL;
}

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief   获取当前正在播放的子列表
 * @return  子列表指针，NULL 表示列表为空
 *
 * @note    当 active_list == ALL 时，查找第一个非空子列表作为 playing_list。
 *          子列表的 current_index 在该子列表内有效。
 */
static voice_music_sub_list_t *_playing_list(void)
{
    if (g_list->active_list == MUSIC_LIST_ALL) {
        /* 返回第一个有播放进度的子列表，否则返回第一个非空子列表 */
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            if (g_list->lists[i].current_index >= 0) {
                return &g_list->lists[i];
            }
        }
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            if (g_list->lists[i].count > 0) {
                return &g_list->lists[i];
            }
        }
        return NULL;
    }
    voice_music_sub_list_t *list = &g_list->lists[g_list->active_list];
    return (list->count > 0) ? list : NULL;
}

/**
 * @brief   获取指定列表的子列表
 */
static voice_music_sub_list_t *_list_for(voice_music_list_id_t id)
{
    if (id >= MUSIC_LIST_COUNT) {
        return NULL;
    }
    return &g_list->lists[id];
}

/**
 * @brief   获取给定列表索引的下一个非空子列表
 * @param   current 当前列表索引（-1 表示从第一项开始）
 * @return  列表索引，-1 表示未找到
 */
static int _next_nonempty_list(int current)
{
    for (int i = current + 1; i < MUSIC_LIST_COUNT; i++) {
        if (g_list->lists[i].count > 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief   获取给定列表索引的上一个非空子列表
 * @param   current 当前列表索引
 * @return  列表索引，-1 表示未找到
 */
static int _prev_nonempty_list(int current)
{
    for (int i = current - 1; i >= 0; i--) {
        if (g_list->lists[i].count > 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief   获取第一个非空列表索引
 */
static int _first_nonempty_list(void)
{
    return _next_nonempty_list(-1);
}

/**
 * @brief   获取最后一个非空列表索引
 */
static int _last_nonempty_list(void)
{
    for (int i = MUSIC_LIST_COUNT - 1; i >= 0; i--) {
        if (g_list->lists[i].count > 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief   为指定子列表生成 Fisher-Yates 随机播放顺序
 * @param   list 子列表
 */
static void _shuffle_generate(voice_music_sub_list_t *list)
{
    if (list->count == 0) {
        list->shuffle_count = 0;
        return;
    }

    /* 按需分配 shuffle_order */
    if (list->shuffle_order == NULL) {
        list->shuffle_order = lisa_mem_calloc(list->count, sizeof(int));
        if (list->shuffle_order == NULL) {
            list->shuffle_count = 0;
            return;
        }
    }

    /* 初始化顺序索引 */
    for (int i = 0; i < list->count; i++) {
        list->shuffle_order[i] = i;
    }

    /* Fisher-Yates 洗牌 */
    for (int i = list->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = list->shuffle_order[i];
        list->shuffle_order[i] = list->shuffle_order[j];
        list->shuffle_order[j] = tmp;
    }

    list->shuffle_count = list->count;
}

/**
 * @brief   在指定子列表中按步长查找下一个有效索引
 * @param   list 子列表
 * @param   step 步长（+1 下一首，-1 上一首，0 从头找）
 * @return  有效索引，-1 表示未找到
 *
 * @note    会跳过列表不匹配的曲目。如果遍历完整个列表仍未找到则返回 -1。
 *          当 current_index 为 -1 时，从头开始查找第一个匹配的列表的曲目。
 */
static int _filtered_index(voice_music_sub_list_t *list, int step)
{
    if (list->count == 0) {
        return -1;
    }

    if (list->current_index < 0) {
        return 0;
    }

    int cursor = list->current_index;
    for (int i = 0; i < list->count; i++) {
        cursor = (cursor + step + list->count) % list->count;
        if (cursor != list->current_index) {
            return cursor;
        }
    }

    return -1;
}

/**
 * @brief   获取某一列表索引的播放索引
 * @param   src_idx   列表索引
 * @param   list_idx  列表内索引
 * @return  track 列表索引（src_idx 直接就是列表）
 *
 * 跨列表时为调用方提供当前列表信息。
 */

/**
 * @brief   获取某一列表的总曲目数
 */
static int _list_count_for(voice_music_list_id_t id)
{
    if (id == MUSIC_LIST_ALL) {
        int total = 0;
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            total += g_list->lists[i].count;
        }
        return total;
    }
    voice_music_sub_list_t *list = _list_for(id);
    return list ? list->count : 0;
}

/**
 * @brief   在 ALL 模式下按列表顺序查找第 N 首曲目
 * @param   index     目标逻辑索引（所有列表内计数）
 * @param   out_src   输出：找到的曲目所在列表索引
 * @param   out_idx   输出：找到的曲目在子列表内的索引
 * @return  0 成功，-1 失败
 */
static int _find_across_lists(int index, int *out_src, int *out_idx)
{
    int remaining = index;

    for (int s = 0; s < MUSIC_LIST_COUNT; s++) {
        if (g_list->lists[s].count == 0) {
            continue;
        }
        if (remaining < g_list->lists[s].count) {
            *out_src = s;
            *out_idx = remaining;
            return 0;
        }
        remaining -= g_list->lists[s].count;
    }

    return -1;
}

/**
 * @brief   解析曲目的云端播放 URL
 * @param   track 曲目数据
 * @return  0 成功，-1 失败
 */
static int _resolve_url(music_item_t *track)
{
    if (track->m_url[0] != '\0') {
        return 0;
    }
    char url[256] = {0};
    int ret = lsc_music_request_url(track->mid, url);
    if (ret != 0 || url[0] == '\0') {
        LOGE("Failed to resolve URL for: %s", track->mid);
        return -1;
    }
    strncpy(track->m_url, url, AUIDO_OUT_URL_LEN - 1);
    return 0;
}

/**
 * @brief   释放子列表内存
 */
static void _sub_list_free(voice_music_sub_list_t *list)
{
    if (list->items != NULL) {
        lisa_mem_free(list->items);
        list->items = NULL;
    }
    if (list->shuffle_order != NULL) {
        lisa_mem_free(list->shuffle_order);
        list->shuffle_order = NULL;
    }
    list->count = 0;
    list->current_index = -1;
    list->shuffle_count = 0;
}

/**
 * @brief   为子列表设置新内容
 * @return  0 成功，-1 失败（分配失败）
 */
static int _sub_list_set(voice_music_sub_list_t *list, music_item_t *items, int count)
{
    _sub_list_free(list);

    list->items = lisa_mem_calloc(count, sizeof(music_item_t));
    if (list->items == NULL) {
        return -1;
    }

    memcpy(list->items, items, count * sizeof(music_item_t));
    list->count = count;
    list->current_index = -1;

    _shuffle_generate(list);
    return 0;
}

/* ============================================================================
 * 公开 API 实现
 * ============================================================================ */

/**
 * @brief   初始化音乐列表模块
 * @return  0 成功，-1 失败
 *
 * @note    从 PSRAM 分配上下文（约 100 字节），子列表内存在 set 时按需分配。
 */
int voice_music_list_init(void)
{
    if (_list_ready()) {
        return 0;
    }

    voice_music_list_ctx_t *ctx = lisa_mem_calloc(1, sizeof(voice_music_list_ctx_t));
    if (ctx == NULL) {
        LOGE("Failed to alloc music list");
        return -1;
    }

    ctx->active_list = MUSIC_LIST_ALL;
    ctx->mode = MUSIC_MODE_SEQUENTIAL;

    ctx->lock = lisa_mutex_create();
    if (ctx->lock == NULL) {
        LOGE("Failed to create mutex");
        lisa_mem_free(ctx);
        return -1;
    }

    g_list = ctx;
    return 0;
}

/**
 * @brief   设置指定列表的音乐列表（替换该列表的旧列表）
 * @param   id 曲目列表
 * @param   items  曲目数组
 * @param   count  曲目数量
 * @return  0 成功，-1 失败
 *
 * @note    释放该列表旧列表后分配新列表，自动切换 active_list。
 */
int voice_music_list_set(voice_music_list_id_t id, music_item_t *items, int count)
{
    if (items == NULL || count <= 0 || count > MUSIC_LIST_MAX_PER_LIST) {
        return -1;
    }
    if (!_list_ready()) {
        return -1;
    }

    voice_music_sub_list_t *list = _list_for(id);
    if (list == NULL) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    int ret = _sub_list_set(list, items, count);
    if (ret != 0) {
        lisa_mutex_unlock(g_list->lock);
        return -1;
    }

    g_list->active_list = id;

    lisa_mutex_unlock(g_list->lock);

    LOGI("Music list set: id=%d, count=%d", id, count);
    return 0;
}

/**
 * @brief   清空音乐列表
 * @param   id 要清空的列表，MUSIC_LIST_ALL 清空全部
 * @return  0 成功，-1 失败
 */
int voice_music_list_clear(voice_music_list_id_t id)
{
    if (!_list_ready()) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    if (id == MUSIC_LIST_ALL) {
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            _sub_list_free(&g_list->lists[i]);
        }
    } else {
        voice_music_sub_list_t *list = _list_for(id);
        if (list != NULL) {
            _sub_list_free(list);
        }
    }

    lisa_mutex_unlock(g_list->lock);
    return 0;
}

/**
 * @brief   获取指定索引的曲目（逻辑索引，受 active_list 过滤）
 * @param   index 曲目索引（0-based，相对于 active_list 范围）
 * @param   out   输出参数
 * @return  0 成功，-1 失败
 *
 * @note    单一列表：在子列表内按 index 查找。
 *          ALL：按 ONLINE→OFFLINE 顺序跨列表查找，index 为全局逻辑索引。
 */
int voice_music_list_get_by_index(int index, music_item_t *out)
{
    if (out == NULL || index < 0) {
        return -1;
    }
    if (!_list_ready()) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    voice_music_sub_list_t *list = NULL;

    if (g_list->active_list == MUSIC_LIST_ALL) {
        int src_idx = -1, item_idx = -1;
        if (_find_across_lists(index, &src_idx, &item_idx) != 0) {
            lisa_mutex_unlock(g_list->lock);
            return -1;
        }
        list = &g_list->lists[src_idx];
        list->current_index = item_idx;
        /* 清除其他子列表的播放位置 */
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            if (i != src_idx) {
                g_list->lists[i].current_index = -1;
            }
        }
    } else {
        list = &g_list->lists[g_list->active_list];
        if (list->count == 0 || index >= list->count) {
            lisa_mutex_unlock(g_list->lock);
            return -1;
        }
        list->current_index = index;
    }

    memcpy(out, &list->items[list->current_index], sizeof(music_item_t));
    lisa_mutex_unlock(g_list->lock);

    return _resolve_url(out);
}

/**
 * @brief   获取当前正在播放的曲目
 * @param   out 输出参数
 * @return  0 成功，-1 失败
 */
int voice_music_list_get_current(music_item_t *out)
{
    if (out == NULL) {
        return -1;
    }
    if (!_list_ready()) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    voice_music_sub_list_t *list = _playing_list();
    if (list == NULL || list->current_index < 0 || list->current_index >= list->count) {
        lisa_mutex_unlock(g_list->lock);
        return -1;
    }

    memcpy(out, &list->items[list->current_index], sizeof(music_item_t));
    lisa_mutex_unlock(g_list->lock);

    return _resolve_url(out);
}

/**
 * @brief   在单一子列表内按模式获取下一首
 * @param   list 子列表
 * @param   out  输出参数
 * @return  0 成功，-1 结束
 */
static int _get_next_in_list(voice_music_sub_list_t *list, music_item_t *out)
{
    if (list->count == 0) {
        return -1;
    }

    int next = -1;

    switch (g_list->mode) {
    case MUSIC_MODE_ONCE:
        list->current_index = -1;
        return -1;

    case MUSIC_MODE_REPEAT_ONE:
        if (list->current_index >= 0) {
            next = list->current_index;
        }
        break;

    case MUSIC_MODE_SHUFFLE:
        if (list->shuffle_count > 0) {
            for (int i = 0; i < list->shuffle_count; i++) {
                if (list->shuffle_order[i] == list->current_index) {
                    if (i + 1 < list->shuffle_count) {
                        next = list->shuffle_order[i + 1];
                    }
                    break;
                }
            }
            if (next < 0) {
                next = list->shuffle_order[0];
            }
        }
        break;

    case MUSIC_MODE_SEQUENTIAL:
        /* 顺序播放不循环：到末尾停止 */
        if (list->current_index >= 0 && list->current_index + 1 < list->count) {
            next = list->current_index + 1;
        }
        break;

    case MUSIC_MODE_LOOP_ALL:
        next = _filtered_index(list, 1);
        if (next < 0) {
            next = _filtered_index(list, 0);
        }
        break;
    }

    if (next < 0) {
        list->current_index = -1;
        return -1;
    }

    list->current_index = next;
    memcpy(out, &list->items[next], sizeof(music_item_t));
    return 0;
}

/**
 * @brief   获取下一首曲目
 * @param   out 输出参数
 * @return  0 成功，-1 失败
 *
 * @note    单一列表：在当前子列表内按模式推进。
 *          ALL：先尝试在当前子列表推进，到尾后切换下一列表。
 *          SEQUENTIAL 跨列表到尾返回 -1，LOOP_ALL / SHUFFLE 循环回到第一列表。
 */
int voice_music_list_get_next(music_item_t *out)
{
    if (out == NULL) {
        return -1;
    }
    if (!_list_ready()) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    if (g_list->active_list == MUSIC_LIST_ALL) {
        /* 确定当前所在列表 */
        int cur_src = -1;
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            if (g_list->lists[i].current_index >= 0) {
                cur_src = i;
                break;
            }
        }
        if (cur_src < 0) {
            cur_src = _first_nonempty_list();
            if (cur_src < 0) {
                lisa_mutex_unlock(g_list->lock);
                return -1;
            }
        }

        voice_music_sub_list_t *list = &g_list->lists[cur_src];

        if (g_list->mode == MUSIC_MODE_ONCE) {
            list->current_index = -1;
            lisa_mutex_unlock(g_list->lock);
            return -1;
        }

        if (g_list->mode == MUSIC_MODE_REPEAT_ONE) {
            if (list->current_index >= 0 && list->current_index < list->count) {
                memcpy(out, &list->items[list->current_index], sizeof(music_item_t));
                lisa_mutex_unlock(g_list->lock);
                return _resolve_url(out);
            }
            lisa_mutex_unlock(g_list->lock);
            return -1;
        }

        /* 先在当前列表内取下一首 */
        int ret = _get_next_in_list(list, out);
        if (ret == 0) {
            lisa_mutex_unlock(g_list->lock);
            return _resolve_url(out);
        }

        /* 当前列表到头，尝试下一列表 */
        int next_src = _next_nonempty_list(cur_src);

        if (next_src < 0 && (g_list->mode == MUSIC_MODE_LOOP_ALL ||
                             g_list->mode == MUSIC_MODE_SHUFFLE)) {
            next_src = _first_nonempty_list();
        }

        if (next_src < 0) {
            lisa_mutex_unlock(g_list->lock);
            return -1;
        }

        /* 清除当前列表的播放位置，切换到下一列表 */
        list->current_index = -1;
        voice_music_sub_list_t *next_list = &g_list->lists[next_src];

        if (g_list->mode == MUSIC_MODE_SHUFFLE) {
            if (next_list->shuffle_count > 0) {
                next_list->current_index = next_list->shuffle_order[0];
                memcpy(out, &next_list->items[next_list->current_index], sizeof(music_item_t));
                lisa_mutex_unlock(g_list->lock);
                return _resolve_url(out);
            }
        }

        /* SEQUENTIAL / LOOP_ALL 跨列表：从下一列表的第一首开始 */
        next_list->current_index = 0;
        memcpy(out, &next_list->items[0], sizeof(music_item_t));
        lisa_mutex_unlock(g_list->lock);
        return _resolve_url(out);
    }

    /* 单一列表模式 */
    voice_music_sub_list_t *list = &g_list->lists[g_list->active_list];
    int ret = _get_next_in_list(list, out);
    lisa_mutex_unlock(g_list->lock);
    if (ret != 0) {
        return -1;
    }
    return _resolve_url(out);
}

/**
 * @brief   在单一子列表内按模式获取上一首
 */
static int _get_prev_in_list(voice_music_sub_list_t *list, music_item_t *out)
{
    if (list->count == 0) {
        return -1;
    }

    int prev = -1;

    if (g_list->mode == MUSIC_MODE_SHUFFLE && list->shuffle_count > 0) {
        for (int i = 0; i < list->shuffle_count; i++) {
            if (list->shuffle_order[i] == list->current_index) {
                if (i > 0) {
                    prev = list->shuffle_order[i - 1];
                }
                break;
            }
        }
        if (prev < 0 && list->shuffle_count > 0) {
            prev = list->shuffle_order[list->shuffle_count - 1];
        }
    } else {
        /* 非随机模式：根据当前模式决定前一首行为 */
        if (list->current_index < 0) {
            prev = -1;
        } else if (list->current_index == 0) {
            if (g_list->mode == MUSIC_MODE_LOOP_ALL) {
                /* 列表循环：第一首的前一首是最后一首 */
                prev = list->count - 1;
            } else {
                /* 顺序/单次/单曲循环：第一首时重播第一首 */
                prev = 0;
            }
        } else {
            prev = list->current_index - 1;
        }
    }

    if (prev < 0) {
        return -1;
    }

    list->current_index = prev;
    memcpy(out, &list->items[prev], sizeof(music_item_t));
    return 0;
}

/**
 * @brief   获取上一首曲目
 * @param   out 输出参数
 * @return  0 成功，-1 失败
 *
 * @note    单一列表：在当前子列表内后退。
 *          ALL：先尝试在当前子列表内后退，到开头后切换上一列表末尾。
 */
int voice_music_list_get_prev(music_item_t *out)
{
    if (out == NULL) {
        return -1;
    }
    if (!_list_ready()) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    if (g_list->active_list == MUSIC_LIST_ALL) {
        int cur_src = -1;
        for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
            if (g_list->lists[i].current_index >= 0) {
                cur_src = i;
                break;
            }
        }
        if (cur_src < 0) {
            cur_src = _last_nonempty_list();
            if (cur_src < 0) {
                lisa_mutex_unlock(g_list->lock);
                return -1;
            }
        }

        voice_music_sub_list_t *list = &g_list->lists[cur_src];

        int ret = _get_prev_in_list(list, out);
        if (ret == 0) {
            lisa_mutex_unlock(g_list->lock);
            return _resolve_url(out);
        }

        /* 当前列表到头，尝试上一列表 */
        int prev_src = _prev_nonempty_list(cur_src);

        if (prev_src < 0 && (g_list->mode == MUSIC_MODE_LOOP_ALL ||
                             g_list->mode == MUSIC_MODE_SHUFFLE)) {
            prev_src = _last_nonempty_list();
        }

        if (prev_src < 0) {
            lisa_mutex_unlock(g_list->lock);
            return -1;
        }

        list->current_index = -1;
        voice_music_sub_list_t *prev_list = &g_list->lists[prev_src];

        if (g_list->mode == MUSIC_MODE_SHUFFLE) {
            if (prev_list->shuffle_count > 0) {
                int last = prev_list->shuffle_order[prev_list->shuffle_count - 1];
                prev_list->current_index = last;
                memcpy(out, &prev_list->items[last], sizeof(music_item_t));
                lisa_mutex_unlock(g_list->lock);
                return _resolve_url(out);
            }
        }

        /* 跳到上一列表的最后一首 */
        prev_list->current_index = prev_list->count - 1;
        memcpy(out, &prev_list->items[prev_list->current_index], sizeof(music_item_t));
        lisa_mutex_unlock(g_list->lock);
        return _resolve_url(out);
    }

    /* 单一列表模式 */
    voice_music_sub_list_t *list = &g_list->lists[g_list->active_list];
    int ret = _get_prev_in_list(list, out);
    lisa_mutex_unlock(g_list->lock);
    if (ret != 0) {
        return -1;
    }
    return _resolve_url(out);
}

/**
 * @brief   获取当前 active_list 范围的曲目总数
 * @return  曲目数量
 */
int voice_music_list_count(void)
{
    if (!_list_ready()) {
        return 0;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);
    int count = _list_count_for(g_list->active_list);
    lisa_mutex_unlock(g_list->lock);
    return count;
}

/**
 * @brief   获取当前播放索引
 * @return  当前索引（-1 表示未播放）
 */
int voice_music_list_current_index(void)
{
    if (!_list_ready()) {
        return -1;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    voice_music_sub_list_t *list = _playing_list();
    int idx = (list != NULL) ? list->current_index : -1;

    lisa_mutex_unlock(g_list->lock);
    return idx;
}

void voice_music_list_set_current_index(int index)
{
    if (!_list_ready()) {
        return;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);

    voice_music_sub_list_t *list = _playing_list();
    if (list != NULL && index >= 0 && index < list->count) {
        list->current_index = index;
    }

    lisa_mutex_unlock(g_list->lock);
}

/**
 * @brief   设置活跃列表
 * @param   id 要过滤的列表，ALL 启用跨列表遍历
 */
void voice_music_list_set_active(voice_music_list_id_t id)
{
    if (!_list_ready()) {
        return;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);
    g_list->active_list = id;
    /* 切换列表时清除各列表播放位置 */
    for (int i = 0; i < MUSIC_LIST_COUNT; i++) {
        g_list->lists[i].current_index = -1;
    }
    lisa_mutex_unlock(g_list->lock);
}

/**
 * @brief   设置播放模式
 * @param   mode 播放模式
 */
void voice_music_list_set_mode(voice_music_mode_t mode)
{
    if (!_list_ready()) {
        return;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);
    g_list->mode = mode;
    lisa_mutex_unlock(g_list->lock);
}

/**
 * @brief   获取当前活跃列表
 * @return  当前列表设置
 */
voice_music_list_id_t voice_music_list_get_active(void)
{
    if (!_list_ready()) {
        return MUSIC_LIST_ALL;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);
    voice_music_list_id_t id = g_list->active_list;
    lisa_mutex_unlock(g_list->lock);
    return id;
}

/**
 * @brief   获取当前播放模式
 * @return  当前播放模式
 */
voice_music_mode_t voice_music_list_get_mode(void)
{
    if (!_list_ready()) {
        return MUSIC_MODE_SEQUENTIAL;
    }

    lisa_mutex_lock(g_list->lock, LISA_WAIT_FOREVER);
    voice_music_mode_t mode = g_list->mode;
    lisa_mutex_unlock(g_list->lock);
    return mode;
}
