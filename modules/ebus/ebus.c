#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define TAG "ebus.core"

#include "port/dlist.h"
#include "port/platform.h"
#include "ebus/ebus.h"

#include "lisa_thread.h"
#include "lisa_log.h"
#include "lisa_queue.h"
#include "lisa_time.h"
#include "lisa_semaphore.h"

#define EBUS_NAME_MAX_LEN     32
#define EBUS_CHN_NAME_MAX_LEN 32

#define EBUS_CB_EXEC_TIME_MS_MAX            (CONFIG_EBUS_PERF_MONITOR_TIME_MS_MAX)
#define EBUS_CB_EXEC_TIME_MS_WARN_THRESHOLD (CONFIG_EBUS_PERF_MONITOR_WARN_THRESHOLD_MS)

typedef struct ebus_chn {
    char name[EBUS_CHN_NAME_MAX_LEN];
    void *bus;
    sys_dnode_t node;
    sys_dlist_t subscriber_list;
    ebus_env_mutex_handle_t mutex;
} ebus_chn_t;

typedef struct {
    ebus_chn_cb_t cb;
    void *user_data;
    ebus_subscribe_type_e type;
    uint32_t filter;
    sys_dnode_t node;
} ebus_chn_subscriber_t;

typedef struct ebus_handle {
    char name[EBUS_NAME_MAX_LEN];
    sys_dnode_t node;
    sys_dlist_t chn_list;
    ebus_env_mutex_handle_t mutex;
    lisa_queue_t *queue;
    lisa_thread_t *thread;
    lisa_thread_t *daemon_thread;
    lisa_semaphore_t *daemon_start_sem;
    lisa_semaphore_t *daemon_stop_sem;
    void *curr_cb;
    void *curr_evt;
    void *curr_ch;
} ebus_handle_t;

typedef struct {
    ebus_env_mutex_handle_t mutex;
    sys_dlist_t bus_list;
} _ebus_list_t;

static _ebus_list_t *s_ebus_list = NULL;

struct ebus_msg {
    ebus_chn_t *chn;
    uint32_t evt;
    void *data;
    uint32_t len;
};

int ebus_init(void)
{
    LOGI("ebus_init");
    if (s_ebus_list != NULL) {
        LOGI("ebus_init, ebus_list is not null");
        return 0;
    }

    s_ebus_list = platform_malloc(sizeof(_ebus_list_t));
    if (s_ebus_list == NULL) {
        LOGE("ebus_init, malloc ebus_list failed");
        return -ENOMEM;
    }

    memset(s_ebus_list, 0, sizeof(_ebus_list_t));

    if (0 != ebus_env_mutex_create(&s_ebus_list->mutex)) {
        LOGE("ebus_init, create mutex failed");
        platform_free(s_ebus_list);
        return -ENOMEM;
    }

    sys_dlist_init(&s_ebus_list->bus_list);
    LOGI("ebus_init, ebus_list init success");

    return 0;
}

ebus_handle_t *ebus_create(const char *bus_name)
{

    ebus_handle_t *bus;

    ebus_env_mutex_lock(&s_ebus_list->mutex, EBUS_ENV_MAX_DELAY);
    SYS_DLIST_FOR_EACH_CONTAINER(&s_ebus_list->bus_list, bus, node)
    {
        if (strcmp(bus->name, bus_name) == 0) {
            ebus_env_mutex_unlock(&s_ebus_list->mutex);
            return NULL;
        }
    }
    ebus_env_mutex_unlock(&s_ebus_list->mutex);
    bus = platform_malloc(sizeof(ebus_handle_t));
    if (bus == NULL) {
        return NULL;
    }

    memset(bus, 0, sizeof(ebus_handle_t));
    snprintf(bus->name, sizeof(bus->name), "%s", bus_name);
    ebus_env_mutex_create(&bus->mutex);
    sys_dlist_init(&bus->chn_list);

    sys_dlist_append(&s_ebus_list->bus_list, &bus->node);

    return bus;

_FOUND:
    EBUS_ERR("ebus %s already exists", bus_name);
    platform_free(bus);
    return NULL;
}

