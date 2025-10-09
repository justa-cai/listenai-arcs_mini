#include "cmd.h"

#include "shell.h"
#include "sysheap.h"

static int heap_cmd_handler(int argc, char **argv)
{
    heap_summary_info();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, heap,
                 heap_cmd_handler, heap info);
