#ifndef _LIB_SDC_H_
#define _LIB_SDC_H_
#include "../../include/lib_sdc.h"
#include "ftsdc021.h"

#undef SDC021_BURNIN

SD_RESULT
lib_sdc_erase(u8 ip_idx, u32 addr, u32 cnt);
SD_RESULT
lib_sdc_scan_card(u8 ip_idx, u8 speed);
SD_RESULT
lib_sdc_card_exist(u8 ip_idx);
SD_RESULT
lib_sdc_card_writable(u8 ip_idx);
SD_RESULT
lib_sdc_get_card_info(u8 ip_idx, u32* p_blk_len, u32* p_blk_num, u32* p_erase_size);
SD_RESULT
lib_sdc_read_sector(u8 ip_idx, u32 sector, u32 cnt, void* buff);
SD_RESULT
lib_sdc_write_sector(u8 ip_idx, u32 sector, u32 cnt, void* buff);
SD_RESULT
lib_sdc_card_insert(u8 ip_idx);
u32
lib_sdc_card_type(u8 ip_idx);
SD_RESULT
lib_sdc_set_bus_width(u8 ip_idx, u8 width);


#endif
