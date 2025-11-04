#ifndef _CSK_QSPI_LCD_H_
#define _CSK_QSPI_LCD_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "Driver_QSPI_LCD.h"
#include "Driver_GPDMA.h"

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Global Variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#define QSPI_LCD_DATA_BITS           8
#define QSPI_LCD_DMA_MAX_TRANCNT     8192        // 512

typedef enum {
    QSPI_LCD_TXIO_PIO,
    QSPI_LCD_TXIO_DMA,
    QSPI_LCD_TXIO_BUTT,
} qspi_lcd_txio_t;

typedef enum {
    QSPI_LCD_CPOL0_CPOH0,
    QSPI_LCD_CPOL0_CPOH1,
    QSPI_LCD_CPOL1_CPOH0,
    QSPI_LCD_CPOL1_CPOH1,
    QSPI_LCD_CPOL_CPOH_BUTT,
} qspi_lcd_cpol_cpoh_t;

typedef enum {
    QSPI_LCD_FORMAT_RGB565,
    QSPI_LCD_FORMAT_RGB888,
    QSPI_LCD_FORMAT_BUTT,
} qspi_lcd_format_t;

typedef struct {
    uint32_t clk_hz;
    qspi_lcd_txio_t txio;                       /*!< QSPI_OUT_TXIO_PIO/QSPI_OUT_TXIO_DMA */
    qspi_lcd_cpol_cpoh_t cp;                    /*!< QSPI_OUT_CPOL0_CPOH0/01/10/11 */
    bool is_msb;                                /*!< true/false */
    uint8_t lane_num;
    uint16_t width;
    uint16_t height;
    qspi_lcd_format_t format;
} qspi_lcd_config_t;


int32_t qspi_lcd_init(qspi_lcd_config_t *qspi_cfg);
int32_t qspi_lcd_deinit(void);
int32_t qspi_lcd_write(void *pdata, uint32_t num);
void qspi_lcd_wait_done(void);
bool is_qspi_lcd_irq_tx_done(void);
int32_t qspi_lcd_set_lane_num(uint8_t lane_num);
int32_t qspi_lcd_set_data_bit(uint8_t bit);
int32_t qspi_lcd_set_dma_size(uint32_t size_byte);
int32_t qspi_lcd_dma_enable(void);
int32_t qspi_lcd_dma_disable(void);
void qspi_lcd_reg_dump(void);
void qspi_lcd_reset(void);

int32_t qspi_lcd_gpdma_init(csk_gpdma_ch_t gpdma_ch);
int32_t qspi_lcd_gpdma_start(csk_gpdma_ch_t gpdma_ch, void* pbuf, uint32_t size_word);
int32_t qspi_lcd_gpdma_stop(csk_gpdma_ch_t gpdma_ch);
uint32_t qspi_lcd_gpdma_get_cnt(void);


#endif
