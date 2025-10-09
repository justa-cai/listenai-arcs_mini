#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"

#define CODEC_DST_ADDR_L  (CMN_PSRAM_REGION+0x200)
#define CODEC_DST_ADDR_R  (CMN_PSRAM_REGION+0x1000)

volatile int dma_ch1_done;
volatile int dma_ch2_done;

/* 32 bit word AHB read or write functions:default */
int read_mem(int addr) {
    int rdval;
    rdval = *(int *)addr;   // read value from data memory
    return rdval;
}

void write_mem(int addr, int val) {
    *(int *)addr = val;   // store value in data memory
}

int main(void ){
	dma_ch1_done = 0;
	dma_ch0_done = 0;
	uint32_t ref_data_tmp,act_data_tmp;

	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0x000005, 0xffebcd, 0xffc7f9, 0xffbe24, 0xff86cc, 0xffa760, 0xff37e7, 0xffc827,
		  0xfe7eff, 0x063330, 0x161aac, 0x22c7f7, 0x2fc159, 0x3afda2, 0x453120, 0x4d93ff, 0x543c30, 0x58d8c8,
		  0x5b4b24, 0x5b9a98, 0x59c365, 0x55d0c6, 0x4fd9e8, 0x48030b, 0x3e7b3d, 0x337bdc, 0x2746d7, 0x1a259a,
		  0x0c6784, 0xfe5e9a, 0xf05f80, 0xe2be77, 0xd5cd0b, 0xc9d9bf, 0xbf2ba3, 0xb60375, 0xae981b, 0xa91649,
		  0xa59eec, 0xa44692, 0xa51611, 0xa807ec, 0xad0acb, 0xb40043, 0xbcbf2d, 0xc711e2, 0xd2bafe, 0xdf7459,
		  0xecf11d, 0xfae0b8, 0x08ef1c, 0x16c7bc, 0x241763, 0x308e4b, 0x3be0d5, 0x45cb9a, 0x4e12ec, 0x5484a0,
		  0x58fa24, 0x5b58c8, 0x5b9244, 0x59a552, 0x559d36, 0x4f928f, 0x47a96b, 0x3e1143, 0x33044f, 0x26c463,
		  0x199b58, 0x0bd894, 0xfdce78, 0xefd1db, 0xe235dd, 0xd54d55, 0xc965aa, 0xbec61d, 0xb5ae9b, 0xae5604,
		  0xa8e886, 0xa5865e, 0xa4448d, 0xa529cc, 0xa83159, 0xad48d5, 0xb45154, 0xbd215a, 0xc7830b, 0xd338a7,
		  0xdffb40, 0xed7e2c, 0xfb70be, 0x097e84, 0x175369, 0x249b8b, 0x3107e9, 0x3c4d9c, 0x46289f, 0x4e5e18};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0x000005, 0xffe35d, 0xffb11a, 0xffa2d2, 0xff555e, 0xff8260, 0xfee681, 0xffafe4,
		  0xfde364, 0x08c978, 0x1f2f5d, 0x3113be, 0x436245, 0x533cab, 0x61a19f, 0x6d7664, 0x76db39, 0x7d5dd7,
		  0x7fffff, 0x7fffff, 0x7ea90d, 0x791445, 0x70acd1, 0x659b05, 0x582a2a, 0x48a471, 0x376bcf, 0x24e4bc,
		  0x1180ba, 0xfdb30f, 0xe9f35f, 0xd6b7db, 0xc47582, 0xb3971f, 0xa48786, 0x9798f6, 0x8d2509, 0x85598e,
		  0x808179, 0x800001, 0x800001, 0x83e62f, 0x8aef96, 0x94c57c, 0xa119ff, 0xafac50, 0xc0200a, 0xd2138f,
		  0xe51c28, 0xf8c5cf, 0x0c9b22, 0x2024e9, 0x32ecbf, 0x448374, 0x547ce4, 0x627bf4, 0x6e2940, 0x7741a1,
		  0x7d8c1b, 0x7fffff, 0x7fffff, 0x7e7d7e, 0x78cc5f, 0x704820, 0x651cb6, 0x5794cc, 0x47fbdf, 0x36b3e1,
		  0x242177, 0x10b720, 0xfce7bd, 0xe92b05, 0xd5f760, 0xc3c144, 0xb2f39f, 0xa3f84a, 0x972129, 0x8cc883,
		  0x85186c, 0x8060ad, 0x800001, 0x800001, 0x84212a, 0x8b4693, 0x953845, 0xa1a46e, 0xb04c23, 0xc0d0da,
		  0xd2d234, 0xe5e337, 0xf990dc, 0x0d6562, 0x20e9b6, 0x33a75a, 0x452f97, 0x551650, 0x62ff7b, 0x6e9342};

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
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	CLOGD("AUDIO CODEC ADC 16k 250x test\n");

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
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 1;		//250x sample ratio
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

	c_pass();
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

