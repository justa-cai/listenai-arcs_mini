/*
 * main.h
 *
 *  Created on: Apr. 9, 2024 for MARS/ARCS
 *      Author: bauldeng
 */

#ifndef __AUD_I2S_I2S_XFER_H
#define __AUD_I2S_I2S_XFER_H

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "nos_timer.h"
#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "log_print.h"
#include "Driver_I2S.h"
#include "Driver_GPIO.h"

//----------------------------------------------------------------------------
#define CHIP_I2S_DEV_CNT     2 //1 //

#if (CHIP_I2S_DEV_CNT >= 2)
#define USE_ONE_I2S_PER_CHIP    0 //1 // CAN BE 0 (default) or 1 if TWO I2S
#else
#define USE_ONE_I2S_PER_CHIP    1 // CAN ONLY BE 1 if NOT TWO_I2S
#endif


// There are several test scenarios in all for each I2S -
// 1) Slave RX  <-  Master TX
// 2) Slave TX  ->  Master RX
// 3) Master RX  <-  Slave TX
// 4) Master TX  ->  Slave RX
//
// 5) Slave RX/TX  <->  Master TX/RX    //TODO:
// 6) Master RX/TX  <->  Slave TX/RX    //TODO:

#if USE_ONE_I2S_PER_CHIP
// SLV_RX_FIRST = 1: from 1) to 4)
// SLV_RX_FIRST = 0: from 4) to 1)
#define TEST_ORDER_SLV_RX_FIRST         1 //0 //
#endif

//-----------------------------------
#define I2S_IDX0    0
#define I2S_IDX1    1
#define I2S_IDX2    2

#define ARCS_C0_FPGA    0
#define ARCS_C0_ASIC    1
#define ARCS_D0_FPGA    2
#define ARCS_D0_ASIC    3

#define DUT_SOC_VER ARCS_D0_FPGA // ARCS_C0_ASIC
//-----------------------------------
// Work as OUTPUT pin when I2S acts as slave
// Work as INTERRUT pin when I2S acts as master
#define NOTIFY_PIN_NUM      10 //FIXME:
#define NOTIFY_PIN          CSK_IOMUX_PAD_A, NOTIFY_PIN_NUM, 0 //FIXME:
#define NOTIFY_PIN_BIT     (0x1 << NOTIFY_PIN_NUM)

//-----------------------------------
#if (DUT_SOC_VER == ARCS_C0_ASIC)
//FIXME: I2S0 Pins on ARCS C0 EVB
#define I2S0_BCK    CSK_IOMUX_PAD_A, 0, 10 // GPIOA, 0, 10
#define I2S0_LRCK   CSK_IOMUX_PAD_A, 1, 10 // GPIOA, 1, 10
#define I2S0_DIN    CSK_IOMUX_PAD_A, 2, 10 // GPIOA, 2, 10
#define I2S0_DOUT   CSK_IOMUX_PAD_A, 11, 10 // GPIOA, 11, 10
// use I2S0_DIN or I2S0_DOUT if I2S0_DIN or I2S0_DOUT if NOT used?
#define I2S0_MCLK   CSK_IOMUX_PAD_A, 10, 18 // GPIOA, 10, Function 18 (DBG_CLK)

//FIXME: I2S1 Pins on ARCS C0 EVB
#if (CHIP_I2S_DEV_CNT >= 2)
#define I2S1_BCK    CSK_IOMUX_PAD_A, 12, 10 // GPIOA, 12, 10
#define I2S1_LRCK   CSK_IOMUX_PAD_A, 13, 10 // GPIOA, 13, 10
#define I2S1_DIN    CSK_IOMUX_PAD_A, 14, 10 // GPIOA, 14, 10
#define I2S1_DOUT   CSK_IOMUX_PAD_A, 15, 10 // GPIOA, 15, 10
// use I2S1_DIN or I2S1_DOUT if I2S1_DIN or I2S1_DOUT if NOT used?
#define I2S1_MCLK   I2S0_MCLK
#endif // CHIP_I2S_DEV_CNT >= 2)

#elif (DUT_SOC_VER == ARCS_D0_FPGA)

