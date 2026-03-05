
#ifndef __FLASH_IF_H__
#define __FLASH_IF_H__
#include "spiflash.h"

int32_t flash_if_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1);
int32_t flash_if_read_jedec_id(uint32_t *jedec_id);

int32_t flash_if_read(int32_t offset, void *data, size_t len);

int32_t flash_if_write(int32_t offset, const void *data, size_t len);

int32_t flash_if_erase(int32_t offset, size_t size);

int32_t flash_if_erase_page(off_t offset, size_t size);

int32_t flash_if_write_protection_set(bool enable);

uint8_t flash_if_check_security_support(void);

int8_t flash_if_erase_otp(void);

int8_t flash_if_set_otp_flag(uint32_t flag);

int flash_if_security_erase(off_t offset);

int flash_if_security_read(off_t offset, void *data, size_t len);

int flash_if_security_write(off_t offset, const void * data, size_t len);

#endif
