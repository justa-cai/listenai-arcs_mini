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
	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0xffffec, 0xffac61, 0xffdace, 0x0007a5, 0x001691, 0x00af5c, 0xffc99a, 0x01dffc,
	0xfd6b27, 0x1f2e81, 0x20e5c9, 0x01a541, 0xec2357, 0xd3a773, 0xc5b6f9, 0xbe5336, 0xc22908, 0xcebd7a,
	0xe31580, 0xfbdbab, 0x1593f7, 0x2baaec, 0x3b64ce, 0x41ee25, 0x3e460b, 0x318bb5, 0x1cdc96, 0x03fdcc,
	0xea9f5e, 0xd44107, 0xc4b9b5, 0xbe2ad9, 0xc17be6, 0xce8feb, 0xe31580, 0xfbdbab, 0x1593f7, 0x2baaec,
	0x3b64ce, 0x41ee25, 0x3e460b, 0x318bb5, 0x1cdc96, 0x03fdcc, 0xea9f5e, 0xd44107, 0xc4b9b5, 0xbe2ad9,
	0xc17be6, 0xce8feb, 0xe31580, 0xfbdbab, 0x1593f7, 0x2baaec, 0x3b64ce, 0x41ee25, 0x3e460b, 0x318bb5,
	0x1cdc96, 0x03fdcc, 0xea9f5e, 0xd44107, 0xc4b9b5, 0xbe2ad9, 0xc17cad, 0xce9620, 0xe30dad, 0xfbe85d,
	0x15803b, 0x2bc8a9, 0x3b37da, 0x4236fc, 0x3da45c, 0x2fb5be, 0x1d2716, 0x03deff, 0xeaad51, 0xd43b5f,
	0xc4bb17, 0xbe2b74, 0xc17adb, 0xce9499, 0xe31580, 0xfbdbab, 0x1593f7, 0x2baaec, 0x3b64ce, 0x41ee25,
	0x3e460b, 0x318bb5, 0x1cdc96, 0x03fdcc, 0xea9f5e, 0xd44107, 0xc4b9b5, 0xbe2ad9, 0xc17be6, 0xce8feb};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0xfffde4, 0xff4abb, 0xffc8c8, 0xfff192, 0x005dfe, 0x01165c, 0x00029e, 0x030bdc,
	0xfc67d4, 0x4276eb, 0x40cc74, 0x034dc7, 0xd79556, 0xa71d06, 0x8b21ca, 0x800001, 0x84e978, 0x9d97db,
	0xc6ab61, 0xf82510, 0x2b840c, 0x578a26, 0x77501d, 0x7fffff, 0x7c1f72, 0x62daec, 0x395b0a, 0x07bf53,
	0xd4a743, 0xa84622, 0x8927b6, 0x800001, 0x838d44, 0x9d482b, 0xc6ab61, 0xf82510, 0x2b840c, 0x578a26,
	0x77501d, 0x7fffff, 0x7c1f72, 0x62daec, 0x395b0a, 0x07bf53, 0xd4a743, 0xa84622, 0x8927b6, 0x800001,
	0x838d44, 0x9d482b, 0xc6ab61, 0xf82510, 0x2b840c, 0x578a26, 0x77501d, 0x7fffff, 0x7c1f72, 0x62daec,
	0x395b0a, 0x07bf53, 0xd4a743, 0xa84622, 0x8927b6, 0x800001, 0x838e16, 0x9d4e57, 0xc6a389, 0xf831d5,
	0x2b7022, 0x57a846, 0x772266, 0x7fffff, 0x7b7871, 0x6107d7, 0x39a7a4, 0x079f3a, 0xd4b614, 0xa83fe5,
	0x89297b, 0x800001, 0x838c61, 0x9d4cca, 0xc6ab61, 0xf82510, 0x2b840c, 0x578a26, 0x77501d, 0x7fffff,
	0x7c1f72, 0x62daec, 0x395b0a, 0x07bf53, 0xd4a743, 0xa84622, 0x8927b6, 0x800001, 0x838d44, 0x9d482b};
#endif

	logInit(0, 115200);
	CLOGD("AUDIO CODEC DMIC 2M 250x test enter\n");

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

	CLOGD("AUDIO CODEC DMIC 2M 250x test\n");

	//Configure DMA.CH0  APC_rx_l to mem
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = dst_l; //Destination addr
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) | (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 4096;

	//Configure DMA.CH1 APC rx r to mem
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = dst_r; //Destination addr
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) | (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 4096;

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

	DMIC_ENABLE_SETTING();
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 1;		//250x sample ratio
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
