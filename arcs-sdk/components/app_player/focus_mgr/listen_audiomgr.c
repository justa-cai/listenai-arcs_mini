#define TAG "audiomgr"

#include <stdio.h>
#include <string.h>

#include "listen_audiomgr.h"
#include "lisa_mem.h"
#include "lisa_log.h"

static const char *s_state_names[] = {
    [FOREGROUND] = "FOREGROUND",
    [BACKGROUND] = "BACKGROUND",
    [FOCUS_NONE] = "NONE",
};

const char *listen_audiomgr_get_state_name(focus_state_e state)
{
    if (state > FOCUS_NONE) {
        return "UNKNOWN";
    }
    return s_state_names[state];
}

const char *listen_audiomgr_get_channel_name(listen_audiomgr_t *handle, int id)
{
    if (handle == NULL) {
        return "UNKNOWN";
    }
    for (int i = 0; i < handle->channel_count; i++) {
        if (handle->channels[i].id == id && handle->channels[i].registered) {
            return handle->channels[i].name ? handle->channels[i].name : "UNKNOWN";
        }
    }
    return "UNKNOWN";
}

listen_audiomgr_t *listen_audiomgr_create(void)
{
    listen_audiomgr_t *handle = (listen_audiomgr_t *)lisa_mem_calloc(1, sizeof(listen_audiomgr_t));
    if (handle == NULL) {
        LISA_LOGE(TAG, "Failed to allocate listen_audiomgr_t");
        return NULL;
    }

    handle->channel_count = 0;
    handle->foreground_id = -1;
    handle->background_id = -1;

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        handle->channels[i].registered = false;
        handle->channels[i].state = FOCUS_NONE;
    }

    LISA_LOGI(TAG, "listen_audiomgr created");
    return handle;
}

void listen_audiomgr_destroy(listen_audiomgr_t *handle)
{
    if (handle != NULL) {
        lisa_mem_free(handle);
        LISA_LOGI(TAG, "listen_audiomgr destroyed");
    }
}

int listen_audiomgr_register_channel(listen_audiomgr_t *handle,
                                     int id,
                                     const char *name,
                                     int priority,
                                     int *capture_ids,
                                     int capture_count,
                                     void (*on_focus_change)(focus_state_e, int, void *),
                                     void *user_data)
{
    if (handle == NULL) {
        LISA_LOGE(TAG, "handle is NULL");
        return -1;
    }

    if (handle->channel_count >= MAX_PLAYER_COUNT) {
        LISA_LOGE(TAG, "Max channel count reached");
        return -1;
    }

    // 检查ID是否已存在
    for (int i = 0; i < handle->channel_count; i++) {
        if (handle->channels[i].id == id && handle->channels[i].registered) {
            LISA_LOGE(TAG, "Channel id %d already registered", id);
            return -1;
        }
    }

    // 注册新通道
    focus_channel_t *channel = &handle->channels[handle->channel_count];
    channel->id = id;
    channel->name = (char *)name;
    channel->priority = priority;
    channel->state = FOCUS_NONE;
    channel->on_focus_change = on_focus_change;
    channel->user_data = user_data;
    channel->registered = true;

    // 复制抢占列表
    int copy_count = (capture_count > MAX_CAPTURE_COUNT) ? MAX_CAPTURE_COUNT : capture_count;
    channel->capture_count = copy_count;
    if (capture_ids != NULL && copy_count > 0) {
        memcpy(channel->capture_ids, capture_ids, copy_count * sizeof(int));
    }

    handle->channel_count++;

    LISA_LOGI(TAG, "Channel registered: id=%d, name=%s, priority=%d, capture_count=%d",
              id, name ? name : "NULL", priority, copy_count);
    return 0;
}

/**
 * @brief 根据ID查找通道
 */
static focus_channel_t *_get_channel_by_id(listen_audiomgr_t *handle, int id)
{
    for (int i = 0; i < handle->channel_count; i++) {
        if (handle->channels[i].id == id && handle->channels[i].registered) {
            return &handle->channels[i];
        }
    }
    return NULL;
}

/**
 * @brief 检查 requester 是否可以抢占 target
 */
