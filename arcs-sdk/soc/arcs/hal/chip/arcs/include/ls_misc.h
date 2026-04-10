
#ifndef _LS_TEMP_H_
#define _LS_TEMP_H_

#include <stdbool.h>
#include <stdint.h>

#define FIXED_POINT_ROUND(x) ((int)((x) + 0.5))  // Round to the nearest integer

// Define temperature constants
#define TEMP_NORMAL (25)
#define TEMP_LOW (-40)
#define TEMP_HIGH (85)
#if RF_BOARD_VER == 2 //Taoyun
#define TEMP_THRESHOLD (10)
#else
#define TEMP_THRESHOLD (30)
#endif
#define DEF_BIASL_WF (7)

void ls_get_efuse_para(void);

int32_t ls_get_cur_temp(void);
void ls_set_temp_thr(uint32_t thr);
bool ls_temp_por_update(void);
void ls_temp_default_por(void);
bool ls_temp_rf_por_config(int32_t temp, bool realtime);
int32_t ls_calc_temp(float vptat);


int8_t ls_efuse_read_word(uint8_t addr, uint32_t *val);
int ls_efuse_write_word(uint32_t addr, uint32_t val);

int8_t ls_get_wifi_mac(uint8_t mac_addr[6]);

/// @}

#endif
