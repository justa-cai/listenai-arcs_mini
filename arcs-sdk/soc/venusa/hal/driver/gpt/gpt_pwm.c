/**
 * @file gpt_pwm.c
 * @brief General Purpose Timer (GPT) PWM Functionality Implementation
 *        This file contains the core implementation of Pulse Width Modulation (PWM)
 *        functionality built upon the GPT peripheral driver. It provides low-level APIs
 *        for configuring and controlling PWM signals across multiple channels and output ports.
 *
 * @details
 *   Implements essential PWM operations including:
 *     - Channel enable/disable with port masking
 *     - Duty cycle configuration per port
 *     - Phase delay adjustments
 *     - Frequency control via reload registers
 *     - Polarity settings (initial and running states)
 *     - Event callback registration
 *     - Comprehensive channel state management
 *
 *   The implementation follows a shadow load mechanism to ensure glitch-free updates
 *   during live operation. All configuration changes are double-buffered until explicitly
 *   applied through the shadow load register.
 *
 * @author USER
 * @date Created on May 12, 2025
 * @note This implementation assumes exclusive access to GPT hardware resources
 *       when performing configuration changes. Interrupt handling should be disabled
 *       during critical sections where register modifications occur.
 */


#include "gpt_pwm.h"


/** Global PWM information structure instance */
GPT_PWM_Info_t __pwm_info = {
	0,
};

/**
 * @brief Disable PWM output for specified channel and port
 *
 * This function disables the PWM output capability for a specific channel and port combination.
 * It clears the corresponding enable bit in the channel control register and disables the channel.
 *
 * @param[in] res         Pointer to GPT resource structure
 * @param[in] channel     Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] port_mask   Bitmap indicating which ports to disable (bit positions correspond to physical ports)
 *
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t HAL_GPT_DisablePWM(void *res, GPT_Channel_Num_t channel, GPT_PWM_PortBitMask_t port_mask) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Calculate address of channel control register with offset
    volatile uint32_t *gpt_ch_ctrl = (uint32_t*)&gpt->reg->REG_CH0_CTRL.all + channel;
    volatile uint32_t gpt_ch_ctrl_data = *gpt_ch_ctrl;

    // Clear PWM enable bits for specified ports
    gpt_ch_ctrl_data &= ~(port_mask << GPT_CH0_CTRL_CH0_PWM_OUT_EN_Pos);
    *gpt_ch_ctrl = gpt_ch_ctrl_data;

    if(gpt_ch_ctrl_data & GPT_CH0_CTRL_CH0_PWM_OUT_EN_Msk) {
        // If any PWM port is still enabled, just return
    } else {
        // Fully disable the channel after removing PWM functionality
        HAL_GPT_DisableChannel(res, channel);
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Enable PWM output for specified channel and port
 *
 * This function enables PWM output capability for a specific channel and port combination.
 * It sets the corresponding enable bit in the channel control register and configures
 * interrupt masking before enabling the channel and starting PWM operation.
 *
 * @param[in] res         Pointer to GPT resource structure
 * @param[in] channel     Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] port_mask   Bitmap indicating which ports to enable (bit positions correspond to physical ports)
 *
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t HAL_GPT_EnablePWM(void *res, GPT_Channel_Num_t channel, GPT_PWM_PortBitMask_t port_mask) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Get direct pointer to channel control register
    volatile uint32_t *gpt_ch_ctrl = &gpt->reg->REG_CH0_CTRL.all + channel;

    // Set PWM enable bits for specified ports
    *gpt_ch_ctrl |= port_mask << GPT_CH0_CTRL_CH0_PWM_OUT_EN_Pos;

    // Disable cycle complete interrupt for this channel during PWM setup
    gpt->reg->REG_IMR_ISR_GPT.bit.PWM_CYCLE_INT_MASK &= ~(1U << channel);

    // Enable the channel after configuring PWM settings
    HAL_GPT_EnableChannel(res, channel);

    // Start PWM output generation
    gpt->reg->REG_CH_CNT_CTRL.bit.TIMER1_PWM_EN |= 1U << channel;

    return CSK_DRIVER_OK;
}

/**
 * @brief Set phase delay for PWM signal on specified port
 *
 * Configures the phase delay value for a specific PWM output port on a given channel.
 * Supports multiple independent phase delays per channel across different output ports.
 *
 * @param[in] res        Pointer to GPT resource structure
 * @param[in] channel    Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] port       Output port selection (GPT_PWM_PORT_0 to GPT_PWM_PORT_3)
 * @param[in] dly        Phase delay value in timer counts
 *
 * @return CSK_DRIVER_OK on success, error code if parameters are invalid
 */
