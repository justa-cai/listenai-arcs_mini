#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // 添加abs函数需要的头文件

#include "arcs_ap.h"
#include "lisa_log.h"
#include "lisa_mutex.h"

#include "battery.h"
#include "power/power_manager.h"
#include "lisa_timer.h"
#include "board.h"
#include "lisa_gpio.h"
#include "lisa_adc.h"
#include "voice_msg.h"

#define TAG "battery"

#define BAT_ADC_PAD CONFIG_BATTERY_COLLECTION_ADC_PAD
#define BAT_ADC_CH  CONFIG_BATTERY_COLLECTION_ADC_CHANNEL

#define CHARGE_DET_PAD CONFIG_BATTERY_COLLECTION_CHARGE_DETECT_PAD

#define VBAT_MAX_VOLTAGE                (4350) /* 锂电池理论最大电压 */
#define VBAT_MIN_VOLTAGE                (3500) /* 锂电池理论最小电压 */
#define VBAT_PARTIAL_VOLTAGE_PERCENTAGE (40)   /* 硬件电池分压2/5, 百分比为40 */

#define VBAT_SAMPLE_PRIOD (1000)

/* 低于该电压认为未接入电池，单位mV */
#define VBAT_PRESENT_THRESHOLD (2000)

// 电压滤波相关定义
#define VOLTAGE_FILTER_WINDOW_SIZE  5
#define PERCENTAGE_CHANGE_THRESHOLD 10 // 电量百分比变化阈值

static lisa_device_t *bat_adc_dev = NULL;
static lisa_device_t *charge_det_dev = NULL;

lisa_timer_t *battery_timer = NULL;

// 移动平均滤波器变量
static uint16_t voltage_history[VOLTAGE_FILTER_WINDOW_SIZE] = {0};
static uint8_t voltage_index = 0;
static bool voltage_buffer_full = false;
static uint16_t last_vbat_voltage_mv = 0;
static uint16_t last_vbat_detect_mv = 0;