ebus_handle_t *ebus_find(const char *bus_name)
{
    ebus_handle_t *bus;

    ebus_env_mutex_lock(&s_ebus_list->mutex, EBUS_ENV_MAX_DELAY);
    SYS_DLIST_FOR_EACH_CONTAINER(&s_ebus_list->bus_list, bus, node)
    {
        if (strcmp(bus->name, bus_name) == 0) {
            ebus_env_mutex_unlock(&s_ebus_list->mutex);
            return bus;
        }
    }
    ebus_env_mutex_unlock(&s_ebus_list->mutex);

    return NULL;
}

ebus_handle_t *ebus_ch_bus_get(ebus_chn_t *ch)
{
    return ch ? ch->bus : NULL;
}

ebus_chn_t *ebus_ch_find(ebus_handle_t *bus, const char *chn_name)
{
    ebus_chn_t *chn;

    if (bus == NULL || chn_name == NULL) {
        return NULL;
    }

    ebus_env_mutex_lock(&bus->mutex, EBUS_ENV_MAX_DELAY);
    SYS_DLIST_FOR_EACH_CONTAINER(&bus->chn_list, chn, node)
    {
        if (strcmp(chn->name, chn_name) == 0) {
            ebus_env_mutex_unlock(&bus->mutex);
            return chn;
        }
    }
    ebus_env_mutex_unlock(&bus->mutex);

    return NULL;
}

ebus_chn_t *ebus_ch_find_by_name(const char *bus_name, const char *chn_name)
{
    ebus_handle_t *bus;
    ebus_chn_t *chn;

    bus = ebus_find(bus_name);
    if (bus == NULL) {
        return NULL;
    }

    chn = ebus_ch_find(bus, chn_name);
    if (chn == NULL) {
        return NULL;
    }

    return chn;
}

static void ebus_daemon_thread(void *arg)
{
    ebus_handle_t *bus = (ebus_handle_t *)arg;
    while (1) {
        lisa_semaphore_take(bus->daemon_start_sem, LISA_OS_WAIT_FOREVER);
        if (lisa_semaphore_take(bus->daemon_stop_sem, EBUS_CB_EXEC_TIME_MS_MAX) != LISA_OK) {
            LOGE("ebus performance monitor, bus blocked, bus: %s, chn: %p evt: %d cb: %p", bus->name, bus->curr_ch,
                 bus->curr_evt, bus->curr_cb);
#if CONFIG_EBUS_PERF_MONITOR_PANIC_ON_TIMEOUT
            /* 运行到这里, 说明总线上存在事件处理函数执行时间超过EBUS_CB_EXEC_TIME_MS_MAX */
            assert(0);
#endif
        }
    }
}

static void ebus_thread_entry(void *arg)
{
    ebus_handle_t *bus = (ebus_handle_t *)arg;
    struct ebus_msg msg;
    while (1) {
        lisa_queue_t *queue = bus->queue;
        assert(queue != NULL);
        lisa_err_t st = lisa_queue_pop(queue, &msg, sizeof(msg), LISA_OS_WAIT_FOREVER);
        if (st != LISA_OK) {
            continue;
        }
        ebus_chn_t *chn = msg.chn;
        if (chn == NULL) {
            continue;
        }
        LOGI("ebus msg received, chn:%p, evt:%d", chn, msg.evt);
        ebus_chn_subscriber_t *subscriber;

        ebus_env_mutex_lock(&chn->mutex, EBUS_ENV_MAX_DELAY);
        SYS_DLIST_FOR_EACH_CONTAINER(&chn->subscriber_list, subscriber, node)
        {
            if (((subscriber->filter == EBUS_EVENT_ALL) || (subscriber->filter == msg.evt)) &&
                (subscriber->cb != NULL)) {
#if CONFIG_EBUS_PERF_MONITOR
                uint32_t time = lisa_os_get_tick_ms();
                bus->curr_ch = chn;
                bus->curr_evt = msg.evt;
                bus->curr_cb = subscriber->cb;
                assert(bus->daemon_start_sem != NULL);
                lisa_semaphore_give(bus->daemon_start_sem);
#endif
                subscriber->cb(chn, msg.evt, msg.data, msg.len, subscriber->user_data);

#if CONFIG_EBUS_PERF_MONITOR
                assert(bus->daemon_stop_sem != NULL);
                lisa_semaphore_give(bus->daemon_stop_sem);
                uint32_t elapsed = lisa_os_get_tick_ms() - time;
                if (elapsed > EBUS_CB_EXEC_TIME_MS_WARN_THRESHOLD) {
                    LOGW("event bus performance monitor, event %d exec elapsed time: %d/%d ms, cb:%p", msg.evt, elapsed,
                         EBUS_CB_EXEC_TIME_MS_WARN_THRESHOLD, subscriber->cb);
                }
#endif
            }
        }
        ebus_env_mutex_unlock(&chn->mutex);
        if (msg.data != NULL) {
            platform_free(msg.data);
        }
    }
}

