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
	uint32_t rdata;
	rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
	CLOGD("AP DMA INT rdata = 0x%0x.\n",rdata);
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

	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0xffffe3, 0xffe87d, 0xffc7e1, 0xffb9b0, 0xff88af, 0xffa150, 0xff3c75, 0xffbf7b,
	0xfe8d7c, 0x074b87, 0x171bc0, 0x23b32a, 0x30a313, 0x3bc0a6, 0x45de07, 0x4e1c6d, 0x54a470, 0x5917e1,
	0x5b634b, 0x5b8b01, 0x598c71, 0x5573ba, 0x4f58f1, 0x476137, 0x3dbc56, 0x32a45e, 0x265bcd, 0x192c91,
	0x0b6657, 0xfd5b43, 0xef602b, 0xe1c90e, 0xd4e765, 0xc90936, 0xbe751c, 0xb56b40, 0xae21c7, 0xa8c4a1,
	0xa573cf, 0xa4430f, 0xa53a3b, 0xa852e7, 0xad7ad6, 0xb492bf, 0xbd70aa, 0xc7de23, 0xd39d54, 0xe0675a,
	0xedef25, 0xfbe3c9, 0x09f121, 0x17c2a0, 0x250550, 0x31699b, 0x3ca465, 0x4672de, 0x4e99ea, 0x54e82d,
	0x5937f0, 0x5b6f5d, 0x5b8119, 0x596ccd, 0x553ead, 0x4f103c, 0x470656, 0x3d5149, 0x322bf3, 0x25d8ad,
	0x18a1e2, 0x0ad72c, 0xfccb36, 0xeed2c9, 0xe140f8, 0xd46872, 0xc89613, 0xbe10bc, 0xb517ac, 0xade11f,
	0xa8985a, 0xa55cda, 0xa442a5, 0xa54f85, 0xa87de0, 0xadba49, 0xb4e520, 0xbdd3fc, 0xc8504e, 0xd41bc5,
	0xe0eecc, 0xee7c86, 0xfc73e1, 0x0a805f, 0x184de7, 0x2588c8, 0x31e264, 0x3d1020, 0x46ceab, 0x4ee3bb};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0xffffd2, 0xffdeb2, 0xffb0fa, 0xff9c85, 0xff5812, 0xff79bd, 0xfeed16, 0xffa356,
	0xfdf8d2, 0x0a5487, 0x2099b5, 0x325fd0, 0x44a0c0, 0x544fc0, 0x6295c9, 0x6e367d, 0x776f0b, 0x7db572,
	0x7fffff, 0x7fffff, 0x7e59f4, 0x7891da, 0x6ff659, 0x64b6ff, 0x571c9d, 0x47747c, 0x362020, 0x23855d,
	0x1015ce, 0xfc452a, 0xe88b10, 0xd55da2, 0xc33171, 0xb270da, 0xa38614, 0x96c203, 0x8c7e68, 0x84e5b0,
	0x8046b2, 0x800001, 0x800001, 0x84507c, 0x8b8d2c, 0x959490, 0xa2141f, 0xb0cccc, 0xc15f35, 0xd36a87,
	0xe68292, 0xfa3355, 0x0e0736, 0x2186f3, 0x343c68, 0x45b902, 0x5590b1, 0x63683b, 0x6ee742, 0x77cedf,
	0x7de1c0, 0x7fffff, 0x7fffff, 0x7e2c43, 0x7847e4, 0x6f8fae, 0x6436f3, 0x5685b8, 0x46cab1, 0x356739,
	0x22c178, 0x0f4bef, 0xfb79e3, 0xe7c31c, 0xd49de2, 0xc27e3f, 0xb1ceb9, 0xa2f861, 0x964c1f, 0x8c23ce,
	0x84a6c4, 0x802818, 0x800001, 0x800001, 0x848d66, 0x8be64f, 0x96091c, 0xa2a03b, 0xb16df6, 0xc21125,
	0xd429f8, 0xe74a0b, 0xfafe7d, 0x0ed137, 0x224b2f, 0x34f619, 0x4663f1, 0x5628a1, 0x63ea0b, 0x6f4f57};


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
	//Set RX CH0 DMA Threshold
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;

	//Enable DMAC Interrupt
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	CLOGD("AUDIO CODEC ADC 48k 250x test\n");

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
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 8; 		//48KHz Sample Rate
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
