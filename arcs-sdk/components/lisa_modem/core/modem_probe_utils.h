/**
 * @file modem_probe_utils.h
 * @brief Shared probe helpers for modem drivers
 */

#ifndef LISA_MODEM_CORE_MODEM_PROBE_UTILS_H
#define LISA_MODEM_CORE_MODEM_PROBE_UTILS_H

#include "core/modem_driver_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

void modem_probe_fill_field(char *dst, size_t dst_size, const char *src);
bool modem_probe_common(at_client_t *client, modem_probe_result_t *result,
                        const char *driver_name, const char *model_match);

#ifdef __cplusplus
}
#endif

#endif
