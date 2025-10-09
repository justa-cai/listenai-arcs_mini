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
	uint32_t ref_data_l[80] = {0x000000, 0x000000, 0x000000, 0x000009, 0x00006b, 0xfff546, 0x0005b1, 0xfff352, 0x00142b, 0xffec4c,
		0x002796, 0xffdc9e, 0x00408b, 0xffbff3, 0x0062d6, 0xff9009, 0x009838, 0xff3f2c, 0x00fcf3, 0xfe99f3,
		0x020a4f, 0xfc22f2, 0x1a7370, 0x275c5b, 0x0e71be, 0xfe7e84, 0xe8659b, 0xd8de9b, 0xc9f04d, 0xc2fb42,
		0xbfc1cf, 0xc47de6, 0xcdda8a, 0xdd39fc, 0xeeed71, 0x031a6a, 0x169a8f, 0x280277, 0x3551d2, 0x3dd60c,
		0x3fb4e8, 0x3ba490, 0x31e8d5, 0x2312a9, 0x10b736, 0xfd0538, 0xe980b5, 0xd816d4, 0xca993f, 0xc2854e,
		0xc02a84, 0xc434cb, 0xce1850, 0xdd0ac7, 0xef0d08, 0x02fa90, 0x16a63a, 0x27ecc9, 0x35527e, 0x3dc884,
		0x3fb572, 0x3ba49e, 0x31e8d5, 0x2312a9, 0x10b736, 0xfd0538, 0xe980b5, 0xd816d4, 0xca993f, 0xc2854e,
		0xc02a84, 0xc434cb, 0xce1850, 0xdd0ac7, 0xef0d08, 0x02fa90, 0x16a63a, 0x27ecc9, 0x35527e, 0x3dc884};

	uint32_t ref_data_r[80] = {0x000000, 0x000000, 0x000000, 0x000000, 0xffffe6, 0xffebd0, 0x0009ac, 0xffe90d, 0x002514, 0xffdd2b,
		0x004944, 0xffc0f0, 0x007794, 0xff8bcb, 0x00b699, 0xff3304, 0x011836, 0xfe9d8c, 0x01cf94, 0xfd70ff,
		0x03ac73, 0xf953d0, 0x3a7897, 0x4dadfd, 0x1d271a, 0xfc382d, 0xd09c00, 0xb1243c, 0x93ee00, 0x8563bc,
		0x800001, 0x89a49a, 0x9bcceb, 0xbadd2f, 0xde538a, 0x06ac8c, 0x2d5c6d, 0x5085fc, 0x6aa41a, 0x7c835f,
		0x7ec7eb, 0x772435, 0x6386ca, 0x45b384, 0x215ffc, 0xf97c6d, 0xd2aad5, 0xafb262, 0x952938, 0x84894d,
		0x8030e4, 0x891d0f, 0x9c4002, 0xba8517, 0xde8ddb, 0x067069, 0x2d7127, 0x505c71, 0x6aa431, 0x7c6996,
		0x7ec8cc, 0x77244e, 0x6386ca, 0x45b384, 0x215ffc, 0xf97c6d, 0xd2aad5, 0xafb262, 0x952938, 0x84894d,
		0x8030e4, 0x891d0f, 0x9c4002, 0xba8517, 0xde8ddb, 0x067069, 0x2d7127, 0x505c71, 0x6aa431, 0x7c6996};
#endif

	logInit(0, 115200);
	CLOGD("AUDIO CODEC DMIC 4M 500x test\n");

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
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;

	//Enable DMAC Interrupt
	enable_GINT();
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	CLOGD("AUDIO CODEC DMIC 4M 500x test\n");

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
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 3;		//100x sample ratio
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 0; 		//8KHz Sample Rate
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
