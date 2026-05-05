/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EC801E_USB_AT_PARSE_H
#define EC801E_USB_AT_PARSE_H

struct ec801e_qnetdevctl_status {
    int type;
    int cid;
    int urc_enabled;
    int state;
};

int ec801e_parse_qcfg_value(const char *text, const char *key, int *out_value);
int ec801e_parse_qnetdevctl(const char *text, struct ec801e_qnetdevctl_status *out_status);

#endif
