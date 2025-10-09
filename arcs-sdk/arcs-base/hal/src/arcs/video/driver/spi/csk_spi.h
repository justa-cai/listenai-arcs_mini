#ifndef _CSK_SPI_H_
#define _CSK_SPI_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chip.h"
#include "IOMuxManager.h"
#include "Driver_SPI.h"
#include "log_print.h"
#include "csk_timer.h"


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Global Variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//#define SPI_TRANSFER_TIMEOUT    3
#define SPI_DATA_BITS           8

//SPI1 GPIO Configuration
#if (IC_BOARD == 0)
#define SPI_BUS_SPEED           5000000 // 2000000 // 2MHz
#else
#define SPI_BUS_SPEED          50000000 // 60000000 // 10MHz, 15MHz, 30MHz, 60MHz//#error Redefine PINs of SPI OLED on ASIC!! // FIXME:
#endif // IC_BOARD

#define SPI_DMA_MAX_TRANCNT     512


void csk_spi_init(uint8_t spi_index);
void csk_spi_write(void *pdata, uint32_t num);
void csk_spi_write_hold(void *pdata, uint32_t num);

uint32_t csk_spi_get_dma_size(void);
bool csk_spi_get_dma_done(void);
bool csk_spi_check_timeout(void);

void csk_spi_data_bit_set(uint8_t bit);

#endif
