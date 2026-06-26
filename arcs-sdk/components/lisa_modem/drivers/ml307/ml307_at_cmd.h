/**
 * @file ml307_at_cmd.h
 * @brief ML307 AT transaction helpers
 */

#ifndef LISA_MODEM_DRIVERS_ML307_AT_CMD_H
#define LISA_MODEM_DRIVERS_ML307_AT_CMD_H

#include <stdbool.h>
#include <stddef.h>
#include "at_client.h"
#include "drivers/ml307/ml307_endpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

void ml307_at_cmd_handle_urc(ml307_endpoint_t *endpoint, const char *command,
                             at_arg_value_t *arguments, size_t arg_count);
const at_line_stream_handler_t *ml307_at_cmd_get_line_stream_handler(void);
bool ml307_at_cmd_connect(ml307_endpoint_t *endpoint, const char *host, int port);
int ml307_at_cmd_disconnect(ml307_endpoint_t *endpoint);
int ml307_at_cmd_send_chunk(ml307_endpoint_t *endpoint, const char *data, size_t length);
int ml307_at_cmd_send(ml307_endpoint_t *endpoint, const char *data, size_t length);
int ml307_at_cmd_sendto(ml307_endpoint_t *endpoint, const char *host, uint16_t port,
                        const char *data, size_t length);
int ml307_at_cmd_prefetch(ml307_endpoint_t *endpoint);

#ifdef __cplusplus
}
#endif

#endif
