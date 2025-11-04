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
    dma_ch2_done = 0;
    uint32_t ref_data_tmp,act_data_tmp;

    uint32_t ref_data_l[100] = {0x000000, 0x000000, 0xffffd7, 0xffeeaa, 0xffdef6, 0xffd62d, 0xffbc00, 0xffc9cd, 0xff8a64, 0xffdcf8,
                                  0xff117a, 0x0581bd, 0x0e6e35, 0x14a527, 0x1bcc73, 0x222b87, 0x28b880, 0x2ecd42, 0x34bd57, 0x3a45a8,
                                  0x3f71e8, 0x4441a9, 0x48aa89, 0x4ca654, 0x502e30, 0x533dd9, 0x55cf82, 0x57e150, 0x596d96, 0x5a7411,
                                  0x5af199, 0x5ae713, 0x5a5310, 0x593720, 0x57959e, 0x556fa5, 0x52c94d, 0x4fa608, 0x4c0b43, 0x47fd91,
                                  0x43842d, 0x3ea476, 0x3966a5, 0x33d32a, 0x2df084, 0x27c951, 0x216629, 0x1ad0eb, 0x14129e, 0x0d3736,
                                  0x064751, 0xff4dae, 0xf855cf, 0xf16820, 0xea9292, 0xe3db6f, 0xdd4ede, 0xd6f784, 0xd0dcfc, 0xcb09de,
                                  0xc58691, 0xc05aba, 0xbb8f12, 0xb72aa3, 0xb3332c, 0xafb04e, 0xaca53c, 0xaa18ce, 0xa80c82, 0xa685b6,
                                  0xa5846b, 0xa50c1d, 0xa51c1a, 0xa5b5c7, 0xa6d6da, 0xa87db9, 0xaaa91f, 0xad5487, 0xb07c72, 0xb41be0,
                                  0xb82dfa, 0xbcac38, 0xc18f70, 0xc6d0dc, 0xcc67d4, 0xd24ced, 0xd87730, 0xdedc6c, 0xe572e4, 0xec3307,
                                  0xf30f49, 0xfa0007, 0x00f994, 0x07f195, 0x0eddc5, 0x15b351, 0x1c686f, 0x22f2b2, 0x294869, 0x2f600f};

    uint32_t ref_data_r[100] = {0x000000, 0x000000, 0xffffbb, 0xffe74b, 0xffd1b1, 0xffc479, 0xffa0db, 0xffb251, 0xff5be6, 0xffcb8e,
                                    0xfeb650, 0x07d7f6, 0x145963, 0x1d22e7, 0x2738a1, 0x30376f, 0x39746a, 0x420a07, 0x4a6b01, 0x5238e5,
                                    0x5985c4, 0x604f80, 0x6688ec, 0x6c25d9, 0x712403, 0x7572f3, 0x79169c, 0x7bfcb1, 0x7e309a, 0x7f952c,
                                    0x7fffff, 0x7fffff, 0x7f70d4, 0x7dde23, 0x7b9788, 0x788af7, 0x74d112, 0x70614c, 0x6b4d2b, 0x65942f,
                                    0x5f4396, 0x5863ee, 0x50fe86, 0x491fa1, 0x40d27a, 0x382392, 0x2f2064, 0x25d649, 0x1c532e, 0x12a5b8,
                                    0x08dbb8, 0xff0493, 0xf52ef0, 0xeb69d4, 0xe1c30c, 0xd84a3e, 0xcf0d21, 0xc619b3, 0xbd7d7a, 0xb544ff,
                                    0xad7d77, 0xa63231, 0x9f6e01, 0x993a65, 0x93a2ed, 0x8eae1e, 0x8a61a9, 0x86cc0d, 0x83e27e, 0x81c9aa,
                                    0x8047ff, 0x800001, 0x800001, 0x808015, 0x824777, 0x847a61, 0x879da2, 0x8b5561, 0x8fd158, 0x94e951,
                                    0x9aaa1e, 0xa0ffd5, 0xa7e592, 0xaf5007, 0xb733b2, 0xbf852b, 0xc83725, 0xd13ce7, 0xda8a21, 0xe40ec5,
                                    0xedbe5b, 0xf788ea, 0x015fd3, 0x0b3598, 0x14f9d5, 0x1e9efb, 0x281560, 0x315020, 0x3a402b, 0x42d8ea};

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

	CLOGD("AUDIO CODEC ADC 16k 125x test\n");

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
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 2;		//125x sample ratio
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
void eclic_dma_gp_int_handler(void) {
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
