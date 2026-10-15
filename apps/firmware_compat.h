#ifndef ARCS_FIRMWARE_COMPAT_H
#define ARCS_FIRMWARE_COMPAT_H

#include <stdint.h>

#define ARCS_CP_PSRAM_CONFIG_OFFSET 0x180U

typedef struct {
    uint32_t base;
    uint32_t size;
} arcs_cp_psram_config_t;

_Static_assert(sizeof(arcs_cp_psram_config_t) == 8U,
               "Unexpected CP PSRAM config size");

#endif /* ARCS_FIRMWARE_COMPAT_H */
