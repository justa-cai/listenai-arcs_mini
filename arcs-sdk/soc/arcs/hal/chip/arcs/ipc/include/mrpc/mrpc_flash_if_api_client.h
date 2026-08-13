#ifndef __MRPC_FLASH_IF_API_CLIENT_H__
#define __MRPC_FLASH_IF_API_CLIENT_H__

int32_t flash_if_init(FLASH_DEV * dev, unsigned char ud0, unsigned char ud1);

int32_t flash_if_read(int32_t offset, void * data, size_t len);

int32_t flash_if_write(int32_t offset, const void * data, size_t len);

int32_t flash_if_erase(int32_t offset, size_t size);

int32_t flash_if_write_protection_set(bool enable);

int flash_if_security_read(off_t offset, void * data, size_t len);

int flash_if_security_write(off_t offset, const void * data, size_t len);

int flash_if_security_erase(off_t offset);

uint8_t flash_if_check_security_support(void);


#endif