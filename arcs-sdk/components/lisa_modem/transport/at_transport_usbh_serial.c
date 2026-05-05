/**
 * @file at_transport_usbh_serial.c
 * @brief CherryUSB host serial transport for AT commands
 */

#include "transport/at_transport_usbh_serial.h"
#include "at_mem.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "at_transport_usb"
#include "lisa_log.h"

typedef struct {
    struct usbh_serial *serial;
    at_transport_usbh_serial_config_t config;
    volatile bool rx_enabled;
    volatile bool reconfiguring;
    bool owns_serial;
} usbh_serial_transport_priv_t;

static int usbh_serial_transport_apply_config(usbh_serial_transport_priv_t *priv)
{
    struct usbh_serial_termios termios;

    if (!priv || !priv->serial) {
        return -1;
    }

    termios.baudrate = priv->config.baudrate;
    termios.databits = priv->config.databits;
    termios.parity = priv->config.parity;
    termios.stopbits = priv->config.stopbits;
    termios.rtscts = priv->config.rtscts;
    termios.rx_timeout = priv->config.rx_timeout_ms;

    return usbh_serial_control(priv->serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
}

static int usbh_serial_transport_open(at_transport_t *transport)
{
    usbh_serial_transport_priv_t *priv = (usbh_serial_transport_priv_t *)transport->priv;

    if (!priv || !priv->serial) {
        return -1;
    }

    if (usbh_serial_transport_apply_config(priv) < 0) {
        LISA_LOGE(TAG, "Failed to configure USB host serial transport");
        return -1;
    }

    priv->rx_enabled = true;

    LISA_LOGI(TAG, "USB host serial transport opened");
    return 0;
}

static void usbh_serial_transport_close(at_transport_t *transport)
{
    usbh_serial_transport_priv_t *priv = (usbh_serial_transport_priv_t *)transport->priv;

    if (!priv) {
        return;
    }

    priv->rx_enabled = false;
}

static int usbh_serial_transport_send(at_transport_t *transport, const uint8_t *data, size_t len)
{
    usbh_serial_transport_priv_t *priv = (usbh_serial_transport_priv_t *)transport->priv;

    if (!priv || !priv->serial || !data || len == 0) {
        return -1;
    }

    return usbh_serial_write(priv->serial, data, (uint32_t)len);
}

static int usbh_serial_transport_read(at_transport_t *transport, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    usbh_serial_transport_priv_t *priv = (usbh_serial_transport_priv_t *)transport->priv;
    int ret;

    (void)timeout_ms;

    if (!priv || !priv->serial || !data || len == 0U) {
        return -1;
    }

    if (!priv->rx_enabled || priv->reconfiguring) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return 0;
    }

    ret = usbh_serial_read(priv->serial, data, (uint32_t)len);
    return ret < 0 ? 0 : ret;
}

static int usbh_serial_transport_set_rx_callback(at_transport_t *transport, at_transport_rx_cb_t cb, void *ctx)
{
    (void)transport;
    (void)cb;
    (void)ctx;
    return 0;
}

static int usbh_serial_transport_ioctl(at_transport_t *transport, int cmd, void *arg)
{
    usbh_serial_transport_priv_t *priv = (usbh_serial_transport_priv_t *)transport->priv;

    if (!priv || !priv->serial) {
        return -1;
    }

    switch (cmd) {
    case AT_TRANSPORT_IOCTL_SET_BAUDRATE:
        if (!arg) {
            return -1;
        }
        priv->reconfiguring = true;
        priv->config.baudrate = *(uint32_t *)arg;
        if (usbh_serial_transport_apply_config(priv) < 0) {
            priv->reconfiguring = false;
            return -1;
        }
        priv->reconfiguring = false;
        return 0;

    case AT_TRANSPORT_IOCTL_GET_BAUDRATE:
        if (!arg) {
            return -1;
        }
        *(uint32_t *)arg = priv->config.baudrate;
        return 0;

    case AT_TRANSPORT_IOCTL_RX_ENABLE:
        priv->rx_enabled = true;
        return 0;

    case AT_TRANSPORT_IOCTL_RX_DISABLE:
        priv->rx_enabled = false;
        return 0;

    default:
        return -1;
    }
}

static void usbh_serial_transport_destroy(at_transport_t *transport)
{
    usbh_serial_transport_priv_t *priv;

    if (!transport) {
        return;
    }

    priv = (usbh_serial_transport_priv_t *)transport->priv;
    if (priv) {
        if (priv->owns_serial && priv->serial) {
            (void)usbh_serial_close(priv->serial);
        }
        at_mem_free(priv);
    }
    at_mem_free(transport);
}

static const at_transport_ops_t s_usbh_serial_transport_ops = {
    .open_fn = usbh_serial_transport_open,
    .close_fn = usbh_serial_transport_close,
    .send_fn = usbh_serial_transport_send,
    .read_fn = usbh_serial_transport_read,
    .set_rx_callback_fn = usbh_serial_transport_set_rx_callback,
    .ioctl_fn = usbh_serial_transport_ioctl,
    .destroy_fn = usbh_serial_transport_destroy,
};

at_transport_t *at_transport_usbh_serial_create(struct usbh_serial *serial, bool owns_serial,
                                                const at_transport_usbh_serial_config_t *config)
{
    at_transport_t *transport;
    usbh_serial_transport_priv_t *priv;

    if (!serial) {
        return NULL;
    }

    transport = (at_transport_t *)at_mem_calloc(1, sizeof(*transport));
    if (!transport) {
        return NULL;
    }

    priv = (usbh_serial_transport_priv_t *)at_mem_calloc(1, sizeof(*priv));
    if (!priv) {
        at_mem_free(transport);
        return NULL;
    }

    priv->serial = serial;
    priv->owns_serial = owns_serial;
    priv->config = config ? *config : (at_transport_usbh_serial_config_t)AT_TRANSPORT_USBH_SERIAL_CONFIG_DEFAULT();

    transport->ops = &s_usbh_serial_transport_ops;
    transport->priv = priv;
    return transport;
}
