/**
 * @file bt_audio_framework.c
 * @brief 蓝牙音频框架主实现
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_framework.h"

#define BT_AUDIO_FRAMEWORK_VERSION "1.0.0"

static bool g_framework_initialized = false;

bt_audio_error_t bt_audio_framework_init(void)
{
    if (g_framework_initialized) {
        return BT_AUDIO_OK;
    }
    
    bt_audio_error_t ret;
    
    /* 初始化codec管理器 */
    ret = bt_audio_codec_manager_init();
    if (ret != BT_AUDIO_OK) {
        return ret;
    }
    
    /* 注册所有已编译的codec */
#ifdef CONFIG_AUDIO_CODEC_SBC
    extern bt_audio_error_t bt_audio_codec_sbc_register(void);
    ret = bt_audio_codec_sbc_register();
    if (ret != BT_AUDIO_OK) {
        bt_audio_codec_manager_deinit();
        return ret;
    }
#endif
    
#ifdef CONFIG_AUDIO_CODEC_LC3
    extern bt_audio_error_t bt_audio_codec_lc3_register(void);
    ret = bt_audio_codec_lc3_register();
    if (ret != BT_AUDIO_OK) {
        bt_audio_codec_manager_deinit();
        return ret;
    }
#endif
    
#ifdef CONFIG_AUDIO_CODEC_MSBC
    extern bt_audio_error_t bt_audio_codec_msbc_register(void);
    ret = bt_audio_codec_msbc_register();
    if (ret != BT_AUDIO_OK) {
        bt_audio_codec_manager_deinit();
        return ret;
    }
#endif
    
    /* 初始化音频接口管理器 */
    ret = bt_audio_interface_manager_init();
    if (ret != BT_AUDIO_OK) {
        bt_audio_codec_manager_deinit();
        return ret;
    }
    
    /* 注册本地音频接口（物理硬件） */
#ifdef CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL
    extern bt_audio_error_t bt_audio_interface_lisa_register(void);
    ret = bt_audio_interface_lisa_register();
    if (ret != BT_AUDIO_OK) {
        bt_audio_interface_manager_deinit();
        bt_audio_codec_manager_deinit();
        return ret;
    }
#endif

    /* 注册空音频接口（测试用） */
#ifdef CONFIG_LISA_BT_AUDIO_INTERFACE_NULL
    extern bt_audio_error_t bt_audio_interface_null_register(void);
    ret = bt_audio_interface_null_register();
    if (ret != BT_AUDIO_OK) {
        bt_audio_interface_manager_deinit();
        bt_audio_codec_manager_deinit();
        return ret;
    }
#endif
    
    /* 初始化会话管理器 */
    ret = bt_audio_session_manager_init();
    if (ret != BT_AUDIO_OK) {
        bt_audio_interface_manager_deinit();
        bt_audio_codec_manager_deinit();
        return ret;
    }
    
    g_framework_initialized = true;
    return BT_AUDIO_OK;
}

bt_audio_error_t bt_audio_framework_deinit(void)
{
    if (!g_framework_initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 按相反顺序去初始化 */
    bt_audio_session_manager_deinit();
    bt_audio_interface_manager_deinit();
    bt_audio_codec_manager_deinit();
    
    g_framework_initialized = false;
    return BT_AUDIO_OK;
}

const char* bt_audio_framework_get_version(void)
{
    return BT_AUDIO_FRAMEWORK_VERSION;
}
