#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "IOMuxManager.h"
#include "arcs_ap.h"

#include "Driver_GPADC.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
#endif

#define GPADC_VIN0_PIN_NUM      2       // GPIOB_02
#define GPADC_VIN1_PIN_NUM      3       // GPIOB_03
#define GPADC_VIN2_PIN_NUM      4       // GPIOB_04
#define GPADC_VIN3_PIN_NUM      5       // GPIOB_05
#define GPADC_VIN4_PIN_NUM      6       // GPIOB_06
#define GPADC_VIN5_PIN_NUM      7       // GPIOB_07

static void gpadc_test(void)
{
#ifdef CONFIG_BOARD_ARCS_MINI
    /* ARCS_MINI: 使用 BAT_ADC_PIN (PB5) = VIN3 + VBAT 内部通道 */
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN3_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    HAL_GPADC_Initialize(GPADC());

    HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_3 |
                                CSK_GPADC_CHANNEL_SEL_VBAT |
                                CSK_GPADC_DMA_ENABLE(0));
#else
    /* 配置PB4、PB6、PB7作为GPADC的输入引脚，具体AONMUX列表和ANAMUX列表见芯片手册的APPENDIX章节 */
    /* AON_MUX设置PB4为ANA引脚 */
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN2_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    /* ANA_MUX设置PB4为GPADC输入 */
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    /* 设置PB6为GPADC输入引脚 */
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN4_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    /* 设置PB7为GPADC输入引脚 */
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPADC_VIN5_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    /* 初始化GPADC */
    HAL_GPADC_Initialize(GPADC());

    /* 设置ADC的采样通道VIN2(PB4)、VIN4(PB6)、VIN4(PB7)和VBAT(测量本芯片内部电压) */
    HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_2 | 
                                CSK_GPADC_CHANNEL_SEL_4 | 
                                CSK_GPADC_CHANNEL_SEL_5 | 
                                CSK_GPADC_CHANNEL_SEL_VBAT | 
                                CSK_GPADC_DMA_ENABLE(0));
#endif

    /*
     * ref=0，表示参考电压为Vbg, 1.2V
     * ref = 1，也表示参考电压为VDD_VA, 1.2V
     * ref = 2, 表示参考电压为VDD_IO / 2 或者 VDD_IO / 3
     * ref = 3，表示参考电压为Vref_ext, 即外部参考电压
    */
    HAL_GPADC_SetVrefSel(GPADC(), 0);

    /* 如果为1,则实际的采样值是adc采样值的3倍 */
    HAL_GPADC_SetVinBuf_Enable(GPADC(), 1);

    uint32_t adc_value;
    for(;;){
        printf("start adc ..............................................\n");

        /* 启动GPADC采样 */
		HAL_GPADC_Start(GPADC());
        /* 等待采样完成 */
		HAL_GPADC_PollForConversion(GPADC(), 0);

        /* 获取GPADC采样结果 */
#ifdef CONFIG_BOARD_ARCS_MINI
        /* 获取VIN3(PB5/BAT_ADC)的采样结果 */
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_3);
		printf("VIN3(BAT_ADC) adc value 0x%lx/%dmV\n", adc_value, (uint16_t)(adc_value*1000.0/1024*1.2*3));
#else
        /* 获取VIN2的采样结果 */
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_2);
		printf("channel type id %d, adc value 0x%lx/%dmV\n", CSK_GPADC_CHANNEL2, adc_value, (uint16_t)(adc_value*1000.0/1024*1.2*3));

        /* 获取VIN4的采样结果 */
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_4);
		printf("channel type id %d, adc value 0x%lx/%dmV\n", CSK_GPADC_CHANNEL4, adc_value, (uint16_t)(adc_value*1000.0/1024*1.2*3));

        /* 获取VIN5的采样结果 */
		adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_5);
		printf("channel type id %d, adc value 0x%lx/%dmV\n", CSK_GPADC_CHANNEL5, adc_value, (uint16_t)(adc_value*1000.0/1024*1.2*3));
#endif

        /* 获取芯片内部电压的采样结果 */
        adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_VBAT);
		printf("VBAT adc value 0x%lx/%dmV\n", adc_value, (uint16_t)(adc_value*1000.0/1024*1.2*3));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(int argc, char **argv)
{
    printf("Hello, world! GPADC\n");

    gpadc_test();

    return 0;
}