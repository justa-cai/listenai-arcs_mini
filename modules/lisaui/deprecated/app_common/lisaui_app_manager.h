/**
 * @file lisaui_app_manager.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-24
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_APP_MANAGER_H__
#define __LISAUI_APP_MANAGER_H__
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lisaui_dbus.h"
#include "lisaui_view_manager.h"
#include "lisaui_type.h"
#include "lisaui_log.h"

#ifndef APP_ICON_ZOOM
#define APP_ICON_ZOOM(x) ((uint16_t)((x) * 256))
#endif

struct app_icon_t {
    const char *title;
    lisaui_icon_res_t *icon;
    lv_obj_t *lv_obj_icon;
    uint16_t zoom;
    uint16_t icon_width;
    uint16_t icon_height;
};

// typedef enum lisaui_app_type_e {
//     LISAUI_APP_TYPE_NORMAL = 0,
//     LISAUI_APP_TYPE_USER,
//     LISAUI_APP_TYPE_SYSTEM, // 系统应用,会自动创建并启用
// } lisaui_app_type_t;

#define LISAUI_APP_TYPE_NORMAL   (0)
#define LISAUI_APP_TYPE_USER     (1)
#define LISAUI_APP_TYPE_SYSTEM   (2)
#define LISAUI_APP_TYPE_LAUNCHER (3)

#define LISAUI_APP_MAX     (30) // 支持注册最大 App 数目
#define LISAUI_APP_ID_NONE (0)  // 默认 App ID

#define LISAUI_APP_MAX_RUNNING (5)  // 支持最多运行的 App 数目
#define LISAUI_APP_HASH_SIZE   (19) //
struct app_info_t {
    const char *name;
    const char *package_name;
    const int id;
    int uuid;
    const int type;
};

typedef enum {
    LISAUI_EVENT_OP_BACK_VIEW,
    LISAUI_EVENT_OP_CLOSE_VIEW,
    LISAUI_EVENT_OP_ENTER_HOME_VIEW,

    LISAUI_EVENT_APP_EXIT,
    LISAUI_EVENT_APP_ENTER,
    LISAUI_EVENT_APP_CREATE,
    LISAUI_EVENT_APP_DESTROY,
    LISAUI_EVENT_APP_CLOSE,
} lisaui_event_t;

typedef struct _lisaui_app_t {
    lisaui_err_t (*create)(void *parent);
    lisaui_err_t (*destroy)(void);
    lisaui_err_t (*enter)(void);
    lisaui_err_t (*exit)(void);
    lisaui_err_t (*event_handler)(const int id, lisaui_event_t event, void *param);

    void *(*get_app_view)(void);

    struct app_info_t info;
    struct app_icon_t *icon;
    uint8_t hidden_icon;
    struct _lisaui_app_t *app;
    lisaui_view_stack_t view_stack;
} lisaui_app_t;

typedef struct {
    const char *app_name;
    lisaui_err_t (*app_init_func)(void);
} app_entry_t;

typedef struct _app_running_t {
    lisaui_app_t *app;
    int valid;
} app_runtime_t;

typedef struct _app_hash_slot_t {
    int key;
    lisaui_app_t *app;
} app_hash_slot_t;

struct lisaui_app_manager_t {
    bool initialized;
    lisaui_app_t *apps_list[LISAUI_APP_MAX]; // 已注册的 App 列表
    lisaui_dbus_t *dbus;
    lisaui_view_stack_t manager_view_stack;
    int current_appid;
    int previous_appid;
    int registered_count;
    int unhidden_count; // 未隐藏的 App icon 的数量
    bool lock_app_view;

    app_runtime_t running_queue[LISAUI_APP_MAX_RUNNING];
    int queue_front;
    int queue_rear;
    int running_count;

    app_hash_slot_t app_hash[LISAUI_APP_HASH_SIZE];
};

#ifdef __APPLE__
// macOS 的 Mach-O 格式要求段和节名称
#define LISAUI_APP_SECTION __attribute__((used, section("__DATA,.lisaui_apps")))
#else
// Linux 或其他平台的 ELF 格式
#define LISAUI_APP_SECTION __attribute__((used, section(".lisaui_apps")))
#endif

#define CONFIG_LISAUI_APP_TEMPLATE_MACRO_ENABLE 1
#ifndef CONFIG_LISAUI_APP_DEFAULT_VIEW_DEPTH
#define CONFIG_LISAUI_APP_DEFAULT_VIEW_DEPTH 10
#endif

#if CONFIG_LISAUI_APP_TEMPLATE_MACRO_ENABLE
#define LISAUI_REGISTER_APP(name, p_app, init_func)                                                                    \
    LISAUI_APP_SECTION                                                                                                 \
    const app_entry_t lisaui_app_##name##_entry = {                                                                    \
        .app_name = #name,                                                                                             \
        .app_init_func = init_func,                                                                                    \
    }

#define LISAUI_USE_APP(name)                                                                                           \
    extern const app_entry_t lisaui_app_##name##_entry;                                                                \
    __attribute__((unused)) void *_app_##name = (void *)&lisaui_app_##name##_entry

#define LISAUI_DECLARE_APP_FUNC(name, func_create, func_destroy, func_enter, func_exit, func_get_view)                 \
    lisaui_err_t app_##name##_##func_create(void *parent);                                                             \
    lisaui_err_t app_##name##_##func_destroy(void);                                                                    \
    lisaui_err_t app_##name##_##func_enter(void);                                                                      \
    lisaui_err_t app_##name##_##func_exit(void);                                                                       \
    void *app_##name##_get_view(void);

#define LISAUI_DEFINE_APP_FUNC(name, func, param, entity)                                                              \
    app_##name##_##func(param)                                                                                         \
    {                                                                                                                  \
        entity LISAUI_LOGI(TAG, "[%d:%s] " #func "\n", __LINE__, __func__);                                            \
    }

#define LISAUI_DEFINE_APP(app, app_id, name_cn, name_en, app_icon, init_entity)                                        \
    static struct app_icon_t app_icon_res_##app = {                                                                    \
        .title = name_cn,                                                                                              \
        .icon = app_icon,                                                                                              \
        .zoom = APP_ICON_ZOOM(0),                                                                                      \
    };                                                                                                                 \
    lisaui_app_t app_##app = {                                                                                         \
        .create = app_##app##_create,                                                                                  \
        .destroy = app_##app##_destroy,                                                                                \
        .enter = app_##app##_enter,                                                                                    \
        .exit = app_##app##_exit,                                                                                      \
        .get_app_view = app_##app##_get_view,                                                                          \
        .info =                                                                                                        \
            {                                                                                                          \
                .name = name_en,                                                                                       \
                .package_name = "com.listenai.lisaui." #app,                                                           \
                .id = app_id,                                                                                          \
            },                                                                                                         \
        .icon = &app_icon_res_##app,                                                                                   \
        .view_stack =                                                                                                  \
            {                                                                                                          \
                .capacity = CONFIG_LISAUI_APP_DEFAULT_VIEW_DEPTH,                                                      \
                .depth = 0,                                                                                            \
                .head = NULL,                                                                                          \
                .tail = NULL,                                                                                          \
            },                                                                                                         \
    };                                                                                                                 \
    lisaui_err_t app_##app##_init(void)                                                                                \
    {                                                                                                                  \
        lisaui_app_register(&app_##app);                                                                               \
        init_entity;                                                                                                   \
        return LISAUI_ERR_OK;                                                                                          \
    }                                                                                                                  \
    LISAUI_REGISTER_APP(app, &app_##app, app_##app##_init)

#define LISAUI_APP_INIT_CHECK(app_id, obj, err, ret)                                                                   \
    if (obj == NULL) {                                                                                                 \
        lisaui_app_t *app = lisaui_app_manager_get_app(app_id);                                                        \
        if (app == NULL) {                                                                                             \
            LISAUI_LOGE(TAG, "[%s] %d not registered", __func__, app_id);                                              \
            return err;                                                                                                \
        }                                                                                                              \
        if (lisaui_app_enter(app_id) != LISAUI_ERR_OK) {                                                               \
            LISAUI_LOGE(TAG, "[%s] %s not init", __func__, app->info.name);                                            \
            return err;                                                                                                \
        }                                                                                                              \
        obj = app->get_app_view();                                                                                     \
        if (obj == NULL) {                                                                                             \
            return err;                                                                                                \
        }                                                                                                              \
    }

#define LISAUI_APP_ENTITY(app)     app_##app
#define LISAUI_APP_VIEW_STACK(app) (app_##app.app_view_stack)
#endif

#define LISAUI_EXEC_HOOK(hook, app, ret)                                                                               \
    if (hook) {                                                                                                        \
        ret = hook(app);                                                                                               \
    }

#define LISAUI_DEFINE_WEAK_FUNC __attribute__((weak))

/**
 * @brief App manager 相关 hook 函数，用于扩展应用程序管理器功能
 *
 * @note 目前只支持 APP 注册、注销、进入、退出、关闭时的 hook, 且只支持一个 hook
 *
 */
