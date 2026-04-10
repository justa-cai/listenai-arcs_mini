#include <stdint.h>
#include <stdbool.h>

#include "arcs_ap.h"
#include "lisa_log.h"

#include "battery.h"
#include "battery_ui.h"
#include "power/power_manager.h"
#include "board.h"
#include "lisa_gpio.h"
#include "lisa_adc.h"
#include "sysutils.h"

#define TAG "battery"

#define BAT_ADC_PAD CONFIG_BATTERY_COLLECTION_ADC_PAD
#define BAT_ADC_CH  CONFIG_BATTERY_COLLECTION_ADC_CHANNEL

#define CHARGE_DET_PAD CONFIG_BATTERY_COLLECTION_CHARGE_DETECT_PAD

#define VBAT_MAX_VOLTAGE                (4350) /* 锂电池理论最大电压 */
#define VBAT_MIN_VOLTAGE                (3500) /* 锂电池理论最小电压 */
#define VBAT_PARTIAL_VOLTAGE_PERCENTAGE (40)   /* 硬件电池分压2/5, 百分比为40 */

/* 低电压关机保护：连续低于截止电压 N 次（1次=1s）才关机 */
#define LOW_VOLTAGE_SHUTDOWN_CONFIRM_TICKS (5)

/* USB 插拔去抖：连续 N 次采样一致才认为状态变化（1次=1s） */
#define USB_PLUGGED_DEBOUNCE_TICKS (3)

/* 低于该电压认为未接入电池，单位mV */
#define VBAT_PRESENT_THRESHOLD (2000)

#define VOLTAGE_FILTER_WINDOW_SIZE 5

static lisa_device_t *bat_adc_dev __psram_bss__ = NULL;
static lisa_device_t *charge_det_dev __psram_bss__ = NULL;

// 移动平均滤波器变量
static uint16_t voltage_history[VOLTAGE_FILTER_WINDOW_SIZE] __psram_bss__ = {0};
static uint8_t voltage_index __psram_bss__ = 0;
static bool voltage_buffer_full __psram_bss__ = false;
static uint8_t low_voltage_shutdown_cnt __psram_bss__ = 0;

bool battery_usb_plugged_stable_get(void)
{
    static bool raw_last __psram_bss__ = false;
    static bool stable_state __psram_bss__ = false;
    static uint8_t stable_cnt __psram_bss__ = 0;

    bool raw_now = power_is_usb_plugged();

    if (raw_now != raw_last) {
        raw_last = raw_now;
        stable_cnt = 0;
    } else if (stable_cnt < USB_PLUGGED_DEBOUNCE_TICKS) {
        stable_cnt++;
    }

    if (stable_cnt >= USB_PLUGGED_DEBOUNCE_TICKS) {
        stable_state = raw_now;
    }

    return stable_state;
}

// 电池电压百分比查找表 (按10%步进，从0%到100%)
// 请根据实际电池特性填充对应的电压值 (单位: mV)
//
// 建议测量方法：
// 1. 将电池充满到100%，记录电压值
// 2. 以10%为步长，逐步放电并记录对应电压值
// 3. 确保测量时电池处于静置状态（非充放电状态）
// 4. 多次测量取平均值以提高准确性
//
// 数据来源：240032-7-6/7/8 实测恒流放电（155mA）曲线，3组取平均值
// 截止电压约 3.0V，满电开路电压约 4.20V（155mA 负载下测量值）
static const uint16_t battery_voltage_table_discharge[11] = {
    3010, // 0%  - 实测截止电压（约3.0V）
    3440, // 10%
    3576, // 20%
    3719, // 30%
    3781, // 40%
    3820, // 50%
    3860, // 60%
    3925, // 70%
    3973, // 80%
    4013, // 90%
    4160  // 100% - 满电端电压（155mA 负载）
};

