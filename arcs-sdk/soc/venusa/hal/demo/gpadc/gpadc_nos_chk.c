#include "stdio.h"
#include <string.h>

#include "venusa_ap.h"

#include "gpadc.h"
//#include "Driver_GPT_IC.h"
//#include "Driver_GPT_PWM.h"
//#include "Driver_GPT_TIMER.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "Driver_SPI.h"
#include "Driver_KEYSENSE.h"
#include "ClockManager.h"
#include "PowerManager.h"

#include "log_print.h"
#include "systick.h"
#include "dma.h"
#include "unity.h"

/********************** Private micro define begin ***************************/
#define SPI_CS_PIN              11
#define SPI_CLK_PIN             16
#define SPI_MOSI_PIN            13
#define SPI_MISO_PIN        	12

#define SPI_BUS_SPEED           6000000 // 2000000 // 2MHz
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
void GPADC_KeySense_Trigger_Interrupt(void);
//void GPADC_Channel1_Dma(void);
//void GPADC_MultiChannel_Dma(void);
//void GPADC_SingleChannel_Dma(void);
void GPADC_Channel_Temperature(void);
void GPADC_Channel0_Dma_SPI(void);
void GPADC_Keysense0_Wakeup(void);

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

#include <stdint.h>

// 根据实测调整此值，示例值基于24MHz主频和典型循环开销
#define DELAY_COUNT_PER_MS 24000  // 约1ms延时

void delay_ms(uint32_t ms) {
    while (ms--) {
        volatile uint32_t count = DELAY_COUNT_PER_MS;
        while (count--) {
            __asm__("nop");  // 插入空指令，防止优化
        }
    }
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
    logInit(1, 115200);

	__HAL_PMU_GPADC_RST_ENABLE();
	HAL_CRM_SetGpadcClkDiv(12);

    UNITY_BEGIN();
//    RUN_TEST(GPADC_AllChannel_Polling);
//    RUN_TEST(GPADC_AllChannel_Interrupt);
//  RUN_TEST(GPADC_SingleChannel_Dma);
  RUN_TEST(GPADC_Channel0_Dma_SPI);
//  RUN_TEST(GPADC_KeySense0_Trigger);
//  RUN_TEST(GPADC_KeySense1_Trigger);
//	RUN_TEST(GPADC_Keysense0_Wakeup);
//  RUN_TEST(GPADC_Channel_Temperature);
//	RUN_TEST(GPADC_AllChannel_GPTTrigger);
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
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_01.bit.PAD_AON_GPIOB_01_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;

	uint32_t cnt, state, adc_value;
    //initialize, all channels are configured
	HAL_GPADC_Initialize(GPADC());
	HAL_GPADC_Control(GPADC(),  (CSK_GPADC_CHANNEL_SEL_0 | \
	                           CSK_GPADC_CHANNEL_SEL_1 | CSK_GPADC_CHANNEL_SEL_2 | CSK_GPADC_CHANNEL_SEL_VBAT) | \
	                           CSK_GPADC_DMA_ENABLE(0));

	HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2
	HAL_GPADC_SetTriggerNum(GPADC(), 2);

	while(1) {
		HAL_GPADC_Start(GPADC());
		HAL_GPADC_PollForConversion(GPADC(), 0);
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_0);
		CLOGD("channel is %d, adc value is 0x%x", 0, adc_value);
		CLOGD("channel is %d, adc value is %dmV", 0, (uint16_t)(adc_value*1000/1024*1.2)*3);
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_1);
		CLOGD("channel is %d, adc value is 0x%x", 1, adc_value);
		CLOGD("channel is %d, adc value is %dmV", 1, (uint16_t)(adc_value*1000/1024*1.2)*3);
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_2);
		CLOGD("channel is %d, adc value is 0x%x", 2, adc_value);
		CLOGD("channel is %d, adc value is %dmV", 2, (uint16_t)(adc_value*1000/1024*1.2)*3);
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_VBAT);
		TEST_ASSERT_INT_WITHIN(900,1100,adc_value);
		CLOGD("channel is VBAT, adc value is 0x%x", adc_value);
		CLOGD("channel is VBAT, adc value is %dmV", (uint16_t)(adc_value*1000/1024*1.2*3));
		SysTick_Delay_Ms(500);
	}

}


