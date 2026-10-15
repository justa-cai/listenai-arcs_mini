#include <stdint.h>

#include "memap.h"
#include "firmware_compat.h"

_Static_assert(CONFIG_CP_FIRMWARE_COMPAT_OFFSET == ARCS_CP_PSRAM_CONFIG_OFFSET,
               "CP PSRAM config offset differs from the AP contract");
_Static_assert(CONFIG_MEM_PSRAM_BASE == MEM_CP_PSRAM_BASE,
               "prj.conf CP PSRAM base differs from memap.h");
_Static_assert(CONFIG_MEM_PSRAM_SIZE == MEM_CP_PSRAM_SIZE,
               "prj.conf CP PSRAM size differs from memap.h");

/* Store the CP PSRAM interval selected by prj.conf. The common linker script
 * pins these two words immediately after the standard ARCS image header. */
__attribute__((section(".cp_firmware_compat"), used, aligned(4)))
const arcs_cp_psram_config_t g_cp_firmware_psram = {
    .base = CONFIG_MEM_PSRAM_BASE,
    .size = CONFIG_MEM_PSRAM_SIZE,
};
