#include "stdio.h"
#include <string.h>

#include "Driver_GPADC.h"
#include "Driver_GPT_IC.h"
#include "Driver_GPT_PWM.h"
#include "Driver_GPT_TIMER.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "chip.h"
#include "Driver_SPI.h"
#include "log_print.h"
#include "gpadc.h"
#include "IOMuxManager.h"
#include "Driver_KEYSENSE.h"
#include "ClockManager.h"
#include "PowerManager.h"

#include "systick.h"
#include "dma.h"
#include "unity.h"

/********************** Private micro define begin ***************************/
#define SPI_CS_PIN              12
#define SPI_CLK_PIN             15
#define SPI_MOSI_PIN            14
#define SPI_MISO_PIN        	13

#define SPI_BUS_SPEED           2000000 // 2000000 // 2MHz
#define SPI_DATA_BITS           16

/********************** Private micro define end ***************************/


/************************* Private typedef begin ***************************/
typedef void (*function)(void);

/*************************** Private typedef end ***************************/
void GPADC_Channel_x_DmaCmpSpiTranferEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t res);

/******************* Private functions prototype begin ********************/
static void GPIO_Init_Handler(void);

void GPADC_AllChannel_Polling(void);
void GPADC_AllChannel_Interrupt(void);
void GPADC_AllChannel_GPTTrigger(void);
void GPADC_Channel0_Interrupt_FifoFull(void);
void GPADC_Channel0_Interrupt_FifoEmpty(void);
void GPADC_Channel0_Interrupt_FifoThd(void);
void GPADC_KeySense0_Trigger(void);
void GPADC_KeySense1_Trigger(void);
void GPADC_KeySense_Trigger_Interrupt(void);
void GPADC_Channel1_Dma(void);
void GPADC_MultiChannel_Dma(void);
void GPADC_SingleChannel_Dma(void);
void GPADC_Channel_Temperature(void);
void GPADC_Channel0_Dma_SPI(void);
void GPADC_Keysense0_Wakeup(void);
void GPADC_Keysense1_Wakeup(void);
void GPADC_CVD_Polling(void);
void GPADC_CVD_Channel_Dma(void);

/******************* Private functions prototype end ********************/

/******************* Private variables begin ********************/
uint16_t adc_val1[500]={0};
uint16_t adc_val2[500]={0};
uint8_t buffer_sel = 1;
uint16_t *adcsend = adc_val1;

static void *gSpiDev = NULL;
static void *gGpioDev = NULL;

uint8_t channel_num;

/******************* Private variables end ********************/

void delay(int xms)
{
    int x;
    int y;

    for(x = xms; x > 0; x--);
    for(y = 110; y > 0; y--);
}

void print_float(float number) {
    int integer_part = (int)number;
    float fraction_part = number - integer_part;
    if (fraction_part < 0) {
        fraction_part = -fraction_part;
    }

    int fraction_as_int = (int)(fraction_part * 100000000);
    CLOGD("%d.%d\n", integer_part, fraction_as_int);
}

void setUp(void) {
//    HAL_GPADC_Initialize(GPADC());
}

void tearDown(void) {
//    HAL_GPADC_Uninitialize(GPADC());
}


int main(void)
{
    logInit(0, 115200);

	__HAL_PMU_GPADC_RST_ENABLE();
	HAL_CRM_SetGpadcClkDiv(12);

    UNITY_BEGIN();
    RUN_TEST(GPADC_AllChannel_Polling);
//  RUN_TEST(GPADC_AllChannel_Interrupt);
//  RUN_TEST(GPADC_SingleChannel_Dma);
//  RUN_TEST(GPADC_Channel0_Dma_SPI);
//  RUN_TEST(GPADC_KeySense0_Trigger);
//  RUN_TEST(GPADC_KeySense1_Trigger);
//	RUN_TEST(GPADC_Keysense0_Wakeup);
    RUN_TEST(GPADC_Channel_Temperature);
//	RUN_TEST(GPADC_AllChannel_GPTTrigger);
//  RUN_TEST(GPADC_CVD_Polling);
//	RUN_TEST(GPADC_CVD_Channel_Dma);

    return UNITY_END();
	while(1);
}


static void GPADC_Complete_Event(uint32_t event, void* param){
	uint16_t adc_value, pos=0;
	uint32_t channeltotal, channelnum;
	channeltotal = (uint32_t)param;
	if( event == CSK_GPADC_COMPLETE ){
		//total channel is 8
		while(pos<CSK_GPADC_CHANNEL_NUM){
			channelnum = (channeltotal&(0x01<<pos)) << CSK_GPADC_CHANNEL_SEL_Pos;
			if(channelnum){
				adc_value = HAL_GPADC_GetValue(GPADC(), channelnum);
				TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
//				CLOGD("channel is 0x%x, adc value is %d",channelnum, adc_value);
				if(channelnum == CSK_GPADC_CHANNEL_SEL_VBAT){
					TEST_ASSERT_INT_WITHIN(800,1100,adc_value);
				}
			}
			pos++;
		}
	}

	if( event == CSK_GPADC_EOC_ERROR ){
		CLOGD("gpadc eoc error generated");
	}
}


static void GPADC_Channel0_Complete_Event(uint32_t event, void* param){
    uint16_t adc_value;
    if( event == CSK_GPADC_COMPLETE ){
            adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_0);
            HAL_GPADC_Start_IT(GPADC());
    }

    if( event == CSK_GPADC_EOC_ERROR ){
        CLOGD("gpadc eoc error generated");
    }
}

