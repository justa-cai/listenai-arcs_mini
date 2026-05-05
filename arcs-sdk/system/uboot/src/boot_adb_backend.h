#ifndef __BOOT_ADB_BACKEND_H__
#define __BOOT_ADB_BACKEND_H__

#include <stdbool.h>

#include "boot_adb_runtime.h"

bool boot_adb_backend_bind_runtime_ops(boot_adb_runtime_ops_t *ops);

#endif /* __BOOT_ADB_BACKEND_H__ */
