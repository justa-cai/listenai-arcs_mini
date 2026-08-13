#ifndef _ARCS_XCCELA_CONFIG_H__
#define _ARCS_XCCELA_CONFIG_H__

#include <stdint.h>

#include "../../PSRAM_Unified_Common.h"

void __psram_xccela_mr_seq_configure(void);
void __psram_xccela_ahb_seq_configure(uint32_t clock_freq, uint32_t density);
void __psram_xccela_controller_die_type(void);
void __psram_xccela_controller_die_page_size(uint32_t density);
void __psram_xccela_controller_timing_configure(uint32_t clock_freq);
int32_t __psram_xccela_info_extra(uint32_t *density);
void __psram_xccela_die_para_configure(uint32_t clock_freq, uint32_t density);
void __psram_xccela_mr_print(void);
int32_t __psram_xccela_enter_sleep_mode(__psram_unified_sleep_mode_t sleep_mode);
void __psram_xccela_exit_sleep_mode(void);
void __psram_xccela_refresh_rate_normal_set(void);

#endif /* _ARCS_XCCELA_CONFIG_H__ */