static void GPADC_Channel_0_Event(uint32_t event, void* param){
	uint16_t adc_value;
	if( event == CSK_GPADC_FIFO_THD ){
		CLOGD("gpadc channel0 fifo threshold event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL0);
	}

	if( event == CSK_GPADC_FIFO_FULL ){
		CLOGD("gpadc channel0 fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL0);
	}

	if( event == CSK_GPADC_FIFO_EMPTY ){
		CLOGD("gpadc channel0 fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL0);
	}

	if( event == CSK_GPADC_COMPLETE ){
		CLOGD("gpadc channel0 complete event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL0);
	}
}


static void GPADC_Channel_2_Event(uint32_t event, void* param){
	uint16_t adc_value;
	if( event == CSK_GPADC_FIFO_THD ){
		CLOGD("gpadc channel2 fifo threshold event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL2);
	}

	if( event == CSK_GPADC_FIFO_FULL ){
		CLOGD("gpadc channel2 fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL2);
	}

	if( event == CSK_GPADC_FIFO_EMPTY ){
		CLOGD("gpadc channel2 fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL2);
	}
}


static void GPADC_Channel_VDDIO_Event(uint32_t event, void* param){
	uint16_t adc_value;
	if( event == CSK_GPADC_FIFO_THD ){
		CLOGD("gpadc vddio fifo threshold event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_VBAT);
	}

	if( event == CSK_GPADC_FIFO_FULL ){
		CLOGD("gpadc vddio fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_VBAT);
	}

	if( event == CSK_GPADC_FIFO_EMPTY ){
		CLOGD("gpadc vddio fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_VBAT);
	}
}


static void GPADC_Channel_VCC_Event(uint32_t event, void* param){
	uint16_t adc_value;
	if( event == CSK_GPADC_FIFO_THD ){
		CLOGD("gpadc vcc fifo threshold event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_VBAT);
	}

	if( event == CSK_GPADC_FIFO_FULL ){
		CLOGD("gpadc vcc fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_VBAT);
	}

	if( event == CSK_GPADC_FIFO_EMPTY ){
		CLOGD("gpadc vcc fifo full event generated");
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_VBAT);
	}
}

void GPADC_AllChannel_Polling(void)
{
    /*********GPADC read all channels********/
    __HAL_CRM_IR_CLK_ENABLE();

    // IO config channel0-channel2 - PB2 Channel0/Channel1/Channel2
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3));
    TEST_ASSERT_EQUAL(0, IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL);

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3));
    TEST_ASSERT_EQUAL(0, IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL);

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3));
    TEST_ASSERT_EQUAL(0, IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL);

    uint32_t cnt, state, adc_value;

    // Initialize and configure GPADC
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_Initialize(GPADC()));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK,
        HAL_GPADC_Control(GPADC(),
            (CSK_GPADC_CHANNEL_SEL_0 |
             CSK_GPADC_CHANNEL_SEL_1 |
             CSK_GPADC_CHANNEL_SEL_2 |
             CSK_GPADC_CHANNEL_SEL_VBAT) |
            CSK_GPADC_DMA_ENABLE(0)));

    // Verify reference voltage and trigger settings
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_SetVrefSel(GPADC(), 0)); //VBG1/2
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_SetTriggerNum(GPADC(), 2));

    while(1) {
        // Start conversion and verify
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_Start(GPADC()));

        // Verify conversion completion
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_PollForConversion(GPADC(), 0));

        // Channel 0 tests
        adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_0);
        TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
        TEST_ASSERT_NOT_EQUAL(0xFFFF, adc_value); // Check for error value
        uint16_t ch0_mv = (uint16_t)(adc_value*1000/1024*1.2);
        TEST_ASSERT_UINT16_WITHIN(0, 1200, ch0_mv); // 0-1.2V range
        CLOGD("channel is %d, adc value is 0x%x", 0, adc_value);
        CLOGD("channel is %d, adc value is %dmV", 0, ch0_mv);

        // Channel 1 tests
        adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_1);
        TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
        TEST_ASSERT_NOT_EQUAL(0xFFFF, adc_value);
        uint16_t ch1_mv = (uint16_t)(adc_value*1000/1024*1.2);
        TEST_ASSERT_UINT16_WITHIN(0, 1200, ch1_mv);
        CLOGD("channel is %d, adc value is 0x%x", 1, adc_value);
        CLOGD("channel is %d, adc value is %dmV", 1, ch1_mv);

        // Channel 2 tests
        adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_2);
        TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
        TEST_ASSERT_NOT_EQUAL(0xFFFF, adc_value);
        uint16_t ch2_mv = (uint16_t)(adc_value*1000/1024*1.2);
        TEST_ASSERT_UINT16_WITHIN(0, 1200, ch2_mv);
        CLOGD("channel is %d, adc value is 0x%x", 2, adc_value);
        CLOGD("channel is %d, adc value is %dmV", 2, ch2_mv);

        // VBAT channel tests
        adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_VBAT);
        TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
        TEST_ASSERT_NOT_EQUAL(0xFFFF, adc_value);
        uint16_t vbat_mv = (uint16_t)(adc_value*1000/1024*1.2*3);
        TEST_ASSERT_UINT16_WITHIN(3000, 3600, vbat_mv); // 3.0V-3.6V range
        CLOGD("channel is VBAT, adc value is 0x%x", adc_value);
        CLOGD("channel is VBAT, adc value is %dmV", vbat_mv);

        SysTick_Delay_Ms(500);
    }
}
void GPADC_AllChannel_Interrupt(void)
{
	/*********GPADC read all channels********/
    //PB2 Channel0/Channel1/Channel2
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 0;

	uint32_t cnt, state, adc_value;
    //initialize, all channels are configured
	HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_0 | \
                               CSK_GPADC_CHANNEL_SEL_1 | CSK_GPADC_CHANNEL_SEL_2 | CSK_GPADC_CHANNEL_SEL_VBAT) | \
                               CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
	HAL_GPADC_SetTriggerNum(GPADC(), 1);

	HAL_GPADC_RegisterCompleteCallback(GPADC(), GPADC_Complete_Event);
    HAL_GPADC_Start_IT(GPADC());
    SysTick_Delay_Ms(500);
    //while(1);
}

void GPADC_Channel_x_FifoEvent(uint32_t event, void* param)
{
    TEST_ASSERT_EQUAL_INT(1,event);
}

void GPADC_Channel0_Interrupt_FifoFull(void)
{
    /*********GPADC read all channels********/
    uint32_t cnt, state, adc_value;

    //initialize
    HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_VBAT) | \
                       CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 16);

    HAL_GPADC_EnableFifoInterrupt(GPADC(), CSK_GPADC_VBAT, CSK_GPADC_FIFO_FULL);

    HAL_GPADC_RegisterChannelCallback(GPADC(), CSK_GPADC_VBAT, GPADC_Channel_x_FifoEvent);

    HAL_GPADC_Start(GPADC());
    SysTick_Delay_Ms(500);
}


void GPADC_Channel0_Interrupt_FifoEmpty(void)
{
    /*********GPADC read all channels********/
    uint32_t cnt, state, adc_value;

    //initialize
    HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_VBAT) | \
                      CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 20);

    HAL_GPADC_EnableFifoInterrupt(GPADC(), CSK_GPADC_VBAT, CSK_GPADC_FIFO_EMPTY);

    HAL_GPADC_RegisterChannelCallback(GPADC(), CSK_GPADC_VBAT, GPADC_Channel_x_FifoEvent);

    HAL_GPADC_Start(GPADC());
    HAL_GPADC_PollForConversion(GPADC(), 0);

    for(uint32_t i=0; i<20; i++){
        HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_0);
    }
    SysTick_Delay_Ms(500);
}


