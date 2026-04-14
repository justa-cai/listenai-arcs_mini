#include "shell.h"
#include "project_version.h"

static int cmd_version(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    shellPrint(shell, "%s-%s\r\n", PROJECT_VERSION_STR, PROJECT_VERSION_COMMIT);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, version,
                 cmd_version, show firmware version);
