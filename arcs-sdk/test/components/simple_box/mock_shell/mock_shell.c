/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_shell.h"
#include "shell.h"

static Shell shell;
static char shellBuffer[512];

static char logBuffer[1024];
static char log_buf_index = 0;

extern unsigned int shellRunCommand(Shell *shell, ShellCommand *command);

signed short userShellWrite(char *data, unsigned short len)
{
    unsigned short length = len;
    signed short copied = 0;
    while (length--)
    {
        copied++;
        logBuffer[log_buf_index++] = *data++;
    }
    return copied;
}

void mock_shell_init(void)
{

    shell.read = NULL;
    shell.write = userShellWrite;

    shellInit(&shell, shellBuffer, 512);
}

int mock_shell_run(const char *cmd)
{
    return shellRun(&shell, cmd);
}

void mock_shell_reset(void)
{
    log_buf_index = 0;
    memset(logBuffer, 0, sizeof(logBuffer));
}

char *mock_shell_get_log_buf(size_t *log_size)
{
    *log_size = log_buf_index;
    return logBuffer;
}
