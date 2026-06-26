/**
 * @file at_transport_uart.c
 * @brief UART Transport Implementation for AT Commands
 * @details Extracted from at_uart.c - handles UART hardware management,
 *          receive task, and data forwarding via callback.
 */

#include "at_transport_uart.h"
#include "at_mem.h"
#include "lisa_device.h"
#include "lisa_uart.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define TAG "at_transport_uart"
#include "lisa_log.h"

/**
 * @brief UART transport private data
 */
typedef struct {
    lisa_device_t *uart_dev;                /**< UART device pointer */
    uint32_t baudrate;                      /**< Current baudrate */
    uint16_t rx_buf_size;                   /**< Per-buffer size */
    uint16_t rx_buf_count;                  /**< Buffer count */
    volatile bool config_in_progress;       /**< Configuration in progress flag */
    char dev_name[16];                      /**< Device name */
} uart_transport_priv_t;

/* Transport ops implementation */

static int uart_open(at_transport_t *transport)
{
    uart_transport_priv_t *priv = (uart_transport_priv_t *)transport->priv;

    priv->uart_dev = lisa_device_get(priv->dev_name);
    if (!lisa_device_ready(priv->uart_dev)) {
        LISA_LOGE(TAG, "Failed to get UART device or device not ready");
        return -1;
    }

    lisa_uart_config_t uart_config = LISA_UART_CONFIG_HIGH_SPEED();
    uart_config.baudrate = priv->baudrate;
    uart_config.rx_buf_config.buffer_count = priv->rx_buf_count;
    uart_config.rx_buf_config.buffer_size = priv->rx_buf_size;

    int ret = lisa_uart_configure(priv->uart_dev, &uart_config);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to configure UART");
        return -1;
    }

    ret = lisa_uart_rx_enable(priv->uart_dev);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to enable UART RX");
        return -1;
    }

    LISA_LOGI(TAG, "UART transport opened: %s @ %u baud", priv->dev_name, priv->baudrate);
    return 0;
}

static void uart_close(at_transport_t *transport)
{
    uart_transport_priv_t *priv = (uart_transport_priv_t *)transport->priv;

    lisa_uart_rx_disable(priv->uart_dev);
    LISA_LOGI(TAG, "UART transport closed");
}

static int uart_send(at_transport_t *transport, const uint8_t *data, size_t len)
{
    uart_transport_priv_t *priv = (uart_transport_priv_t *)transport->priv;
    return lisa_uart_write_sync(priv->uart_dev, data, len, 1000);
}

static int uart_read(at_transport_t *transport, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    uart_transport_priv_t *priv = (uart_transport_priv_t *)transport->priv;
    lisa_uart_api_t *api;
    int ret;

    if (!priv || !priv->uart_dev || !data || len == 0U) {
        return -1;
    }

    if (priv->config_in_progress) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return 0;
    }

    api = (lisa_uart_api_t *)priv->uart_dev->api;
    if (!api || !api->read_sync) {
        return -1;
    }

    ret = api->read_sync(priv->uart_dev, data, (uint32_t)len, timeout_ms);
    if (ret == LISA_DEVICE_ERR_TIMEOUT) {
        return 0;
    }
    if (ret == LISA_DEVICE_ERR_OVERFLOW) {
        LISA_LOGE(TAG, "Buffer overflow! Recovering...");
        lisa_uart_rx_disable(priv->uart_dev);
        vTaskDelay(pdMS_TO_TICKS(10));
        lisa_uart_rx_enable(priv->uart_dev);
        LISA_LOGI(TAG, "Buffer overflow recovery completed");
        return 0;
    }
    return ret;
}

static int uart_set_rx_callback(at_transport_t *transport, at_transport_rx_cb_t cb, void *ctx)
{
    (void)transport;
    (void)cb;
    (void)ctx;
    return 0;
}

static int uart_ioctl(at_transport_t *transport, int cmd, void *arg)
{
    uart_transport_priv_t *priv = (uart_transport_priv_t *)transport->priv;

    switch (cmd) {
    case AT_TRANSPORT_IOCTL_SET_BAUDRATE: {
        uint32_t new_baudrate = *(uint32_t *)arg;
        LISA_LOGI(TAG, "Changing baudrate to %u", new_baudrate);

        priv->config_in_progress = true;

        lisa_uart_config_t uart_config = LISA_UART_CONFIG_HIGH_SPEED();
        uart_config.baudrate = new_baudrate;
        uart_config.rx_buf_config.buffer_count = priv->rx_buf_count;
        uart_config.rx_buf_config.buffer_size = priv->rx_buf_size;

        lisa_uart_rx_disable(priv->uart_dev);
        int ret = lisa_uart_configure(priv->uart_dev, &uart_config);
        lisa_uart_rx_enable(priv->uart_dev);

        priv->config_in_progress = false;

        if (ret != 0) {
            LISA_LOGE(TAG, "Failed to set baudrate");
            return -1;
        }

        priv->baudrate = new_baudrate;
        return 0;
    }

    case AT_TRANSPORT_IOCTL_GET_BAUDRATE: {
        *(uint32_t *)arg = priv->baudrate;
        return 0;
    }

    case AT_TRANSPORT_IOCTL_RX_ENABLE: {
        return lisa_uart_rx_enable(priv->uart_dev);
    }

    case AT_TRANSPORT_IOCTL_RX_DISABLE: {
        lisa_uart_rx_disable(priv->uart_dev);
        return 0;
    }

    default:
        LISA_LOGW(TAG, "Unsupported ioctl command: %d", cmd);
        return -1;
    }
}

static void uart_destroy(at_transport_t *transport)
{
    if (!transport) {
        return;
    }

    if (transport->priv) {
        at_mem_free(transport->priv);
    }
    at_mem_free(transport);
}

static const at_transport_ops_t uart_ops = {
    .open_fn = uart_open,
    .close_fn = uart_close,
    .send_fn = uart_send,
    .read_fn = uart_read,
    .set_rx_callback_fn = uart_set_rx_callback,
    .ioctl_fn = uart_ioctl,
    .destroy_fn = uart_destroy,
};

at_transport_t *at_transport_uart_create(const char *uart_dev_name,
                                          const at_transport_uart_config_t *config)
{
    if (!uart_dev_name) return NULL;

    at_transport_t *transport = (at_transport_t *)at_mem_calloc(1, sizeof(at_transport_t));
    if (!transport) return NULL;

    uart_transport_priv_t *priv = (uart_transport_priv_t *)at_mem_calloc(1, sizeof(uart_transport_priv_t));
    if (!priv) {
        at_mem_free(transport);
        return NULL;
    }

    strncpy(priv->dev_name, uart_dev_name, sizeof(priv->dev_name) - 1);

    if (config) {
        priv->baudrate = config->baudrate;
        priv->rx_buf_size = config->rx_buf_size;
        priv->rx_buf_count = config->rx_buf_count;
    } else {
        at_transport_uart_config_t default_config = AT_TRANSPORT_UART_CONFIG_DEFAULT();
        priv->baudrate = default_config.baudrate;
        priv->rx_buf_size = default_config.rx_buf_size;
        priv->rx_buf_count = default_config.rx_buf_count;
    }

    transport->ops = &uart_ops;
    transport->priv = priv;

    return transport;
}

void at_transport_uart_destroy(at_transport_t *transport)
{
    at_transport_destroy(transport);
}
