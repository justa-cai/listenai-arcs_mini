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
	*pin_base &= ~0x1f;
	*pin_base |= (pin_cfg & 0x1f);
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

	switch(pin_mode) {
	case HAL_IOMUX_NONE_MODE:
		*pin_base &= (~0x70000);
		break;

	case HAL_IOMUX_PULLUP_MODE:
		*pin_base &= (~0x10000);
		*pin_base |= 0x60000;
		break;

	case HAL_IOMUX_PULLDOWN_MODE:
		*pin_base &= (~0x20000);
		*pin_base |= 0x50000;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	return CSK_DRIVER_OK;
}

int32_t AON_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg) {
	volatile uint32_t* volatile aon_pin_base = NULL;
	volatile uint32_t  value;

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

	value = *aon_pin_base;
	/* Clear function select field and set value */
	value &= ~0x1f;
	*aon_pin_base = value | (pin_cfg & 0x1f);
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

	switch(pin_mode) {
	case HAL_IOMUX_NONE_MODE:
		*aon_pin_base &= (~0x70000);
		break;

	case HAL_IOMUX_PULLUP_MODE:
		*aon_pin_base &= (~0x10000);
		*aon_pin_base |= 0x60000;
		break;

	case HAL_IOMUX_PULLDOWN_MODE:
		*aon_pin_base &= (~0x20000);
		*aon_pin_base |= 0x50000;
		break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}

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
	*pin_base &= (~(0x0f<<5));
	*pin_base |= ((pin_cfg<<5) & (0x0f<<5));
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

	switch(data) {
	case HAL_IOMUX_FORCE_OFF:
		// clear force bit and value
		*pin_base &= (~0x1E00000);
		break;

	case HAL_IOMUX_FORCE_OUT_LOW:
		// clear force bit and value
		*pin_base &= (~0x1E00000);
		// enable force control
		*pin_base |= (0x400000);
		// open out force
		*pin_base |= (0x1000000);
	    // Set value
	    *pin_base |= (data << 23);
	    break;

	case HAL_IOMUX_FORCE_OUT_HIGH:
		// clear force bit and value
		*pin_base &= (~0x1E00000);
		// enable force control
		*pin_base |= (0x400000);
		// open out force
		*pin_base |= (0x1000000);
	    // Set value
	    *pin_base |= (data << 23);
	    break;

	default:
		return CSK_DRIVER_ERROR_PARAMETER;
	}
	return CSK_DRIVER_OK;
}
