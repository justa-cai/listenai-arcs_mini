#ifndef __LISTENAI_FLASH_H__
#define __LISTENAI_FLASH_H__

#include "spiflash.h"

void listen_flash_init(void);

FLASH_DEV * listen_flash_get_dev(void);

#endif
