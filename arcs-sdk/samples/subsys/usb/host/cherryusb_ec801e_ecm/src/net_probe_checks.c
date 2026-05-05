/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "net_probe_checks.h"

#include <stdio.h>

#define DNS_HEADER_SIZE            12
#define NTP_PACKET_SIZE            48
#define NTP_UNIX_EPOCH_DELTA_SEC   2208988800UL

static uint16_t read_be16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

static uint32_t read_be32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

int ec801e_validate_dns_response(const uint8_t *response, size_t response_len, uint16_t transaction_id)
{
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;

    if (response == NULL || response_len < DNS_HEADER_SIZE) {
        return -1;
    }

    if (read_be16(response) != transaction_id) {
        return -1;
    }

    flags = read_be16(&response[2]);
    if ((flags & 0x8000U) == 0U) {
        return -1;
    }
    if ((flags & 0x000fU) != 0U) {
        return -1;
    }

    qdcount = read_be16(&response[4]);
    ancount = read_be16(&response[6]);
    if (qdcount == 0U || ancount == 0U) {
        return -1;
    }

    return 0;
}

int ec801e_validate_ntp_response(const uint8_t *response, size_t response_len, uint32_t *unix_time)
{
    uint8_t mode;
    uint32_t ntp_seconds;

    if (response == NULL || unix_time == NULL || response_len < NTP_PACKET_SIZE) {
        return -1;
    }

    mode = response[0] & 0x07U;
    if (mode != 4U && mode != 5U) {
        return -1;
    }

    ntp_seconds = read_be32(&response[40]);
    if (ntp_seconds <= NTP_UNIX_EPOCH_DELTA_SEC) {
        return -1;
    }

    *unix_time = ntp_seconds - NTP_UNIX_EPOCH_DELTA_SEC;
    return 0;
}

int ec801e_parse_http_status(const char *response, int *status_code)
{
    int status;

    if (response == NULL || status_code == NULL) {
        return -1;
    }

    if (sscanf(response, "HTTP/%*u.%*u %d", &status) != 1) {
        return -1;
    }

    if (status < 100 || status > 599) {
        return -1;
    }

    *status_code = status;
    return 0;
}
