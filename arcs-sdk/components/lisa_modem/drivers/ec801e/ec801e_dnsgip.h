/**
 * @file ec801e_dnsgip.h
 * @brief EC801E DNS URC helpers
 */

#ifndef LISA_MODEM_DRIVERS_EC801E_DNSGIP_H
#define LISA_MODEM_DRIVERS_EC801E_DNSGIP_H

#include "at_client.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ec801e_dnsgip_try_extract_ip(at_arg_value_t *args, size_t count,
                                  char *ip_addr, size_t size, bool *done);

#ifdef __cplusplus
}
#endif

#endif
