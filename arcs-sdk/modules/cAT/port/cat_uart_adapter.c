/*
 * Copyright (c) 2024 ListenAI
 * SPDX-License-Identifier: MIT
 *
 * cAT UART Adapter Implementation for ARCS SDK
 */

#include "cat_uart_adapter.h"
#include "lisa_device.h"
#include "lisa_uart.h"

#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define LOG_TAG "cat_uart"
#include "lisa_log.h"

#define RINGBUF_SIZE 512

/* Adapter context structure */
struct cat_uart_adapter {
    lisa_device_t *uart_dev;
    cat_uart_config_t config;

    /* IO interface for cAT */
    struct cat_io_interface io_interface;
    struct cat_mutex_interface mutex_interface;

    /* Task control */
    TaskHandle_t rx_task_handle;
    TaskHandle_t process_task_handle;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t data_sem;
    volatile bool task_running;

    /* Ring buffer */
    uint8_t *ringbuf;
    uint16_t ringbuf_size;
    volatile uint16_t ringbuf_head;
    volatile uint16_t ringbuf_tail;

    struct cat_object *cat_obj;
};

/* Static adapter instance (for simple single-instance use) */
static struct cat_uart_adapter *s_adapter = NULL;

/* Ring buffer helper functions */
static inline bool ringbuf_is_empty(struct cat_uart_adapter *adapter)
{
    return adapter->ringbuf_head == adapter->ringbuf_tail;
}

static inline bool ringbuf_is_full(struct cat_uart_adapter *adapter)
{
    return ((adapter->ringbuf_head + 1) % adapter->ringbuf_size) == adapter->ringbuf_tail;
}

static bool ringbuf_put(struct cat_uart_adapter *adapter, uint8_t data)
{
    if (ringbuf_is_full(adapter)) {
        return false;
    }
    adapter->ringbuf[adapter->ringbuf_head] = data;
    adapter->ringbuf_head = (adapter->ringbuf_head + 1) % adapter->ringbuf_size;
    return true;
}

static bool ringbuf_get(struct cat_uart_adapter *adapter, uint8_t *data)
{
    if (ringbuf_is_empty(adapter)) {
        return false;
    }
    *data = adapter->ringbuf[adapter->ringbuf_tail];
    adapter->ringbuf_tail = (adapter->ringbuf_tail + 1) % adapter->ringbuf_size;
    return true;
}