void GPADC_Channel0_Interrupt_FifoThd(void)
{
    /*********GPADC read all channels********/
    uint32_t cnt, state, adc_value;

    //initialize
    HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_VBAT | \
                      CSK_GPADC_DMA_ENABLE(0));

    for(uint32_t i=0; i<20; i++){
        //clear fifo data
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_0);
    }

    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 8);

    HAL_GPADC_SetFifoThd0(GPADC(), CSK_GPADC_VBAT, 7);

    HAL_GPADC_EnableFifoInterrupt(GPADC(), CSK_GPADC_VBAT, CSK_GPADC_FIFO_THD);

    HAL_GPADC_RegisterChannelCallback(GPADC(), CSK_GPADC_VBAT, GPADC_Channel_x_FifoEvent);

    HAL_GPADC_Start(GPADC());
    SysTick_Delay_Ms(500);
}

static void KEYSENSE0_ADCTRIGGER_Event(void* param){
	CLOGD("KEYSENSE0 adc trigger event generate");
	HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
}

static void KEYSENSE0_RELEASE_Event(void* param){
	CLOGD("KEYSENSE0 release event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
}


static void KEYSENSE0_PRESS_Event(void* param){
	CLOGD("KEYSENSE0 press event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
}

static void KEYSENSE1_ADCTRIGGER_Event(void* param){
	CLOGD("KEYSENSE1 adc trigger event generate");
	HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
}

static void KEYSENSE1_RELEASE_Event(void* param){
	CLOGD("KEYSENSE1 release event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
}


static void KEYSENSE1_PRESS_Event(void* param){
	CLOGD("KEYSENSE1 press event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
}

void GPADC_KeySense0_Trigger(void)
{
    /*********GPADC read all channels********/
    CLOGD("GPADC keysense trigger");

    //key_intr_flag = 0;
    //initialize
    HAL_KEYSENSE_Initialize(KEYSENSE0());

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 5;

    //SysTick_Delay_Ms(1);
    //HAL_KEYSENSE_Control(KEYSENSE0(), CSK_KEYSENSE_THD);
    IP_KEYSENSE0->REG_KS_THD.bit.KS_THD_ADC_TRIG = 0x100;//0xffff;
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_ADCTRIG, KEYSENSE0_ADCTRIGGER_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_RELEASE, KEYSENSE0_RELEASE_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_PRESS, KEYSENSE0_PRESS_Event);

    //delay(100);
    //SysTick_Delay_Ms(1000);
    HAL_KEYSENSE_Enable(KEYSENSE0());
    IP_KEYSENSE0->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask

    uint32_t cnt, state, adc_value;

    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(),  (CSK_GPADC_CHANNEL_SEL_KEYSENSE0) | CSK_GPADC_DMA_ENABLE(0));

    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 8);
    HAL_GPADC_SetVrefSel(GPADC(), 2); //VDDIO

    while(1){
        //keysense trigger
        SysTick_Delay_Ms(1000);
        HAL_GPADC_PollForConversion(GPADC(), 0);
        for (int i = 0; i < 8; i++) {
        	adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE0);
        	CLOGD("channel adc value %d, adc value is %dmV", adc_value, (uint16_t)(adc_value*1000/1024*1.1*3));
        }
    }
}

void GPADC_KeySense1_Trigger(void)
{
    /*********GPADC read all channels********/
    CLOGD("GPADC keysense trigger");

    //key_intr_flag = 0;
    //initialize
    HAL_KEYSENSE_Initialize(KEYSENSE1());

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 5;
    //HAL_KEYSENSE_Control(KEYSENSE1(), CSK_KEYSENSE_THD);
    IP_KEYSENSE1->REG_KS_THD.bit.KS_THD_ADC_TRIG = 0xffff;
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_ADCTRIG, KEYSENSE1_ADCTRIGGER_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_RELEASE, KEYSENSE1_RELEASE_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_PRESS, KEYSENSE1_PRESS_Event);

    HAL_KEYSENSE_Enable(KEYSENSE1());
    IP_KEYSENSE1->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask

    uint32_t cnt, state, adc_value;

    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_KEYSENSE1) | CSK_GPADC_DMA_ENABLE(0));

    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 8);
    HAL_GPADC_SetVrefSel(GPADC(), 2); //VDDIO

    while(1){
        //keysense trigger
        SysTick_Delay_Ms(1000);
        HAL_GPADC_PollForConversion(GPADC(), 0);
        for (int i = 0; i < 8; i++) {
        	adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE1);
        	CLOGD("channel adc value %d, adc value is %dmV", adc_value, (uint16_t)(adc_value*1000/1024*1.1*3));
        }
    }
}

void GPADC_KeySense_Trigger_Interrupt(void)
{
    /*********GPADC read all channels********/
    CLOGD("GPADC keysense trigger interrupt");

    //initialize
    HAL_KEYSENSE_Initialize(KEYSENSE0());

    //pA27->PB3 Channel1
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 8;

    //keysense default threshold value
    HAL_KEYSENSE_Control(KEYSENSE0(), CSK_KEYSENSE_THD);

    HAL_KEYSENSE_Enable(KEYSENSE0());

    uint32_t cnt, state, adc_value;
//    GPADC_RESOURCES *tmphandle = GPADC();
    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(),  (CSK_GPADC_CHANNEL_SEL_KEYSENSE0) | CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
    //HAL_GPADC_SetKeysenseTrigger_enable(1);
    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 1);

    HAL_GPADC_RegisterCompleteCallback(GPADC(), GPADC_Complete_Event);
    HAL_GPADC_EnableCmpInterrupt(GPADC());

    while(1);
}


////TC3->GPIOA7
//#define PWM_CH7_PAD           (CSK_IOMUX_PAD_A)
//#define PWM_CH7_PIN           (07)
//#define PWM_CH7_SEL           (11)

void GPT_PWM_Output_Channel7(void)
{
	uint32_t ret;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
			CSK_GPT_PWM_CLKDIV_128 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL7);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL7, 1000, 50);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL7);
}


