#include <stdio.h>
#include <stdlib.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"

#include "Driver_GPT_TIMER.h"
#include "Driver_GPT_Common.h"
#include "Driver_GPT_PWM.h"
#include "IOMuxManager.h"

#define PWM_MAX_DUTY		1000
#define PWM_STEP_DUTY		100

volatile uint16_t glb_pwm_h_duty = 0;

static GPT_Config_Para_t gpt_config_para = {
	.clk_src = GPT_CLK_SRC_T0,
	.prediv = 2,
	.clk_div = GPT_CLK_DIV_4,
};

/*
 * 16_bit PWM周期计算示例：
 *
 *   clk_src = 12_000_000 Hz
 *   pre_div = 2
 *   div     = 4
 *   CNT     = 1000
 *
 *   F_timer = clk_src / ((pre_div + 1) * div)
 *           = 12_000_000 / (3 * 4)
 *           = 12_000_000 / 12
 *           = 1MHz
 *
 *        T  = (1us)
 *
 *   Timer period (T) = CNT / F_timer
 *                    = 1000 / 1MHz
 *                    = 1ms
 */

static void gpt_test_pwm_duty_ramp(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_0 Function

	uint16_t duty = 0;

	GPT_PWM_Config_t gpt_pwm_config = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), GPT_CHANNEL_0, &gpt_config_para);

	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, &gpt_pwm_config);

	HAL_GPT_RegsterPWMCallback(GPT0(), GPT_CHANNEL_0, NULL, NULL);

	HAL_GPT_SetPWMFrequence(GPT0(), GPT_CHANNEL_0, PWM_MAX_DUTY);

	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, duty);

	HAL_GPT_EnablePWM(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0_MASK);

	for(uint32_t i = 0; i < 10; i++) {
        SysTick_Delay_Ms(1000);

        duty += PWM_STEP_DUTY;
        if (duty > PWM_MAX_DUTY) duty = PWM_STEP_DUTY;

        HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, duty);
        CLOGD("PWM High duty ----> %d (%.1f%%)", duty, (duty / 10.0));
    }

	HAL_GPT_DisablePWM(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0_MASK);
}

static void gpt_test_multiple_channels_diff_duty(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_0 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_1 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_2 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_3 Function

	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_4 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_5 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_6 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_7 Function

	uint16_t duty_port_0 = 100;
	uint16_t duty_port_1 = 200;
	uint16_t duty_port_2 = 300;
	uint16_t duty_port_3 = 400;

	uint16_t duty_port_4 = 500 * 2;
	uint16_t duty_port_5 = 600 * 2;
	uint16_t duty_port_6 = 700 * 2;
	uint16_t duty_port_7 = 800 * 2;

	GPT_PWM_Config_t gpt_pwm_config_port_0 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_1 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_2 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_3 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_4 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_5 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_6 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_7 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), GPT_CHANNEL_0, &gpt_config_para);
	HAL_GPT_Control(GPT0(), GPT_CHANNEL_1, &gpt_config_para);

	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, &gpt_pwm_config_port_0);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_1, &gpt_pwm_config_port_1);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_2, &gpt_pwm_config_port_2);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_3, &gpt_pwm_config_port_3);

	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0, &gpt_pwm_config_port_4);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_1, &gpt_pwm_config_port_5);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_2, &gpt_pwm_config_port_6);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_3, &gpt_pwm_config_port_7);

	HAL_GPT_SetPWMFrequence(GPT0(), GPT_CHANNEL_0, PWM_MAX_DUTY);		// T = 1ms
	HAL_GPT_SetPWMFrequence(GPT0(), GPT_CHANNEL_1, PWM_MAX_DUTY * 2);	// T = 2ms

	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, duty_port_0);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_1, duty_port_1);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_2, duty_port_2);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_3, duty_port_3);

	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0, duty_port_4);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_1, duty_port_5);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_2, duty_port_6);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_3, duty_port_7);

	HAL_GPT_EnablePWM(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);
	HAL_GPT_EnablePWM(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);

	SysTick_Delay_Ms(1000);

	HAL_GPT_DisablePWM(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);
	HAL_GPT_DisablePWM(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);
}

