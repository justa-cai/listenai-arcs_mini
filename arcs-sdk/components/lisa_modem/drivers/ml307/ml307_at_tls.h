/**
 * @file ml307_at_tls.h
 * @brief ML307 TLS AT helpers
 */

#ifndef LISA_MODEM_DRIVERS_ML307_AT_TLS_H
#define LISA_MODEM_DRIVERS_ML307_AT_TLS_H

#include <stdbool.h>
#include "at_client.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ml307_at_tls_configure_socket(at_client_t *client, int connect_id, bool enable);

#ifdef __cplusplus
}
#endif

#endif