// I2S0 Pins on ARCS D0 FPGA
#define I2S0_BCK    CSK_IOMUX_PAD_A, 16, 9
#define I2S0_LRCK   CSK_IOMUX_PAD_A, 13, 9
#define I2S0_DIN    CSK_IOMUX_PAD_A, 14, 9
#define I2S0_DOUT   CSK_IOMUX_PAD_A, 15, 9
// use I2S0_DIN or I2S0_DOUT if I2S0_DIN or I2S0_DOUT if NOT used?
#define I2S0_MCLK   CSK_IOMUX_PAD_A, 10, 18 // GPIOA, 10, Function 18 (DBG_CLK)

//FIXME: I2S1 Pins on ARCS D0 FPGA
#if (CHIP_I2S_DEV_CNT >= 2)
#define I2S1_BCK    CSK_IOMUX_PAD_A, 24, 10
//#define I2S1_LRCK   CSK_IOMUX_PAD_A, 25, 10 //E32
#define I2S1_LRCK   CSK_IOMUX_PAD_A, 29, 10 //D19
#define I2S1_DIN    CSK_IOMUX_PAD_A, 26, 10
#define I2S1_DOUT   CSK_IOMUX_PAD_A, 27, 10
// use I2S1_DIN or I2S1_DOUT if I2S1_DIN or I2S1_DOUT if NOT used?
#define I2S1_MCLK   I2S0_MCLK
#endif // CHIP_I2S_DEV_CNT >= 2)

#endif // DUT_SOC_VER
//----------------------------------------------------------------------------

#define MAX_XFER_WORD       256 //1024
// Assume that sample count to send is TX_SAMP_CNT,
// and it may be TX_SAMP_CNT bytes, halfwords, or words, depending on sample bits!
#define TX_SAMP_CNT        MAX_XFER_WORD


bool enable_i2s_mclk(uint8_t i2s_idx);

void * i2s_in_init(uint8_t i2s_idx, uint32_t dev_bmp_flag, CSK_I2S_SignalEvent_t cb_event);
void * i2s_out_init(uint8_t i2s_idx, uint32_t dev_bmp_flag, CSK_I2S_SignalEvent_t cb_event);
void * i2s_inout_init(uint8_t i2s_idx, uint32_t dev_bmp_flag, CSK_I2S_SignalEvent_t cb_event);

int32_t i2s_config(void *i2s_dev, bool is_master, uint8_t use_tdm, uint8_t ch_cnt, int8_t ch_bits,
                  I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, uint32_t bck_slave);

void i2s_abort_rx(void *i2s_dev);
void i2s_abort_tx(void *i2s_dev);
void i2s_abort_txrx(void *i2s_dev);
void i2s_close(void *i2s_dev);

//----------------------------------------------------------------------------
// RX / TX buffer initialize & compare
void init_tx_buf(uint32_t total_bytes);
void init_rx_buf();
bool is_rx_eq_tx(uint32_t ch_bytes, int8_t ch_bits);

//----------------------------------------------------------------------------

typedef struct {
    uint8_t lr_bmp : 2;
    uint8_t alt_apc_ch : 1; // 1: use alternate APC channel if any
    uint8_t use_tdm : 1;
    uint8_t ch_cnt : 4;
    //uint8_t is_master : 1;

    int8_t ch_bits; // n bits High @WORD if n>0, else n bits Low @WORD
    I2S_PROTOCOL i2s_prt;

    uint32_t sampling_freq;
    uint32_t bck_slave;

    // requested sample count of data to transfer
    uint32_t req_cnt;

    // hint of this test case
    const char * hint;

} I2S_TCASE_CONFIG;

typedef enum {
    I2S_TEST_SLAVE_RX = 0, //'  RS',
    I2S_TEST_SLAVE_TX = 1, //'  TS',
    I2S_TEST_MASTER_RX = 2, //'  RM',
    I2S_TEST_MASTER_TX = 3, //'  TM',
    I2S_TEST_SLAVE_RX_MASTER_TX = 4, //'TMRS',
    I2S_TEST_SLAVE_TX_MASTER_RX = 5, //'RMTS',
    I2S_TEST_TYPE_COUNT
} I2S_TEST_TYPE;

