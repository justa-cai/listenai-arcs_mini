#ifndef __ADB_SHELL_H__
#define __ADB_SHELL_H__

#include <stdint.h>

void adb_shell_init(void);
int adb_shell_write_datas(const char *data, int size);
int adb_printf(const char *format, ...);

#endif
