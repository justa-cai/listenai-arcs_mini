#ifndef LISA_SDMMC_H
#define LISA_SDMMC_H

#include "lisa_device.h"

#define LISA_DEVICE_OK 0
#define LISA_SDMMC_STATUS_OK 0

int lisa_sdmmc_probe(lisa_device_t *dev);
int lisa_sdmmc_status(lisa_device_t *dev);

#endif
