/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ec801e_usb_at.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "usbh_core.h"
#include "usbh_serial.h"

#include "ec801e_usb_at_parse.h"

#define EC801E_USB_VID                  0x2c7c
#define EC801E_USB_PID_ECM              0x0903
#define EC801E_USB_AT_INTF_NUM          3
#define EC801E_USB_AT_MAX_TTYS          4
#define EC801E_USB_AT_CMD_BUF_SIZE      64
#define EC801E_USB_AT_RESP_BUF_SIZE     512
#define EC801E_USB_AT_BOOT_DELAY_MS     3000
#define EC801E_USB_AT_SCAN_DELAY_MS     500
#define EC801E_USB_AT_POLL_INTERVAL_MS  20
#define EC801E_USB_AT_CMD_TIMEOUT_MS    3000
#define EC801E_USB_AT_LINK_TIMEOUT_MS   15000

struct ec801e_usb_at_port {
    struct usbh_serial *serial;
    char devname[16];
};

static void ec801e_usb_at_close(struct ec801e_usb_at_port *port)
{
    if (port->serial != NULL) {
        usbh_serial_close(port->serial);
        port->serial = NULL;
    }
    port->devname[0] = '\0';
}

static int ec801e_usb_at_configure(struct usbh_serial *serial)
{
    struct usbh_serial_termios termios;

    memset(&termios, 0, sizeof(termios));
    termios.baudrate = 115200;
    termios.stopbits = USBH_SERIAL_STOPBITS_1;
    termios.parity = USBH_SERIAL_PARITY_NONE;
    termios.databits = USBH_SERIAL_DATABITS_8;
    termios.rtscts = false;
    termios.rx_timeout = EC801E_USB_AT_POLL_INTERVAL_MS;
    return usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
}