static void gpt_test_multiple_channels_diff_duty_diff_dly(void)
{
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_0 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_1 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_2 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_3 Function

	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_4 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_5 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_6 Function
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER11);  //PWM Port_7 Function

	uint16_t duty_port_0 = 800;
	uint16_t duty_port_1 = 800;
	uint16_t duty_port_2 = 700;
	uint16_t duty_port_3 = 700;

	uint16_t duty_port_4 = 600;
	uint16_t duty_port_5 = 600;
	uint16_t duty_port_6 = 500;
	uint16_t duty_port_7 = 500;

	uint16_t ph_dly_port_0 = 0;
	uint16_t ph_dly_port_1 = 200;
	uint16_t ph_dly_port_2 = 0;
	uint16_t ph_dly_port_3 = 200;

	uint16_t ph_dly_port_4 = 0;
	uint16_t ph_dly_port_5 = 200;
	uint16_t ph_dly_port_6 = 0;
	uint16_t ph_dly_port_7 = 200;

	GPT_PWM_Config_t gpt_pwm_config_port_0 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_1 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_2 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_3 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_HIGH,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_4 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_5 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_LOW,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_6 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	GPT_PWM_Config_t gpt_pwm_config_port_7 = {
		.init_level 		= GPT_PWM_INIT_LEVEL_HIGH,
		.output_polarity 	= GPT_PWM_OUTPUT_ACTIVE_LOW,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), GPT_CHANNEL_0, &gpt_config_para);
	HAL_GPT_Control(GPT0(), GPT_CHANNEL_1, &gpt_config_para);

	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, &gpt_pwm_config_port_0);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_1, &gpt_pwm_config_port_1);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_2, &gpt_pwm_config_port_2);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_3, &gpt_pwm_config_port_3);

	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0, &gpt_pwm_config_port_4);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_1, &gpt_pwm_config_port_5);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_2, &gpt_pwm_config_port_6);
	HAL_GPT_PWMControl(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_3, &gpt_pwm_config_port_7);

	SysTick_Delay_Ms(500);

	HAL_GPT_SetPWMFrequence(GPT0(), GPT_CHANNEL_0, PWM_MAX_DUTY);		// T = 1ms
	HAL_GPT_SetPWMFrequence(GPT0(), GPT_CHANNEL_1, PWM_MAX_DUTY);	    // T = 1ms

	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, duty_port_0);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_1, duty_port_1);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_2, duty_port_2);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_3, duty_port_3);

	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0, duty_port_4);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_1, duty_port_5);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_2, duty_port_6);
	HAL_GPT_SetPWMDuty(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_3, duty_port_7);

	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0, ph_dly_port_0);
	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_1, ph_dly_port_1);
	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_2, ph_dly_port_2);
	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_3, ph_dly_port_3);

	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0, ph_dly_port_4);
	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_1, ph_dly_port_5);
	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_2, ph_dly_port_6);
	HAL_GPT_SetPWMDelayPhase(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_3, ph_dly_port_7);

	HAL_GPT_EnablePWM(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);
	HAL_GPT_EnablePWM(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);

	SysTick_Delay_Ms(1000);

	HAL_GPT_DisablePWM(GPT0(), GPT_CHANNEL_0, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);
	HAL_GPT_DisablePWM(GPT0(), GPT_CHANNEL_1, GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK);
}

typedef struct 
{
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
	// {gpt_test_pwm_duty_ramp, "gpt_test_pwm_duty_ramp"},
	// {gpt_test_multiple_channels_diff_duty, "gpt_test_multiple_channels_diff_duty"},
	{gpt_test_multiple_channels_diff_duty_diff_dly, "gpt_test_multiple_channels_diff_duty_diff_dly"},
};

int main(void) {
	logInit(0, 115200);
	CLOGD("enter main: gpt pwm test\r\n");
	uint32_t index;

	for (index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++)
	{
		test_case_t item = test_array[index];
		CLOGD("test case: %s", item.name);
    	item.function();
	}

	while (1);

	return 0;
}
