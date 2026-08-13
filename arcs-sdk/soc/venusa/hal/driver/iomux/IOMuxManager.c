/**
 * @file    IOMuxManager.c
 * @brief   IOMUX Driver Source File
 * @details This file contains the implementation of the IOMUX driver functions
 *          responsible for configuring pin multiplexers and electrical characteristics.
 *          Handles both core domain and AON (Always-On) domain pin control.
 *
 * @note    Direct hardware register access is performed through memory-mapped structures.
 *          All operations maintain proper sequencing requirements for safe register updates.
 */

/**
 * @brief IOMUX driver main entry point
 *        Centralized management of all pin multiplexer configurations
 */
#include "IOMuxManager.h"

#include "venusa_ap.h"

/**
 * @brief Configures individual pin multiplexer settings
 *        Maps specified physical pin to selected functional module
 *
 * @param[in] pad      Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num  Pin index within selected pad group (zero-based)
 * @param[in] pin_cfg  Function select code (lower 5 bits valid)
 * @retval CSK_DRIVER_OK Successfully configured
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @details Validates function code against maximum allowed value (CSK_IOMUX_FUNC_ALTER22).
 *          Supports three physical pad groups (A/B/C) with per-group pin count limits.
 *          Clears existing function selection before applying new configuration.
 *          Modifies corresponding core domain register based on pad selection.
 */
