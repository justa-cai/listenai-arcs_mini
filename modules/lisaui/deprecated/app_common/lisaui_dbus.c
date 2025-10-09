/**
 * @file lisaui_dbus.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-03-02
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lisaui_app_common.h"
#include "lisaui_type.h"
#include "lisaui_dbus.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "lisaui_dbus";

static void lisaui_dbus_task(void *param);

lisaui_err_t lisaui_dbus_create(lisaui_dbus_t *bus)
{
    if (!bus) {
        return LISAUI_ERR_INVALID_PARAM;
    }
    memset(bus, 0, sizeof(lisaui_dbus_t));
    bus->node = NULL;

#ifdef FREERTOS
    bus->mutex = xSemaphoreCreateMutex();
    if (!bus->mutex) {
        return LISAUI_ERR_FAIL;
    }

    bus->event_queue = xQueueCreate(LISAUI_DBUS_MAX_QUEUE_SIZE, sizeof(lisaui_dbus_msg_t));
    if (!bus->event_queue) {
        vSemaphoreDelete(bus->mutex);
        return LISAUI_ERR_FAIL;
    }

    if (xTaskCreate(lisaui_dbus_task, "lisaui_dbus_task", LISAUI_DBUS_TASK_STACK_SIZE, bus, LISAUI_DBUS_TASK_PRIORITY,
                    &bus->task_handle) != pdPASS) {
        vQueueDelete(bus->event_queue);
        vSemaphoreDelete(bus->mutex);
        return LISAUI_ERR_FAIL;
    }
#endif

    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_dbus_destroy(lisaui_dbus_t *bus)
{
    if (!bus) {
        return LISAUI_ERR_INVALID_PARAM;
    }

#ifdef FREERTOS
    if (bus->task_handle) {
        vTaskDelete(bus->task_handle);
    }
#endif

    lisaui_dbus_node_t *node = bus->node;
    while (node) {
        lisaui_dbus_node_t *next = node->next;
        lisaui_free(node);
        node = next;
    }

#ifdef FREERTOS
    if (bus->event_queue) {
        vQueueDelete(bus->event_queue);
    }
    if (bus->mutex) {
        vSemaphoreDelete(bus->mutex);
    }
#endif
    lisaui_free(bus);

    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_dbus_subscribe(lisaui_dbus_t *bus, const char *event, lisaui_dbus_handler_t handler)
{
    if (!bus || !event || !handler) {
        return LISAUI_ERR_INVALID_PARAM;
    }

    lisaui_dbus_node_t *node = (lisaui_dbus_node_t *)lisaui_malloc(sizeof(lisaui_dbus_node_t));
    if (!node) {
        return LISAUI_ERR_NO_MEMORY;
    }

    node->event = event;
    node->handler = handler;
    node->next = NULL;

#ifdef FREERTOS
    xSemaphoreTake(bus->mutex, portMAX_DELAY);
#else
    pthread_mutex_lock(&bus->mutex);
#endif
    node->next = bus->node;
    bus->node = node;

#ifdef FREERTOS
    xSemaphoreGive(bus->mutex);
#else
    pthread_mutex_unlock(&bus->mutex);
#endif

    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_dbus_unsubscribe(lisaui_dbus_t *bus, const char *event, lisaui_dbus_handler_t handler)
{
    if (!bus || !event || !handler) {
        return LISAUI_ERR_INVALID_PARAM;
    }

#ifdef FREERTOS
    xSemaphoreTake(bus->mutex, portMAX_DELAY);
#else
    pthread_mutex_lock(&bus->mutex);
#endif

    lisaui_dbus_node_t *prev = NULL;
    lisaui_dbus_node_t *curr = bus->node;

    while (curr) {
        if (strcmp(curr->event, event) == 0 && curr->handler == handler) {
            if (prev) {
                prev->next = curr->next;
            } else {
                bus->node = curr->next;
            }
            lisaui_free(curr);
#ifdef FREERTOS
            xSemaphoreGive(bus->mutex);
#else
            pthread_mutex_unlock(&bus->mutex);
#endif
            return LISAUI_ERR_OK;
        }
        prev = curr;
        curr = curr->next;
    }

#ifdef FREERTOS
    xSemaphoreGive(bus->mutex);
#else
    pthread_mutex_unlock(&bus->mutex);
#endif

    return LISAUI_ERR_FAIL;
}

lisaui_err_t lisaui_dbus_publish(lisaui_dbus_t *bus, const char *event, void *data)
{
    if (!bus || !event) {
        return LISAUI_ERR_INVALID_PARAM;
    }

    lisaui_dbus_msg_t msg = {.event = event, .data = data};

#ifdef FREERTOS
    if (xQueueSend(bus->event_queue, &msg, 0) != pdPASS) {
        LISAUI_LOGW(TAG, "Event queue full, dropping event: %s", event);
        return LISAUI_ERR_FAIL;
    }
#else

#endif
    return LISAUI_ERR_OK;
}

static void lisaui_dbus_task(void *param)
{
    lisaui_dbus_t *bus = (lisaui_dbus_t *)param;
    lisaui_dbus_msg_t msg;

    for (;;) {
#ifdef FREERTOS
        if (xQueueReceive(bus->event_queue, &msg, portMAX_DELAY) == pdPASS) {
            LISAUI_LOGD(TAG, "Dispatching async event: %s", msg.event);

            xSemaphoreTake(bus->mutex, portMAX_DELAY);
#else
        if (pthread_mutex_lock(&bus->mutex) != 0) {
            LISAUI_LOGE(TAG, "Failed to lock mutex");
            //     continue;
            // pthread_mutex_lock(&bus->mutex);
#endif

            lisaui_dbus_node_t *node = bus->node;
            while (node) {
                if (strcmp(node->event, msg.event) == 0) {
                    // 使用 LVGL 的异步 UI 安全回调
#if defined(LV_USE_USER_DATA) && defined(LV_USE_ASYNC)
                    lv_async_call((lv_async_cb_t)node->handler, msg.data);
#else
                    node->handler(msg.data);
#endif
                }
                node = node->next;
            }

#ifdef FREERTOS
            xSemaphoreGive(bus->mutex);
#else
            pthread_mutex_unlock(&bus->mutex);
#endif
        }
    }
}

#ifdef LISAUI_DBUS_TEST

void test_lisaui_dbus_example1_handler(void *data)
{
    LISAUI_LOGI(TAG, "Event handler called with data: %s", (char *)data);
}

void test_lisaui_dbus_example1(void)
{
    lisaui_dbus_t *bus = (lisaui_dbus_t *)lisaui_malloc(sizeof(lisaui_dbus_t));
    if (!bus) {
        LISAUI_LOGE(TAG, "Failed to allocate memory for event bus");
        return;
    }
    if (lisaui_dbus_create(bus) != LISAUI_ERR_OK) {
        LISAUI_LOGE(TAG, "Failed to create event bus");
        return;
    }

    LISAUI_LOGI(TAG, "Event bus created");
    lisaui_dbus_subscribe(bus, "test_event", test_lisaui_dbus_example1_handler);
    LISAUI_LOGI(TAG, "Event subscribed");
    lisaui_dbus_publish(bus, "test_event", "Hello, World!");
    LISAUI_LOGI(TAG, "Event published");

    lisaui_dbus_destroy(bus);
}
#endif
