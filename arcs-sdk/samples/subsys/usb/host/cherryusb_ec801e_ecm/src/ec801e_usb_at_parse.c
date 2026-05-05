/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ec801e_usb_at_parse.h"

#include <stdio.h>
#include <string.h>

int ec801e_parse_qcfg_value(const char *text, const char *key, int *out_value)
{
    const char *line;
    char parsed_key[32];
    int value;

    if (text == NULL || key == NULL || out_value == NULL) {
        return -1;
    }

    line = strstr(text, "+QCFG:");
    if (line == NULL) {
        return -1;
    }

    if (sscanf(line, "+QCFG: \"%31[^\"]\",%d", parsed_key, &value) != 2) {
        return -1;
    }

    if (strcmp(parsed_key, key) != 0) {
        return -1;
    }

    *out_value = value;
    return 0;
}

int ec801e_parse_qnetdevctl(const char *text, struct ec801e_qnetdevctl_status *out_status)
{
    const char *line;
    struct ec801e_qnetdevctl_status status;

    if (text == NULL || out_status == NULL) {
        return -1;
    }

    line = strstr(text, "+QNETDEVCTL:");
    if (line == NULL) {
        return -1;
    }

    if (sscanf(line, "+QNETDEVCTL: %d,%d,%d,%d",
               &status.type, &status.cid, &status.urc_enabled, &status.state) != 4) {
        return -1;
    }

    *out_status = status;
    return 0;
}
