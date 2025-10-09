/*
 * main.h
 *
 *  Created on: Nov. 6, 2020 for VENUS (CP)
 *  Ported on: Dec. 18, 2023 for ARCS (AP)
 *      Author: bauldeng
 */

#ifndef __AUD_IN_AUD_OUT_MAIN_H
#define __AUD_IN_AUD_OUT_MAIN_H

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "chip.h"
#include "IOMuxManager.h"
#include "log_print.h"
#include "nos_timer.h"
#include "Driver_I2S.h"
#include "Driver_ADC_PDM.h"
#include "Driver_DAC.h"
#include "Driver_GPIO.h"

//----------------------------------------------------------------------------
#define DMA_CH_AUD_TX_DEF           ((uint8_t) 2)   // CLASSD/I2S TX
#define DMA_CH_AUD_RX_DEF           ((uint8_t) 1)   // ADC/DMIC/I2S RX
#define DMA_CH_AUD_ECHO_DEF         ((uint8_t) 3)   // CLASSD/I2S TX LOOPBACK

//-----------------------------------
// I2S0 Pin

#define I2S0_BCK    4 // GPIOA, 0, 10
#define I2S0_LRCK   5 // GPIOA, 1, 10
#define I2S0_DIN    6 // GPIOA, 2, 10
#define I2S0_DOUT   7 // GPIOA, 3, 10
//#define I2S0_BCK    24 // GPIOA, 8, 10
//#define I2S0_LRCK   13 // GPIOA, 9, 10
//#define I2S0_DIN    14 // GPIOA, 10, 10
//#define I2S0_DOUT   15 // GPIOA, 11, 10


// I2S0 Pin function select
#define I2S0_FUNC      9 // function 10 (except MCLK)

// I2S0 Pin group
#define I2S0_GROUP  CSK_IOMUX_PAD_A

#define I2S0_MCLK           I2S0_DOUT // same as I2S0_DIN or I2S0_DOUT if NOT used
#define I2S0_MCLK_GROUP     I2S0_GROUP // CSK_IOMUX_PAD_A // CSK_IOMUX_PAD_B
#define I2S0_MCLK_FUNC      18 // function 18 (DBG_CLK)

//-----------------------------------
// I2S1 Pin (NOT verified on ARCS_C0 EVB)

#define I2S1_BCK    4 // GPIOA, 4, 10
#define I2S1_LRCK   5 // GPIOA, 5, 10
#define I2S1_DIN    6 // GPIOA, 6, 10
#define I2S1_DOUT   7 // GPIOA, 7, 10

// I2S1 Pin function select
#define I2S1_FUNC       10 // function 10 (except MCLK)

// I2S1 Pin group
#define I2S1_GROUP  CSK_IOMUX_PAD_A

#define I2S1_MCLK           I2S1_DIN // same as I2S0_DIN or I2S0_DOUT if NOT used
#define I2S1_MCLK_GROUP     I2S1_GROUP // CSK_IOMUX_PAD_A // CSK_IOMUX_PAD_B
#define I2S1_MCLK_FUNC      18 // function 18 (DBG_CLK)

//NOTE: I2C0 and I2C1 cannot coexist in the following pin configuration!!
//-----------------------------------
// I2C0 Pin

//#define I2C0_SCL    1 // GPIOB, 1, 8
//#define I2C0_SDA    0 // GPIOB, 0, 8
//
//// I2C0 Pin group
//#define I2C0_GROUP  CSK_IOMUX_PAD_B

#define I2C0_SCL    4 // GPIOA, 4, 8
#define I2C0_SDA    5 // GPIOA, 5, 8
//#define I2C0_SCL    6 // GPIOA, 6, 8
//#define I2C0_SDA    7 // GPIOA, 7, 8

// I2C0 Pin group
#define I2C0_GROUP  CSK_IOMUX_PAD_A

// I2C0 Pin function select
#define I2C0_FUNC   8 // function 8

//-----------------------------------
// I2C1 Pin (NOT verified on ARCS_C0 EVB)

#define I2C1_SCL    0 // GPIOB, 0, 9
#define I2C1_SDA    1 // GPIOB, 1, 9

// I2C1 Pin function select
#define I2C1_FUNC   9 // function 9

// I2C1 Pin group
#define I2C1_GROUP  CSK_IOMUX_PAD_B

//-----------------------------------
// DMIC Pin

//FIXME:
#define DMIC01_CLK    3    // GPIOA, 3, 25 // collide with UART0 TX(LOG)!
#define DMIC01_DAT    4    // GPIOA, 4, 25 // collide with I2C0 SCL!

// DMIC01 Pin function select
#define DMIC_FUNC   25      // function 25

// DMIC01 Pin group
#define DMIC_GROUP  CSK_IOMUX_PAD_A

//----------------------------------------------------------------------------
//I2C device operations
void * get_i2c_dev();
bool init_i2c_dev(void *i2c_dev);
bool i2c_write(void *i2c_dev, uint32_t addr, const uint8_t* data, uint32_t num, uint32_t max_wait_ms);
bool i2c_read(void *i2c_dev, uint32_t addr, uint8_t* data, uint32_t num, uint32_t max_wait_ms);

//I2S/ADC_PDM/DAC functions
bool trigger_i2s_in();
bool trigger_i2s_out();
bool trigger_adc_dmic_in();
bool trigger_dac_out();

// Ping Pong support in X_IN_Y_OUT
bool init_i2s_in_pipo();
bool trigger_i2s_in_pipo();
bool init_i2s_out_pipo();
bool trigger_i2s_out_pipo();
bool init_adc_dmic_in_pipo();
bool trigger_adc_dmic_in_pipo();
bool init_dac_out_pipo();
bool trigger_dac_out_pipo();

// Ping Pong support is optional in the ONESHOT cases
bool i2s_oneshot_play();
bool i2s_oneshot_record();
bool dmic_oneshot_record();
bool adc_oneshot_record();
bool dac_oneshot_play();

//I2S/ADC_PDM/DAC callback functions
void cb_i2s_in_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_out_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_inout_event(uint32_t event_info, uint32_t usr_param);
void cb_adc_dmic_in_event(uint32_t event_info, uint32_t usr_param);
void cb_dac_out_event(uint32_t event_info, uint32_t usr_param);

void cb_i2s_oneshot_play_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_oneshot_record_event(uint32_t event_info, uint32_t usr_param);
void cb_dmic_oneshot_record_event(uint32_t event_info, uint32_t usr_param);
void cb_adc_oneshot_record_event(uint32_t event_info, uint32_t usr_param);
void cb_dac_oneshot_play_event(uint32_t event_info, uint32_t usr_param);


// init I2S IN device (also init ES7210 codec if mic_in_bmp != 0)
void * init_i2s_es7210_in(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t mic_in_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);
// init I2S IN device (also init WM8960 codec if mic_in_bmp != 0)
void * init_i2s_wm8960_in(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t mic_in_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);
// init I2S OUT device (also init WM8960 codec if spk_out_bmp != 0)
void * init_i2s_wm8960_out(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t spk_out_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);

// init I2S IN/OUT device (WM8960 codec)
void * init_i2s_wm8960_inout(bool i2s_master, uint16_t i2s_ch_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);

// init DMIC IN device group (support samp_bits = 24 only)
void * init_dmic_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event);

// init ADC IN device group (support samp_bits = 24 only)
void * init_adc_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event);

// init DAC OUT device group (support samp_bits = 24 only)
void * init_dac_out(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_DAC_SignalEvent_t cb_event);

#endif /* __AUD_IN_AUD_OUT_MAIN_H */
