#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"

#define CODEC_DST_ADDR_L  (CMN_PSRAM_REGION+0x200)
#define CODEC_DST_ADDR_R  (CMN_PSRAM_REGION+0x1000)
volatile int dma_ch1_done;
volatile int dma_ch0_done;

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
void eclic_dma_gp_int_handler (void) {
	int rdata;
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

	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0xfffff8, 0xffde93, 0xff9c21, 0xff8d57, 0xff3999, 0xff841e, 0xfeefd0, 0x00007f,
	0xfe2d9e, 0x0aee03, 0x28119c, 0x3eb3ea, 0x50cd86, 0x5aa20a, 0x5c63be, 0x55537e, 0x465f88, 0x30c7af,
	0x167675, 0xfa0c77, 0xde30a5, 0xc57d2d, 0xb2408b, 0xa646cf, 0xa2addc, 0xa7cbe8, 0xb526e2, 0xc97f52,
	0xe2ee89, 0xff14c2, 0x1b5118, 0x35004a, 0x49bc58, 0x57959d, 0x5d40cb, 0x5a36c5, 0x4ebfd6, 0x3bee44,
	0x2383d4, 0x07c877, 0xeb530c, 0xd0cbbb, 0xbaad25, 0xab07be, 0xa351e7, 0xa443ad, 0xadc675, 0xbef72d,
	0xd63ad1, 0xf164d0, 0x0dec45, 0x29267e, 0x40891a, 0x51e506, 0x5b9ac6, 0x5cc2d9, 0x5540da, 0x45c8f3,
	0x2fcca5, 0x15596d, 0xf8e7f5, 0xdd1ff3, 0xc499ce, 0xb19fc3, 0xa5f76a, 0xa2b793, 0xa82dd4, 0xb5d7a6,
	0xca6e7f, 0xe405c2, 0x003a20, 0x1c6928, 0x35f0a7, 0x4a6eb8, 0x57f951, 0x5d4cba, 0x59e96d, 0x4e20d1,
	0x3b0c4f, 0x227406, 0x06a410, 0xea3579, 0xcfcf99, 0xb9ea03, 0xaa8feb, 0xa3306d, 0xa47bc3, 0xae52e2,
	0xbfcaf0, 0xd7420f, 0xf286f5, 0x0f0dea, 0x2a2d18, 0x415c00, 0x527042, 0x5bd199, 0x5c9ff6, 0x54c7c2};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0xfffff4, 0xffd0c5, 0xff732a, 0xff5e14, 0xfee846, 0xff50ed, 0xfe8062, 0xfffff2,
	0xfd6f9b, 0x0f70cb, 0x3888ec, 0x58799a, 0x720428, 0x7fddb0, 0x7fffff, 0x7865d8, 0x634b72, 0x44d4c3,
	0x1fb17e, 0xf79ae4, 0xd04a77, 0xad71a6, 0x924a38, 0x816a45, 0x800001, 0x8388b6, 0x966520, 0xb317c4,
	0xd6fccb, 0xfeb374, 0x268c1e, 0x4ac81f, 0x680c20, 0x7b925f, 0x7fffff, 0x7f4e07, 0x6f1c64, 0x54910c,
	0x321c32, 0x0afbd5, 0xe2d316, 0xbd6570, 0x9e2e94, 0x881bda, 0x800001, 0x800001, 0x8bfbcd, 0xa43c48,
	0xc50fdc, 0xeb640c, 0x13a4fe, 0x3a10a9, 0x5b0f64, 0x738ec1, 0x7fffff, 0x7fffff, 0x784b8d, 0x62770c,
	0x437272, 0x1e1f5c, 0xf5fdf6, 0xcec9cd, 0xac30da, 0x916753, 0x80fab3, 0x800001, 0x8412e1, 0x975e59,
	0xb4690d, 0xd88700, 0x00513f, 0x2816e6, 0x4c1b4f, 0x6907e4, 0x7c1efc, 0x7fffff, 0x7ee0e1, 0x6e3bd4,
	0x535233, 0x309c9f, 0x095f7f, 0xe13ff3, 0xbc01e1, 0x9d1b1d, 0x8772f6, 0x800001, 0x800001, 0x8cc20f,
	0xa566cc, 0xc6830c, 0xecfd42, 0x153e08, 0x3b82dd, 0x5c38e1, 0x7452c5, 0x7fffff, 0x7fffff, 0x77a0a1};


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

	//Enable DMAC Interrupt
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	CLOGD("AUDIO CODEC ADC 8k 500x test\n");

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
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 0;
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 0; 		//8KHz Sample Rate
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