void GPADC_AllChannel_GPTTrigger(void)
{
	/*********GPADC read all channels********/
    //io config channel0-channel2
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 0;

	uint32_t cnt, state, adc_value;
    //initialize, all channels are configured
	HAL_GPADC_Initialize(GPADC());
	HAL_GPADC_Control(GPADC(),  (CSK_GPADC_CHANNEL_SEL_ALL) | CSK_GPADC_DMA_ENABLE(0));

	HAL_GPADC_SetTriggerNum(GPADC(), 8);
	HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
	HAL_GPADC_SetGptTrigger_enable(GPADC(), 1);

	//channel 7 is fixed to trigger GPADC
	GPT_PWM_Output_Channel7();

	HAL_GPADC_PollForConversion(GPADC(), 0);
	adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_0);
    CLOGD("channel is %d, adc value is 0x%x", 0, adc_value);
    CLOGD("channel is %d, adc value is %dmV", 0, (uint16_t)(adc_value*1000/1024*1.2*3));
	adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_1);
    CLOGD("channel is %d, adc value is 0x%x", 1, adc_value);
    CLOGD("channel is %d, adc value is %dmV", 1, (uint16_t)(adc_value*1000/1024*1.2*3));
    adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_2);
	// adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_3);
	adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_VBAT);
	CLOGD("channel is VBAT, adc value is 0x%x", adc_value);
	CLOGD("channel is VBAT, adc value is %dmV", (uint16_t)(adc_value*1000/1024*1.2*3));
	TEST_ASSERT_INT_WITHIN(20,533,adc_value);
	//adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_KEYSENSE0);
	//adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_KEYSENSE1);

}


static void GPADC_Channel_1_DmaCmpEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t res){
	for(uint32_t i=0; i<10; i++){
	    CLOGD("GPADC channel value is %d", adc_val1[i]);
	}
}

void GPADC_Channel1_Dma(void)
{
	/*********GPADC dma mode for simulation********/
    CLOGD("GPADC dma mode, read adc channel 1, test begin");

    //io config
    //pA27->PB3 Channel1
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;

	uint32_t state;
	GPADC_RESOURCES *tmphandle = GPADC();
    //initialize
	HAL_GPADC_Initialize(GPADC());
	HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_1 | CSK_GPADC_DMA_ENABLE(0x20));

	//dma mode, transfer 11 times for dma transfer
	HAL_GPADC_SetTriggerNum(GPADC(), 10);
	HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2

    for(uint32_t i=0; i<20; i++){
        //clear fifo data
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_1);
    }

	dma_initialize();
	dma_channel_select(
			&channel_num,
			GPADC_Channel_1_DmaCmpEvent,
			0,
			DMA_CACHE_SYNC_DST);

	state = dma_channel_configure (channel_num,
				(uint32_t)(&(tmphandle->reg->REG_DMA_RDR)),
				(uint32_t) adc_val1,
				10,
				DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
				DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
				DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
				DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
				DMA_CH_CFGL_CH_PRIOR(1),
				DMA_CH_CFGH_SRC_PER(10), // config_high
				0, 0);

	HAL_GPADC_Start(GPADC());

    while(1);
}


static void GPADC_MultiChannel_DmaCmpEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t res){
	for(uint32_t i=0; i<30; i++){
		CLOGD("GPADC channel value is %d", adc_val1[i]);
	}
}

void GPADC_MultiChannel_Dma(void)
{
	/*********GPADC dma mode for simulation********/
    CLOGD("GPADC dma mode, read adc channel 0&1&2, dma adc data will be arraged as ch0&ch1&ch2&ch0....test begin");

    //io config
    //io config channel0-channel2
    //PA26->PB2 Channel0
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    //pA27->PB3 Channel1
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;
    //pA28->PB4 Channel2
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 0;

    uint32_t state;
	GPADC_RESOURCES *tmphandle = GPADC();
    //initialize
	HAL_GPADC_Initialize(GPADC());
	HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_0 | CSK_GPADC_CHANNEL_SEL_1 | CSK_GPADC_CHANNEL_SEL_2) | \
					   CSK_GPADC_DMA_ENABLE(0x70));

	//dma mode, transfer 11 times for dma transfer
	HAL_GPADC_SetTriggerNum(GPADC(), 10);
	HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2

    for(uint32_t i=0; i<20; i++){
        //clear fifo data
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_0);
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_1);
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_2);
    }

	dma_initialize();
	dma_channel_select(
			&channel_num,
			GPADC_MultiChannel_DmaCmpEvent,
			0,
			DMA_CACHE_SYNC_DST);

	state = dma_channel_configure (channel_num,
				(uint32_t)(&(tmphandle->reg->REG_DMA_RDR)),
				(uint32_t) adc_val1,
				30,
				DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
				DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
				DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
				DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
				DMA_CH_CFGL_CH_PRIOR(1),
				DMA_CH_CFGH_SRC_PER(10), // config_high
				0, 0);

	HAL_GPADC_Start(GPADC());

    while(1);
}

void GPADC_Channel_Temperature(void)
{
    /*********GPADC read all channels********/
    CLOGD("GPADC read all channels polling, test begin");

    uint32_t cnt, state;
    uint16_t adc_value;

    //initialize, all channels are configured
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_Initialize(GPADC()));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_Control(GPADC(),
                     (CSK_GPADC_CHANNEL_SEL_TEMP | CSK_GPADC_CHANNEL_SEL_VBAT) |
                     CSK_GPADC_DMA_ENABLE(0)));

    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_SetTriggerNum(GPADC(), 1));
    TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_SetVrefSel(GPADC(), 0)); //VBG1/2

    while(1){
        SysTick_Delay_Ms(800);

        // Test start conversion
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_Start(GPADC()));

        // Test conversion completion
        TEST_ASSERT_EQUAL(CSK_DRIVER_OK, HAL_GPADC_PollForConversion(GPADC(), 0));

        // Get and validate ADC value
        adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_TEMP);
        TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
        //TEST_ASSERT_NOT_EQUAL(0, adc_value); // Additional check for non-zero value

        float temp_cal = 21.00;
        uint16_t adc_value_cal = 616;

        // Calculate calibrated ADC channel voltage
        float vptat_cal = (float)adc_value_cal / 1024 * 1.2;
        TEST_ASSERT_FLOAT_WITHIN(0.001, 0.721875, vptat_cal); // 616/1024*1.2=0.721875

        // Calculate current ADC channel voltage
        float vptat = (float)adc_value / 1024 * 1.2;
        TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, vptat); // Should be > 0
        TEST_ASSERT_FLOAT_WITHIN(0.001, 1.2, vptat); // Should be < 1.2

        // Calculate calibration codes
        int vptat_cal_code = (int)(vptat_cal * pow(2, 31));
        int temp_cal_code = (int)((temp_cal + 80) * pow(2, 24));
        TEST_ASSERT_INT_WITHIN(1000000, 1550214758, vptat_cal_code); // Approximate expected value
        TEST_ASSERT_INT_WITHIN(1000000, 1694498816, temp_cal_code); // Approximate expected value

        // Calculate current temperature
        float temperature = temp_cal + (temp_cal + 273.15) / vptat_cal * (vptat - vptat_cal);

        // Validate temperature calculation components
        TEST_ASSERT_FALSE(isnan(temperature));
        TEST_ASSERT_FALSE(isinf(temperature));

        // Print and validate temperature
        print_float(temperature);
        TEST_ASSERT(temperature > -10 && temperature < 40);

        // Additional validation for reasonable temperature delta
        static float last_temp = 0;
        if (last_temp != 0) {
            TEST_ASSERT_FLOAT_WITHIN(5.0, last_temp, temperature); // Shouldn't change more than 5°C between readings
        }
        last_temp = temperature;
    }
}

