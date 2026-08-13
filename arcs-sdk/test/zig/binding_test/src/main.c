/* Zig Bindings Struct Layout Verification Test */

#define LOG_TAG "bind_test"

#include <lisa_log.h>
#include <stddef.h>
#include <stdint.h>

#include "lisa_device.h"
#include "lisa_gpio.h"
#include "lisa_uart.h"
#include "lisa_i2c.h"
#include "lisa_spi.h"
#include "lisa_adc.h"
#include "lisa_pwm.h"
#include "lisa_flash.h"
#include "lisa_display.h"
#include "lisa_camera.h"
#include "lisa_audio.h"
#include "lisa_rtc.h"
#include "app_player.h"
#include "HTTPCUsr_api.h"

extern int zig_check_size(const char *name, uint32_t c_size, uint32_t zig_size);
extern int zig_binding_test_start(void);
extern int zig_binding_test_end(int fails);
extern uint32_t zig_get_sizeof(uint32_t id);

int main(int argc, char **argv)
{
    int fails = 0;

    fails += zig_binding_test_start();

    /* 每个 CHECK: C sizeof vs Zig sizeof (通过 id 查询) */
    #define CHECK(id, name, type) \
        fails += zig_check_size(name, (uint32_t)sizeof(type), zig_get_sizeof(id))

    CHECK(0,  "Device",          lisa_device_t);
    CHECK(1,  "DeviceStats",     lisa_device_stats_t);
    CHECK(2,  "GpioApi",         lisa_gpio_api_t);
    CHECK(3,  "AudioFormat",     lisa_audio_format_t);
    CHECK(4,  "AudioGain",       lisa_audio_gain_t);
    CHECK(5,  "AudioEvent",      lisa_audio_event_t);
    CHECK(6,  "AudioRecordCfg",  lisa_audio_record_config_t);
    CHECK(7,  "AudioPlayCfg",    lisa_audio_play_config_t);
    CHECK(8,  "AudioApi",        lisa_audio_api_t);
    CHECK(64, "AudioRecordChGain", lisa_audio_record_channel_gain_t);
    CHECK(9,  "UartConfig",      lisa_uart_config_t);
    CHECK(10, "UartApi",         lisa_uart_api_t);
    CHECK(11, "I2cMsg",          lisa_i2c_msg_t);
    CHECK(12, "I2cApi",          lisa_i2c_api_t);
    CHECK(13, "SpiConfig",       lisa_spi_config_t);
    CHECK(14, "SpiApi",          lisa_spi_api_t);
    CHECK(15, "AdcApi",          lisa_adc_api_t);
    CHECK(16, "PwmApi",          lisa_pwm_api_t);
    CHECK(17, "FlashParams",     lisa_flash_parameters_t);
    CHECK(18, "FlashApi",        lisa_flash_api_t);
    CHECK(19, "DisplayCaps",     lisa_display_capabilities_t);
    CHECK(20, "DisplayBufDesc",  lisa_display_buffer_desc_t);
    CHECK(21, "DisplayApi",      lisa_display_api_t);
    CHECK(22, "RtcTime",         lisa_rtc_time_t);
    CHECK(23, "RtcApi",          lisa_rtc_api_t);
    CHECK(24, "DisplayBusType",  lisa_display_bus_type_t);
    CHECK(25, "DisplayCmdBusType", lisa_display_cmd_bus_type_t);
    CHECK(26, "RgbInputFormat",  lisa_rgb_input_format_t);
    CHECK(27, "RgbPolarity",     lisa_rgb_polarity_t);
    CHECK(28, "RgbOutputFormat", lisa_rgb_output_format_t);
    CHECK(29, "BacklightType",   lisa_display_backlight_type_t);
    CHECK(30, "BacklightPolarity", lisa_display_backlight_polarity_t);
    CHECK(31, "DisplaySpi4Cfg",  lisa_display_bus_spi_4wire_config_t);
    CHECK(32, "DisplaySpi3Cfg",  lisa_display_bus_spi_3wire_config_t);
    CHECK(33, "DisplayQspiCfg",  lisa_display_bus_qspi_config_t);
    CHECK(34, "DisplayRgbTimings", ((lisa_display_bus_rgb_config_t *)0)->timings);
    CHECK(35, "DisplayRgbCfg",   lisa_display_bus_rgb_config_t);
    CHECK(36, "DisplayCmdSwSpiCfg", lisa_display_cmd_sw_spi_config_t);
    CHECK(37, "DisplayCmdBusCfg", lisa_display_cmd_bus_config_u);
    CHECK(38, "DisplayBacklightPwmCfg", lisa_display_backlight_pwm_config_t);
    CHECK(39, "DisplayBacklightSwCfg", lisa_display_backlight_single_wire_config_t);
    CHECK(40, "DisplayBacklightCfg", ((lisa_display_backlight_t *)0)->config);
    CHECK(41, "DisplayBacklight", lisa_display_backlight_t);
    CHECK(42, "DisplayBusCfg",    lisa_display_bus_config_u);
    CHECK(43, "DisplayConfig",    lisa_display_config_t);
    CHECK(44, "CameraPixelFormat", lisa_camera_pixel_format_t);
    CHECK(45, "CameraFrameSize",  lisa_camera_framesize_t);
    CHECK(46, "CameraBusType",    lisa_camera_bus_type_e);
    CHECK(47, "CameraSensorIndex", lisa_camera_sensor_index_t);
    CHECK(48, "CameraSensorPwdn", lisa_camera_sensor_pwdn_t);
    CHECK(49, "CameraBusDvpCfg",  lisa_camera_bus_dvp_config_t);
    CHECK(50, "CameraBusSpiCfg",  lisa_camera_bus_spi_config_t);
    CHECK(51, "CameraBusCfgUnion", lisa_camera_bus_config_u);
    CHECK(52, "CameraBusCfg",     lisa_camera_bus_config_t);
    CHECK(53, "CameraHwCfg",      lisa_camera_hw_config_t);
    CHECK(54, "CameraConfig",     lisa_camera_config_t);
    CHECK(55, "CameraFb",         lisa_camera_fb_t);
    CHECK(56, "CameraCrop",       lisa_camera_crop_t);
    CHECK(57, "CameraCaps",       lisa_camera_capabilities_t);
    CHECK(58, "CameraApi",        lisa_camera_api_t);
    CHECK(59, "AppPlayerConfig",  app_player_config_t);
    CHECK(60, "AppPlayerPlayOpt", app_player_play_opt_t);
    CHECK(61, "HttpParameters",   HTTPParameters);
    CHECK(62, "HttpClientInfo",   HTTP_CLIENT);
    CHECK(63, "HttpRedirectParam", HTTP_REDIRECT_PARAM);

    return zig_binding_test_end(fails);
}