typedef struct {
    void * i2s_mst;
    void * i2s_slv;
    I2S_TEST_TYPE test_type;
    I2S_TCASE_CONFIG * tcase_cfg;
} I2S_TEST_CASE;

// return I2S device if successfully, or else return NULL.
void * i2s_rx_case(uint8_t i2s_idx, bool as_master, bool start_rx, I2S_TCASE_CONFIG *cfg_p);
void * i2s_tx_case(uint8_t i2s_idx, bool as_master, bool start_tx, I2S_TCASE_CONFIG *cfg_p);

#define i2s_slave_rx_case(i2s_idx, start_rx, cfg_p)       i2s_rx_case(i2s_idx, false, start_rx, cfg_p)
#define i2s_master_rx_case(i2s_idx, start_rx, cfg_p)      i2s_rx_case(i2s_idx, true, start_rx, cfg_p)
#define i2s_slave_tx_case(i2s_idx, start_tx, cfg_p)       i2s_tx_case(i2s_idx, false, start_tx, cfg_p)
#define i2s_master_tx_case(i2s_idx, start_tx, cfg_p)      i2s_tx_case(i2s_idx, true, start_tx, cfg_p)

//void * i2s_slave_txrx_case(uint8_t i2s_idx, I2S_TCASE_CONFIG *cfg_p); //TODO:
//void * i2s_master_txrx_case(uint8_t i2s_idx, I2S_TCASE_CONFIG *cfg_p); //TODO:

int32_t i2s_rx_case_start(void *i2s_dev, I2S_TCASE_CONFIG *cfg_p);
int32_t i2s_tx_case_start(void *i2s_dev, I2S_TCASE_CONFIG *cfg_p);
int32_t i2s_txrx_case_start(void *i2s_dev, I2S_TCASE_CONFIG *cfg_p);

// I2S callback functions
void cb_i2s_slave_rx_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_slave_tx_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_master_rx_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_master_tx_event(uint32_t event_info, uint32_t usr_param);

// wait transfer done with timeout
bool WAIT_SLV_RX_DONE_TIMEOUT(uint32_t max_wait_ms);
bool WAIT_SLV_TX_DONE_TIMEOUT(uint32_t max_wait_ms);
bool WAIT_MST_RX_DONE_TIMEOUT(uint32_t max_wait_ms);
bool WAIT_MST_TX_DONE_TIMEOUT(uint32_t max_wait_ms);

bool WAIT_SLV_RX_MST_TX_DONE_TIMEOUT(uint32_t max_wait_ms);
bool WAIT_SLV_TX_MST_RX_DONE_TIMEOUT(uint32_t max_wait_ms);

//bool WAIT_SLV_TXRX_DONE_TIMEOUT(uint32_t max_wait_ms);
//bool WAIT_MST_TXRX_DONE_TIMEOUT(uint32_t max_wait_ms);


#if USE_ONE_I2S_PER_CHIP
// if USE_ONE_I2S_PER_CHIP == 1, use a pair of GPIO pins on master/slave side
// to indicate that I2S slave is ready and I2S master can TX/RX now!
void gpio_init();
void gpio_uninit();

//
// I2S slave configure a GPIO pin as OUTPUT, initialized to LOW level;
// Write 1 to the GPIO pin, notify I2S master when slave is ready to RX or TX;
// Write 0 to the GPIO pin, restore to idle when RX or TX is completed or timeout!
//
void gpio_out_slv_init();
void gpio_out_slv_notify();
void gpio_out_slv_restore();

//
// I2S master configure a GPIO pin as INTERRUPT (High level trigger);
// Disable the GPIO interrupt when initialized or its ISR is entered (GPIO interrupt is accepted);
// Enable the GPIO interrupt when I2S master is initialized and configured.
//
void gpio_int_mst_init();
bool gpio_int_mst_wait(uint32_t max_wait_ms); //return false if timeout
void gpio_int_mst_dis();
void gpio_int_mst_en();

#endif // USE_ONE_I2S_PER_CHIP


#endif /* __AUD_I2S_I2S_XFER_H */