int32_t HAL_GPT_SetPWMDelayPhase(void* res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, uint16_t dly) {
    CHECK_RESOURCES(res);
    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Validate input parameters
    if (channel > GPT_CHANNEL_1 || port > GPT_PWM_PORT_3) {
        return CSK_DRIVER_ERROR;
    }

    // Select appropriate configuration register based on channel and port
    switch (channel) {
    case GPT_CHANNEL_0:
        switch (port) {
            case GPT_PWM_PORT_0:
                gpt->reg->REG_CH0_PWM_CFG3.bit.CH0_PWM0_PH_DLY = dly;
                break;
            case GPT_PWM_PORT_1:
                gpt->reg->REG_CH0_PWM_CFG3.bit.CH0_PWM1_PH_DLY = dly;
                break;
            case GPT_PWM_PORT_2:
                gpt->reg->REG_CH0_PWM_CFG4.bit.CH0_PWM2_PH_DLY = dly;
                break;
            case GPT_PWM_PORT_3:
                gpt->reg->REG_CH0_PWM_CFG4.bit.CH0_PWM3_PH_DLY = dly;
                break;
            default:
                return CSK_DRIVER_ERROR;
        }
        break;

    case GPT_CHANNEL_1:
        switch (port) {
            case GPT_PWM_PORT_0:
                gpt->reg->REG_CH1_PWM_CFG3.bit.CH1_PWM0_PH_DLY = dly;
                break;
            case GPT_PWM_PORT_1:
                gpt->reg->REG_CH1_PWM_CFG3.bit.CH1_PWM1_PH_DLY = dly;
                break;
            case GPT_PWM_PORT_2:
                gpt->reg->REG_CH1_PWM_CFG4.bit.CH1_PWM2_PH_DLY = dly;
                break;
            case GPT_PWM_PORT_3:
                gpt->reg->REG_CH1_PWM_CFG4.bit.CH1_PWM3_PH_DLY = dly;
                break;
            default:
                return CSK_DRIVER_ERROR;
        }
        break;

    default:
        return CSK_DRIVER_ERROR;
    }

    // Apply changes using shadow load mechanism
    gpt->reg->REG_SHADOW_LOAD.all |= 0x1 << channel;

    return CSK_DRIVER_OK;
}

/**
 * @brief Set duty cycle for PWM signal on specified port
 *
 * Configures the high time duration (duty cycle) for a specific PWM output port on a given channel.
 * Allows independent duty cycle control across multiple output ports per channel.
 *
 * @param[in] res        Pointer to GPT resource structure
 * @param[in] channel    Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] port       Output port selection (GPT_PWM_PORT_0 to GPT_PWM_PORT_3)
 * @param[in] h_duty     High time duration in timer counts
 *
 * @return CSK_DRIVER_OK on success, parameter error if inputs are invalid
 */