void GPADC_AllChannel_Interrupt(void)
{
	/*********GPADC read all channels********/
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_01.bit.PAD_AON_GPIOB_01_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 0;

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

    HAL_GPADC_SetTriggerNum(GPADC(), 8);

    HAL_GPADC_SetFifoThd(GPADC(), CSK_GPADC_VBAT, 7);

    HAL_GPADC_EnableFifoInterrupt(GPADC(), CSK_GPADC_VBAT, CSK_GPADC_FIFO_THD);

    HAL_GPADC_RegisterChannelCallback(GPADC(), CSK_GPADC_VBAT, GPADC_Channel_x_FifoEvent);

    HAL_GPADC_Start(GPADC());
    SysTick_Delay_Ms(500);
}

static void KEYSENSE0_WAKEUP_Event(void* param){
	CLOG("KEYSENSE0 wakeup event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP);
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

void GPADC_KeySense0_Trigger(void)
{
    /*********GPADC read all channels********/
    CLOGD("GPADC keysense trigger");

    //key_intr_flag = 0;
    //initialize
    HAL_KEYSENSE_Initialize(KEYSENSE0());

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.bit.PAD_AON_GPIOB_00_ANA_SEL = 0;

    //SysTick_Delay_Ms(1);
    HAL_KEYSENSE_Control(KEYSENSE0(), CSK_KEYSENSE_THD);
   // IP_KEYSENSE0->REG_KS_THD.bit.KS_THD_ADC_TRIG = 0xffff;//0xffff;

    //HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_WAKEUP, KEYSENSE0_WAKEUP_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_ADCTRIG, KEYSENSE0_ADCTRIGGER_Event);
    //HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_RELEASE, KEYSENSE0_RELEASE_Event);
    //HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_PRESS, KEYSENSE0_PRESS_Event);

    //delay(100);
    //SysTick_Delay_Ms(1000);
   // HAL_KEYSENSE_InterruptEnable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP | \
        										 CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER | \
    											 CSK_KEYSENSE_INTERRUPT_MODE_RELEASE |\
    											 CSK_KEYSENSE_INTERRUPT_MODE_PRESS);

    HAL_KEYSENSE_InterruptEnable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
    HAL_KEYSENSE_Enable(KEYSENSE0());
   // IP_KEYSENSE0->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask

    uint32_t cnt, state, adc_value;

    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(),  CSK_GPADC_CHANNEL_SEL_KEYSENSE0 | CSK_GPADC_DMA_ENABLE(0));

    //dma mode, transfer infinite for debug
    HAL_GPADC_SetTriggerNum(GPADC(), 8);
    HAL_GPADC_SetVrefSel(GPADC(), 2); //VDDIO

    while(1){
        //keysense trigger
    	 //HAL_GPADC_Start(GPADC());
    	IP_GPADC->REG_ADC_CTRL0.bit.KS_TRIG_EN = 1;
        SysTick_Delay_Ms(1000);
        HAL_GPADC_PollForConversion(GPADC(), 0);
        for (int i = 0; i < 8; i++) {
        	adc_value = HAL_GPADC_GetValue(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE0);
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

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.bit.PAD_AON_GPIOB_00_ANA_SEL = 0;

    //keysense default threshold value
    HAL_KEYSENSE_Control(KEYSENSE0(), CSK_KEYSENSE_THD);

    HAL_KEYSENSE_Enable(KEYSENSE0());

    uint32_t cnt, state, adc_value;

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

void GPADC_Channel_Temperature(void)
{
    /*********GPADC read all channels********/
    CLOGD("GPADC read all channels polling, test begin");

    uint32_t cnt, state;
    uint16_t adc_value;

    //initialize, all channels are configured
    HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_TEMP | CSK_GPADC_CHANNEL_SEL_VBAT) | \
                                CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetTriggerNum(GPADC(), 1);
    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2

    while(1){
        SysTick_Delay_Ms(800);
        HAL_GPADC_Start(GPADC());
        HAL_GPADC_PollForConversion(GPADC(), 0);
        adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_TEMP);

		float temp_cal = 21.00;
		uint16_t adc_value_cal = 591;

		// ����У׼adcͨ����ѹ
		float vptat_cal = (float)adc_value_cal / 1024 * 1.2;
		// ���㵱ǰadcͨ����ѹ
		float vptat = (float)adc_value / 1024 * 1.2;

		// ����У׼��
		int vptat_cal_code = (int)(vptat_cal * pow(2, 31));
		int temp_cal_code = (int)((temp_cal + 80) * pow(2, 24));

		//���㵱ǰ�¶�
        float temprature = temp_cal + (temp_cal + 273.15) /vptat_cal * (vptat - vptat_cal);
        print_float(temprature);
    }
}

#if 0

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
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;

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
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_01_ANA_SEL = 0;
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_02_ANA_SEL = 0;

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

void GPADC_SingleChannel_Dma(void)
{
    /*********GPADC dma mode for simulation********/
    CLOGD("adc-dma single channel test begin");
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_01_ANA_SEL = 0;

    uint32_t state;
    GPADC_RESOURCES *tmphandle = GPADC();
    //initialize
    HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_0) | CSK_GPADC_DMA_ENABLE(0x10));

    //dma mode, transfer 11 times for dma transfer
    HAL_GPADC_SetTriggerNum(GPADC(), 0);
    HAL_GPADC_SetVrefSel(GPADC(), 0);

    //fifo clear
    HAL_GPADC_FifoClear(GPADC(), CSK_GPADC_CHANNEL_SEL_0, 1); //clear fifo data

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

