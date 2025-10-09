
#ifndef _LS_TEMP_H_
#define _LS_TEMP_H_

#include <stdint.h>

#define FIXED_POINT_ROUND(x) ((int)((x) + 0.5))  // Round to the nearest integer

// Define temperature constants
#define TEMP_NORMAL (25)
#define TEMP_LOW (-40)
#define TEMP_HIGH (85)
#define HYSTERESIS_THRESHOLD (10)
#define DEF_BIASL_WF (7)
#define TEMP_SENSE_PERIOD_LONG 30000000 //in us

void ls_read_efuse_temp_para(void);

int32_t ls_get_cur_temp(void);

void ls_temp_por_update(void);
void ls_temp_default_por(void);

int8_t ls_efuse_read_word(uint8_t addr, uint32_t *val);
int ls_efuse_write_word(uint32_t addr, uint32_t val);

int8_t ls_get_wifi_mac(uint8_t mac_addr[6]);

/// @}

#endif
