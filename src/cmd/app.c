#include "shell.h"
#include "stdint.h"
#include "string.h"
#include "cmd.h"

static int wakeup(int argc, char **argv)
{
    extern void client_wakeup_test();
    client_wakeup_test();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, wakeup,
                 wakeup, wakeup test cmd);
