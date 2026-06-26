/**
 * @file ec801e_at_cmd.h
 * @brief EC801E AT transaction helpers
 */

#ifndef LISA_MODEM_DRIVERS_EC801E_AT_CMD_H
#define LISA_MODEM_DRIVERS_EC801E_AT_CMD_H

#include "ec801e_endpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

void ec801e_at_cmd_handle_urc(ec801e_endpoint_ctx_t *ctx, const char *command,
                              at_arg_value_t *arguments, size_t arg_count);
bool ec801e_at_cmd_connect(ec801e_endpoint_t *endpoint, const char *host, uint16_t port);
bool ec801e_at_cmd_open_udp_service(ec801e_endpoint_t *endpoint);
int ec801e_at_cmd_disconnect(ec801e_endpoint_t *endpoint);
int ec801e_at_cmd_send_chunk(ec801e_endpoint_t *endpoint, const void *data, size_t length,
                             const char *host, uint16_t port);
int ec801e_at_cmd_send(ec801e_endpoint_t *endpoint, const void *data, size_t length,
                       const char *host, uint16_t port);
int ec801e_at_cmd_prefetch(ec801e_endpoint_t *endpoint);

#ifdef __cplusplus
}
#endif

#endif
