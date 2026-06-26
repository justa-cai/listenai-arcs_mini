/**
 * @file lisa_modem_status.h
 * @brief Public modem initialization and runtime status types.
 */

#ifndef LISA_MODEM_STATUS_H
#define LISA_MODEM_STATUS_H

#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LISA_MODEM_ERR_NOT_INITIALIZED = 0,     /* Modem has not been initialized yet. */
    LISA_MODEM_ERR_READY,                   /* Modem is initialized and the 4G network is ready. */
    LISA_MODEM_ERR_INVALID_ARG,             /* Invalid API argument. */
    LISA_MODEM_ERR_NO_MEMORY,               /* Memory allocation failed. */
    LISA_MODEM_ERR_TRANSPORT_CREATE_FAILED, /* Failed to create the AT transport. */
    LISA_MODEM_ERR_CLIENT_CREATE_FAILED,    /* Failed to create the AT client. */
    LISA_MODEM_ERR_CLIENT_BIND_FAILED,      /* Failed to bind the AT client to transport. */
    LISA_MODEM_ERR_UART_AT_SYNC_FAILED,     /* Failed to synchronize AT over UART. */
    LISA_MODEM_ERR_DRIVER_NOT_FOUND,        /* No matching modem driver was detected. */
    LISA_MODEM_ERR_DRIVER_CREATE_FAILED,    /* Failed to create the modem driver context. */
    LISA_MODEM_ERR_DRIVER_INIT_FAILED,      /* Modem driver initialization failed. */
    LISA_MODEM_ERR_UART_BAUD_ADAPT_FAILED,  /* Failed to adapt the UART baud rate. */
    LISA_MODEM_ERR_AT_COMMAND_FAILED,       /* Required AT command failed. */
    LISA_MODEM_ERR_SOCKET_CONFIG_FAILED,    /* Failed to configure modem socket mode. */
    LISA_MODEM_ERR_SIM_QUERY_FAILED,        /* Failed to query SIM status. */
    LISA_MODEM_ERR_SIM_NOT_INSERTED,        /* SIM card is not inserted. */
    LISA_MODEM_ERR_SIM_NOT_READY,           /* SIM card is present but not ready. */
    LISA_MODEM_ERR_NETWORK_REGISTER_FAILED, /* Network registration timed out or failed. */
    LISA_MODEM_ERR_NETWORK_DENIED,          /* Network registration was denied. */
    LISA_MODEM_ERR_PDP_ACTIVATE_FAILED,     /* PDP context activation failed. */
    LISA_MODEM_ERR_TRAFFIC_EXCEEDED,        /* Data service may be blocked by traffic limit. */
    LISA_MODEM_ERR_NETDEV_REGISTER_FAILED,  /* Failed to register the modem netdev. */
} lisa_modem_error_t;

typedef struct {
    lisa_modem_error_t last_error;
} lisa_modem_status_t;

static inline void lisa_modem_status_clear(lisa_modem_status_t *status)
{
    if (!status) {
        return;
    }

    memset(status, 0, sizeof(*status));
    status->last_error = LISA_MODEM_ERR_NOT_INITIALIZED;
}

static inline bool lisa_modem_cme_may_indicate_traffic_exceeded(int cme_error)
{
    switch (cme_error) {
        case 133: /* requested service option not subscribed */
        case 134: /* service option temporarily out of order */
        case 148: /* unspecified GPRS/data service error on common AT modules */
            return true;
        default:
            return false;
    }
}

static inline void lisa_modem_status_note_cme(lisa_modem_status_t *status, int cme_error)
{
    if (!status || cme_error <= 0) {
        return;
    }

    if (lisa_modem_cme_may_indicate_traffic_exceeded(cme_error)) {
        status->last_error = LISA_MODEM_ERR_TRAFFIC_EXCEEDED;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* LISA_MODEM_STATUS_H */
