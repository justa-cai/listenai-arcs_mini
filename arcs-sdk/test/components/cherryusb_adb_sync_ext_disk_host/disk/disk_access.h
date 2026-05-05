#ifndef TEST_DISK_ACCESS_H
#define TEST_DISK_ACCESS_H

#include <stdint.h>

#define DISK_IOCTL_GET_SECTOR_SIZE 2

int disk_access_ioctl(const char *name, uint8_t cmd, void *buff);
int disk_access_read(const char *name, uint8_t *data, uint64_t start_sector, uint32_t num_sector);
int disk_access_write(const char *name, const uint8_t *data, uint64_t start_sector, uint32_t num_sector);

#endif
