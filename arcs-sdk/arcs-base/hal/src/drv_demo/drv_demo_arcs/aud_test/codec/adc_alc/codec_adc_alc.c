#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "ClockManager.h"

int main(void ){
	uint32_t rdata;

	logInit(0, 115200);

	//Enable Audio Clock
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	__HAL_CRM_ADC_CLK_ENABLE();


	ADC_ENABLE_SETTING( );

	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN =1 ;    //Enable ADC Clock
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;   //Reset ADC

	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.ALCMIN = 3;       //ALCMIN = 3;
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.ALCMAX = 8;       //ALCMAX = 5;
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.NG = 16;          //Noise Gate = -72dB(0x10)
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.NG_EN = 1;        //Noise Gate Enable = 1;
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.ALCSEL_L = 1;     //ADC ALC Function Enable = 1;
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.ALCSEL_R = 1;     //ADC ALC Function Enable = 1;
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.TARGET_L = 16;      //ADC ALC Target level = -33dB(0x10)
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.TARGET_R = 16;
	IP_AUDIO_CODEC->REG_AUD_R8_ADC_CTRL3.bit.TOLERANCE = 2;    //ALC Target error tolerance Setting = +/-2dB(0x2)

	IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.ALCDCY = 3;       //ADC Decay time  = 2.66ms
	IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.ALCATK = 6;       //ADC ALC attack time = 2.66ms
	IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.DMIC_MODE = 0;
	IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.AUTORST_TYPE = 0;


	while(1);
}
