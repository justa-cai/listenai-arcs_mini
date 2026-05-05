#include "boot_gpio_check.h"

#ifdef CONFIG_BOOT_CHECK_POWER_KEY

#include <stdint.h>

#include "chip.h"
#include "ClockManager.h"
#include "IOMuxManager.h"

static bool pin_matches_level(int port, int pin, int expected)
{
    volatile uint32_t *cmn_reg;
    volatile uint32_t *aon_reg = NULL;
    GPIO_RegDef *gpio;
    uint32_t cmn_saved;
    uint32_t aon_saved = 0;

    if (port == CSK_IOMUX_PAD_A) {
        if (pin > CSK_IOMUX_PAD_A_MAX_PIN) {
            return false;
        }
        __HAL_CRM_GPIO0_CLK_ENABLE();
        cmn_reg = (volatile uint32_t *)&IP_CMN_IOMUX->REG_PAD_GPIOA_00.all + pin;
        gpio = IP_GPIOA;
    } else if (port == CSK_IOMUX_PAD_B) {
        if (pin > CSK_IOMUX_PAD_B_MAX_PIN) {
            return false;
        }
        __HAL_CRM_GPIO1_CLK_ENABLE();
        aon_reg = (volatile uint32_t *)&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + pin;
        cmn_reg = (volatile uint32_t *)&IP_CMN_IOMUX->REG_PAD_GPIOB_00.all + pin;
        gpio = IP_GPIOB;
    } else {
        return false;
    }

    if (aon_reg != NULL) {
        aon_saved = *aon_reg;
        *aon_reg = (aon_saved & ~0x1fu) | (CSK_AON_IOMUX_FUNC_NORMAL & 0x1fu);
    }
    cmn_saved = *cmn_reg;
    *cmn_reg = (cmn_saved & ~0x1fu) | (CSK_IOMUX_FUNC_DEFAULT & 0x1fu);

    gpio->REG_CHANNELDIR.all &= ~(1u << pin);
    int level = (gpio->REG_DATAIN.all >> pin) & 0x1u;

    *cmn_reg = cmn_saved;
    if (aon_reg != NULL) {
        *aon_reg = aon_saved;
    }

    return level == (expected & 0x1);
}

bool boot_gpio_check_power_key_triggered(void)
{
    if (!pin_matches_level(CONFIG_BOOT_CHECK_POWER_KEY_PORT,
                           CONFIG_BOOT_CHECK_POWER_KEY_PIN,
                           CONFIG_BOOT_CHECK_POWER_KEY_LEVEL)) {
        return false;
    }

#ifdef CONFIG_BOOT_CHECK_POWER_KEY_REQUIRE_USB
    if (!pin_matches_level(CONFIG_BOOT_CHECK_POWER_KEY_USB_PORT,
                           CONFIG_BOOT_CHECK_POWER_KEY_USB_PIN,
                           CONFIG_BOOT_CHECK_POWER_KEY_USB_LEVEL)) {
        return false;
    }
#endif

    return true;
}

#endif /* CONFIG_BOOT_CHECK_POWER_KEY */
