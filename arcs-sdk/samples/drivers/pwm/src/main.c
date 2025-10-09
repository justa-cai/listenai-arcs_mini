#include <stdio.h>
#include <stdint.h>

#include "IOMuxManager.h"
#include "Driver_GPT_PWM.h"

#include "FreeRTOS.h"
#include "task.h"

#define PWM_CH0_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH0_PIN           (20)
#define PWM_CH0_SEL           (12)

#define PWM_CH1_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH1_PIN           (21)
#define PWM_CH1_SEL           (12)

void GPT_PWM_Output(void)
{
    uint32_t ret;

    /* 设置PA20和PA21引脚为PWM输出, 具体IOMUX列表见芯片手册的APPENDIX章节 */
    IOMuxManager_PinConfigure(PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL);
    IOMuxManager_PinConfigure(PWM_CH1_PAD, PWM_CH1_PIN, PWM_CH1_SEL);

    /* 初始化GPT0_PWM */
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

    /* 使能GPT0_PWM */
	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK) {
		printf("line: %d, Error = %d\n", __LINE__, ret);
        goto error;
    }

    for (int ch = 0; ch < 2; ch++) {
        /* 配置PWM的ch通道的时钟源为PCLK，时钟分频，设置PWM输出模式， */
        ret = HAL_GPT_PWMControl(GPT0_PWM(), 
                        CSK_GPT_PWM_MODE | 
                        CSK_GPT_PWM_CLKSRC_PCLK | 
                        CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | 
                        CSK_GPT_PWM_OUTPOLARITY_LOW |
                        CSK_GPT_PWM_CLKDIV_1 | 
                        CSK_GPT_PWM_OPERATION_MODE_PWM, ch);
        if(ret != CSK_DRIVER_OK) {
            printf("line: %d, ch: %d, Error = %d\n", __LINE__, ch, ret);
            goto error;
        }

        /* 设置PWM的ch通道的频率和占空比 */
        ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), ch, (ch + 1) * 1000, 50 + ch * 20);
        if(ret != CSK_DRIVER_OK) {
            printf("line: %d, ch: %d, Error = %d\n", __LINE__, ch, ret);
            goto error;
        }
        
        /* 启动ch通道的PWM输出 */
        HAL_GPT_EnablePWM(GPT0_PWM(), ch);
    }

    return;

error:

    /* 关闭GPT0_PWM */
    HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_OFF);
    /* 反初始化GPT0_PWM */
    HAL_GPT_PWMUninitialize(GPT0_PWM());

    return;
}

int main(int argc, char **argv)
{
    printf("Hello, world! PWM\n");

    GPT_PWM_Output();

    return 0;
}
