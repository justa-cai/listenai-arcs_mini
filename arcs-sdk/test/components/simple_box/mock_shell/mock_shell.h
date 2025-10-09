/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void mock_shell_init(void);

void mock_shell_reset(void);

int mock_shell_run(const char *cmd);

char *mock_shell_get_log_buf(size_t *log_size);

#ifdef __cplusplus
}
#endif