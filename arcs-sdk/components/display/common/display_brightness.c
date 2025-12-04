#include "Driver_GPIO.h"
#include "Driver_GPT_PWM.h"
#include "IOMuxManager.h"
#include "lisa_display.h"
#include "lisa_log.h"


#if CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_WIRE_DIMMING

#define DISPLAY_BRIGHTNESS_STEPS 16

struct display_brightness_ctx {
	uint8_t level;
    uint8_t steps;
    uint8_t pin;
};

static struct display_brightness_ctx g_display_brightness_ctx;
#else
uint8_t disp_brightness_channel = 0;
uint32_t brightness_freq = 0;
#endif
void *backlight_dev = NULL;

void disp_comm_brightness_init(struct blacklight_config *config)
{
#ifdef CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_PWM
    backlight_dev = config->dev;
    brightness_freq = config->freq;
    disp_brightness_channel = config->channel;

    HAL_GPT_PWMInitialize(config->dev, NULL);
    HAL_GPT_PWMPowerControl(config->dev, CSK_POWER_FULL);
    HAL_GPT_PWMControl(config->dev, CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
                            CSK_GPT_PWM_CLKDIV_16 | CSK_GPT_PWM_OPERATION_MODE_PWM, disp_brightness_channel);
#elif CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_WIRE_DIMMING
    backlight_dev = config->pin.pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    GPIO_SetDir(backlight_dev, (1UL << config->pin.pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(backlight_dev, (1UL << config->pin.pin), 0);
    g_display_brightness_ctx.steps = DISPLAY_BRIGHTNESS_STEPS;
    g_display_brightness_ctx.pin = config->pin.pin;
#endif
}

void disp_comm_brightness_set(uint8_t value)
{
#ifdef CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_PWM
    /**
        * TODO: drv api --- duty should not be 0 or 100!!!
    */
    if(value == 0){
        HAL_GPT_DisablePWM(backlight_dev, disp_brightness_channel);
        return;
    }else if(value >= 100){
        value = 99;
    }

    HAL_GPT_SetPWMFreqDuty(backlight_dev, disp_brightness_channel, brightness_freq, value);
    HAL_GPT_EnablePWM(backlight_dev, disp_brightness_channel);
#elif CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_WIRE_DIMMING
    if(value > 100){
        value = 100;   
    }

    int level = value * g_display_brightness_ctx.steps / 100;
	if (level == g_display_brightness_ctx.level) {
		return;
	}

    LOGI("brightness set to %d", level);

    if(level == 0){
        // shut down
        GPIO_PinWrite(backlight_dev, (1UL << g_display_brightness_ctx.pin), 0);
        SysTick_Delay_Ms(3);
    }else{
        // 如果是shut down状态，需要先打开
        if(g_display_brightness_ctx.level == 0){
            g_display_brightness_ctx.level = g_display_brightness_ctx.steps;
            GPIO_PinWrite(backlight_dev, (1UL << g_display_brightness_ctx.pin), 1);
            SysTick_Delay_Us(30);
        }

        int pulses_start = g_display_brightness_ctx.steps - g_display_brightness_ctx.level;
        int pulses_end = g_display_brightness_ctx.steps - level;
        int pulses = (g_display_brightness_ctx.steps + pulses_end - pulses_start) % g_display_brightness_ctx.steps;

        for (int i = 0; i < pulses; i++) {
            GPIO_PinWrite(backlight_dev, (1UL << g_display_brightness_ctx.pin), 0);
            SysTick_Delay_Us(1);
            GPIO_PinWrite(backlight_dev, (1UL << g_display_brightness_ctx.pin), 1);
            SysTick_Delay_Us(1);
        }
    }

    g_display_brightness_ctx.level = level;
    
#endif

}