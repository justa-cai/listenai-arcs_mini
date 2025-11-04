#include "arcs_ap.h"
#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "ClockManager.h"

#define DATA_LENGTH      4096  // Number of samples to capture
#define DEBUG_CLK_SEL    31    // Debug clock selection value
#define DEBUG_CLK_FSEL   18    // Debug clock pin function selection

volatile int dma_ch0_done;
volatile int dma_ch1_done;

uint32_t dst_l[DATA_LENGTH];
uint32_t dst_r[DATA_LENGTH];
static void* GPIOA_Handler = NULL;

/* 32 bit word AHB read or write functions */
int read_mem(int addr) {
    return *(int *)addr;
}

void write_mem(int addr, int val) {
    *(int *)addr = val;
}

typedef struct {
    uint8_t adcosr;     // Over-sampling ratio (1:250x, 2:125x)
    uint8_t adcsr;      // Sample rate (0:8KHz, 3:16KHz)
    const char* desc;   // Description of the test case
} test_config_t;

const test_config_t test_configs[] = {
	{0, 0, "DMIC 8KHz 500x"},  // 500x oversampling, 8KHz
    {1, 0, "DMIC 8KHz 250x"},  // 250x oversampling, 8KHz
	{3, 0, "DMIC 8KHz 100x"},  // 100x oversampling, 8KHz
	{1, 3, "DMIC 16KHz 250x"},  // 250x oversampling, 16KHz
    {2, 3, "DMIC 16KHz 125x"},  // 125x oversampling, 16KHz
	{4, 3, "DMIC 16KHz 50x"},  // 50x oversampling, 16KHz
	{4, 8, "DMIC 48KHz 50x"}  // 50x oversampling, 48KHz
};

void dump_data(const char* channel, uint32_t* data, uint32_t count) {
    CLOGD("%s Channel Data Dump (%d samples):\n", channel, count);
    for(uint32_t i = 0; i < count; i++) {
        if(i % 8 == 0) {
            CLOGD("\n[%04d]: ", i);
        }
        CLOGD("%08X ", data[i]);
    }
    CLOGD("\n");
}

void setup_gpio() {
    GPIOA_Handler = GPIOA();
    GPIO_Initialize(GPIOA_Handler, NULL, NULL);

    // Configure DMIC pins
    IP_CMN_IOMUX->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_FSEL = 24; // CLK
    IP_CMN_IOMUX->REG_PAD_GPIOA_04.bit.PAD_GPIOA_04_FSEL = 24; // D0
    IP_CMN_IOMUX->REG_PAD_GPIOA_05.bit.PAD_GPIOA_05_FSEL = 24; // D1

    // Setup debug clock output
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = DEBUG_CLK_SEL;
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1; // ADC MCLK test
    IP_CMN_IOMUX->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_FSEL = DEBUG_CLK_FSEL;
}

void setup_dma() {
    // Configure DMA Channel 0 (Left channel)
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all);
    IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = (uint32_t)dst_l;
    IP_GPDMA->REG_DMA_CH0_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) |
                                     (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = DATA_LENGTH;

    // Configure DMA Channel 1 (Right channel)
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all);
    IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = (uint32_t)dst_r;
    IP_GPDMA->REG_DMA_CH1_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) |
                                     (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = DATA_LENGTH;

    IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;
}

void setup_audio(const test_config_t* config) {
    // Enable clocks
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
    __HAL_CRM_DAC_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;

    // Configure APC RX channel
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0;  // Source select
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x3; // 24-bit mode
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x3; // 24-bit mode
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;

    // Configure DMIC
    DMIC_ENABLE_SETTING();
    IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.DMIC_MODE = 1;
    IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.DMIC_SRC = 0;

    // Configure ADC parameters
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = config->adcosr;
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = config->adcsr;
    IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.ADCVOL_R = 70;
    IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.ADCVOL_L = 70;
    IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.HPF2EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R6_ADC_CTRL1.bit.HPF1EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;

    // Enable APC and channels
    APC_Enable(1);
    APC_Rx_Ch_L_Enable(0, 1);
    APC_Rx_Ch_R_Enable(0, 1);
}

void eclic_dma_gp_int_handler(void) {
    uint32_t rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
    CLOGD("GPDMA INT status: 0x%08X\n", rdata);

    if(rdata & 0x1) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x1;
        dma_ch0_done = 1;
    }
    if(rdata & 0x2) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x2;
        dma_ch1_done = 1;
    }
}

void run_test_case(const test_config_t* config) {
    dma_ch0_done = 0;
    dma_ch1_done = 0;

    logInit(0, 115200);
    CLOGD("\nStarting test case: %s\n", config->desc);

    // Setup hardware
    setup_gpio();
    setup_dma();
    setup_audio(config);

    // Enable DMA interrupts
    enable_GINT();
    register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
    ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

    // Start DMA transfers
    IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
    IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

    CLOGD("Recording DMIC data...\n");
    while(!dma_ch1_done);  // Wait for DMA to complete

    // Dump captured data
    dump_data("Left", dst_l, DATA_LENGTH);
    dump_data("Right", dst_r, DATA_LENGTH);

    CLOGD("Test case %s completed\n", config->desc);
}

int main(void) {
    // Run all test cases
    for(size_t i = 0; i < sizeof(test_configs)/sizeof(test_configs[0]); i++) {
        run_test_case(&test_configs[i]);

        // Add delay between test cases
        for(int j = 0; j < 1000000; j++) { asm("nop"); }
    }

    CLOGD("All DMIC test cases completed\n");
    while(1);
}
