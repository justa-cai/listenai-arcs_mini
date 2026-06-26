/**
 * @file ec801e_endpoint_internal.h
 * @brief Private EC801E endpoint internals
 */

#ifndef LISA_MODEM_DRIVERS_EC801E_ENDPOINT_INTERNAL_H
#define LISA_MODEM_DRIVERS_EC801E_ENDPOINT_INTERNAL_H

#include "drivers/common/modem_runtime_common.h"
#include "drivers/ec801e/ec801e_endpoint.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef modem_runtime_wait_hook_t ec801e_endpoint_wait_hook_t;

struct ec801e_endpoint_ctx {
    at_client_t *client;
    bool initialized;
    lisa_modem_status_t status;
    modem_network_status_t network_status;
    bool network_ready;
    uint8_t active_pdp_cid;
    char ip_address[16];
    volatile bool dns_pending;
    volatile bool dns_success;
    char dns_result[16];

    SemaphoreHandle_t dns_mutex;
    at_urc_callback_node_t *urc_node;
    modem_dispatcher_t *dispatcher;

    ec801e_endpoint_t endpoints[EC801E_MAX_ENDPOINTS];
};

#ifdef __cplusplus
}
#endif

#endif