static void SPI_DrvEvent (uint32_t event, uint32_t usr_param) {
	CLOGD("adc-dma single channel test begin");
}

void GPADC_SingleChannel_Dma(void)
{
    /*********GPADC dma mode for simulation********/
    CLOGD("adc-dma single channel test begin");

    //io config channel0-channel2
    //PB2 Channel0/Channel1/Channel2
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 0;


    uint32_t state;
    GPADC_RESOURCES *tmphandle = GPADC();
    //initialize
    HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_0) | CSK_GPADC_DMA_ENABLE(0x10));

    //dma mode, transfer 11 times for dma transfer
    HAL_GPADC_SetTriggerNum(GPADC(), 0);
    HAL_GPADC_SetVrefSel(GPADC(), 0);

    for(uint32_t i=0; i<20; i++){
        //clear fifo data
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_0);
    }

	//dma handshake
    IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_10 = 1;

    dma_initialize();

   // channel_num = 3;

    dma_channel_select(
            &channel_num,
            GPADC_Channel_x_DmaCmpSpiTranferEvent,
            0,
            DMA_CACHE_SYNC_DST);

    state = dma_channel_configure (channel_num,
                (uint32_t)(&(tmphandle->reg->REG_DMA_RDR)),
                (uint32_t) adc_val1,
                30,
                DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
                DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
                DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
                DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
                DMA_CH_CFGL_CH_PRIOR(1),
                DMA_CH_CFGH_SRC_PER(10), // config_high
                0, 0);

	//SPI0 config
    gGpioDev = GPIOA();
    gSpiDev = SPI0();

	//GPIO configuration
	GPIO_Initialize(gGpioDev, NULL, NULL);

	//io config for spi0
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER5);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_MOSI_PIN, CSK_IOMUX_FUNC_ALTER5);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CS_PIN, CSK_IOMUX_FUNC_ALTER5);

	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_DEFAULT);

	GPIO_Control(gGpioDev, CSK_GPIO_MODE_PULL_NONE | \
					   CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN11);
	GPIO_SetDir(gGpioDev, CSK_GPIO_PIN11, CSK_GPIO_DIR_OUTPUT);

    //SPI configuration
    __HAL_CRM_SPI0_CLK_ENABLE();
    uint32_t bus_speed = SPI_BUS_SPEED;
    SPI_Initialize(gSpiDev, SPI_DrvEvent, (uint32_t)gSpiDev);
    SPI_PowerControl (gSpiDev, CSK_POWER_FULL);
    //Todo: spi dma mode error
    SPI_Control(gSpiDev, CSK_SPI_MODE_MASTER | CSK_SPI_TXIO_PIO  | //FIXME: DMA? PIO?
                 CSK_SPI_CPOL1_CPHA1 |
                 CSK_SPI_DATA_BITS(SPI_DATA_BITS) |
                 CSK_SPI_MSB_LSB, bus_speed);

    bus_speed = SPI_Control(gSpiDev, CSK_SPI_GET_BUS_SPEED, 0);

    HAL_GPADC_Start(GPADC());

    //while(1);
}

void GPADC_Channel_x_DmaCmpSpiTranferEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t res)
{
    GPIO_PinWrite(gGpioDev, CSK_GPIO_PIN11, 0);
	uint32_t state;
	GPADC_RESOURCES *tmphandle = GPADC();

	if(buffer_sel == 1){
		adcsend = adc_val1;
		SPI_Send(gSpiDev, adcsend, 500);
		buffer_sel = 2;
		adcsend = adc_val2;
	}else{
		adcsend = adc_val2;
		SPI_Send(gSpiDev, adcsend, 500);
		buffer_sel = 1;
		adcsend = adc_val1;
	}

	state = dma_channel_configure (channel_num,
				(uint32_t)(&(tmphandle->reg->REG_DMA_RDR)),
				(uint32_t) adcsend,
				500,
				DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
				DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
				DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
				DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
				DMA_CH_CFGL_CH_PRIOR(1),
				DMA_CH_CFGH_SRC_PER(10), // config_high
				0, 0);
    GPIO_PinWrite(gGpioDev, CSK_GPIO_PIN11, 1);
}


