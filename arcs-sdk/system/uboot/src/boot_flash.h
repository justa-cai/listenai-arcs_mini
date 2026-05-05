#ifndef __BOOT_FLASH_H__
#define __BOOT_FLASH_H__

#include <stdint.h>
#include <stdbool.h>

int boot_flash_reg_set(uint8_t idx, uint32_t addr, uint32_t data_in, uint32_t data_mask, uint32_t *data_out);
int boot_flash_reg_get(uint8_t idx, uint32_t addr, uint32_t *data_out);
int boot_flash_init(void);
int boot_flash_id_get(uint32_t *id);
int boot_flash_size_get(uint32_t *size);
bool boot_flash_auto_unlock_mode_set(bool m);
bool boot_flash_auto_unlock_mode_get(void);
void boot_flash_read(uint8_t *src, uint8_t *dst, uint32_t size);
int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size);
int boot_flash_erase(uint8_t *addr, uint32_t size);
void boot_flash_session_begin(void);
void boot_flash_session_end(void);

void boot_flash_lock_clear(void);
void boot_flash_lock_resume(void);
void boot_flash_lock_boot(void);
void boot_flash_unlock_boot(void);

struct flash_status_register {
    uint32_t register_1;
    uint32_t register_2;
    uint32_t register_3;
};

struct flash_status_register *boot_flash_status_register_get(uint8_t idx);

#endif
