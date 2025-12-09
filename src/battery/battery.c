#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // 添加abs函数需要的头文件

#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "Driver_GPADC.h"
#include "Driver_GPIO.h"
#include "lisa_log.h"
#include "lisa_mutex.h"

#include "adc_sample.h"
#include "battery.h"
#include "power_manager.h"
#include "lisa_timer.h"

#include "assistant_controller.h"
#include "assistant_view.h"

#define TAG "battery"

#define VBAT_ID_PIN_NUM            (4)
#define VBAT_VOLTAGE_PIN_NUM       (5)
#define VBAT_CHARGE_STATUS_PIN_NUM (8)
#define USB_PLUG_DETECT_PIN_NUM    (0) // USB插入检测

#define GPADC_CHANNEL_VBAT_VOLTAGE_PIN CSK_GPADC_CHANNEL_SEL_3

#define VBAT_MAX_VOLTAGE                (4350) /* 锂电池理论最大电压 */
#define VBAT_MIN_VOLTAGE                (3500) /* 锂电池理论最小电压 */
#define VBAT_PARTIAL_VOLTAGE_PERCENTAGE (40)   /* 硬件电池分压2/5, 百分比为40 */

#define VBAT_SAMPLE_PRIOD (1000)

// 电压滤波相关定义
#define VOLTAGE_FILTER_WINDOW_SIZE  5
#define PERCENTAGE_CHANGE_THRESHOLD 10 // 电量百分比变化阈值

lisa_timer_t *battery_timer = NULL;

// 移动平均滤波器变量
static uint16_t voltage_history[VOLTAGE_FILTER_WINDOW_SIZE] = {0};
static uint8_t voltage_index = 0;
static bool voltage_buffer_full = false;
static bool show_battery_status = false;

// 电池电压百分比查找表 (按10%步进，从0%到100%)
// 请根据实际电池特性填充对应的电压值 (单位: mV)
//
// 建议测量方法：
// 1. 将电池充满到100%，记录电压值
// 2. 以10%为步长，逐步放电并记录对应电压值
// 3. 确保测量时电池处于静置状态（非充放电状态）
// 4. 多次测量取平均值以提高准确性
//
static const uint16_t battery_voltage_table[11] = {
    3300, // 0%  - 最低工作电压，系统关机电压
    3783, // 10%
    3843, // 20%
    3870, // 30%
    3885, // 40%
    3919, // 50%
    3973, // 60%
    4024, // 70%
    4085, // 80%
    4179, // 90%
    4199  // 100%
};

bool get_show_battery_status(void)
{
    return show_battery_status;
}

/**
 * @brief 移动平均滤波器 - 对电压进行平滑滤波
 * @param new_voltage 新的电压值 (mV)
 * @return 滤波后的电压值 (mV)
 */
static uint16_t voltage_moving_average_filter(uint16_t new_voltage)
{
    // 将新电压值存入循环缓冲区
    voltage_history[voltage_index] = new_voltage;
    voltage_index = (voltage_index + 1) % VOLTAGE_FILTER_WINDOW_SIZE;

    // 检查缓冲区是否已满
    if (!voltage_buffer_full && voltage_index == 0) {
        voltage_buffer_full = true;
    }

    // 计算平均值
    uint32_t sum = 0;
    uint8_t count = voltage_buffer_full ? VOLTAGE_FILTER_WINDOW_SIZE : voltage_index;

    for (uint8_t i = 0; i < count; i++) {
        sum += voltage_history[i];
    }

    return (uint16_t)(sum / count);
}

/**
 * @brief 基于查找表的电池百分比计算
 * @param voltage 当前电池电压 (mV)
 * @return 电池电量百分比 (0-100)
 */
static uint8_t voltage_to_percentage_by_table(uint16_t voltage)
{
    // 边界处理
    if (voltage <= battery_voltage_table[0]) {
        return 0;
    }
    if (voltage >= battery_voltage_table[10]) {
        return 100;
    }

    // 在查找表中寻找合适的区间
    for (uint8_t i = 0; i < 10; i++) {
        if (voltage >= battery_voltage_table[i] && voltage <= battery_voltage_table[i + 1]) {
            // 线性插值计算精确百分比
            uint16_t voltage_diff = battery_voltage_table[i + 1] - battery_voltage_table[i];
            uint16_t current_diff = voltage - battery_voltage_table[i];

            // 避免除零错误
            if (voltage_diff == 0) {
                return i * 10;
            }

            // 计算插值百分比
            uint8_t base_percentage = i * 10;
            uint8_t interpolated_percentage = (uint8_t)((current_diff * 10) / voltage_diff);

            return base_percentage + interpolated_percentage;
        }
    }

    // 如果没有找到合适区间，使用线性计算作为备选
    return (uint8_t)((voltage - VBAT_MIN_VOLTAGE) * 100 / (VBAT_MAX_VOLTAGE - VBAT_MIN_VOLTAGE));
}