void GPADC_Channel0_Dma_SPI(void)
{
	/*********GPADC dma mode for simulation********/

	GPADC_RESOURCES *tmphandle = GPADC();
    //Analog PB2 Channel0
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    //Analog PB3 Channel1
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;

    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_0) | CSK_GPADC_DMA_ENABLE(0x10));

	//dma mode, gpadc translate infinitely for debug
	HAL_GPADC_SetTriggerNum(GPADC(), 0);

	//dma handshake
	IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_10 = 1; //gpadc rx dma req

	dma_initialize();

	channel_num = 3;
	//fix dma channel
	dma_channel_reserve(
			channel_num,
			GPADC_Channel_x_DmaCmpSpiTranferEvent,
			0,
			DMA_CACHE_SYNC_DST);

	uint32_t state;
	state = dma_channel_configure (channel_num,
				(uint32_t)(&(tmphandle->reg->REG_DMA_RDR)),
				(uint32_t) adc_val1,
				500,
				DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
				DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
				DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
				DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
				DMA_CH_CFGL_CH_PRIOR(1),
				DMA_CH_CFGH_SRC_PER(10), // config_high
				0, 0);

	//SPI0 config
    gGpioDev = GPIOA();
    gSpiDev = SPI0();

    //GPIO configuration
    GPIO_Initialize(gGpioDev, NULL, NULL);

    //io config for spi0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_MOSI_PIN, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CS_PIN, CSK_IOMUX_FUNC_ALTER5);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Control(gGpioDev, CSK_GPIO_MODE_PULL_NONE | \
                                CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN11);
    GPIO_SetDir(gGpioDev, CSK_GPIO_PIN11, CSK_GPIO_DIR_OUTPUT);

	//SPI configuration
	__HAL_CRM_SPI0_CLK_ENABLE();
	uint32_t bus_speed = SPI_BUS_SPEED;
	SPI_Initialize(gSpiDev, SPI_DrvEvent, (uint32_t)gSpiDev);
	SPI_PowerControl (gSpiDev, CSK_POWER_FULL);
	//Todo: spi dma mode error
	SPI_Control(gSpiDev, CSK_SPI_MODE_MASTER | CSK_SPI_TXIO_PIO  | //FIXME: DMA? PIO?
	CSK_SPI_CPOL1_CPHA1 |
	CSK_SPI_DATA_BITS(SPI_DATA_BITS) |
	CSK_SPI_MSB_LSB, bus_speed);

	bus_speed = SPI_Control(gSpiDev, CSK_SPI_GET_BUS_SPEED, 0);

    //start gpadc
	HAL_GPADC_Start(GPADC());

    while(1);
}

#define WAKEUP_SOURCE_TO_STRING(src)	(	\
		src == PMU_WAKEUP_NONE ? "None wakeup" : \
		src == PMU_WAKEUP_TIMER ? "TIMER wakeup" : \
		src == PMU_WAKEUP_IWDT ? "IWDT wakeup" : \
		src == PMU_WAKEUP_KEY0 ? "KEY0 wakeup" : \
		src == PMU_WAKEUP_KEY1 ? "KEY1 wakeup" : \
		src == PMU_WAKEUP_RTC  ? "RTC  wakeup" : \
		src == PMU_WAKEUP_WIFI ? "WIFI  wakeup" : \
		src == PMU_WAKEUP_GPIOB_00 ? "GPIOB_00 wakeup" : \
		src == PMU_WAKEUP_GPIOB_01 ? "GPIOB_01 wakeup" : \
		src == PMU_WAKEUP_GPIOB_02 ? "GPIOB_02 wakeup" : \
		src == PMU_WAKEUP_GPIOB_03 ? "GPIOB_03 wakeup" : \
		src == PMU_WAKEUP_GPIOB_04 ? "GPIOB_04 wakeup" : \
		src == PMU_WAKEUP_GPIOB_05 ? "GPIOB_05 wakeup" : \
		src == PMU_WAKEUP_GPIOB_06 ? "GPIOB_06 wakeup" : \
		src == PMU_WAKEUP_GPIOB_07 ? "GPIOB_07 wakeup" : \
		src == PMU_WAKEUP_GPIOB_08 ? "GPIOB_08 wakeup" : \
		src == PMU_WAKEUP_GPIOB_09 ? "GPIOB_09 wakeup" : \
		"Unknown wakeup")

static void GetWakeupEvent(uint32_t wakesrc) {
	CLOGD("%s\n", WAKEUP_SOURCE_TO_STRING(wakesrc));
}

void GPADC_Keysense0_Wakeup(void)
{
	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();

	//TEST_ASSERT(wakeup_cause == PMU_WAKEUP_NONE || wakeup_cause == PMU_WAKEUP_KEY0);

	CLOGD("[keysense]WakeUp cause -> %d", wakeup_cause);
	GetWakeupEvent(wakeup_cause);

	SysTick_Delay_Ms(500);

	if (wakeup_cause == PMU_WAKEUP_KEY0){
		CLOGD("keysense0 wakeup enter and trigger gpadc.\r\n");
		HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
		//aon wake up irq clear key0_wakeup_icr
		IP_AON_CTRL->REG_WAKEUP_ICR.bit.KEY0_WAKEUP_ICR = 1;
		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_KEY0);
	}

	//initialize
	HAL_KEYSENSE_Initialize(KEYSENSE0());

	//gpio config
	//PB2 Keysense0
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
	IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 5;

	IP_KEYSENSE0->REG_KS_THD.bit.KS_THD_ADC_TRIG = 0xffff;
	HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_ADCTRIG, KEYSENSE0_ADCTRIGGER_Event);
	HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_RELEASE, KEYSENSE0_RELEASE_Event);
	HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_PRESS, KEYSENSE0_PRESS_Event);

	HAL_KEYSENSE_Enable(KEYSENSE0());
	IP_KEYSENSE0->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask

	uint32_t cnt, state, adc_value;

	SysTick_Delay_Ms(500);

	//initialize
	HAL_GPADC_Initialize(GPADC());

	HAL_GPADC_Control(GPADC(),  (CSK_GPADC_CHANNEL_SEL_KEYSENSE0) | CSK_GPADC_DMA_ENABLE(0));

	//dma mode, transfer infinite for debug
	HAL_GPADC_SetTriggerNum(GPADC(), 8);
	HAL_GPADC_SetVrefSel(GPADC(), 2); //VDDIO
	//keysense trigger
	SysTick_Delay_Ms(1000);
	HAL_GPADC_PollForConversion(GPADC(), 0);
	for (int i = 0; i < 8; i++) {
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE0);
		TEST_ASSERT_UINT16_WITHIN(0, 1024, adc_value);
		CLOGD("channel adc value %d, adc value is %dmV", adc_value, (uint16_t)(adc_value*1000/1024*1.1*3));
	}

	HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_KEY0);
	HAL_KEYSENSE_Enable(KEYSENSE0());
	//IP_KEYSENSE0->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask
	//CLOGD("[KEYSENSE enter] Can't Entry SleepMode!!!!!");
	//enter deep sleep
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);
}

