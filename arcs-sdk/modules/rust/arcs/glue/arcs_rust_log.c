/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arcs_rust_log.h"
#include <elog.h>

void arcs_rust_log(uint8_t level, const char *tag, const char *msg, size_t msg_len)
{
    /* "%.*s" lets elog_output handle a non-NUL-terminated buffer correctly. */
    elog_output(level, tag, __FILE__, __func__, __LINE__,
                "%.*s", (int)msg_len, msg);
}
