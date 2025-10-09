#include "lsfs.h"
#include "shell.h"

static int mkfs_fatfs(int argc, char **argv)
{
    int ret;
    Shell *shell = shellGetCurrent();

    if (argc < 2) {
        shellPrint(shell, "invalid para, usage: mkfatfs <disk_name>\n");
        return -1;
    }

    ret = lsfs_mkfs(LSFS_FATFS, argv[1], NULL, 0);
    if (ret != 0) {
        shellPrint(shell, "failed to mkfatfs: %d\n", ret);
        return ret;
    }

    shellPrint(shell, "create fatfs on disk %s successfully\n", argv[1]);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, mkfatfs,
                 mkfs_fatfs, mkfatfs);