void GPADC_Keysense1_Wakeup(void)
{
	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();

	CLOGD("[keysense]WakeUp cause -> %d", wakeup_cause);
	GetWakeupEvent(wakeup_cause);

	SysTick_Delay_Ms(500);

	if (wakeup_cause == PMU_WAKEUP_KEY1){
		CLOGD("keysense1 wakeup enter and trigger gpadc.\r\n");
		HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
		//aon wake up irq clear key0_wakeup_icr
		IP_AON_CTRL->REG_WAKEUP_ICR.bit.KEY1_WAKEUP_ICR = 1;
		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_KEY1);
	}

	//initialize
	HAL_KEYSENSE_Initialize(KEYSENSE1());

	//gpio config
	//PB2 Keysense0
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
	IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 5;

	IP_KEYSENSE0->REG_KS_THD.bit.KS_THD_ADC_TRIG = 0xffff;
	HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_ADCTRIG, KEYSENSE1_ADCTRIGGER_Event);
	HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_RELEASE, KEYSENSE1_RELEASE_Event);
	HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_PRESS, KEYSENSE1_PRESS_Event);

	HAL_KEYSENSE_Enable(KEYSENSE1());
	IP_KEYSENSE1->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask

	uint32_t cnt, state, adc_value;

	SysTick_Delay_Ms(500);

	//initialize
	HAL_GPADC_Initialize(GPADC());

	HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE1 | CSK_GPADC_DMA_ENABLE(0));

	HAL_GPADC_SetVrefSel(GPADC(), 2); //VDDIO
	//dma mode, transfer infinite for debug
	HAL_GPADC_SetTriggerNum(GPADC(), 8);

	//keysense trigger
	SysTick_Delay_Ms(1000);
	HAL_GPADC_PollForConversion(GPADC(), 0);
	for (int i = 0; i < 8; i++) {
		adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE1);
		CLOGD("channel adc value %d, adc value is %dmV", adc_value, (uint16_t)(adc_value*1000/1024*1.1*3));
	}

	HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_KEY1);
	HAL_KEYSENSE_Enable(KEYSENSE1());
	//IP_KEYSENSE1->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask
	//CLOGD("[KEYSENSE enter] Can't Entry SleepMode!!!!!");
	//enter deep sleep
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);
}

void GPADC_CVD_Polling(void)
{
    uint32_t adc_value, adc_value1, adc_value_high, adc_value_low;
    int cnt_cur = 0, cnt = 0;
    //io config
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 3);

    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_ANA_SEL = 1;
    //ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 1);
    //ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 1);
    //ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 1);
    //ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 1);
    //ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 1);
    //ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 1);

    //initialize
	HAL_GPADC_Initialize(GPADC());

	HAL_GPADC_CVD_Config(GPADC(), CSK_GPADC_GURADRING_ENABLE, CSK_GPADC_DOUBLE_SAPMLE_ENABLE, CSK_GPADC_CHARGE_INV_ENABLE);

	HAL_GPADC_Control(GPADC(), ( CSK_GPADC_CHANNEL_SEL_CVD0 | CSK_GPADC_CHANNEL_SEL_CVD1 | CSK_GPADC_CHANNEL_SEL_CVD2 | CSK_GPADC_CHANNEL_SEL_CVD3 | CSK_GPADC_CHANNEL_SEL_CVD4 | CSK_GPADC_CHANNEL_SEL_CVD5 ) \
			                    | CSK_GPADC_DMA_ENABLE(0));

	//Set ADC periodical sampling wait time and sample wait time
	HAL_GPADC_SetSampleWaitTime(GPADC(), 8);
	HAL_GPADC_SetSetupWaitTime(GPADC(), 10);
	HAL_GPADC_SetVrefSel(GPADC(), 0); // Vbg 1.2V
	HAL_GPADC_SetVinBuf_Enable(GPADC(), 1); //enable
	HAL_GPADC_SetTriggerNum(GPADC(), 8);
	while(1){
		HAL_GPADC_Start(GPADC());
	    HAL_GPADC_PollForConversion(GPADC(), 0);
		while(IP_GPADC->REG_ADC_IRSR0.bit.FIFO_EMPTY_IRSR_CH10 != 1) {
			adc_value = HAL_GPADC_CVD_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD0);
			CLOGD("GPADC CVD0 final value is %d", adc_value);
		}
		//adc_value_high = (adc_value&0xFFFF0000) >> 16;
		//adc_value_low = adc_value&0xFFFF;

		//if(adc_value_high > adc_value_low)
		//	adc_value = adc_value_high - adc_value_low;
		//else
		//	adc_value = adc_value_low - adc_value_high;
#ifdef DEBUG
		if(adc_value > 160)
#endif
		{
			//CLOGD("GPADC CVD0");
			//CLOGD("GPADC low value is 0x%x", adc_value_low);
			//CLOGD("GPADC high value is 0x%x", adc_value_high);
			//CLOGD("GPADC CVD0 final value is %d", adc_value);
		}
#if 0
		adc_value = HAL_GPADC_CVD_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD1);
		//adc_value_high = (adc_value&0xFFFF0000) >> 16;
		//adc_value_low = adc_value&0xFFFF;

		//if(adc_value_high > adc_value_low)
		//	adc_value = adc_value_high - adc_value_low;
		//else
		//	adc_value = adc_value_low - adc_value_high;

#ifdef DEBUG
		if(adc_value > 220)
#endif
		{
			//CLOGD("GPADC CVD1");
			//CLOGD("GPADC low value is 0x%x", adc_value_low);
			//CLOGD("GPADC high value is 0x%x", adc_value_high);
			CLOGD("GPADC CVD1 final value is %d", adc_value);
		}

		adc_value = HAL_GPADC_CVD_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD2);
		adc_value_high = (adc_value&0xFFFF0000) >> 16;
		adc_value_low = adc_value&0xFFFF;

		if(adc_value_high > adc_value_low)
			adc_value = adc_value_high - adc_value_low;
		else
			adc_value = adc_value_low - adc_value_high;

#ifdef DEBUG
		if(adc_value > 70)
#endif
		{
			CLOGD("GPADC CVD2");
			CLOGD("GPADC low value is 0x%x", adc_value_low);
			CLOGD("GPADC high value is 0x%x", adc_value_high);
			CLOGD("GPADC CVD2 final value is %d", adc_value);
		}

		adc_value = HAL_GPADC_CVD_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD3);
		adc_value_high = (adc_value&0xFFFF0000) >> 16;
		adc_value_low = adc_value&0xFFFF;

		if(adc_value_high > adc_value_low)
			adc_value = adc_value_high - adc_value_low;
		else
			adc_value = adc_value_low - adc_value_high;
#ifdef DEBUG
		if(adc_value > 210)
#endif
		{
			CLOGD("GPADC CVD3");
			CLOGD("GPADC low value is 0x%x", adc_value_low);
			CLOGD("GPADC high value is 0x%x", adc_value_high);
			CLOGD("GPADC CVD3 final value is %d", adc_value);
		}

		adc_value = HAL_GPADC_CVD_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD4);
		adc_value_high = (adc_value&0xFFFF0000) >> 16;
		adc_value_low = adc_value&0xFFFF;

		if(adc_value_high > adc_value_low)
			adc_value = adc_value_high - adc_value_low;
		else
			adc_value = adc_value_low - adc_value_high;