static int ec801e_usb_at_try_open(struct ec801e_usb_at_port *port)
{
    char devname[16];

    for (int i = 0; i < EC801E_USB_AT_MAX_TTYS; ++i) {
        struct usbh_serial *serial;

        (void)snprintf(devname, sizeof(devname), "/dev/ttyUSB%d", i);
        serial = usbh_serial_open(devname, USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
        if (serial == NULL) {
            continue;
        }

        if (serial->hport->device_desc.idVendor != EC801E_USB_VID ||
            serial->hport->device_desc.idProduct != EC801E_USB_PID_ECM ||
            serial->intf != EC801E_USB_AT_INTF_NUM) {
            usbh_serial_close(serial);
            continue;
        }

        if (ec801e_usb_at_configure(serial) < 0) {
            usbh_serial_close(serial);
            continue;
        }

        port->serial = serial;
        strncpy(port->devname, devname, sizeof(port->devname) - 1);
        port->devname[sizeof(port->devname) - 1] = '\0';
        printf("[AT] Using USB AT interface %u via %s\r\n", serial->intf, port->devname);
        return 0;
    }

    return -1;
}

static int ec801e_usb_at_open_wait(struct ec801e_usb_at_port *port)
{
    bool announced = false;

    while (1) {
        if (ec801e_usb_at_try_open(port) == 0) {
            return 0;
        }

        if (!announced) {
            printf("[AT] Waiting for EC801E USB AT interface (intf=%u)...\r\n", EC801E_USB_AT_INTF_NUM);
            announced = true;
        }
        vTaskDelay(pdMS_TO_TICKS(EC801E_USB_AT_SCAN_DELAY_MS));
    }
}

static void ec801e_usb_at_drain(struct usbh_serial *serial)
{
    char scratch[64];

    while (usbh_serial_read(serial, scratch, sizeof(scratch)) > 0) {
    }
}

static bool ec801e_usb_at_response_has_ok(const char *response)
{
    return strstr(response, "\r\nOK\r\n") != NULL ||
           strstr(response, "\nOK\r\n") != NULL ||
           strcmp(response, "OK\r\n") == 0;
}

static bool ec801e_usb_at_response_has_error(const char *response)
{
    return strstr(response, "ERROR") != NULL;
}

static int ec801e_usb_at_command(struct usbh_serial *serial, const char *command,
                                 char *response, size_t response_size, uint32_t timeout_ms)
{
    TickType_t start;
    char txbuf[EC801E_USB_AT_CMD_BUF_SIZE];
    char rxbuf[64];
    size_t used = 0;
    int len;

    if (serial == NULL || command == NULL || response == NULL || response_size == 0) {
        return -1;
    }

    ec801e_usb_at_drain(serial);

    len = snprintf(txbuf, sizeof(txbuf), "%s\r\n", command);
    if (len <= 0 || (size_t)len >= sizeof(txbuf)) {
        return -1;
    }

    len = usbh_serial_write(serial, txbuf, (uint32_t)len);
    if (len <= 0 || (size_t)len != strlen(txbuf)) {
        return -1;
    }

    response[0] = '\0';
    start = xTaskGetTickCount();
    while ((uint32_t)((xTaskGetTickCount() - start) * portTICK_PERIOD_MS) < timeout_ms) {
        int ret;

        ret = usbh_serial_read(serial, rxbuf, sizeof(rxbuf) - 1);
        if (ret > 0) {
            size_t copy_len = (size_t)ret;

            rxbuf[ret] = '\0';
            if ((used + copy_len) >= (response_size - 1)) {
                copy_len = (response_size - 1) - used;
            }
            memcpy(&response[used], rxbuf, copy_len);
            used += copy_len;
            response[used] = '\0';

            if (ec801e_usb_at_response_has_ok(response)) {
                return 0;
            }
            if (ec801e_usb_at_response_has_error(response)) {
                return -1;
            }
        } else if (ret < 0) {
            return -1;
        }

        vTaskDelay(pdMS_TO_TICKS(EC801E_USB_AT_POLL_INTERVAL_MS));
    }

    return -1;
}

static int ec801e_usb_at_expect_ok(struct usbh_serial *serial, const char *command)
{
    char response[EC801E_USB_AT_RESP_BUF_SIZE];
    int ret;

    printf("[AT] >> %s\r\n", command);
    ret = ec801e_usb_at_command(serial, command, response, sizeof(response), EC801E_USB_AT_CMD_TIMEOUT_MS);
    if (ret < 0) {
        printf("[AT] Command failed: %s\r\n[AT] Response: %s\r\n", command, response);
        return ret;
    }
    return 0;
}

static int ec801e_usb_at_query_qcfg(struct usbh_serial *serial, const char *key, int *value)
{
    char command[32];
    char response[EC801E_USB_AT_RESP_BUF_SIZE];
    int ret;

    snprintf(command, sizeof(command), "AT+QCFG=\"%s\"", key);
    printf("[AT] >> %s\r\n", command);
    ret = ec801e_usb_at_command(serial, command, response, sizeof(response), EC801E_USB_AT_CMD_TIMEOUT_MS);
    if (ret < 0) {
        printf("[AT] Query failed: %s\r\n[AT] Response: %s\r\n", command, response);
        return ret;
    }

    ret = ec801e_parse_qcfg_value(response, key, value);
    if (ret < 0) {
        printf("[AT] Failed to parse %s response: %s\r\n", key, response);
        return ret;
    }
    printf("[AT] %s=%d\r\n", key, *value);
    return 0;
}

static int ec801e_usb_at_query_qnetdevctl(struct usbh_serial *serial,
                                          struct ec801e_qnetdevctl_status *status)
{
    char response[EC801E_USB_AT_RESP_BUF_SIZE];
    int ret;

    printf("[AT] >> AT+QNETDEVCTL?\r\n");
    ret = ec801e_usb_at_command(serial, "AT+QNETDEVCTL?", response, sizeof(response), EC801E_USB_AT_CMD_TIMEOUT_MS);
    if (ret < 0) {
        printf("[AT] Query failed: AT+QNETDEVCTL?\r\n[AT] Response: %s\r\n", response);
        return ret;
    }

    ret = ec801e_parse_qnetdevctl(response, status);
    if (ret < 0) {
        printf("[AT] Failed to parse QNETDEVCTL response: %s\r\n", response);
        return ret;
    }

    printf("[AT] QNETDEVCTL type=%d cid=%d urc=%d state=%d\r\n",
           status->type, status->cid, status->urc_enabled, status->state);
    return 0;
}

static int ec801e_usb_at_session_ready(struct usbh_serial *serial)
{
    if (ec801e_usb_at_expect_ok(serial, "AT") < 0) {
        return -1;
    }
    if (ec801e_usb_at_expect_ok(serial, "ATE0") < 0) {
        return -1;
    }
    return 0;
}

static int ec801e_usb_at_fix_usb_mode(struct usbh_serial *serial, bool *needs_reboot)
{
    int usbnet = -1;
    int nat = -1;

    *needs_reboot = false;

    if (ec801e_usb_at_query_qcfg(serial, "usbnet", &usbnet) < 0) {
        return -1;
    }
    if (ec801e_usb_at_query_qcfg(serial, "nat", &nat) < 0) {
        return -1;
    }

    if (usbnet != 1) {
        if (ec801e_usb_at_expect_ok(serial, "AT+QCFG=\"usbnet\",1") < 0) {
            return -1;
        }
        *needs_reboot = true;
    }

    if (nat != 1) {
        if (ec801e_usb_at_expect_ok(serial, "AT+QCFG=\"nat\",1") < 0) {
            return -1;
        }
        *needs_reboot = true;
    }

    return 0;
}

static int ec801e_usb_at_connect_netcard(struct usbh_serial *serial)
{
    TickType_t start;
    struct ec801e_qnetdevctl_status status = { 0 };

    if (ec801e_usb_at_query_qnetdevctl(serial, &status) == 0 &&
        status.type == 3 && status.cid == 1 && status.urc_enabled == 1 && status.state == 1) {
        return 0;
    }

    if (ec801e_usb_at_expect_ok(serial, "AT+QNETDEVCTL=3,1,1") < 0) {
        return -1;
    }

    start = xTaskGetTickCount();
    while ((uint32_t)((xTaskGetTickCount() - start) * portTICK_PERIOD_MS) < EC801E_USB_AT_LINK_TIMEOUT_MS) {
        if (ec801e_usb_at_query_qnetdevctl(serial, &status) == 0 &&
            status.type == 3 && status.cid == 1 && status.urc_enabled == 1 && status.state == 1) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return -1;
}

int ec801e_usb_prepare_for_ecm(void)
{
    struct ec801e_usb_at_port port = { 0 };
    bool rebooted = false;

    while (1) {
        bool needs_reboot = false;

        ec801e_usb_at_open_wait(&port);

        if (ec801e_usb_at_session_ready(port.serial) < 0) {
            ec801e_usb_at_close(&port);
            vTaskDelay(pdMS_TO_TICKS(EC801E_USB_AT_SCAN_DELAY_MS));
            continue;
        }

        if (ec801e_usb_at_fix_usb_mode(port.serial, &needs_reboot) < 0) {
            ec801e_usb_at_close(&port);
            return -1;
        }

        if (needs_reboot) {
            if (rebooted) {
                printf("[AT] USB mode still requires reboot after one recovery cycle\r\n");
                ec801e_usb_at_close(&port);
                return -1;
            }

            if (ec801e_usb_at_expect_ok(port.serial, "AT+CFUN=1,1") < 0) {
                ec801e_usb_at_close(&port);
                return -1;
            }

            printf("[AT] USB mode updated, waiting for module reboot...\r\n");
            rebooted = true;
            ec801e_usb_at_close(&port);
            vTaskDelay(pdMS_TO_TICKS(EC801E_USB_AT_BOOT_DELAY_MS));
            continue;
        }

        if (ec801e_usb_at_connect_netcard(port.serial) < 0) {
            printf("[AT] Failed to bring up USB netcard with QNETDEVCTL\r\n");
            ec801e_usb_at_close(&port);
            return -1;
        }

        ec801e_usb_at_close(&port);
        printf("[AT] EC801E USB AT preparation complete\r\n");
        return 0;
    }
}
