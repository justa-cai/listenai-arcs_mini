/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EC801E_NET_PROBE_CHECKS_H
#define EC801E_NET_PROBE_CHECKS_H

#include <stddef.h>
#include <stdint.h>

int ec801e_validate_dns_response(const uint8_t *response, size_t response_len, uint16_t transaction_id);
int ec801e_validate_ntp_response(const uint8_t *response, size_t response_len, uint32_t *unix_time);
int ec801e_parse_http_status(const char *response, int *status_code);

#endif
