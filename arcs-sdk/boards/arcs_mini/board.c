#include "board.h"

#ifdef CONFIG_ARCS_AP_CORE
static void lisa_ap_log_uart_iomux_config()
{
    IOMuxManager_PinConfigure(LISA_AP_LOG_UART_TX_PORT, LISA_AP_LOG_UART_TX_PIN, LISA_AP_LOG_UART_TX_FUNC);
}
#endif

#ifdef CONFIG_ARCS_CP_CORE
static void lisa_cp_log_uart_iomux_config()
{
    IOMuxManager_PinConfigure(LISA_CP_LOG_UART_TX_PORT, LISA_CP_LOG_UART_TX_PIN, LISA_CP_LOG_UART_TX_FUNC);
    IOMuxManager_PinConfigure(LISA_CP_LOG_UART_RX_PORT, LISA_CP_LOG_UART_RX_PIN, LISA_CP_LOG_UART_RX_FUNC);
}

static void lisa_aon_iomux_config()
{
    AON_IOMuxManager_PinConfigure(LISA_BAT_ADC_AON_PORT, LISA_BAT_ADC_AON_PIN, LISA_BAT_ADC_AON_FUNC);
}

static void lisa_misc_iomux_config()
{
    IOMuxManager_PinConfigure(LISA_PA_MUTE_PORT, LISA_PA_MUTE_PIN, LISA_PA_MUTE_FUNC);

    IOMuxManager_PinConfigure(LISA_USB_DET_PORT, LISA_USB_DET_PIN, LISA_USB_DET_FUNC);

    IOMuxManager_PinConfigure(LISA_LED_GREEN_PORT, LISA_LED_GREEN_PIN, LISA_LED_GREEN_FUNC);

    IOMuxManager_PinConfigure(LISA_PWR_LOCK_PORT, LISA_PWR_LOCK_PIN, LISA_PWR_LOCK_FUNC);

    IOMuxManager_PinConfigure(LISA_PWR_KEY_PORT, LISA_PWR_KEY_PIN, LISA_PWR_KEY_FUNC);

    IOMuxManager_PinConfigure(LISA_CHGR_STATUS_PORT, LISA_CHGR_STATUS_PIN, LISA_CHGR_STATUS_FUNC);
}
#endif

#ifdef CONFIG_LISA_DISPLAY
static void lisa_display_iomux_config()
{
    IOMuxManager_PinConfigure(LISA_DISPLAY_SPI_CS_PORT, LISA_DISPLAY_SPI_CS_PIN, LISA_DISPLAY_SPI_CS_FUNC);
    IOMuxManager_PinConfigure(LISA_DISPLAY_SPI_SDA_PORT, LISA_DISPLAY_SPI_SDA_PIN, LISA_DISPLAY_SPI_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_DISPLAY_SPI_DC_PORT, LISA_DISPLAY_SPI_DC_PIN, LISA_DISPLAY_SPI_DC_FUNC);
    IOMuxManager_PinConfigure(LISA_DISPLAY_SPI_CLK_PORT, LISA_DISPLAY_SPI_CLK_PIN, LISA_DISPLAY_SPI_CLK_FUNC);

    IOMuxManager_PinConfigure(LISA_DISPLAY_BL_PWM_PORT, LISA_DISPLAY_BL_PWM_PIN, LISA_DISPLAY_BL_PWM_FUNC);

    IOMuxManager_PinConfigure(LISA_DISPLAY_RESET_PORT, LISA_DISPLAY_RESET_PIN, LISA_DISPLAY_RESET_FUNC);
#ifdef CONFIG_LISA_DISPLAY_TE_SYNC
    IOMuxManager_PinConfigure(LISA_DISPLAY_TE_PORT, LISA_DISPLAY_TE_PIN, LISA_DISPLAY_TE_FUNC);
#endif
}
#endif


#ifdef CONFIG_COMPONENT_CAMERA
static void lisa_camera_iomux_config()
{
    IOMuxManager_PinConfigure(LISA_CAMERA_I2C_SDA_PORT, LISA_CAMERA_I2C_SDA_PIN, LISA_CAMERA_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_I2C_SCL_PORT, LISA_CAMERA_I2C_SCL_PIN, LISA_CAMERA_I2C_SCL_FUNC);

    IOMuxManager_PinConfigure(LISA_CAMERA_MCLK_PORT, LISA_CAMERA_MCLK_PIN, LISA_CAMERA_MCLK_FUNC);

    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_HSYNC_PORT, LISA_CAMERA_DVP_HSYNC_PIN, LISA_CAMERA_DVP_HSYNC_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_VSYNC_PORT, LISA_CAMERA_DVP_VSYNC_PIN, LISA_CAMERA_DVP_VSYNC_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_PCLK_PORT,  LISA_CAMERA_DVP_PCLK_PIN, LISA_CAMERA_DVP_PCLK_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA0_PORT, LISA_CAMERA_DVP_DATA0_PIN, LISA_CAMERA_DVP_DATA0_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA1_PORT, LISA_CAMERA_DVP_DATA1_PIN, LISA_CAMERA_DVP_DATA1_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA2_PORT, LISA_CAMERA_DVP_DATA2_PIN, LISA_CAMERA_DVP_DATA2_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA3_PORT, LISA_CAMERA_DVP_DATA3_PIN, LISA_CAMERA_DVP_DATA3_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA4_PORT, LISA_CAMERA_DVP_DATA4_PIN, LISA_CAMERA_DVP_DATA4_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA5_PORT, LISA_CAMERA_DVP_DATA5_PIN, LISA_CAMERA_DVP_DATA5_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA6_PORT, LISA_CAMERA_DVP_DATA6_PIN, LISA_CAMERA_DVP_DATA6_FUNC);
    IOMuxManager_PinConfigure(LISA_CAMERA_DVP_DATA7_PORT, LISA_CAMERA_DVP_DATA7_PIN, LISA_CAMERA_DVP_DATA7_FUNC);
}
#endif

static void hw_board_iomux_config(void)
{
#ifdef CONFIG_ARCS_AP_CORE
    lisa_ap_log_uart_iomux_config();
#endif

#ifdef CONFIG_ARCS_CP_CORE
    lisa_cp_log_uart_iomux_config();

    lisa_misc_iomux_config();

    lisa_aon_iomux_config();
#endif

#ifdef CONFIG_LISA_DISPLAY
    lisa_display_iomux_config();
#endif

#ifdef CONFIG_COMPONENT_CAMERA
    lisa_camera_iomux_config();
#endif
}

void pre_main_hook(void)
{
    hw_board_iomux_config();

    GPIO_Initialize(GPIOA(), NULL, NULL);
    GPIO_Initialize(GPIOB(), NULL, NULL);
}

const char* board_get_name(void)
{
    return "arcs_mini";
}