static bool _can_capture(focus_channel_t *requester, focus_channel_t *target)
{
    if (requester == NULL || target == NULL) {
        return false;
    }

    for (int i = 0; i < requester->capture_count; i++) {
        if (requester->capture_ids[i] == target->id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 设置通道焦点状态
 */
static void _set_channel_focus(listen_audiomgr_t *handle, int id, focus_state_e state, int by_which_id)
{
    focus_channel_t *channel = _get_channel_by_id(handle, id);
    if (channel == NULL) {
        LISA_LOGE(TAG, "Channel id %d not found", id);
        return;
    }

    if (channel->state == state) {
        return;  // 状态未变化
    }

    // 更新前景/背景指针
    if (handle->foreground_id == id && state != FOREGROUND) {
        handle->foreground_id = -1;
    } else if (handle->background_id == id && state != BACKGROUND) {
        handle->background_id = -1;
    }

    if (state == FOREGROUND) {
        handle->foreground_id = id;
    } else if (state == BACKGROUND) {
        handle->background_id = id;
    }

    channel->state = state;

    // 获取触发焦点变化的通道名称（仅用于日志）
    const char *by_which_name = listen_audiomgr_get_channel_name(handle, by_which_id);

    LISA_LOGI(TAG, "Channel focus: id=%d, state=%s, by_which=%s",
              id, listen_audiomgr_get_state_name(state), by_which_name);

    // 调用回调，传递 by_which_id
    if (channel->on_focus_change != NULL) {
        channel->on_focus_change(state, by_which_id, channel->user_data);
    }
}

void listen_audiomgr_acquire_channel(listen_audiomgr_t *handle, int id)
{
    if (handle == NULL) {
        LISA_LOGE(TAG, "handle is NULL");
        return;
    }

    LISA_LOGI(TAG, "Channel acquire: id=%d, name=%s", id, listen_audiomgr_get_channel_name(handle, id));

    focus_channel_t *requester = _get_channel_by_id(handle, id);
    if (requester == NULL) {
        LISA_LOGE(TAG, "Channel id %d not registered", id);
        return;
    }

    focus_channel_t *foreground = (handle->foreground_id >= 0) ?
                                  _get_channel_by_id(handle, handle->foreground_id) : NULL;
    focus_channel_t *background = (handle->background_id >= 0) ?
                                  _get_channel_by_id(handle, handle->background_id) : NULL;

    if (foreground != NULL) {
        // 存在前景通道
        if (_can_capture(requester, foreground)) {
            // 请求者可以抢占当前前景，设置当前前景为NONE，再重新请求
            _set_channel_focus(handle, foreground->id, FOCUS_NONE, id);
            listen_audiomgr_acquire_channel(handle, id);
        } else if (requester->priority == foreground->priority) {
            // 优先级相同，设置当前前景为NONE，请求者变为前景
            _set_channel_focus(handle, foreground->id, FOCUS_NONE, id);
            _set_channel_focus(handle, id, FOREGROUND, id);
        } else if (requester->priority < foreground->priority) {
            // 请求者优先级更高（数值小），当前前景变为背景，请求者变为前景
            if (background != NULL) {
                _set_channel_focus(handle, background->id, FOCUS_NONE, id);
            }
            _set_channel_focus(handle, foreground->id, BACKGROUND, id);
            _set_channel_focus(handle, id, FOREGROUND, id);
        } else {
            // 请求者优先级更低，请求者只能变为背景
            if (background == NULL) {
                _set_channel_focus(handle, id, BACKGROUND, id);
            } else if (requester->priority <= foreground->priority) {
                _set_channel_focus(handle, background->id, FOCUS_NONE, id);
                _set_channel_focus(handle, id, BACKGROUND, id);
            }
        }
    } else if (background != NULL) {
        // 无前景但有背景
        if (requester->priority < background->priority) {
            // 请求者优先级更高，直接变为前景
            _set_channel_focus(handle, id, FOREGROUND, id);
        } else if (requester->priority == background->priority) {
            // 优先级相同
            if (background->id == id) {
                // 同一个通道，从背景升为前景
                _set_channel_focus(handle, id, FOREGROUND, id);
            } else {
                // 不同通道，当前背景变NONE，请求者变前景
                _set_channel_focus(handle, background->id, FOCUS_NONE, id);
                _set_channel_focus(handle, id, FOREGROUND, id);
            }
        } else {
            // 请求者优先级更低，背景升前景，请求者变背景
            _set_channel_focus(handle, background->id, FOREGROUND, id);
            _set_channel_focus(handle, id, BACKGROUND, id);
        }
    } else {
        // 无前景也无背景，直接变为前景
        _set_channel_focus(handle, id, FOREGROUND, id);
    }
}

void listen_audiomgr_release_channel(listen_audiomgr_t *handle, int id)
{
    if (handle == NULL) {
        LISA_LOGE(TAG, "handle is NULL");
        return;
    }

    LISA_LOGI(TAG, "Channel release: id=%d", id);

    focus_channel_t *channel = _get_channel_by_id(handle, id);
    focus_state_e prev_state = channel ? channel->state : FOCUS_NONE;

    // 设置释放的通道为NONE
    _set_channel_focus(handle, id, FOCUS_NONE, id);

    // 如果无前景但有背景，背景升为前景
    if (prev_state == FOREGROUND && handle->foreground_id < 0 && handle->background_id >= 0) {
        _set_channel_focus(handle, handle->background_id, FOREGROUND, id);
    }
}
