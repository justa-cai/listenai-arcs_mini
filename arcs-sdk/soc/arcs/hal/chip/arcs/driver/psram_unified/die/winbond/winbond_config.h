#ifndef _ARCS_WINBOND_CONFIG_H__
#define _ARCS_WINBOND_CONFIG_H__

#include <stdint.h>

#include "../../PSRAM_Unified_Common.h"

void __psram_winbond_mr_seq_configure(void);
void __psram_winbond_ahb_seq_configure(void);
void __psram_winbond_controller_die_type(void);
void __psram_winbond_controller_die_page_size(uint32_t density);
void __psram_winbond_controller_timing_configure(uint32_t clock_freq);
int32_t __psram_winbond_info_extra(uint32_t *density);
void __psram_winbond_die_para_configure(uint32_t clock_freq, uint32_t density);
void __psram_winbond_mr_print(void);

#endif /* _ARCS_WINBOND_CONFIG_H__ */
