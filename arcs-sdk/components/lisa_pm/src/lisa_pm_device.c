/**
 * @file lisa_pm_device.c
 * @brief LISA PM phase-1 设备发现、复制注册与 system PM 分发
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_pm_internal.h"

typedef struct lisa_pm_device_node {
    lisa_pm_device_t dev;
    struct lisa_pm_device_node *next;
} lisa_pm_device_node_t;

static lisa_pm_device_node_t s_device_nodes[CONFIG_LISA_PM_SYSTEM_DEVICE_MAX];
static lisa_pm_device_node_t *s_device_list;

typedef int32_t (*lisa_pm_device_op_t)(void *ctx);
typedef lisa_pm_device_op_t (*lisa_pm_device_op_selector_t)(const lisa_pm_system_ops_t *ops);

static bool lisa_pm_device_is_valid(const lisa_pm_device_t *dev)
{
    return dev != NULL && dev->name != NULL && dev->system_ops != NULL;
}

static lisa_pm_device_node_t *lisa_pm_find_node_by_identity(const char *name,
                                                            const lisa_pm_system_ops_t *system_ops,
                                                            void *ctx)
{
    lisa_pm_device_node_t *node = s_device_list;

    while (node != NULL) {
        if (node->dev.name == name &&
            node->dev.system_ops == system_ops &&
            node->dev.ctx == ctx) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

static lisa_pm_device_node_t *lisa_pm_alloc_node(void)
{
    uint32_t i;

    for (i = 0; i < CONFIG_LISA_PM_SYSTEM_DEVICE_MAX; ++i) {
        if (s_device_nodes[i].dev.name == NULL) {
            return &s_device_nodes[i];
        }
    }

    return NULL;
}

static lisa_pm_device_op_t lisa_pm_select_prepare_suspend(const lisa_pm_system_ops_t *ops)
{
    return (ops != NULL) ? ops->prepare_suspend : NULL;
}

static lisa_pm_device_op_t lisa_pm_select_resume_restore(const lisa_pm_system_ops_t *ops)
{
    return (ops != NULL) ? ops->resume_restore : NULL;
}

static int32_t lisa_pm_devices_run_selected(lisa_pm_device_op_selector_t select)
{
    lisa_pm_device_node_t *node = s_device_list;
    int32_t ret = 0;

    while (node != NULL) {
        lisa_pm_device_op_t op = (select != NULL) ? select(node->dev.system_ops) : NULL;

        if (op != NULL) {
            int32_t curr = op(node->dev.ctx);
            if (curr != 0 && ret == 0) {
                ret = curr;
            }
        }
        node = node->next;
    }

    return ret;
}

static int lisa_pm_discover_device_cb(lisa_device_t *dev, void *user_data)
{
    int32_t *result = (int32_t *)user_data;

    if (dev == NULL || result == NULL) {
        return -1;
    }

    if (*result != 0) {
        return -1;
    }

    if (dev->pm == NULL || dev->pm->system_ops == NULL) {
        return 0;
    }

    *result = lisa_pm_device_register_copy(dev->name,
                                           dev->pm->system_ops,
                                           dev->pm->ctx);

    if (*result == -2) {
        *result = 0;
    }

    return (*result == 0) ? 0 : -1;
}

int32_t lisa_pm_device_register_copy(const char *name,
                                     const lisa_pm_system_ops_t *system_ops,
                                     void *ctx)
{
    /* Copy the dispatch tuple into lisa_pm-owned storage so PM scheduling
     * no longer depends on caller-managed registration objects. */
    lisa_pm_device_t descriptor = {
        .name = name,
        .ctx = ctx,
        .system_ops = system_ops,
    };
    lisa_pm_device_node_t *node;

    if (!lisa_pm_device_is_valid(&descriptor)) {
        return -1;
    }

    taskENTER_CRITICAL();

    if (lisa_pm_find_node_by_identity(name, system_ops, ctx) != NULL) {
        taskEXIT_CRITICAL();
        return -2;
    }

    node = lisa_pm_alloc_node();
    if (node == NULL) {
        taskEXIT_CRITICAL();
        return -3;
    }

    memset(node, 0, sizeof(*node));
    node->dev = descriptor;
    node->next = s_device_list;
    s_device_list = node;

    taskEXIT_CRITICAL();

    return 0;
}

int32_t lisa_pm_register_discovered_devices(void)
{
    int32_t ret = 0;
    int foreach_ret = lisa_device_foreach(lisa_pm_discover_device_cb, &ret);

    if (ret != 0) {
        return ret;
    }

    return (foreach_ret < 0) ? -1 : 0;
}

int32_t lisa_pm_device_register(lisa_pm_device_t *dev)
{
    if (!lisa_pm_device_is_valid(dev)) {
        return -1;
    }

    return lisa_pm_device_register_copy(dev->name, dev->system_ops, dev->ctx);
}

int32_t lisa_pm_device_unregister(lisa_pm_device_t *dev)
{
    lisa_pm_device_node_t **iter;

    if (!lisa_pm_device_is_valid(dev)) {
        return -1;
    }

    taskENTER_CRITICAL();

    iter = &s_device_list;
    while (*iter != NULL) {
        lisa_pm_device_node_t *node = *iter;

        if (node->dev.name == dev->name &&
            node->dev.system_ops == dev->system_ops &&
            node->dev.ctx == dev->ctx) {
            *iter = node->next;
            memset(node, 0, sizeof(*node));
            taskEXIT_CRITICAL();
            return 0;
        }
        iter = &node->next;
    }

    taskEXIT_CRITICAL();

    return -1;
}

int32_t lisa_pm_devices_check_idle(void)
{
    lisa_pm_device_node_t *node = s_device_list;

    while (node != NULL) {
        if (node->dev.system_ops->check_idle != NULL) {
            if (node->dev.system_ops->check_idle(node->dev.ctx) == 0) {
                return 0;
            }
        }
        node = node->next;
    }

    return 1;
}

int32_t lisa_pm_devices_prepare_suspend(void)
{
    return lisa_pm_devices_run_selected(lisa_pm_select_prepare_suspend);
}

int32_t lisa_pm_devices_resume_restore(void)
{
    return lisa_pm_devices_run_selected(lisa_pm_select_resume_restore);
}
