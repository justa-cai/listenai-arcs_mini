#ifndef BOOT_FLASH_H
#define BOOT_FLASH_H

#include <stdint.h>

int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size);
int boot_flash_erase(uint8_t *addr, uint32_t size);
void boot_flash_session_begin(void);
void boot_flash_session_end(void);

#endif