// 电池电压百分比查找表 (按10%步进，从0%到100%)
// 请根据实际电池特性填充对应的电压值 (单位: mV)
//
// 建议测量方法：
// 1. 将电池充满到100%，记录电压值
// 2. 以10%为步长，逐步放电并记录对应电压值
// 3. 确保测量时电池处于静置状态（非充放电状态）
// 4. 多次测量取平均值以提高准确性
//
static const uint16_t battery_voltage_table_discharge[11] = {
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

// 充电电压百分比查找表 (按10%步进，从0%到100%)
// 由于充电时端电压会被抬高，建议使用单独的充电曲线。
// 以下为基于实测的初始值，可根据实际电池/充电IC进一步校准。
static const uint16_t battery_voltage_table_charge[11] = {
    3700, // 0%
    4000, // 10%
    4040, // 20%
    4070, // 30%
    4090, // 40%
    4110, // 50%
    4125, // 60%
    4138, // 70%
    4145, // 80%
    4185, // 90%
    4210  // 100%
};

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

static void voltage_filter_reset(uint16_t voltage)
{
    for (uint8_t i = 0; i < VOLTAGE_FILTER_WINDOW_SIZE; i++) {
        voltage_history[i] = voltage;
    }
    voltage_index = 0;
    voltage_buffer_full = true;
}

/**
 * @brief 基于查找表的电池百分比计算
 * @param voltage 当前电池电压 (mV)
 * @param table 电压-百分比查找表
 * @return 电池电量百分比 (0-100)
 */
static uint8_t voltage_to_percentage_by_table(uint16_t voltage, const uint16_t *table)
{
    // 边界处理
    if (voltage <= table[0]) {
        return 0;
    }
    if (voltage >= table[10]) {
        return 100;
    }

    // 在查找表中寻找合适的区间
    for (uint8_t i = 0; i < 10; i++) {
        if (voltage >= table[i] && voltage <= table[i + 1]) {
            // 线性插值计算精确百分比
            uint16_t voltage_diff = table[i + 1] - table[i];
            uint16_t current_diff = voltage - table[i];

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
    bat_adc_dev = lisa_device_get("adc0");
    lisa_adc_channel_config_t adc_ch_cfg = {
        .reference = LISA_ADC_REF_VDD_3V6,
        .resolution = LISA_ADC_RESOLUTION_10BIT,
    };
    lisa_adc_channel_setup(bat_adc_dev, BAT_ADC_CH, &adc_ch_cfg);

    // charge status
    charge_det_dev = lisa_device_get(CHARGE_DET_PAD);
    lisa_gpio_configure(charge_det_dev, CHARGE_DET_PIN, LISA_GPIO_INPUT);
}

static voice_msg_battery_status_t battery_status_to_msg(battery_status_t status)
{
    switch (status) {
    case BATTERY_STATUS_NO_BATTERY:
        return VOICE_MSG_BATTERY_STATUS_NO_BATTERY;
    case BATTERY_STATUS_NOT_CONNECT:
        return VOICE_MSG_BATTERY_STATUS_NOT_CONNECT;
    case BATTERY_STATUS_CHARGING:
        return VOICE_MSG_BATTERY_STATUS_CHARGING;
    case BATTERY_STATUS_CHARGE_DONE:
        return VOICE_MSG_BATTERY_STATUS_CHARGE_DONE;
    case BATTERY_STATUS_UNKNOWN:
    default:
        return VOICE_MSG_BATTERY_STATUS_UNKNOWN;
    }
}

static void battery_voltage_sample_cb(struct lisa_timer *timer)
{
    uint8_t raw_percentage = battery_get_pct();
    static uint8_t last_percentage = 0xFF;
    battery_status_t status = battery_get_status();
    static uint8_t last_status = 0xFF;

    voice_msg_battery_info_t msg = {
        .level = raw_percentage,
        .status = battery_status_to_msg(status),
    };

    if (raw_percentage != last_percentage || status != last_status) {
        voice_msg_pub(VOICE_MSG_POWER_BATTERY_UPDATE, &msg, sizeof(msg));
        last_percentage = raw_percentage;
        last_status = status;
    }

    LISA_LOGI(TAG, "Battery: raw=%d%%, status=%d", raw_percentage, status);

    lisa_timer_start(battery_timer);
}

void battery_init(void)
{
    static bool init_flag = false;

    if (init_flag) {
        return;
    }

    battery_sample_pin_init();

    battery_timer = lisa_timer_create(VBAT_SAMPLE_PRIOD, battery_voltage_sample_cb, NULL);
    if (battery_timer) {
        lisa_timer_start(battery_timer);
    } else {
        LISA_LOGE(TAG, "Failed to create battery sample timer");
    }

    init_flag = true;

    return;
}

static uint16_t battery_get_voltage(void)
{
    static bool last_usb_plugged = false;
    bool usb_plugged;
    uint16_t adc_raw_value;
    uint16_t adc_real_voltage;
    uint16_t vbat_real_voltage;
    uint16_t filtered_voltage;

    lisa_adc_read(bat_adc_dev, BAT_ADC_CH, &adc_raw_value);
    adc_real_voltage = LISA_ADC_RAW_TO_MV(adc_raw_value, 3600, LISA_ADC_RESOLUTION_10BIT);

    /* remove Hardware voltage division  */
    vbat_real_voltage = (uint16_t)(adc_real_voltage * 100 / VBAT_PARTIAL_VOLTAGE_PERCENTAGE);

    usb_plugged = power_is_usb_plugged();
    if (usb_plugged != last_usb_plugged) {
        // 充电状态变化时，重置滤波，避免电压跳变被均值拖尾
        voltage_filter_reset(vbat_real_voltage);
        filtered_voltage = vbat_real_voltage;
        last_usb_plugged = usb_plugged;
    } else {
        // 应用移动平均滤波
        filtered_voltage = voltage_moving_average_filter(vbat_real_voltage);
    }

    LISA_LOGI(TAG, "adc_raw: %d, vbat_raw: %d, vbat_filtered: %d", adc_real_voltage, vbat_real_voltage,
              filtered_voltage);

    return filtered_voltage;
}

uint8_t battery_get_pct(void)
{
    uint16_t filtered_voltage;
    uint8_t vbat_voltage_percentage;

    filtered_voltage = battery_get_voltage();
    // 电压范围限制
    if (filtered_voltage < VBAT_MIN_VOLTAGE) {
        filtered_voltage = VBAT_MIN_VOLTAGE;
        if (!power_is_usb_plugged()) {
            power_shutdown();
        }
    } else if (filtered_voltage > VBAT_MAX_VOLTAGE) {
        filtered_voltage = VBAT_MAX_VOLTAGE;
    }

    /* 使用查找表获取电池电压百分比 */
    if (power_is_usb_plugged()){
        vbat_voltage_percentage = voltage_to_percentage_by_table(filtered_voltage, battery_voltage_table_charge);
    } else {
        vbat_voltage_percentage = voltage_to_percentage_by_table(filtered_voltage, battery_voltage_table_discharge);
    }

    return vbat_voltage_percentage;
}

battery_status_t battery_get_status(void)
{
    static uint8_t s_discharge_static_cnt = 0; // 解决电脑供电时,充电状态不稳定的问题
    battery_status_t ret = 0;
    uint16_t filtered_voltage;

    filtered_voltage = battery_get_voltage();

    if (filtered_voltage < VBAT_PRESENT_THRESHOLD) {
        s_discharge_static_cnt = 0;
        ret = BATTERY_STATUS_NO_BATTERY;
        return ret;
    } else {
        ret = BATTERY_STATUS_NOT_CONNECT;
    }

    if (power_is_usb_plugged()) {
        if (s_discharge_static_cnt < 3) {
            s_discharge_static_cnt++;
        }else{
            ret = BATTERY_STATUS_CHARGING;
        }
    } else {
        s_discharge_static_cnt = 0;
    }

    const uint16_t *table = power_is_usb_plugged() ? battery_voltage_table_discharge : battery_voltage_table_charge;
    if (voltage_to_percentage_by_table(filtered_voltage, table) >= 98) {
        ret = BATTERY_STATUS_CHARGE_DONE;
    }
    
    return ret;
}