static void battery_sample_pin_init(void)
{
    // Vbat voltage adc sample
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, VBAT_VOLTAGE_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    // charge status
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, VBAT_CHARGE_STATUS_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(GPIOB(), (1UL << VBAT_CHARGE_STATUS_PIN_NUM), CSK_GPIO_DIR_INPUT);

    GPIO_Control(GPIOB(), CSK_GPIO_MODE_PULL_UP | CSK_GPIO_DEBOUNCE_DISABLE, (1UL << VBAT_CHARGE_STATUS_PIN_NUM));
}

static void battery_voltage_sample_cb(void *arg)
{
    uint8_t raw_percentage = get_battery_voltage_percentage();
    static uint8_t last_percentage = 0;
    battery_status_t status = get_battery_status();
    view_battery_info_t battery_info;

    // if(abs(raw_percentage - last_percentage) < PERCENTAGE_CHANGE_THRESHOLD) {
    //     return;
    // }

    last_percentage = raw_percentage;
    battery_info.is_charging = (status == BATTERY_STATUS_CHARGING);
    battery_info.power_percent = raw_percentage;
    battery_info.usb_status = get_usb_status();

    LISA_LOGI(TAG, "Battery: raw=%d%%, status=%d, show_battery_status=%d", raw_percentage, status, show_battery_status);
    assist_controller_trigger_event(CONTROLLER_EVENT_BATTERY_INFO_UPDATE, &battery_info, sizeof(battery_info));

    lisa_timer_start(battery_timer);
}

void usb_plug_detect_gpio_init(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, USB_PLUG_DETECT_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(GPIOB(), (1UL << USB_PLUG_DETECT_PIN_NUM), 0);
    GPIO_Control(GPIOB(), CSK_GPIO_DEBOUNCE_DISABLE, (1UL << USB_PLUG_DETECT_PIN_NUM));
}

void battery_adc_sample_init(void)
{
    static bool init_flag = false;

    if (init_flag) {
        return;
    }

    battery_sample_pin_init();

    adc_sample_init(2, true, CSK_GPADC_CHANNEL_SEL_3);

    battery_timer = lisa_timer_create(VBAT_SAMPLE_PRIOD, battery_voltage_sample_cb, NULL);
    if (battery_timer) {
        lisa_timer_start(battery_timer);
    } else {
        LISA_LOGE(TAG, "Failed to create battery sample timer");
    }

    init_flag = true;

    return;
}

uint8_t get_battery_voltage_percentage(void)
{
    uint16_t adc_real_voltage;
    uint16_t vbat_real_voltage;
    uint16_t filtered_voltage;
    uint8_t vbat_voltage_percentage;

    adc_real_voltage = adc_sample_get_channel_value(GPADC_CHANNEL_VBAT_VOLTAGE_PIN);

    /* remove Hardware voltage division  */
    vbat_real_voltage = (uint16_t)(adc_real_voltage * 100 / VBAT_PARTIAL_VOLTAGE_PERCENTAGE);

    // 应用移动平均滤波
    filtered_voltage = voltage_moving_average_filter(vbat_real_voltage);

    LISA_LOGI(TAG, "adc_raw: %d, vbat_raw: %d, vbat_filtered: %d", adc_real_voltage, vbat_real_voltage,
              filtered_voltage);
    
    // 当ADC采样电压低于500mV时，不显示电池图标
    if (filtered_voltage < 500) {
        show_battery_status = 0;
    } else {
        show_battery_status = 1;
    }

    // 电压范围限制
    if (filtered_voltage < VBAT_MIN_VOLTAGE) {
        filtered_voltage = VBAT_MIN_VOLTAGE;
        if (get_usb_status() == USB_STATUS_UNPLUG) {
            power_shutdown();
        }
    } else if (filtered_voltage > VBAT_MAX_VOLTAGE) {
        filtered_voltage = VBAT_MAX_VOLTAGE;
    }

    /* 使用查找表获取电池电压百分比 */
    vbat_voltage_percentage = voltage_to_percentage_by_table(filtered_voltage);

    return vbat_voltage_percentage;
}

battery_status_t get_battery_status(void)
{
    uint32_t pin_value = GPIO_PinRead(GPIOB(), 1 << VBAT_CHARGE_STATUS_PIN_NUM);

    if (pin_value) {
        return BATTERY_STATUS_NOT_CONNECT;
    } else {
        return BATTERY_STATUS_CHARGING;
    }
}

usb_status_t get_usb_status(void)
{
    uint32_t pin_value = GPIO_PinRead(GPIOB(), 1 << USB_PLUG_DETECT_PIN_NUM);

    if (pin_value) {
        return USB_STATUS_PLUG;
    } else {
        return USB_STATUS_UNPLUG;
    }
}
