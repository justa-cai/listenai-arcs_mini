/**
 * @file bt_audio_interface_mgr.c
 * @brief 音频接口管理器实现
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_interface.h"
#include <string.h>
#include <stdlib.h>

#define MAX_INTERFACE_COUNT 4

/* 接口注册表 */
typedef struct {
    const bt_audio_interface_ops_t *ops;
    bool is_registered;
} interface_entry_t;

static struct {
    interface_entry_t interfaces[MAX_INTERFACE_COUNT];
    const bt_audio_interface_ops_t *default_interface;
    bool initialized;
} g_interface_mgr;

bt_audio_error_t bt_audio_interface_manager_init(void)
{
    if (g_interface_mgr.initialized) {
        return BT_AUDIO_OK;
    }
    
    memset(&g_interface_mgr, 0, sizeof(g_interface_mgr));
    g_interface_mgr.initialized = true;
    
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_interface_manager_deinit(void)
{
    if (!g_interface_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 清理所有已注册的接口 */
    for (int i = 0; i < MAX_INTERFACE_COUNT; i++) {
        g_interface_mgr.interfaces[i].is_registered = false;
        g_interface_mgr.interfaces[i].ops = NULL;
    }
    
    g_interface_mgr.default_interface = NULL;
    g_interface_mgr.initialized = false;
    
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_interface_register(const bt_audio_interface_ops_t *interface_ops)
{
    if (!g_interface_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!interface_ops || !interface_ops->name) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 检查是否已注册 */
    for (int i = 0; i < MAX_INTERFACE_COUNT; i++) {
        if (g_interface_mgr.interfaces[i].is_registered &&
            strcmp(g_interface_mgr.interfaces[i].ops->name, interface_ops->name) == 0) {
            return BT_AUDIO_ERR_ALREADY_EXISTS;
        }
    }
    
    /* 找到空槽位并注册 */
    for (int i = 0; i < MAX_INTERFACE_COUNT; i++) {
        if (!g_interface_mgr.interfaces[i].is_registered) {
            g_interface_mgr.interfaces[i].ops = interface_ops;
            g_interface_mgr.interfaces[i].is_registered = true;
            
            /* 如果是第一个注册的接口，设置为默认 */
            if (!g_interface_mgr.default_interface) {
                g_interface_mgr.default_interface = interface_ops;
            }
            
            return BT_AUDIO_OK;
        }
    }
    
    return BT_AUDIO_ERR_NO_MEMORY;
}

bt_audio_error_t bt_audio_interface_unregister(const char *name)
{
    if (!g_interface_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!name) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_INTERFACE_COUNT; i++) {
        if (g_interface_mgr.interfaces[i].is_registered &&
            strcmp(g_interface_mgr.interfaces[i].ops->name, name) == 0) {
            
            /* 如果是默认接口，清除默认设置 */
            if (g_interface_mgr.default_interface == g_interface_mgr.interfaces[i].ops) {
                g_interface_mgr.default_interface = NULL;
            }
            
            g_interface_mgr.interfaces[i].is_registered = false;
            g_interface_mgr.interfaces[i].ops = NULL;
            return BT_AUDIO_OK;
        }
    }
    
    return BT_AUDIO_ERR_NOT_FOUND;
}

const bt_audio_interface_ops_t* bt_audio_interface_get(const char *name)
{
    if (!g_interface_mgr.initialized || !name) {
        return NULL;
    }
    
    for (int i = 0; i < MAX_INTERFACE_COUNT; i++) {
        if (g_interface_mgr.interfaces[i].is_registered &&
            strcmp(g_interface_mgr.interfaces[i].ops->name, name) == 0) {
            return g_interface_mgr.interfaces[i].ops;
        }
    }
    
    return NULL;
}

const bt_audio_interface_ops_t* bt_audio_interface_get_default(void)
{
    if (!g_interface_mgr.initialized) {
        return NULL;
    }
    
    return g_interface_mgr.default_interface;
}

bt_audio_error_t bt_audio_interface_set_default(const char *name)
{
    if (!g_interface_mgr.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!name) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    const bt_audio_interface_ops_t *ops = bt_audio_interface_get(name);
    if (!ops) {
        return BT_AUDIO_ERR_NOT_FOUND;
    }
    
    g_interface_mgr.default_interface = ops;
    return BT_AUDIO_OK;
}
