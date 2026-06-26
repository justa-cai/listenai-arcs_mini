/**
 * @file ec801e_netreg.h
 * @brief EC801E network registration and DNS helpers
 */

#ifndef LISA_MODEM_DRIVERS_EC801E_NETREG_H
#define LISA_MODEM_DRIVERS_EC801E_NETREG_H

#include "drivers/ec801e/ec801e_endpoint_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void ec801e_netreg_handle_modem_urc(ec801e_endpoint_ctx_t *ctx, const char *command,
                                    at_arg_value_t *arguments, size_t arg_count);
network_status_t ec801e_netreg_network_check(ec801e_endpoint_ctx_t *ctx);
bool ec801e_netreg_dns_resolve(ec801e_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
