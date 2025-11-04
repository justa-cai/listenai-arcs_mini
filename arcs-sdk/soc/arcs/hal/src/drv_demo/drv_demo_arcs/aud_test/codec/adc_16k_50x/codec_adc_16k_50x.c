#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"

#define CODEC_DST_ADDR_L  (CMN_PSRAM_REGION+0x200)
#define CODEC_DST_ADDR_R  (CMN_PSRAM_REGION+0x1000)
volatile int dma_ch1_done;
volatile int dma_ch0_done;

int main(void ){
	dma_ch1_done = 0;
	dma_ch0_done = 0;
	uint32_t ref_data_tmp,act_data_tmp;

	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0x000000, 0xfffffe, 0xfffebb, 0xffff45, 0xfffd77, 0xffff26, 0xfffbf9, 0xffff68,
	  0xfffa10, 0x000030, 0xfff787, 0x0001c8, 0xfff3f3, 0x0004d4, 0xffee48, 0x000b47, 0xffe28e, 0x001eba,
	  0xffb199, 0x01eee6, 0x07e7f4, 0x0a8996, 0x0d75c5, 0x101453, 0x12e686, 0x158c4e, 0x184a25, 0x1aea81,
	  0x1d977c, 0x202eb8, 0x22c8f2, 0x255339, 0x27d98a, 0x2a543c, 0x2cc39c, 0x2f2a0c, 0x3184d7, 0x33d1ae,
	  0x36134e, 0x384a80, 0x3a6dd5, 0x3c88a9, 0x3e9400, 0x408d05, 0x427b33, 0x445552, 0x4621ce, 0x47da5a,
	  0x4985e3, 0x4b1b9f, 0x4ca1f1, 0x4e1376, 0x4f74be, 0x50c0cb, 0x51fa14, 0x532005, 0x5431fb, 0x552ee3,
	  0x56161c, 0x56ec87, 0x57abdd, 0x58551c, 0x58e92a, 0x596905, 0x59d307, 0x5a260e, 0x5a67a2, 0x5a8bfc,
	  0x5aa043, 0x5a9e5f, 0x5a84ba, 0x5a567c, 0x5a1156, 0x59b777, 0x594667, 0x58c44d, 0x5824f4, 0x577a2a,
	  0x56b28c, 0x55d839, 0x54ebb4, 0x53e680, 0x52d16a, 0x51a46e, 0x50664c, 0x4f1441, 0x4dae05, 0x4c378d,
	  0x4aac3a, 0x490fc8, 0x476171, 0x45a3bd, 0x43d30e, 0x41f1f2, 0x40034c, 0x3e03d9, 0x3bf200, 0x39d94e};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0x000000, 0xfffffd, 0xfffe2f, 0xfffefe, 0xfffc60, 0xfffede, 0xfffa39, 0xffff4b,
	  0xfff770, 0x000082, 0xfff3b9, 0x0002ed, 0xffee78, 0x00077f, 0xffe61f, 0x00110e, 0xffd4ee, 0x002d73,
	  0xff8e91, 0x02e71f, 0x0b314f, 0x0ee0ca, 0x12fade, 0x16b00c, 0x1aae29, 0x1e64fa, 0x2246e6, 0x25fa8c,
	  0x29c1af, 0x2d6797, 0x3115a0, 0x34aba6, 0x3839a2, 0x3bb85e, 0x3f2a5b, 0x428d23, 0x45df2f, 0x491f47,
	  0x4c4df4, 0x4f6ad3, 0x5273fe, 0x5569c4, 0x584c3d, 0x5b168c, 0x5dcbc8, 0x606c79, 0x62f547, 0x656328,
	  0x67bcea, 0x69fa88, 0x6c1fad, 0x6e2c83, 0x701ae8, 0x71f2de, 0x73ab73, 0x754b2a, 0x76cb57, 0x7832f5,
	  0x79775a, 0x7aaa37, 0x7bac00, 0x7caa2f, 0x7d6bd8, 0x7e2ee0, 0x7eac1b, 0x7f3945, 0x7f8357, 0x7f634e,
	  0x7fffdd, 0x7fd2f7, 0x7ff482, 0x7fc94e, 0x7ed0b8, 0x7ea3a5, 0x7df91a, 0x7d3bcf, 0x7c63b2, 0x7b6854,
	  0x7a56a3, 0x79224f, 0x77d0d5, 0x766454, 0x74d817, 0x733448, 0x7170c0, 0x6f953d, 0x6d9b1f, 0x6b8a84,
	  0x695c79, 0x671837, 0x64b808, 0x6243dc, 0x5fb1f8, 0x5d0c83, 0x5a5378, 0x577df5, 0x549ada, 0x519c43};


	logInit(0, 115200);

	//Enable Audio Clock
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	__HAL_CRM_ADC_CLK_ENABLE();
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;

	//Rx CH Select ADC01
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0;

	//RX Channel L Mode 24bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x1;
	//RX Channel R Mode 24bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x1;
	//Set RX CH DMA Threshold
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;

	//Enable DMA Interrupt
	//GPDMA_Initialize();
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	CLOGD("AUDIO CODEC ADC 16k 50x test\n");

	//Configure DMA.CH0  APC_rx_l to mem
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = CODEC_DST_ADDR_L;               //Destination addr
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 100;

	//Configure DMA.CH1 APC rx r to mem
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = CODEC_DST_ADDR_R;               //Destination addr
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 100;

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;


	ADC_ENABLE_SETTING();
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 4;	//50x sample ratio
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 3; 		//16KHz Sample Rate
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN = 1; //Enable internal clock
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;	//Release Digital ADC reset

	APC_Enable(1);
	APC_Rx_Ch_L_Enable(0,1);
	APC_Rx_Ch_R_Enable(0,1);

	CLOGD("Warning DMA interrupt b ... \n");
	while(!(dma_ch1_done));

	for(int i=0; i<100; i++){
		ref_data_tmp = ref_data_l[i];
		act_data_tmp = read_mem(CODEC_DST_ADDR_L + 4*i);
		if(ref_data_tmp != act_data_tmp){
			CLOGD("Left ERROR:ref_data = %x vs act_data = %x\n",ref_data_tmp,act_data_tmp);
			//c_fail();
		}
	}
	for(int i=0; i<100; i++){
		ref_data_tmp = ref_data_r[i];
		act_data_tmp = read_mem(CODEC_DST_ADDR_R + 4*i);
		if(ref_data_tmp != act_data_tmp){
			CLOGD("Right ERROR:ref_data = %x vs act_data = %x\n",ref_data_tmp,act_data_tmp);
			//c_fail();
		}
	}

	while(1);
}

//Interrupt Handler
void eclic_dma_gp_int_handler (void) {
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
	return ;
}
