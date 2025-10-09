#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "port/dlist.h"
#include "port/platform.h"
#include "ebus/ebus.h"

#define EBUS_NAME_MAX_LEN     32
#define EBUS_CHN_NAME_MAX_LEN 32

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
} ebus_handle_t;

typedef struct {
    ebus_env_mutex_handle_t mutex;
    sys_dlist_t bus_list;
} _ebus_list_t;

static _ebus_list_t *s_ebus_list = NULL;

int ebus_init(void)
{
    s_ebus_list = platform_malloc(sizeof(_ebus_list_t));
    if (s_ebus_list == NULL) {
        return -ENOMEM;
    }
    memset(s_ebus_list, 0, sizeof(_ebus_list_t));
    if (0 != ebus_env_mutex_create(&s_ebus_list->mutex)) {
        platform_free(s_ebus_list);
        return -ENOMEM;
    }
    sys_dlist_init(&s_ebus_list->bus_list);

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
        if (subscriber->type == EBUS_SUBSCRIBER_TYPE_SYNC) {
            if (((subscriber->filter == EBUS_EVENT_ALL) || (subscriber->filter == code)) && (subscriber->cb != NULL)) {

                subscriber->cb(chn, code, message, msg_size, subscriber->user_data);
            }

        } else {
            EBUS_ERR("Unsupport subscriber type:%d", subscriber->type);
        }
    }
    ebus_env_mutex_unlock(&chn->mutex);
    return 0;
}
