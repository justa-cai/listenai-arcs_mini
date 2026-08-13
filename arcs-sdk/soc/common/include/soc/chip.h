#ifndef SOC_COMMON_INCLUDE_SOC_CHIP_H_
#define SOC_COMMON_INCLUDE_SOC_CHIP_H_

#include <stdint.h>

#include "autoconf.h"

#if defined(CONFIG_SOC_ARCS) && (CONFIG_SOC_ARCS == 1)
#include "arcs_ap.h"
#elif defined(CONFIG_SOC_VENUSA) && (CONFIG_SOC_VENUSA == 1)
#include "venusa_ap.h"
#else
#error "Unsupported SoC for <soc/chip.h>"
#endif

#ifndef _FAST_FUNC_SRAM
#define _FAST_FUNC_SRAM
#endif

int soc_boot_core(uint8_t target_core_id, uint32_t boot_addr);

#endif /* SOC_COMMON_INCLUDE_SOC_CHIP_H_ */