int32_t HAL_GPT_SetPWMDuty(void *res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, uint16_t h_duty) {
    CHECK_RESOURCES(res);
    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    switch (channel) {
        case GPT_CHANNEL_0:
            switch (port) {
                case GPT_PWM_PORT_0:
                    gpt->reg->REG_CH0_PWM_CFG0.bit.CH0_PWM0_DUTY_HIGH = h_duty;
                    break;
                case GPT_PWM_PORT_1:
                    gpt->reg->REG_CH0_PWM_CFG0.bit.CH0_PWM1_DUTY_HIGH = h_duty;
                    break;
                case GPT_PWM_PORT_2:
                    gpt->reg->REG_CH0_PWM_CFG1.bit.CH0_PWM2_DUTY_HIGH = h_duty;
                    break;
                case GPT_PWM_PORT_3:
                    gpt->reg->REG_CH0_PWM_CFG1.bit.CH0_PWM3_DUTY_HIGH = h_duty;
                    break;
                default:
                    return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        case GPT_CHANNEL_1:
            switch (port) {
                case GPT_PWM_PORT_0:
                    gpt->reg->REG_CH1_PWM_CFG0.bit.CH1_PWM0_DUTY_HIGH = h_duty;
                    break;
                case GPT_PWM_PORT_1:
                    gpt->reg->REG_CH1_PWM_CFG0.bit.CH1_PWM1_DUTY_HIGH = h_duty;
                    break;
                case GPT_PWM_PORT_2:
                    gpt->reg->REG_CH1_PWM_CFG1.bit.CH1_PWM2_DUTY_HIGH = h_duty;
                    break;
                case GPT_PWM_PORT_3:
                    gpt->reg->REG_CH1_PWM_CFG1.bit.CH1_PWM3_DUTY_HIGH = h_duty;
                    break;
                default:
                    return CSK_DRIVER_ERROR_PARAMETER;
            }
            break;

        default:
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Apply changes using shadow load mechanism
    gpt->reg->REG_SHADOW_LOAD.all |= 0x1 << channel;

    return CSK_DRIVER_OK;
}

/**
 * @brief Comprehensive PWM configuration for specified channel and port
 *
 * Sets various PWM characteristics including initial polarity and output polarity.
 * Must be called before enabling PWM output on the channel.
 *
 * @param[in] res        Pointer to GPT resource structure
 * @param[in] channel    Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] port       Output port selection (GPT_PWM_PORT_0 to GPT_PWM_PORT_3)
 * @param[in] para       Pointer to GPT_PWM_Config_t containing configuration parameters
 *
 * @return CSK_DRIVER_OK on success, error code if parameters are invalid
 */
int32_t HAL_GPT_PWMControl(void *res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, GPT_PWM_Config_t* para) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Ensure channel is disabled while reconfiguring
    HAL_GPT_DisableChannel(res, channel);

    // Access channel control register with proper offset
    volatile uint32_t *gpt_pwm_ch_ctrl = &(gpt->reg->REG_CH0_CTRL.all) + channel;
    volatile uint32_t pwm_ch_ctrl = *gpt_pwm_ch_ctrl;

    // Configure initial polarity (startup state)
    pwm_ch_ctrl &= ~((0x1 << port) << GPT_CH0_CTRL_CH0_PWM_POLARITY_INIT_Pos);
    pwm_ch_ctrl |= ((para->init_level << port) << GPT_CH0_CTRL_CH0_PWM_POLARITY_INIT_Pos);

    // Configure running polarity (active state)
    pwm_ch_ctrl &= ~((0x1 << port) << GPT_CH0_CTRL_CH0_PWM_POLARITY_Pos);
    pwm_ch_ctrl |= ((para->output_polarity << port) << GPT_CH0_CTRL_CH0_PWM_POLARITY_Pos);

    // Write updated control register value
    *gpt_pwm_ch_ctrl = pwm_ch_ctrl;

    // Register channel as being used by PWM system
    HAL_GPT_RegisterChannel(res, channel, HARDWARE_CHANNEL_STAT_USED_BY_PWM);

    return CSK_DRIVER_OK;
}

/**
 * @brief Set PWM frequency for specified channel
 *
 * Updates the reload value to change the PWM signal frequency. Uses shadow loading
 * to ensure smooth transition without glitches.
 *
 * @param[in] res     Pointer to GPT resource structure
 * @param[in] channel Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] freq    Desired PWM frequency in timer counts
 *
 * @return CSK_DRIVER_OK on success, error code if parameters are invalid
 */
int32_t HAL_GPT_SetPWMFrequence(void* res, GPT_Channel_Num_t channel, uint16_t freq) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Direct pointer to channel reload register with offset
    volatile uint32_t *gpt_ch_reload = &gpt->reg->REG_CH0_RELOAD.all + channel;

    // Update reload value to change PWM period/frequency
    *gpt_ch_reload = freq;

    // Force immediate update using shadow load mechanism
    gpt->reg->REG_SHADOW_LOAD.all |= 0x1 << (channel);

    return CSK_DRIVER_OK;
}

/**
 * @brief Register PWM event callback function
 *
 * Associates an event callback function with a specific PWM channel. The callback
 * will be triggered when PWM events occur on the specified channel.
 *
 * @param[in] res        Pointer to GPT resource structure
 * @param[in] channel    Channel number (GPT_CHANNEL_0 or GPT_CHANNEL_1)
 * @param[in] cb_event   Event callback function pointer
 * @param[in] workspace  User-defined data pointer passed to callback
 *
 * @return CSK_DRIVER_OK on success, error code if parameters are invalid
 */
int32_t HAL_GPT_RegsterPWMCallback(void* res, GPT_Channel_Num_t channel, CSK_GPT_SignalEvent_t cb_event, void *workspace) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Store callback function pointer for this channel
    gpt->info->cb_event[channel] = cb_event;
    // Store user workspace pointer associated with this channel
    gpt->pwm_info->workspace = workspace;

    return CSK_DRIVER_OK;
}