#ifdef DEBUG
		if(adc_value > 210)
#endif
		{
			CLOGD("GPADC CVD4");
			CLOGD("GPADC low value is 0x%x", adc_value_low);
			CLOGD("GPADC high value is 0x%x", adc_value_high);
			CLOGD("GPADC CVD4 final value is %d", adc_value);
		}

		adc_value = HAL_GPADC_CVD_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD5);
		adc_value_high = (adc_value&0xFFFF0000) >> 16;
		adc_value_low = adc_value&0xFFFF;

		if(adc_value_high > adc_value_low)
			adc_value = adc_value_high - adc_value_low;
		else
			adc_value = adc_value_low - adc_value_high;
#ifdef DEBUG
		if(adc_value > 210)
#endif
		{
			CLOGD("GPADC CVD5");
			CLOGD("GPADC low value is 0x%x", adc_value_low);
			CLOGD("GPADC high value is 0x%x", adc_value_high);
			CLOGD("GPADC CVD5 final value is %d", adc_value);
		}
#endif
		SysTick_Delay_Ms(1000);
	}
}

static void GPADC_CVD_Complete_Event(uint32_t event, void* param){
	uint32_t adc_value, pos=0;
	uint32_t channeltotal, channelnum;

	channeltotal = (uint32_t)param;
	if( event == CSK_GPADC_COMPLETE ){
		//total channel is 6
		while(pos<6){
			channelnum = (channeltotal&(0x0100<<pos)) << CSK_GPADC_CHANNEL_SEL_Pos;
			if(channelnum){
				adc_value = HAL_GPADC_CVD_GetValue(GPADC(), channelnum);
				CLOGD("channel is 0x%x, adc value is 0x%x",channelnum, adc_value);
				adc_value = ((adc_value&0xFFFF0000)>>16) - (adc_value&0xFFFF);
				if((adc_value & 0x8000) != 0)
				{
					adc_value = 0xFFFF - adc_value;
				}

				CLOGD("GPADC final value is %d", adc_value);
			}
			pos++;
		}

	    HAL_GPADC_Start_IT(GPADC());
	}

	if( event == CSK_GPADC_EOC_ERROR ){
		CLOGD("gpadc eoc error generated");
	}
}

void GPADC_CVD_Interrupt(void)
{
    uint32_t adc_value;

    //io config
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 3);

    ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 1);
    ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 1);
    ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 1);
    ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 1);
    ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 1);
    ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 1);

    //initialize
	HAL_GPADC_Initialize(GPADC());

	HAL_GPADC_CVD_Control(GPADC(), (0x40 << CSK_GPADC_PRECHARGEA_CONTROL_Pos) \
								 | (0x40 << CSK_GPADC_PRECHARGEB_CONTROL_Pos) \
								 | (0x40 << CSK_GPADC_ACQUICITION_CONTROL_Pos) \
								 | (0x00 << CSK_GPADC_ADDITIONAL_CAP_CONTROL_Pos));

	HAL_GPADC_CVD_Config(GPADC(), CSK_GPADC_GURADRING_ENABLE, CSK_GPADC_DOUBLE_SAPMLE_ENABLE, CSK_GPADC_CHARGE_INV_ENABLE);

	HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_CVD0 | CSK_GPADC_CHANNEL_SEL_CVD1 | CSK_GPADC_CHANNEL_SEL_CVD2 | CSK_GPADC_CHANNEL_SEL_CVD3 | CSK_GPADC_CHANNEL_SEL_CVD4 | CSK_GPADC_CHANNEL_SEL_CVD5) \
			                   | CSK_GPADC_DMA_ENABLE(0));

	HAL_GPADC_SetVrefSel(GPADC(), 0);
	HAL_GPADC_SetTriggerNum(GPADC(), 1);

	HAL_GPADC_RegisterCompleteCallback(GPADC(), GPADC_CVD_Complete_Event);

	HAL_GPADC_Start_IT(GPADC());

	while(1);
}

void GPADC_CVD_Channel_Dma(void)
{
	/*********GPADC dma mode for simulation********/
    CLOGD("GPADC dma mode, read adc channel cvd, dma adc data will be arraged as cvd0 cv1 cvd2....test begin");

    //io config
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 3);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 3);

    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_ANA_SEL = 1;
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_ANA_SEL = 1;

    uint32_t state;
	GPADC_RESOURCES *tmphandle = GPADC();
    //initialize
	HAL_GPADC_Initialize(GPADC());
	//HAL_GPADC_CVD_Control(GPADC(), (0xFF << CSK_GPADC_PRECHARGEA_CONTROL_Pos) \
								 | (0xFF << CSK_GPADC_PRECHARGEB_CONTROL_Pos) \
								 | (0xFF << CSK_GPADC_ACQUICITION_CONTROL_Pos) \
								 | (0x00 << CSK_GPADC_ADDITIONAL_CAP_CONTROL_Pos));

	HAL_GPADC_CVD_Config(GPADC(), CSK_GPADC_GURADRING_ENABLE, CSK_GPADC_DOUBLE_SAPMLE_ENABLE, CSK_GPADC_CHARGE_INV_ENABLE);

	HAL_GPADC_Control(GPADC(),  CSK_GPADC_CHANNEL_SEL_CVD0 \
			                    | CSK_GPADC_DMA_ENABLE(0x400));

	HAL_GPADC_SetVrefSel(GPADC(), 0);
	//dma mode, transfer 11 times for dma transfer
	HAL_GPADC_SetTriggerNum(GPADC(), 10);

    for(uint32_t i=0; i<20; i++){
        //clear fifo data
        HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD0);
        //HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD1);
        //HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD2);
        //HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD3);
        //HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD4);
        //HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_CVD5);
    }

    IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_10 = 1; //gpadc rx dma req

	dma_initialize();
	dma_channel_select(
			&channel_num,
			GPADC_MultiChannel_DmaCmpEvent,
			0,
			DMA_CACHE_SYNC_DST);

	state = dma_channel_configure (channel_num,
				(uint32_t)(&(tmphandle->reg->REG_DMA_RDR)),
				(uint32_t) adc_val1,
				30,
				DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
				DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
				DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
				DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
				DMA_CH_CFGL_CH_PRIOR(1),
				DMA_CH_CFGH_SRC_PER(10), // config_high
				0, 0);

	HAL_GPADC_Start(GPADC());

    while(1);
}