/* cAT mutex interface implementations */
static int cat_mutex_lock(void)
{
    if (!s_adapter || !s_adapter->mutex) {
        return -1;
    }
    
    return (xSemaphoreTakeRecursive(s_adapter->mutex, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

static int cat_mutex_unlock(void)
{
    if (!s_adapter || !s_adapter->mutex) {
        return -1;
    }
    
    return (xSemaphoreGiveRecursive(s_adapter->mutex) == pdTRUE) ? 0 : -1;
}

/* cAT IO interface write implementation */
static int cat_io_write(char ch)
{
    if (!s_adapter || !s_adapter->uart_dev) {
        return 0;
    }

    lisa_uart_poll_out(s_adapter->uart_dev, (uint8_t)ch);
    return 1;
}

/* cAT IO interface read implementation */
static int cat_io_read(char *ch)
{
    if (!s_adapter || !ch) {
        return 0;
    }

    uint8_t data;
    if (ringbuf_get(s_adapter, &data)) {
        *ch = (char)data;
        return 1;
    }

    return 0;
}



cat_uart_adapter_t *cat_uart_adapter_init(const cat_uart_config_t *config)
{
    if (!config || !config->uart_device_name) {
        LOGE("Invalid config");
        return NULL;
    }

    /* Allocate adapter context */
    cat_uart_adapter_t *adapter = (cat_uart_adapter_t *)malloc(sizeof(cat_uart_adapter_t));
    if (!adapter) {
        LOGE("Failed to allocate adapter");
        return NULL;
    }
    memset(adapter, 0, sizeof(cat_uart_adapter_t));

    /* Copy configuration */
    memcpy(&adapter->config, config, sizeof(cat_uart_config_t));

    /* Get UART device */
    adapter->uart_dev = lisa_device_get(config->uart_device_name);
    if (!adapter->uart_dev) {
        LOGE("Failed to get UART device: %s", config->uart_device_name);
        free(adapter);
        return NULL;
    }

    /* Configure UART */
    lisa_uart_config_t uart_config = {
        .baudrate = config->baudrate ? config->baudrate : 115200,
        .data_bits = LISA_UART_DATA_BITS_8,
        .stop_bits = LISA_UART_STOP_BITS_1,
        .parity = LISA_UART_PARITY_NONE,
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,
        .transfer_mode = LISA_UART_TRANSFER_MODE_INTERRUPT,
    };

    int ret = lisa_uart_configure(adapter->uart_dev, &uart_config);
    if (ret < 0) {
        LOGE("Failed to configure UART: %d", ret);
        free(adapter);
        return NULL;
    }

    /* Enable RX */
    ret = lisa_uart_rx_enable(adapter->uart_dev);
    if (ret < 0) {
        LOGE("Failed to enable UART RX: %d", ret);
        free(adapter);
        return NULL;
    }

    /* Setup IO interface */
    adapter->io_interface.write = cat_io_write;
    adapter->io_interface.read = cat_io_read;

    /* Setup mutex interface */
    adapter->mutex_interface.lock = cat_mutex_lock;
    adapter->mutex_interface.unlock = cat_mutex_unlock;

    /* Create recursive mutex for unsolicited event support */
    adapter->mutex = xSemaphoreCreateRecursiveMutex();
    if (!adapter->mutex) {
        LOGE("Failed to create recursive mutex");
        lisa_uart_rx_disable(adapter->uart_dev);
        free(adapter);
        return NULL;
    }

    /* Create binary semaphore for data notification */
    adapter->data_sem = xSemaphoreCreateBinary();
    if (!adapter->data_sem) {
        LOGE("Failed to create data semaphore");
        vSemaphoreDelete(adapter->mutex);
        lisa_uart_rx_disable(adapter->uart_dev);
        free(adapter);
        return NULL;
    }

    /* Allocate ring buffer */
    adapter->ringbuf_size = RINGBUF_SIZE;
    adapter->ringbuf = (uint8_t *)malloc(adapter->ringbuf_size);
    if (!adapter->ringbuf) {
        LOGE("Failed to allocate ring buffer");
        vSemaphoreDelete(adapter->data_sem);
        vSemaphoreDelete(adapter->mutex);
        lisa_uart_rx_disable(adapter->uart_dev);
        free(adapter);
        return NULL;
    }

    /* Initialize ring buffer */
    adapter->ringbuf_head = 0;
    adapter->ringbuf_tail = 0;

    adapter->rx_task_handle = NULL;
    adapter->process_task_handle = NULL;
    adapter->task_running = false;

    /* Store as static instance for IO callbacks */
    s_adapter = adapter;

    LOGI("cAT UART adapter initialized on %s @ %u baud",
         config->uart_device_name, (unsigned)uart_config.baudrate);

    return adapter;
}

void cat_uart_adapter_deinit(cat_uart_adapter_t *adapter)
{
    if (!adapter) {
        return;
    }

    /* Stop task if running */
    cat_uart_adapter_stop_service(adapter);

    /* Free ring buffer */
    if (adapter->ringbuf) {
        free(adapter->ringbuf);
    }

    /* Delete semaphores */
    if (adapter->data_sem) {
        vSemaphoreDelete(adapter->data_sem);
    }
    if (adapter->mutex) {
        vSemaphoreDelete(adapter->mutex);
    }

    /* Disable UART RX */
    if (adapter->uart_dev) {
        lisa_uart_rx_disable(adapter->uart_dev);
    }

    /* Clear static instance */
    if (s_adapter == adapter) {
        s_adapter = NULL;
    }

    free(adapter);

    LOGI("cAT UART adapter deinitialized");
}

int cat_uart_adapter_process(cat_uart_adapter_t *adapter, struct cat_object *cat)
{
    if (!adapter || !cat || !adapter->uart_dev) {
        return -1;
    }

    /* Run cAT service once - poll_in is called inside cat_service via cat_io_read */
    cat_status status = cat_service(cat);
    
    if (status == CAT_STATUS_ERROR) {
        LOGE("cAT service error");
        return -1;
    }

    /* If idle (no data to process), sleep to reduce CPU usage */
    if (status == CAT_STATUS_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return 0;
}

static void cat_uart_rx_task(void *arg)
{
    cat_uart_adapter_t *adapter = (cat_uart_adapter_t *)arg;
    uint8_t byte;

    while (adapter->task_running) {
        int len = lisa_uart_read_sync(adapter->uart_dev, &byte, sizeof(byte));
        if (len > 0) {
            if (!ringbuf_put(adapter, byte)) {
                LOGW("Ring buffer full, data lost");
                break;
            }
            xSemaphoreGive(adapter->data_sem);
        }
    }

    adapter->rx_task_handle = NULL;
    vTaskDelete(NULL);
}

static void cat_uart_process_task(void *arg)
{
    cat_uart_adapter_t *adapter = (cat_uart_adapter_t *)arg;

    while (adapter->task_running) {
        if (ringbuf_is_empty(adapter) && adapter->cat_obj->state == CAT_STATE_IDLE) {
            xSemaphoreTake(adapter->data_sem, portMAX_DELAY);
        }

        if (!adapter->task_running) {
            break;
        }

        cat_status status = cat_service(adapter->cat_obj);
        if (status == CAT_STATUS_ERROR) {
            LOGE("cAT service error");
        }
    }

    adapter->process_task_handle = NULL;
    vTaskDelete(NULL);
}

int cat_uart_adapter_start_service(cat_uart_adapter_t *adapter,
                                 struct cat_object *cat,
                                 const struct cat_descriptor *desc)
{
    if (!adapter || !cat || !desc) {
        return -1;
    }

    if (adapter->task_running) {
        LOGW("Task already running");
        return -1;
    }

    /* Initialize cAT object */
    cat_init(cat, desc, &adapter->io_interface, &adapter->mutex_interface);

    adapter->cat_obj = cat;
    adapter->task_running = true;

    BaseType_t ret = xTaskCreate(
        cat_uart_rx_task,
        "cat_rx",
        CONFIG_SDK_MODULE_CAT_UART_RX_TASK_STACK_SIZE / sizeof(StackType_t),
        adapter,
        CONFIG_SDK_MODULE_CAT_UART_RX_TASK_PRIORITY,
        &adapter->rx_task_handle
    );

    if (ret != pdPASS) {
        LOGE("Failed to create RX task");
        adapter->task_running = false;
        return -1;
    }

    ret = xTaskCreate(
        cat_uart_process_task,
        "cat_proc",
        CONFIG_SDK_MODULE_CAT_UART_PROC_TASK_STACK_SIZE / sizeof(StackType_t),
        adapter,
        CONFIG_SDK_MODULE_CAT_UART_PROC_TASK_PRIORITY,
        &adapter->process_task_handle
    );

    if (ret != pdPASS) {
        LOGE("Failed to create process task");
        adapter->task_running = false;
        while (adapter->rx_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return -1;
    }

    LOGI("cAT UART tasks started");
    return 0;
}

int cat_uart_adapter_stop_service(cat_uart_adapter_t *adapter)
{
    if (!adapter) {
        return -1;
    }

    if (!adapter->task_running) {
        return 0;
    }

    adapter->task_running = false;

    /* Wake up process task if waiting */
    if (adapter->data_sem) {
        xSemaphoreGive(adapter->data_sem);
    }

    /* Wait for both tasks to finish */
    while (adapter->rx_task_handle != NULL || adapter->process_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    LOGI("cAT UART tasks stopped");
    return 0;
}

int cat_uart_adapter_send_urc(cat_uart_adapter_t *adapter, const char *data, size_t len)
{
    if (!adapter || !data || !adapter->uart_dev) {
        return -1;
    }

    int ret = lisa_uart_write_sync(adapter->uart_dev, (const uint8_t *)data, len, 1000);
    if (ret < 0) {
        LOGE("Failed to send URC: %d", ret);
        return ret;
    }

    return (int)len;
}