ebus_handle_t *ebus_create_async(const char *bus_name, uint32_t queue_size, uint32_t thread_stack_size,
                                 uint32_t thread_priority)
{
    ebus_handle_t *bus;

    bus = ebus_find(bus_name);
    if (bus != NULL) {
        return bus;
    }

    bus = ebus_create(bus_name);
    if (bus == NULL) {
        return NULL;
    }

    bus->queue = lisa_queue_create(queue_size, (char *)bus_name, sizeof(struct ebus_msg));
    if (bus->queue == NULL) {
        ebus_destroy(bus);
        return NULL;
    }
#if CONFIG_EBUS_PERF_MONITOR
    bus->curr_ch = NULL;
    bus->curr_evt = NULL;
    bus->curr_cb = NULL;

    bus->daemon_start_sem = lisa_semaphore_create(1);
    if (bus->daemon_start_sem == NULL) {
        lisa_queue_delete(bus->queue);
        ebus_destroy(bus);
        return NULL;
    }

    bus->daemon_stop_sem = lisa_semaphore_create(1);
    if (bus->daemon_stop_sem == NULL) {
        lisa_queue_delete(bus->queue);
        lisa_semaphore_delete(bus->daemon_start_sem);
        ebus_destroy(bus);
        return NULL;
    }

    lisa_thread_attr_t daemon_attr = {
        .name = (char *)"ebus_daemon",
        .stack_size = 2048,
        .priority = thread_priority + 1,
    };
    bus->daemon_thread = lisa_thread_create(&daemon_attr, ebus_daemon_thread, bus);
    if (bus->daemon_thread == NULL) {
        lisa_queue_delete(bus->queue);
        lisa_semaphore_delete(bus->daemon_start_sem);
        lisa_semaphore_delete(bus->daemon_stop_sem);
        ebus_destroy(bus);
        return NULL;
    }
#endif

    lisa_thread_attr_t attr = {
        .name = (char *)bus_name,
        .stack_size = thread_stack_size,
        .priority = thread_priority,
    };
    bus->thread = lisa_thread_create(&attr, ebus_thread_entry, bus);
    if (bus->thread == NULL) {
#if CONFIG_EBUS_PERF_MONITOR
        lisa_semaphore_delete(bus->daemon_start_sem);
        lisa_semaphore_delete(bus->daemon_stop_sem);
        lisa_thread_delete(bus->daemon_thread);
#endif
        lisa_queue_delete(bus->queue);
        ebus_destroy(bus);
        return NULL;
    }

    return bus;
}

int ebus_destroy(ebus_handle_t *bus)
{
    ebus_env_mutex_lock(&s_ebus_list->mutex, EBUS_ENV_MAX_DELAY);

    /* remove bus node from the list */
    sys_dlist_remove(&bus->node);

    ebus_env_mutex_unlock(&s_ebus_list->mutex);

    platform_free(bus);

    return 0;
}

ebus_chn_t *ebus_chn_create_attach(ebus_handle_t *bus, const char *chn_name)
{
    ebus_chn_t *chn;
    ebus_chn_t *chn_tmp;

    chn = platform_malloc(sizeof(ebus_chn_t));
    if (chn == NULL) {
        return NULL;
    }

    memset(chn, 0, sizeof(ebus_chn_t));
    if (0 != ebus_env_mutex_create(&chn->mutex)) {
        platform_free(chn);
        return NULL;
    }
    snprintf(chn->name, sizeof(chn->name), "%s", chn_name);
    sys_dlist_init(&chn->subscriber_list);
    chn->bus = bus;

    ebus_env_mutex_lock(&bus->mutex, EBUS_ENV_MAX_DELAY);

    SYS_DLIST_FOR_EACH_CONTAINER(&bus->chn_list, chn_tmp, node)
    {
        if (strcmp(chn_tmp->name, chn_name) == 0) {
            goto _FOUND;
        }
    }

    sys_dlist_append(&bus->chn_list, &chn->node);
    ebus_env_mutex_unlock(&bus->mutex);

    return chn;

_FOUND:
    EBUS_ERR("ebus %s chn %s already exists", bus->name, chn_name);
    ebus_env_mutex_destroy(&chn->mutex);
    platform_free(chn);
    return NULL;
}