typedef lisaui_err_t (*lisaui_app_manager_app_hook_t)(lisaui_app_t *app);
lisaui_err_t lisaui_app_manager_set_register_app_hook(lisaui_app_manager_app_hook_t hook);
lisaui_err_t lisaui_app_manager_set_unregister_app_hook(lisaui_app_manager_app_hook_t hook);
lisaui_err_t lisaui_app_manager_set_enter_app_hook(lisaui_app_manager_app_hook_t hook);
lisaui_err_t lisaui_app_manager_set_exit_app_hook(lisaui_app_manager_app_hook_t hook);
lisaui_err_t lisaui_app_manager_set_close_app_hook(lisaui_app_manager_app_hook_t hook);
lisaui_err_t lisaui_app_manager_default_hook_init(void);
lisaui_err_t lisaui_app_manager_default_hook_deinit(void);

typedef lisaui_err_t (*lisaui_app_manager_hook_t)(struct lisaui_app_manager_t *app_manager);
lisaui_err_t lisaui_app_manager_set_hook(lisaui_app_manager_hook_t hook);

lisaui_err_t lisaui_app_register(lisaui_app_t *app);
lisaui_err_t lisaui_app_unregister(lisaui_app_t *app);
lisaui_err_t lisaui_app_enter(const int app_id);
lisaui_err_t lisaui_app_exit(const int app_id);
lisaui_err_t lisaui_app_close(const int app_id);

