#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#define CODEC_DST_ADDR_L  (CMN_PSRAM_REGION+0x200)
#define CODEC_DST_ADDR_R  (CMN_PSRAM_REGION+0x1000)
volatile int dma_ch1_done;
volatile int dma_ch0_done;

uint32_t dst_l[4096];
uint32_t dst_r[4096];
static void* GPIOA_Handler = NULL;

/* 32 bit word AHB read or write functions:default */
int read_mem(int addr) {
    int rdval;
    rdval = *(int *)addr;   // read value from data memory
    return rdval;
}

void write_mem(int addr, int val) {
    *(int *)addr = val;   // store value in data memory
}

//Interrupt Handler
void eclic_dma_gp_int_handler (void)
{
	uint rdata;
	rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
	CLOGD("GPDMA DMA INT rdata = 0x%0x.\n",rdata);
	if((rdata & 0x1) == 0x1) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x1; //Clear Channel0 INT
		dma_ch0_done = 1;
	}
	if((rdata & 0x2) == 0x2) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x2; //Clear Channel1 INT
		dma_ch1_done = 1;
	}
	return;
}

int main(void ){
	dma_ch1_done = 0;
	dma_ch0_done = 0;
	uint32_t ref_data_tmp,act_data_tmp;
#if 0
	  uint32_t ref_data_l[100] = {0x000000, 0x000000, 0x00003a, 0xffb8f8, 0x002657, 0x006252, 0x006c77, 0x008765, 0xff5088, 0x00d73f,
	  0xfcb405, 0x1b10b4, 0x0526cb, 0xd0283f, 0xc0ca02, 0xd0d95d, 0x001ed6, 0x2d104a, 0x40f6ff, 0x2ddfe3,
	  0x006576, 0xd2e654, 0xbfbd95, 0xd2419c, 0xff9693, 0x2d2c65, 0x40423b, 0x2dafe4, 0x006568, 0xd2e654,
	  0xbfbd95, 0xd2419c, 0xff9693, 0x2d2c65, 0x40426e, 0x2db32b, 0x00616f, 0xd2eca1, 0xbfb402, 0xd24fb1,
	  0xff81eb, 0x2d4c69, 0x40023b, 0x2cb986, 0x008674, 0xd2da0c, 0xbfc251, 0xd2405d, 0xff962e, 0x2d2d69,
	  0x404138, 0x2db272, 0x006569, 0xd2e654, 0xbfbd95, 0xd2419c, 0xff9693, 0x2d2c65, 0x40423b, 0x2dafe4,
	  0x006568, 0xd2e654, 0xbfbd95, 0xd2419c, 0xff9693, 0x2d2c65, 0x40426e, 0x2db32b, 0x00616f, 0xd2eca1,
	  0xbfb402, 0xd24fb1, 0xff81eb, 0x2d4c69, 0x40023b, 0x2cb986, 0x008674, 0xd2da0c, 0xbfc251, 0xd2405d,
	  0xff962e, 0x2d2d69, 0x404138, 0x2db272, 0x006569, 0xd2e654, 0xbfbd95, 0xd2419c, 0xff9693, 0x2d2c65,
	  0x40423b, 0x2dafe4, 0x006568, 0xd2e654, 0xbfbd95, 0xd2419c, 0xff9693, 0x2d2c65, 0x40426e, 0x2db32b};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0x00003d, 0xff6ebe, 0x005064, 0x00bee0, 0x00e1e9, 0x010149, 0xfeb4d2, 0x018fb5,
	  0xf9a5b6, 0x371761, 0x0a3c00, 0xa041c0, 0x8182ea, 0xa1da69, 0x002dc9, 0x5a3850, 0x7fffff, 0x5ba865,
	  0x00e3c0, 0xa5abeb, 0x800001, 0xa4a72b, 0xff1e28, 0x5a70c4, 0x7fffff, 0x5b4af4, 0x00e3a6, 0xa5abeb,
	  0x800001, 0xa4a72b, 0xff1e28, 0x5a70c4, 0x7fffff, 0x5b4e3b, 0x00dfad, 0xa5b237, 0x800001, 0xa4b540,
	  0xff0980, 0x5a90c8, 0x7fffff, 0x5a5496, 0x0104b2, 0xa59fa4, 0x800001, 0xa4a5ec, 0xff1dc4, 0x5a71c9,
	  0x7fffff, 0x5b4d82, 0x00e3a9, 0xa5abeb, 0x800001, 0xa4a72b, 0xff1e28, 0x5a70c4, 0x7fffff, 0x5b4af4,
	  0x00e3a6, 0xa5abeb, 0x800001, 0xa4a72b, 0xff1e28, 0x5a70c4, 0x7fffff, 0x5b4e3b, 0x00dfad, 0xa5b237,
	  0x800001, 0xa4b540, 0xff0980, 0x5a90c8, 0x7fffff, 0x5a5496, 0x0104b2, 0xa59fa4, 0x800001, 0xa4a5ec,
	  0xff1dc4, 0x5a71c9, 0x7fffff, 0x5b4d82, 0x00e3a9, 0xa5abeb, 0x800001, 0xa4a72b, 0xff1e28, 0x5a70c4,
	  0x7fffff, 0x5b4af4, 0x00e3a6, 0xa5abeb, 0x800001, 0xa4a72b, 0xff1e28, 0x5a70c4, 0x7fffff, 0x5b4e3b};
