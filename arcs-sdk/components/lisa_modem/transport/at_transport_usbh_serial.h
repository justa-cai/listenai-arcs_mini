/**
 * @file at_transport_usbh_serial.h
 * @brief CherryUSB host serial transport for AT commands
 */

#ifndef LISA_MODEM_TRANSPORT_AT_TRANSPORT_USBH_SERIAL_H
#define LISA_MODEM_TRANSPORT_AT_TRANSPORT_USBH_SERIAL_H

#include <stdbool.h>

#include "usbh_core.h"
#include "at_transport.h"
#include "usbh_serial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t baudrate;
    uint32_t rx_timeout_ms;
    uint8_t databits;
    uint8_t parity;
    uint8_t stopbits;
    bool rtscts;
} at_transport_usbh_serial_config_t;

#define AT_TRANSPORT_USBH_SERIAL_CONFIG_DEFAULT() { \
    .baudrate = 115200,                             \
    .rx_timeout_ms = 20,                            \
    .databits = USBH_SERIAL_DATABITS_8,             \
    .parity = USBH_SERIAL_PARITY_NONE,              \
    .stopbits = USBH_SERIAL_STOPBITS_1,             \
    .rtscts = false,                                \
}

at_transport_t *at_transport_usbh_serial_create(struct usbh_serial *serial, bool owns_serial,
                                                const at_transport_usbh_serial_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
