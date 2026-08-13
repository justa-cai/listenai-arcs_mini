#ifndef _XCCELA_CONFIG_H__
#define _XCCELA_CONFIG_H__
#include "../../PSRAM_Unified_Common.h"

#include <stdint.h>
#include "venusa_ap.h"

extern void __psram_xccela_mr_seq_configure(void);

extern void __psram_xccela_ahb_seq_configure(uint32_t clock_freq, uint32_t density);

extern void __psram_xccela_controller_die_type(void);

extern void __psram_xccela_controller_die_page_size(uint32_t density);

extern void __psram_xccela_controller_timing_configure(uint32_t clock_freq);

extern int32_t __psram_xccela_info_extra(uint32_t* density);

extern void __psram_xccela_die_para_configure(uint32_t clock_freq, uint32_t density);

extern void __psram_xccela_mr_print(void);

extern void __psram_xccela_enter_sleep_mode(__psram_unified_sleep_mode_t sleep_mode);

extern void __psram_xccela_exit_sleep_mode(void);

extern void __psram_xccela_refresh_rate_normal_set(void);

#endif  // _XCCELA_CONFIG_H__