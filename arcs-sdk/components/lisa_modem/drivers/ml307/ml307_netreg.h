/**
 * @file ml307_netreg.h
 * @brief ML307 network registration, module query and DNS helpers
 */

#ifndef LISA_MODEM_DRIVERS_ML307_NETREG_H
#define LISA_MODEM_DRIVERS_ML307_NETREG_H

#include <stdbool.h>
#include <stddef.h>
#include "at_client.h"
#include "drivers/ml307/ml307_endpoint_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void ml307_netreg_handle_modem_urc(ml307_endpoint_ctx_t *ctx, const char *command,
                                   at_arg_value_t *arguments, size_t arg_count);
network_status_t ml307_netreg_network_check(ml307_endpoint_ctx_t *ctx);
bool ml307_netreg_dns_resolve(ml307_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