ebus_chn_t *ebus_chn_bind(const char *bus_name, const char *chn_name)
{
    ebus_handle_t *bus;
    ebus_chn_t *chn = NULL;
    int found = 0;

    if ((s_ebus_list == NULL) || (bus_name == NULL) || (chn_name == NULL)) {
        return NULL;
    }

    while (!found) {
        ebus_env_mutex_lock(&s_ebus_list->mutex, EBUS_ENV_MAX_DELAY);
        SYS_DLIST_FOR_EACH_CONTAINER(&s_ebus_list->bus_list, bus, node)
        {
            if (strcmp(bus->name, bus_name) == 0) {
                SYS_DLIST_FOR_EACH_CONTAINER(&bus->chn_list, chn, node)
                {
                    if (strcmp(chn->name, chn_name) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
        }
        ebus_env_mutex_unlock(&s_ebus_list->mutex);

        if (!found) {
            ebus_env_thread_delay_ms(10);
        }
    }

    return chn;
}

ebus_chn_t *ebus_chn_get(const char *bus_name, const char *chn_name)
{
    return ebus_chn_bind(bus_name, chn_name);
}

int ebus_message_subscribe(ebus_chn_t *chn, ebus_subscribe_type_e type, uint32_t code, ebus_chn_cb_t cb,
                           void *user_data)
{
    ebus_chn_subscriber_t *subscriber;

    if ((s_ebus_list == NULL) || (chn == NULL)) {
        return -EINVAL;
    }

    subscriber = platform_malloc(sizeof(ebus_chn_subscriber_t));
    if (subscriber == NULL) {
        return -ENOMEM;
    }

    memset(subscriber, 0, sizeof(ebus_chn_subscriber_t));
    subscriber->cb = cb;
    subscriber->type = type;
    subscriber->filter = code;
    subscriber->user_data = user_data;

    ebus_env_mutex_lock(&chn->mutex, EBUS_ENV_MAX_DELAY);
    sys_dlist_append(&chn->subscriber_list, &subscriber->node);
    ebus_env_mutex_unlock(&chn->mutex);

    return 0;
}

int ebus_message_unsubscribe(ebus_chn_t *chn, ebus_chn_cb_t cb)
{

    ebus_chn_subscriber_t *subscriber;

    ebus_env_mutex_lock(&chn->mutex, EBUS_ENV_MAX_DELAY);
    SYS_DLIST_FOR_EACH_CONTAINER(&chn->subscriber_list, subscriber, node)
    {
        if (subscriber->cb == cb) {
            sys_dlist_remove(&subscriber->node);
            platform_free(subscriber);
        }
    }
    ebus_env_mutex_unlock(&chn->mutex);
    return 0;
}

int ebus_message_pub(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size)
{
    ebus_chn_subscriber_t *subscriber;

    ebus_env_mutex_lock(&chn->mutex, EBUS_ENV_MAX_DELAY);
    SYS_DLIST_FOR_EACH_CONTAINER(&chn->subscriber_list, subscriber, node)
    {
        if (((subscriber->filter == EBUS_EVENT_ALL) || (subscriber->filter == code)) && (subscriber->cb != NULL)) {

            subscriber->cb(chn, code, message, msg_size, subscriber->user_data);
        }
    }
    ebus_env_mutex_unlock(&chn->mutex);
    return 0;
}

int ebus_message_pub_async(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size)
{
    struct ebus_msg msg;
    ebus_chn_subscriber_t *subscriber;

    memset(&msg, 0, sizeof(msg));

    msg.chn = chn;
    msg.evt = code;
    msg.len = msg_size;

    LOGI("ebus msg send, chn:%p, evt:%d", chn, code);

    if (message != NULL) {
        msg.data = platform_malloc(msg_size);
        if (msg.data == NULL) {
            LOGE("ebus msg send failed, no mem");
            return -ENOMEM;
        }
        memcpy(msg.data, message, msg_size);
    }

    ebus_handle_t *bus = ebus_ch_bus_get(chn);
    if (bus == NULL) {
        LOGE("ebus msg send failed, bus not found");
        return -EINVAL;
    }

    return lisa_queue_push(bus->queue, &msg, sizeof(msg), LISA_OS_WAIT_FOREVER);
}
