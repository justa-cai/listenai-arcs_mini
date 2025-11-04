#include <stdio.h>
#include <stdlib.h>

#include "log_print.h"

#include "chip.h"

#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "ClockManager.h"

volatile int intr_apc_done;
volatile int j;
int exp_data[40];

#define DEBUG 1
/**
 * @brief Reads a value from the data memory at the specified address.
 *
 * @param addr The memory address to read the value from.
 * @return Returns the value read from the data memory.
 */
int read_mem(int addr) {
    int rdval; // Define an integer variable rdval to store the read value
    rdval = *(int *)addr;   // Read the value from the data memory and assign it to rdval
    return rdval; // Return the value of rdval
}

/**
 * @brief Stores a value at the specified memory address in data memory.
 *
 * @param addr The memory address where the value will be stored.
 * @param val The value to be stored.
 */
void write_mem(int addr, int val) {
    *(int *)addr = val;   // Stores the value at the specified memory address in data memory.
}

static void delay(void)
{
	for(int i = 0; i < 10000; i++)
		;
}

void *gpio_A = NULL;

int main()
{
	int iocfg = 1;
	int rdata;
	unsigned int ref_data_r[80];
	j=0;
	intr_apc_done = 0;

	logInit(0, 115200);
	CLOGD("adc01 -> iis0 loopback test\n");

	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 1;
	__HAL_CRM_ADC_CLK_ENABLE();

	//debug port
#ifdef DEBUG
	//IO Config
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; //iis mclk,10:dmic,31:aud_debug_clk
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1; //4: aud_dmic_clk,13:fsclk,1:adc_mclk_test
	IP_CMN_IOMUX->REG_PAD_GPIOA_10.bit.PAD_GPIOA_10_FSEL = 18; //clk
	//IP_CMN_IOMUX->REG_PAD_GPIOA_08.bit.PAD_GPIOA_08_FSEL = 11; //debug clk-->iis mclk

	// for adc 3bit sdm  data
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_OUT_SEL = 2;
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_OUT_EN = 1;
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 17;
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1; //10:classd mclk test,13:fsclk,1:adc_mclk_test
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_IN_SEL = 2; //3bit sdm
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.DEBUG_MODE = 2; //adc_dig_debug_mode
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELB = 0x5; //data
	//IP_CMN_IOMUX->REG_PAD_GPIOA_12.bit.PAD_GPIOA_12_FSEL = 11; //clk

	//sdm 3bit
	IP_CMN_IOMUX->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_FSEL = 20; //sdm0
	IP_CMN_IOMUX->REG_PAD_GPIOA_17.bit.PAD_GPIOA_17_FSEL = 20;//sdm1
	IP_CMN_IOMUX->REG_PAD_GPIOA_18.bit.PAD_GPIOA_18_FSEL = 20; //sdm2
#endif

	Codec_IO_Cfg(iocfg); //iis0 pin cfg

	//I2S Config
	//Set I2S DATA Length
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_WLEN = 0x2;
	//Set BCK Polar
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_LRCK_POL = 0x1;
	//MSB of Data out is 1 cycle delay to LRCK edge
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_DLY = 0x1;
	//MSB of Data in is 1 cycle delay to LRCK edge
	//IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_RX_DLY = 0x1;
	//BCK Force on for master mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_FORCE_ON = 0x1;
	//Set Master Mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_MASTER_MODE = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_BCK_DIV = 0x9;
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_BCK_DIV_LD = 1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_LRCK = 1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_OUT_SRC = 1;    //adc0->iis loop
	//IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BYPASS_FIFOVLD = 1;

	//IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.ADC_SINGLE_CH_MODE = 1;
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 1; //250x sample ratio
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 8; //48KHz Sample Rate
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_ADC_MODE = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_ADC_SAR_DELAY_CTRL = 0;

	ADC_ENABLE_SETTING();
	IP_AUDIO_CODEC->REG_AUD_R10_ADC_CTRL5.bit.FILGAIN_REG = 0x10000; //should be open,mars and arcs c0 need
	IP_AUDIO_CODEC->REG_AUD_R10_ADC_CTRL5.bit.FILGAIN_REGEN = 1;
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN = 1; //Enable internal clock
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;	//Release Digital ADC reset

	APC_Enable(1);
	APC_Tx_Ch_L_Enable(0,1);
	APC_Tx_Ch_R_Enable(0,1);
	APC_I2S0_Enable(1);

	while(1);
}

