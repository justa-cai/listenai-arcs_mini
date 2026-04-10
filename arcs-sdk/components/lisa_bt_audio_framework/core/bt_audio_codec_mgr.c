/**
 * @file bt_audio_codec_mgr.c
 * @brief Codec管理器实现
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_codec.h"
#include <string.h>
#include <stdlib.h>

#define MAX_CODEC_COUNT 8

/* Codec注册表 */
typedef struct {
    const bt_audio_codec_ops_t *ops;
    bool is_registered;
} codec_entry_t;

static struct {
    codec_entry_t codecs[MAX_CODEC_COUNT];
    bool initialized;
} g_codec_mgr;

bt_audio_error_t bt_audio_codec_manager_init(void)
{
    if (g_codec_mgr.initialized) {
        return BT_AUDIO_OK;
    }
    
    memset(&g_codec_mgr, 0, sizeof(g_codec_mgr));
    g_codec_mgr.initialized = true;
    
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_codec_manager_deinit(void)
{
    if (!g_codec_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 清理所有已注册的codec */
    for (int i = 0; i < MAX_CODEC_COUNT; i++) {
        if (g_codec_mgr.codecs[i].is_registered) {
            if (g_codec_mgr.codecs[i].ops && g_codec_mgr.codecs[i].ops->deinit) {
                g_codec_mgr.codecs[i].ops->deinit();
            }
            g_codec_mgr.codecs[i].is_registered = false;
            g_codec_mgr.codecs[i].ops = NULL;
        }
    }
    
    g_codec_mgr.initialized = false;
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_codec_register(const bt_audio_codec_ops_t *codec_ops)
{
    if (!g_codec_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!codec_ops || !codec_ops->name) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 检查是否已注册 */
    for (int i = 0; i < MAX_CODEC_COUNT; i++) {
        if (g_codec_mgr.codecs[i].is_registered &&
            g_codec_mgr.codecs[i].ops->type == codec_ops->type) {
            return BT_AUDIO_ERR_ALREADY_EXISTS;
        }
    }
    
    /* 找到空槽位并注册 */
    for (int i = 0; i < MAX_CODEC_COUNT; i++) {
        if (!g_codec_mgr.codecs[i].is_registered) {
            g_codec_mgr.codecs[i].ops = codec_ops;
            g_codec_mgr.codecs[i].is_registered = true;
            return BT_AUDIO_OK;
        }
    }
    
    return BT_AUDIO_ERR_NO_MEMORY;
}

bt_audio_error_t bt_audio_codec_unregister(bt_audio_codec_type_t codec_type)
{
    if (!g_codec_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    for (int i = 0; i < MAX_CODEC_COUNT; i++) {
        if (g_codec_mgr.codecs[i].is_registered &&
            g_codec_mgr.codecs[i].ops->type == codec_type) {
            
            /* 调用deinit */
            if (g_codec_mgr.codecs[i].ops->deinit) {
                g_codec_mgr.codecs[i].ops->deinit();
            }
            
            g_codec_mgr.codecs[i].is_registered = false;
            g_codec_mgr.codecs[i].ops = NULL;
            return BT_AUDIO_OK;
        }
    }
    
    return BT_AUDIO_ERR_NOT_FOUND;
}

const bt_audio_codec_ops_t* bt_audio_codec_get(bt_audio_codec_type_t codec_type)
{
    if (!g_codec_mgr.initialized) {
        return NULL;
    }
    
    for (int i = 0; i < MAX_CODEC_COUNT; i++) {
        if (g_codec_mgr.codecs[i].is_registered &&
            g_codec_mgr.codecs[i].ops->type == codec_type) {
            return g_codec_mgr.codecs[i].ops;
        }
    }
    
    return NULL;
}