int32_t IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg) {
	volatile uint32_t* volatile pin_base = NULL;

	if(pin_cfg > CSK_IOMUX_FUNC_ALTER22) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_NORMAL);

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_C:
		if(pin_num > CSK_IOMUX_PAD_C_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOC_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_SDIO:
		if(pin_num > CSK_IOMUX_PAD_SDIO_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_SDIO_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_FLASHIO:
		if(pin_num > CSK_IOMUX_PAD_FLASHIO_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_FLASHIO_00.all) + pin_num;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	/* Clear function select field and set value */
	uint32_t value = *pin_base;
	value &= ~0x1f;
	value |= (pin_cfg & 0x1f);
	*pin_base = value;

	return CSK_DRIVER_OK;
}

/**
 * @brief Sets pin drive strength and pull resistor configuration
 *        Controls electrical characteristics of the pin driver circuitry
 *
 * @param[in] pad       Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num   Pin index within selected pad group (zero-based)
 * @param[in] pin_mode  Drive mode selection (@ref HAL_IOMUX_MODE_*)
 * @retval CSK_DRIVER_OK Successfully configured
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @details Available modes:
 *          - HAL_IOMUX_NONE_MODE: Weak driver (default)
 *          - HAL_IOMUX_PULLUP_MODE: Pullup resistor enabled
 *          - HAL_IOMUX_PULLDOWN_MODE: Pulldown resistor enabled
 *        Applies bitmask operations to modify driver characteristics while preserving
 *        other register fields. Different pad groups use identical register structures.
 */
int32_t IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode) {
	volatile uint32_t* volatile pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_NORMAL);

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_C:
		if(pin_num > CSK_IOMUX_PAD_C_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOC_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_SDIO:
		if(pin_num > CSK_IOMUX_PAD_SDIO_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_SDIO_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_FLASHIO:
		if(pin_num > CSK_IOMUX_PAD_FLASHIO_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_FLASHIO_00.all) + pin_num;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

    uint32_t value = *pin_base;

	switch(pin_mode) {
	case HAL_IOMUX_NONE_MODE:
	    value &= (~0x70000);
	    value |= 0x40000;
		break;

	case HAL_IOMUX_PULLUP_MODE:
	    value &= (~0x10000);
	    value |= 0x60000;
		break;

	case HAL_IOMUX_PULLDOWN_MODE:
	    value &= (~0x20000);
	    value |= 0x50000;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	*pin_base = value;

	return CSK_DRIVER_OK;
}

/**
 * @brief Advanced ON Die (AON) domain pin configuration
 *        Specialized configuration for always-on power domain pins
 *
 * @param[in] pad      Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num  Pin index within selected pad group (zero-based)
 * @param[in] pin_cfg  Function select code (lower 5 bits valid)
 * @retval CSK_DRIVER_OK Successfully configured
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @note Only PAD_B is supported in AON domain. Other pad groups return error.
 *       Uses separate AON-specific register bank from main power domain.
 *       Maintains power domain isolation requirements while allowing limited functionality.
 */
int32_t AON_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg) {
	volatile uint32_t* volatile aon_pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		// Un-support PAD_A
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		aon_pin_base = &(IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_C:
		// Un-support PAD_C
		return CSK_DRIVER_ERROR_PARAMETER;
		break;

	case CSK_IOMUX_PAD_SDIO:
		// Un-support PAD_SDIO
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_FLASHIO:
		// Un-support PAD_FLASH
		return CSK_DRIVER_ERROR_PARAMETER;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	/* Clear function select field and set value */
	uint32_t value = *aon_pin_base;
	value &= ~0x1f;
	value |= (pin_cfg & 0x1f);
	*aon_pin_base = value;

	return CSK_DRIVER_OK;
}

/**
 * @brief AON domain pin drive mode configuration
 *        Analogous to IOMuxManager_ModeConfigure but for AON domain
 *
 * @param[in] pad       Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num   Pin index within selected pad group (zero-based)
 * @param[in] pin_mode  Drive mode selection (@ref HAL_IOMUX_MODE_*)
 * @retval CSK_DRIVER_OK Successfully configured
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @note Shares same mode definitions as main domain but operates on AON registers.
 *       Maintains separate power domain isolation requirements.
 *       Currently only supports PAD_B in AON domain.
 */
int32_t AON_IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode) {
	volatile uint32_t* volatile aon_pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		// Un-support PAD_A
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		aon_pin_base = &(IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_C:
		// Un-support PAD_C
		return CSK_DRIVER_ERROR_PARAMETER;
	
	case CSK_IOMUX_PAD_SDIO:
		// Un-support PAD_SDIO
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_FLASHIO:
		// Un-support PAD_FLASH
		return CSK_DRIVER_ERROR_PARAMETER;


	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	uint32_t value = *aon_pin_base;

	switch(pin_mode) {
	case HAL_IOMUX_NONE_MODE:
	    value &= (~0x70000);
	    value |= 0x40000;
		break;

	case HAL_IOMUX_PULLUP_MODE:
	    value &= (~0x10000);
	    value |= 0x60000;
		break;

	case HAL_IOMUX_PULLDOWN_MODE:
	    value &= (~0x20000);
	    value |= 0x50000;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	*aon_pin_base = value;

	return CSK_DRIVER_OK;
}

/**
 * @brief Analog frontend pin configuration
 *        Specialized setup for analog peripheral connections
 *
 * @param[in] pad      Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num  Pin index within selected pad group (zero-based)
 * @param[in] pin_cfg  Analog function select code
 * @retval CSK_DRIVER_OK Successfully configured
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @note Primarily supports PAD_B and C with analog connectivity requirements.
 *       Performs additional AON domain configuration before applying settings.
 *       Uses different bitfields (bits 5-9) compared to digital function selection.
 */
int32_t ANA_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg) {
	volatile uint32_t* volatile pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		// Un-support PAD_A
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_ANA);

		pin_base = &(IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_C:
		if(pin_num > CSK_IOMUX_PAD_C_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOC_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_SDIO:
		// Un-support PAD_SDIO
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_FLASHIO:
		// Un-support PAD_FLASH
		return CSK_DRIVER_ERROR_PARAMETER;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	/* Clear function select field and set value */
	uint32_t value = *pin_base;
	value &= (~(0x0f<<5));
	value |= ((pin_cfg<<5) & (0x0f<<5));
	*pin_base = value;

	return CSK_DRIVER_OK;
}

/**
 * @brief Forces pin to specified logic level independently of functional state
 *        Overdrive capability for test/debug purposes
 *
 * @param[in] pad       Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num   Pin index within selected pad group (zero-based)
 * @param[in] data      Forced output value (@ref HAL_IOMUX_FORCE_*)
 * @retval CSK_DRIVER_OK Successfully forced
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @details Force options:
 *          - HAL_IOMUX_FORCE_OFF: Release forced state
 *          - HAL_IOMUX_FORCE_OUT_LOW: Drive low continuously
 *          - HAL_IOMUX_FORCE_OUT_HIGH: Drive high continuously
 *        Overrides normal functional behavior when active. Only available in AON domain.
 *        Uses dedicated force control bits (positioned at bit 23) with enable flag.
 */
int32_t AON_IOMuxManager_PinForce(uint8_t pad, uint8_t pin_num, uint8_t data) {
	volatile uint32_t * volatile aon_pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		// Un-support PAD_A
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, pin_num, CSK_AON_GPIO_OUT_FUNC);

		aon_pin_base = &(IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_C:
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_SDIO:
		// Un-support PAD_SDIO
		return CSK_DRIVER_ERROR_PARAMETER;

	case CSK_IOMUX_PAD_FLASHIO:
		// Un-support PAD_FLASH
		return CSK_DRIVER_ERROR_PARAMETER;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	uint32_t value = *aon_pin_base;

	switch(data) {
	case HAL_IOMUX_FORCE_OFF:
	    value &= (~0x1E00000);
		break;

	case HAL_IOMUX_FORCE_OUT_LOW:
		// clear force bit and value
	    value &= (~0x1E00000);
		// enable force control
	    value |= (0x400000);
		// open out force
	    value |= (0x1000000);
        // Set value
	    value |= (data << 23);
	    break;

	case HAL_IOMUX_FORCE_OUT_HIGH:
		// clear force bit and value
	    value &= (~0x1E00000);
		// enable force control
	    value |= (0x400000);
		// open out force
	    value |= (0x1000000);
        // Set value
	    value |= (data << 23);
	    break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	*aon_pin_base = value;

	return CSK_DRIVER_OK;
}

/**
 * @brief General purpose pin forcing mechanism
 *        Cross-domain compatible version of AON-specific forcing
 *
 * @param[in] pad       Physical pad group selector (@ref CSK_IOMUX_PAD_*)
 * @param[in] pin_num   Pin index within selected pad group (zero-based)
 * @param[in] data      Forced output value (@ref HAL_IOMUX_FORCE_*)
 * @retval CSK_DRIVER_OK Successfully forced
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter combination
 *
 * @note Maintains backward compatibility with legacy forcing mechanisms while
 *       providing consistent behavior across all pad groups. Uses same force control
 *       methodology as AON version but applies to core domain registers.
 */
int32_t IOMuxManager_PinForce(uint8_t pad, uint8_t pin_num, uint8_t data) {
	volatile uint32_t * volatile pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_NORMAL);

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
		break;
	case CSK_IOMUX_PAD_C:
		if(pin_num > CSK_IOMUX_PAD_C_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_GPIOC_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_SDIO:
		if(pin_num > CSK_IOMUX_PAD_SDIO_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_SDIO_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_FLASHIO:
		if(pin_num > CSK_IOMUX_PAD_FLASHIO_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CORE_IOMUX->REG_PAD_FLASHIO_00.all) + pin_num;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	uint32_t value = *pin_base;

	switch(data) {
	case HAL_IOMUX_FORCE_OFF:
		// clear force bit and value
	    value &= (~0x1E00000);
		break;

	case HAL_IOMUX_FORCE_OUT_LOW:
		// clear force bit and value
	    value &= (~0x1E00000);
		// enable force control
	    value |= (0x400000);
		// open out force
	    value |= (0x1000000);
        // Set value
	    value |= (data << 23);
	    break;

	case HAL_IOMUX_FORCE_OUT_HIGH:
		// clear force bit and value
	    value &= (~0x1E00000);
		// enable force control
	    value |= (0x400000);
		// open out force
	    value |= (0x1000000);
        // Set value
	    value |= (data << 23);
	    break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	*pin_base = value;

	return CSK_DRIVER_OK;
}
