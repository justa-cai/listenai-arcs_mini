/**
 * @brief IOMUX driver
 * 
 */
#include "IOMuxManager.h"

#include "arcs_ap.h"

int32_t IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg) {
	volatile uint32_t* volatile pin_base = NULL;

	if(pin_cfg > CSK_IOMUX_FUNC_ALTER31) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		//PA28 ~ PA31 generally act as Audio CODEC MIC pins, 1.8V and therefore is powered by LDO_VA
		if (pin_num >= 28 && pin_num <= 31) {
		    //if (IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA == 0)
		    //    IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 1;
		    uint32_t reg_val = IP_AON_CTRL->REG_AON_TUNE0.all;
		    if ((reg_val & AON_CTRL_AON_TUNE0_EN_LDO_VA_Msk) == 0)
		        IP_AON_CTRL->REG_AON_TUNE0.all = reg_val | AON_CTRL_AON_TUNE0_EN_LDO_VA_Msk;
		}

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		break;


	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_NORMAL);

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
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

int32_t IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode) {
	volatile uint32_t* volatile pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_NORMAL);

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
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

int32_t ANA_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg) {
	volatile uint32_t* volatile pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		if(pin_num >26) {
			pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		}
		break;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_ANA);

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
		break;

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

int32_t IOMuxManager_PinForce(uint8_t pad, uint8_t pin_num, uint8_t data) {
	volatile uint32_t * volatile pin_base = NULL;

	switch(pad) {
	case CSK_IOMUX_PAD_A:
		if(pin_num > CSK_IOMUX_PAD_A_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOA_00.all) + pin_num;
		break;

	case CSK_IOMUX_PAD_B:
		if(pin_num > CSK_IOMUX_PAD_B_MAX_PIN) {
			return CSK_DRIVER_ERROR_PARAMETER;
		}

		AON_IOMuxManager_PinConfigure(pad, pin_num, CSK_AON_IOMUX_FUNC_NORMAL);

		pin_base = &(IP_CMN_IOMUX->REG_PAD_GPIOB_00.all) + pin_num;
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

		AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, pin_num, CSK_AON_IOMUX_FUNC_GPIO_OUT);

		aon_pin_base = &(IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all) + pin_num;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	uint32_t value = *aon_pin_base;

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

	*aon_pin_base = value;

	return CSK_DRIVER_OK;
}