#endif

	logInit(0, 115200);
	CLOGD("AUDIO CODEC DMIC 4M 250x test\n");

	GPIOA_Handler = GPIOA();
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);

	//Enable Audio Clock
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	__HAL_CRM_DAC_CLK_ENABLE();
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;

	//Codec_IO_Cfg(2);
	//IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, 24);
	//IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, 24);
	//IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, 24);
	IP_CMN_IOMUX->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_FSEL = 24;//clk
	IP_CMN_IOMUX->REG_PAD_GPIOA_04.bit.PAD_GPIOA_04_FSEL = 24;//d0
	IP_CMN_IOMUX->REG_PAD_GPIOA_05.bit.PAD_GPIOA_05_FSEL = 24;//d1

	//debug port
	// for adc mclk
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; //10:dmic,31:aud_debug_clk
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1; //4: aud_dmic_clk,13:fsclk,1:adc_mclk_test
	IP_CMN_IOMUX->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_FSEL = 18; //debug clk
	//IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, 0);
	//GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN7, CSK_GPIO_DIR_OUTPUT);
    //GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN7, 1);
    //SysTick_Delay_Ms(100);
    //GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN7, 0);

	//Rx CH0 Select ADC01
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0;

	//RX Channel L Mode 24bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x3;
	//RX Channel R Mode 24bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x3;
	//Set RX CH0 DMA Threshold
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x3;

	//Enable DMAC Interrupt
	enable_GINT();
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	CLOGD("AUDIO CODEC DMIC 4M 250x test\n");

	//Configure DMA.CH0  APC_rx_l to mem
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = dst_l;               //Destination addr
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) | (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 4096;

	//Configure DMA.CH1 APC rx r to mem
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = dst_r;               //Destination addr
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) | (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 4096;

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

	DMIC_ENABLE_SETTING();
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 1;		//250x sample ratio
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 3; 		//16KHz Sample Rate
	IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.ADCVOL_R = 70;
	IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.ADCVOL_L = 70;
	IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.HPF2EN = 1;
	IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.HPF1EN = 1;
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN = 1; //Enable internal clock
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;	//Release Digital ADC reset

	APC_Enable(1);
	APC_Rx_Ch_L_Enable(0,1);
	APC_Rx_Ch_R_Enable(0,1);

	CLOGD("Warning DMA interrupt b ... \n");
	while(!(dma_ch1_done));
#if 0
	for(int i=0; i<100; i++){
		ref_data_tmp = ref_data_l[i];
		act_data_tmp = read_mem(dst_l + i);
		if(ref_data_tmp != act_data_tmp){
			CLOGD("Left ERROR:ref_data = %x vs act_data = %x\n",ref_data_tmp,act_data_tmp);
			//c_fail();
		}
	}
	for(int i=0; i<100; i++){
		ref_data_tmp = ref_data_r[i];
		act_data_tmp = read_mem(dst_r + i);
		if(ref_data_tmp != act_data_tmp){
			CLOGD("Right ERROR:ref_data = %x vs act_data = %x\n",ref_data_tmp,act_data_tmp);
			//c_fail();
		}
	}
#endif
	while(1);
}