// 充电电压百分比查找表 (按10%步进，从0%到100%)
// 数据来源：240032-7-6/7/8 实测恒流恒压充电（500mA CC / 4.2V CV）曲线，3组取平均值
// 说明：CC 阶段端电压上升较快（0%→10% 区间）；CV 阶段（10%→100%）电压在 4.04~4.20V 缓慢上升
static const uint16_t battery_voltage_table_charge[11] = {
    3257, // 0%  - 充电起始端电压（近空电池，500mA 充电开始）
    3677, // 10%
    3757, // 20%
    3850, // 30%
    3943, // 40%
    4002, // 50%
    4039, // 60%
    4071, // 70%
    4103, // 80%
    4150, // 90%
    4200  // 100% - CV 截止电压
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

void battery_init(void)
{
    static bool init_flag __psram_bss__ = false;

    if (init_flag) {
        return;
    }

    battery_sample_pin_init();
    battery_ui_init();

    init_flag = true;
}

uint16_t battery_get_voltage_mv(void)
{
    static bool last_usb_plugged __psram_bss__ = false;
    bool usb_plugged;
    uint16_t adc_raw_value;
    uint16_t adc_real_voltage;
    uint16_t vbat_real_voltage;
    uint16_t filtered_voltage;

    lisa_adc_read(bat_adc_dev, BAT_ADC_CH, &adc_raw_value);
    adc_real_voltage = LISA_ADC_RAW_TO_MV(adc_raw_value, 3600, LISA_ADC_RESOLUTION_10BIT);

    /* remove Hardware voltage division  */
    vbat_real_voltage = (uint16_t)(adc_real_voltage * 100 / VBAT_PARTIAL_VOLTAGE_PERCENTAGE);

    usb_plugged = battery_usb_plugged_stable_get();
    if (usb_plugged != last_usb_plugged) {
        // 充电状态变化时，重置滤波，避免电压跳变被均值拖尾
        voltage_filter_reset(vbat_real_voltage);
        filtered_voltage = vbat_real_voltage;
        last_usb_plugged = usb_plugged;
    } else {
        // 应用移动平均滤波
        filtered_voltage = voltage_moving_average_filter(vbat_real_voltage);
    }

    LISA_LOGD(TAG, "adc_raw: %d, vbat_raw: %d, vbat_filtered: %d", adc_real_voltage, vbat_real_voltage,
              filtered_voltage);

    return filtered_voltage;
}

uint8_t battery_get_pct_raw(void)
{
    uint16_t filtered_voltage;
    uint8_t vbat_voltage_percentage;
    bool usb_plugged;
    uint16_t discharge_cutoff_voltage = battery_voltage_table_discharge[0];

    filtered_voltage = battery_get_voltage_mv();
    usb_plugged = battery_usb_plugged_stable_get();

    // 低电压关机保护（仅在未插 USB 时生效，且需要连续确认）
    if (!usb_plugged && filtered_voltage < discharge_cutoff_voltage) {
        if (low_voltage_shutdown_cnt < LOW_VOLTAGE_SHUTDOWN_CONFIRM_TICKS) {
            low_voltage_shutdown_cnt++;
        }
        if (low_voltage_shutdown_cnt >= LOW_VOLTAGE_SHUTDOWN_CONFIRM_TICKS) {
            LISA_LOGW(TAG, "Battery too low (%dmV < %dmV), shutdown", filtered_voltage, discharge_cutoff_voltage);
            power_shutdown();
        }
    } else if (filtered_voltage > VBAT_MAX_VOLTAGE) {
        filtered_voltage = VBAT_MAX_VOLTAGE;
        low_voltage_shutdown_cnt = 0;
    } else {
        low_voltage_shutdown_cnt = 0;
    }

    // 电压下限钳位到曲线 0% 点，避免出现低于曲线范围时的异常跳变
    if (filtered_voltage < discharge_cutoff_voltage) {
        filtered_voltage = discharge_cutoff_voltage;
    }

    /* 使用查找表获取电池电压百分比 */
    if (usb_plugged){
        vbat_voltage_percentage = voltage_to_percentage_by_table(filtered_voltage, battery_voltage_table_charge);
    } else {
        vbat_voltage_percentage = voltage_to_percentage_by_table(filtered_voltage, battery_voltage_table_discharge);
    }

    return vbat_voltage_percentage;
}

battery_status_t battery_get_status(void)
{
    static uint8_t s_discharge_static_cnt __psram_bss__ = 0; // 解决电脑供电时,充电状态不稳定的问题
    battery_status_t ret = 0;
    uint16_t filtered_voltage;

    filtered_voltage = battery_get_voltage_mv();

    if (filtered_voltage < VBAT_PRESENT_THRESHOLD) {
        s_discharge_static_cnt = 0;
        ret = BATTERY_STATUS_NO_BATTERY;
        return ret;
    } else {
        ret = BATTERY_STATUS_NOT_CONNECT;
    }

    bool usb_plugged = battery_usb_plugged_stable_get();

    if (usb_plugged) {
        if (s_discharge_static_cnt < 3) {
            s_discharge_static_cnt++;
        }else{
            ret = BATTERY_STATUS_CHARGING;
        }
    } else {
        s_discharge_static_cnt = 0;
    }

    const uint16_t *table = usb_plugged ? battery_voltage_table_charge : battery_voltage_table_discharge;
    if (usb_plugged && voltage_to_percentage_by_table(filtered_voltage, table) >= 98) {
        ret = BATTERY_STATUS_CHARGE_DONE;
    }
    
    return ret;
}
