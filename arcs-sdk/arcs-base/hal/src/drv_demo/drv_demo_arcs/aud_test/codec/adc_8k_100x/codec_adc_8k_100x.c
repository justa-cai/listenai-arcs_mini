#include "arcs_ap.h"
#include "ClockManager.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"

#define CODEC_DST_ADDR_L  (CMN_PSRAM_REGION+0x200)
#define CODEC_DST_ADDR_R  (CMN_PSRAM_REGION+0x1000)
volatile int dma_ch0_done;
volatile int dma_ch1_done;

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
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x1; //Clear Channel2 INT
		dma_ch0_done = 1;
	}
	if((rdata & 0x2) == 0x2) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x2; //Clear Channel2 INT
		dma_ch1_done = 1;
	}
	return;
}

int main(void ){
	dma_ch0_done = 0;
	dma_ch1_done = 0;
	uint32_t ref_data_tmp,act_data_tmp;

	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0x000009, 0xffec83, 0xffc80a, 0xffbf0b, 0xff8687, 0xffa877, 0xff3751, 0xffc94f,
	0xfe7dc2, 0x05f68f, 0x15e083, 0x22940b, 0x2f8eb1, 0x3ad237, 0x450a4d, 0x4d7568, 0x5424a2, 0x58ca5f,
	0x5b4563, 0x5b9dad, 0x59cf39, 0x55e516, 0x4ff63a, 0x4826b2, 0x3ea567, 0x33ab88, 0x277ae7, 0x1a5cd4,
	0x0ca09e, 0xfe983c, 0xf0984e, 0xe2f51f, 0xd60043, 0xca0850, 0xbf547b, 0xb62598, 0xaeb2bf, 0xa928cb,
	0xa5a8e3, 0xa447bd, 0xa50e6a, 0xa7f7a1, 0xacf23e, 0xb3e008, 0xbc9805, 0xc6e4bb, 0xd288e3, 0xdf3e7c,
	0xecb8be, 0xfaa72c, 0x08b5bb, 0x168fe1, 0x23e25d, 0x305d59, 0x3bb51f, 0x45a622, 0x4df495, 0x546e25,
	0x58ec09, 0x5b5362, 0x5b95b2, 0x59b181, 0x55b1dc, 0x4faf2f, 0x47cd5c, 0x3e3baa, 0x33342d, 0x26f89b,
	0x19d2ad, 0x0c11ba, 0xfe0818, 0xf00a9c, 0xe26c69, 0xd58060, 0xc99407, 0xbeeeb4, 0xb5d077, 0xae7057,
	0xa8fab5, 0xa58ffa, 0xa4455d, 0xa521cc, 0xa820b7, 0xad2ff8, 0xb430cf, 0xbcf9f1, 0xc755ac, 0xd30661,
	0xdfc544, 0xed45ba, 0xfb372d, 0x09452d, 0x171ba4, 0x2466ab, 0x30d725, 0x3c2220, 0x46036b, 0x4e4011};

	uint32_t ref_data_r[100] = {0x000000, 0x000000, 0x00000b, 0xffe45f, 0xffb132, 0xffa419, 0xff54fa, 0xff83f0, 0xfee5a5, 0xffb199,
	0xfde16f, 0x087404, 0x1edd65, 0x30ca6e, 0x431acf, 0x52ff6e, 0x616ace, 0x6d4b50, 0x76b9d6, 0x7d49cd,
	0x7fffff, 0x7fffff, 0x7eba1c, 0x7930bd, 0x70d4e3, 0x65cd44, 0x5865af, 0x48e7b0, 0x37b549, 0x2532a8,
	0x11d14c, 0xfe0461, 0xea4387, 0xd704f9, 0xc4bdc6, 0xb3d8d8, 0xa4c11d, 0x97c92e, 0x8d4a89, 0x8573d9,
	0x808f12, 0x800001, 0x800001, 0x83cf0c, 0x8acd17, 0x9497ea, 0xa0e2d1, 0xaf6c8c, 0xbfd963, 0xd1c789,
	0xe4cc9f, 0xf8749d, 0x0c4a2b, 0x1fd618, 0x32a1f3, 0x443e60, 0x543f3e, 0x624706, 0x6dfe8e, 0x7721bd,
	0x7d7883, 0x7fffff, 0x7fffff, 0x7e8f07, 0x78e94b, 0x7070a2, 0x654f57, 0x57d0a9, 0x483f63, 0x36fd92,
	0x246f8a, 0x1107c5, 0xfd390d, 0xe97b17, 0xd64455, 0xc4094c, 0xb3350b, 0xa4318b, 0x9750f7, 0x8ced97,
	0x85323a, 0x806dc7, 0x800001, 0x800001, 0x840998, 0x8b239a, 0x950a4d, 0xa16ce2, 0xb00c12, 0xc089f2,
	0xd28601, 0xe59396, 0xf93fa1, 0x0d1478, 0x209b05, 0x335cbf, 0x44eac7, 0x54d8ff, 0x62caee, 0x6e68ff};


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

	CLOGD("AUDIO CODEC ADC 8k 250x test\n");

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

	ADC_IO_CFG();
	ADC_ENABLE_SETTING();
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 1;		//250x sample ratio
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = 0; 		//8KHz Sample Rate
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN = 1; //Enable internal clock
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;	//Release Digital ADC reset


	APC_Rx_Ch_L_Enable(0,1);
	APC_Rx_Ch_R_Enable(0,1);
	APC_Enable(1);

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
