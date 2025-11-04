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
	uint32_t ref_data_l[100] = {0x000000, 0x000000, 0x000000, 0xffb513, 0x0034f1, 0x00557f, 0x0083e6, 0x005e6d, 0xff8278, 0x007e2d,
          0xfd5bdb, 0x1bfb19, 0x022ac0, 0xce7a79, 0xc0b60c, 0xd2b534, 0x02b5ee, 0x2ee5b9, 0x40f971, 0x2c06bc,
          0xfdd32a, 0xd1143e, 0xbfbc7c, 0xd413c5, 0x0229e3, 0x2efeed, 0x4041ba, 0x2bddde, 0xfdd323, 0xd1143e,
		  0xbfbc7c, 0xd413c5, 0x0229e3, 0x2efeed, 0x404217, 0x2be117, 0xfdceff, 0xd11af3, 0xbfb212, 0xd42375,
		  0x02123e, 0x2f24f1, 0x3ff0cb, 0x2aef30, 0xfdfc75, 0xd10312, 0xbfc464, 0xd41070, 0x022ada, 0x2eff19,
		  0x404140, 0x2be037, 0xfdd326, 0xd1143e, 0xbfbc7c, 0xd413c5, 0x0229e3, 0x2efeed, 0x4041ba, 0x2bddde,
		  0xfdd323, 0xd1143e, 0xbfbc7c, 0xd413c5, 0x0229e3, 0x2efeed, 0x404217, 0x2be117, 0xfdceff, 0xd11af3,
		  0xbfb212, 0xd42375, 0x02123e, 0x2f24f1, 0x3ff0cb, 0x2aef30, 0xfdfc75, 0xd10312, 0xbfc464, 0xd41070,
		  0x022ada, 0x2eff19, 0x404140, 0x2be037, 0xfdd326, 0xd1143e, 0xbfbc7c, 0xd413c5, 0x0229e3, 0x2efeed,
		  0x4041ba, 0x2bddde, 0xfdd323, 0xd1143e, 0xbfbc7c, 0xd413c5, 0x0229e3, 0x2efeed, 0x404217, 0x2be117};

    uint32_t ref_data_r[100] = {0x000000, 0x000000, 0xffff5c, 0xff6511, 0x007117, 0x00a047, 0x0118fc, 0x00a210, 0xff2c88, 0x00bb6c,
		  0xfb4204, 0x396f13, 0x03e74c, 0x9cd2a6, 0x815704, 0xa5bc19, 0x058c6c, 0x5e0a4a, 0x7fffff, 0x57d264,
		  0xfb8a4e, 0xa1e248, 0x800001, 0xa8703c, 0x04774b, 0x5e3b9e, 0x7fffff, 0x5784a8, 0xfb8a46, 0xa1e248,
		  0x800001, 0xa8703c, 0x04774b, 0x5e3b9e, 0x7fffff, 0x5787e0, 0xfb8621, 0xa1e900, 0x800001, 0xa87ffa,
		  0x045f8b, 0x5e61d9, 0x7fffff, 0x56965a, 0xfbb3db, 0xa1d0f1, 0x800001, 0xa86cd3, 0x04784d, 0x5e3bc2,
		  0x7fffff, 0x578700, 0xfb8a49, 0xa1e248, 0x800001, 0xa8703c, 0x04774b, 0x5e3b9e, 0x7fffff, 0x5784a8,
		  0xfb8a46, 0xa1e248, 0x800001, 0xa8703c, 0x04774b, 0x5e3b9e, 0x7fffff, 0x5787e0, 0xfb8621, 0xa1e900,
		  0x800001, 0xa87ffa, 0x045f8b, 0x5e61d9, 0x7fffff, 0x56965a, 0xfbb3db, 0xa1d0f1, 0x800001, 0xa86cd3,
		  0x04784d, 0x5e3bc2, 0x7fffff, 0x578700, 0xfb8a49, 0xa1e248, 0x800001, 0xa8703c, 0x04774b, 0x5e3b9e,
		  0x7fffff, 0x5784a8, 0xfb8a46, 0xa1e248, 0x800001, 0xa8703c, 0x04774b, 0x5e3b9e, 0x7fffff, 0x5787e0};
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
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x3;

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
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = 0;		//500x sample ratio
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
