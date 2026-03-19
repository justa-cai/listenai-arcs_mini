#ifndef FONT_ROMFS_H
#define FONT_ROMFS_H

#include <stdbool.h>
#include <stdint.h>

bool font_romfs_get(const char *path, uint8_t **data, uint32_t *size);

#endif /* FONT_ROMFS_H */