#endif

static void SPI_DrvEvent (uint32_t event, uint32_t usr_param) {
	CLOGD("adc-dma single channel test begin");
}
#if 0
void GPADC_Channel_x_DmaCmpSpiTranferEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t res)
{
    GPIO_PinWrite(gGpioDev, CSK_GPIO_PIN10, 0);
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

	uint32_t control = DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
				   DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
				   DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
				   DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;

	 dma_channel_setup (channel_num,
		                 1, //en_int
					    control,
					    DMA_CH_CFGL_CH_PRIOR(1),
					    DMA_CH_CFGH_SRC_PER(12), // config_high
		                0,
		                0);

	 dma_channel_start(channel_num, (uint32_t)(&(tmphandle->reg->REG_DMA_RDR)), (uint32_t) adc_val1, 500);

    GPIO_PinWrite(gGpioDev, CSK_GPIO_PIN10, 1);
}
#endif
#if 1
void GPADC_Channel_x_DmaCmpSpiTranferEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t res)
{
    GPIO_PinWrite(gGpioDev, CSK_GPIO_PIN10, 0);
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
				DMA_CH_CFGH_SRC_PER(12), // config_high
				0, 0);
    GPIO_PinWrite(gGpioDev, CSK_GPIO_PIN10, 1);
}
void GPADC_Channel0_Dma_SPI(void)
{
	/*********GPADC dma mode for simulation********/
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_01.bit.PAD_AON_GPIOB_01_ANA_SEL = 0;


	GPADC_RESOURCES *tmphandle = GPADC();

    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_0 | CSK_GPADC_DMA_ENABLE(0x8)); //vin 0

	//dma mode, gpadc translate infinitely for debug
	HAL_GPADC_SetTriggerNum(GPADC(), 0);

	//dma handshake
	IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_12 = 0; //gpadc rx dma req

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
				DMA_CH_CFGH_SRC_PER(12), // config_high
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
#endif
#if 0
void GPADC_Channel0_Dma_SPI(void)
{
	uint32_t ret;
	/*********GPADC dma mode for simulation********/
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 4);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_01.bit.PAD_AON_GPIOB_01_ANA_SEL = 0;


	GPADC_RESOURCES *tmphandle = GPADC();

    //initialize
    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_0 | CSK_GPADC_DMA_ENABLE(0x8)); //vin 0

	//dma mode, gpadc translate infinitely for debug
	HAL_GPADC_SetTriggerNum(GPADC(), 0);

	//dma handshake
	IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_12 = 0; //gpadc rx dma req

	dma_initialize();

	channel_num = 3;
	//fix dma channel
	dma_channel_reserve(
			channel_num,
			GPADC_Channel_x_DmaCmpSpiTranferEvent,
			0,
			DMA_CACHE_SYNC_DST);

	uint32_t control = DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
			   DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
			   DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
			   DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;

	ret =  dma_channel_setup (channel_num,
	                           1, //en_int
							   control,
							   DMA_CH_CFGL_CH_PRIOR(1),
							   DMA_CH_CFGH_SRC_PER(12), // config_high
	                           0,
	                           0);
	if(ret != CSK_DRIVER_OK)
			CLOGD(" dma drv dma_channel_setup error!\n");

	ret = dma_channel_start(channel_num, (uint32_t)(&(tmphandle->reg->REG_DMA_RDR)), (uint32_t) adc_val1, 500);
	if(ret != CSK_DRIVER_OK)
		CLOGD(" dma drv dma_channel_start error!\n");

	//SPI0 config
    gGpioDev = GPIOA();
    gSpiDev = SPI0();

    //GPIO configuration
    GPIO_Initialize(gGpioDev, NULL, NULL);

    //io config for spi0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_MOSI_PIN, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CS_PIN, CSK_IOMUX_FUNC_ALTER5);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Control(gGpioDev, CSK_GPIO_MODE_PULL_NONE | \
                                CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN10);
    GPIO_SetDir(gGpioDev, CSK_GPIO_PIN10, CSK_GPIO_DIR_OUTPUT);

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
#endif
#define WAKEUP_SOURCE_TO_STRING(src)	(	\
		src == PMU_WAKEUP_NONE ? "None wakeup" : \
		src == PMU_WAKEUP_TIMER ? "TIMER wakeup" : \
		src == PMU_WAKEUP_IWDT ? "IWDT wakeup" : \
		src == PMU_WAKEUP_KEY ? "KEY wakeup" : \
		src == PMU_WAKEUP_RTC  ? "RTC wakeup" : \
		src == PMU_WAKEUP_GPIOB_00 ? "GPIOB_00 wakeup" : \
		src == PMU_WAKEUP_GPIOB_01 ? "GPIOB_01 wakeup" : \
		src == PMU_WAKEUP_GPIOB_02 ? "GPIOB_02 wakeup" : \
		src == PMU_WAKEUP_GPIOB_03 ? "GPIOB_03 wakeup" : \
		src == PMU_WAKEUP_GPIOB_04 ? "GPIOB_04 wakeup" : \
		src == PMU_WAKEUP_GPIOB_05 ? "GPIOB_05 wakeup" : \
		"Unknown wakeup")

