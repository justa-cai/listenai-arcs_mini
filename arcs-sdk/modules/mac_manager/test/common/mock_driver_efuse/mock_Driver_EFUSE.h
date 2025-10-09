#ifndef MOCK_DRIVER_EFUSE_H
#define MOCK_DRIVER_EFUSE_H

#include "Driver_EFUSE.h"

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(uint64_t, efuse_read_uuid);

void mock_driver_efuse_init(void);

void mock_driver_efuse_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_DRIVER_EFUSE_H */