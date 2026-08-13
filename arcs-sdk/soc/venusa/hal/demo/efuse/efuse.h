/*
 * efuse.h
 *
 *  Created on: 2025年5月16日
 *      Author: USER
 */

#ifndef SRC_DRV_DEMO_DRV_DEMO_VENUSA_EFUSE_EFUSE_H_
#define SRC_DRV_DEMO_DRV_DEMO_VENUSA_EFUSE_EFUSE_H_

#include "venusa_ap.h"

int8_t efuse_read_word(uint8_t addr, uint32_t *val);

void efuse_program_ctrl(char enable);

int8_t efuse_write_bit(uint8_t addr, uint8_t bit);

int8_t efuse_write_word(uint8_t addr, uint32_t val);

void efuse_force_auto_load();


#endif /* SRC_DRV_DEMO_DRV_DEMO_VENUSA_EFUSE_EFUSE_H_ */