static void GetWakeupEvent(uint32_t wakesrc) {
	CLOGD("%s\n", WAKEUP_SOURCE_TO_STRING(wakesrc));
}

void GPADC_Keysense0_Wakeup(void)
{
	pmu_wakeupsrc_t wakeup_cause = HAL_PMU_GetWakeUpCause();
	HAL_PMU_ClearWakeUpCause();

	CLOGD("[keysense]WakeUp cause -> %d", wakeup_cause);
	GetWakeupEvent(wakeup_cause);

	SysTick_Delay_Ms(500);

	if (wakeup_cause == PMU_WAKEUP_KEY){
		CLOGD("keysense0 wakeup enter and trigger gpadc.\r\n");
		HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
		//aon wake up irq clear key0_wakeup_icr
		//IP_AON_CTRL->REG_WAKEUP_ICR.bit.KEY0_WAKEUP_ICR = 1;
		HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_KEY);
	}

	//initialize
	HAL_KEYSENSE_Initialize(KEYSENSE0());

	//gpio config
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 4);
	IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.bit.PAD_AON_GPIOB_00_ANA_SEL = 0;

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
		CLOGD("channel adc value %d, adc value is %dmV", adc_value, (uint16_t)(adc_value*1000/1024*1.1*3));
	}

	HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_KEY);
	HAL_KEYSENSE_Enable(KEYSENSE0());
	//IP_KEYSENSE0->REG_KS_IMR.bit.KS_ADC_TRIG_IMR = 0; //unmask
	//CLOGD("[KEYSENSE enter] Can't Entry SleepMode!!!!!");
	//enter deep sleep
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);
}