lisaui_err_t lisaui_app_sent_event(const int app_id, lisaui_event_t event, void *param);
lisaui_err_t lisaui_app_sent_event_to_all(lisaui_event_t event, void *param);

lisaui_err_t lisaui_app_lock(void);
lisaui_err_t lisaui_app_unlock(void);
bool lisaui_app_get_lock_state(void);

lisaui_err_t lisaui_app_manager_init(void);
lisaui_err_t lisaui_app_manager_deinit(void);
lisaui_err_t lisaui_app_manager_get_bus(lisaui_dbus_t **dbus);
lisaui_err_t lisaui_app_manager_get_view_stack(lisaui_view_stack_t **view_stack);

lisaui_err_t lisaui_app_manager_show_app_info(lisaui_app_t *app);
lisaui_err_t lisaui_app_manager_show_all_app_info(void);
lisaui_app_t **lisaui_app_manager_get_app_lists(void);

lisaui_err_t lisaui_app_manager_get_app_by_uuid(const int uuid, lisaui_app_t **app);
lisaui_err_t lisaui_app_manager_get_app_by_id(const int app_id, lisaui_app_t **app);
lisaui_app_t *lisaui_app_manager_get_app(const int app_id);

int lisaui_app_manager_get_registered_count(void);
int lisaui_app_manager_get_unhidden_count(void);
int lisaui_app_manager_get_current_appid(void);

void lisaui_app_manager_remove_instance_by_uuid(int uuid);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // __LISAUI_APP_MANAGER_H__