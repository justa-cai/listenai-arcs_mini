#ifndef ARCS_FLASH_H
#define ARCS_FLASH_H

#define FLASH_ADDR_BASE         (0x30000000)

int arcs_flash_init(void);
int arcs_flash_erase(uint32_t addr, size_t len);
int arcs_flash_write(uint32_t addr, const void *data, size_t len);
int arcs_flash_read(uint32_t addr, void *data, size_t len);
void flash_write_test(void);
void flash_read_speed_test(void);
#endif
