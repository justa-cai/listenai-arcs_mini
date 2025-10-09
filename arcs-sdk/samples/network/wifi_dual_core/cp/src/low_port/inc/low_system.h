
#ifndef __low_system_h__
#define __low_system_h__

#include "stdint.h"
#include "low_system2.h"

#define SYS_LED_1	0 
#define SYS_LED_2	1
#define SYS_LED_3	2
#define SYS_LED_4	3
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus/*需要被.c文件使用的函数声明*/

// LISA KV KEYS
#define LISA_KV_DEVICE_SN "device_sn"
#define LISA_KV_DEV_WIFI_MAC "wifi_mac"

/*
 * 完成系统时钟，ram等初始化
*/
int32_t low_system_init(void);

void low_system_term(void);

/*
 * 初始化led
 */
void low_system_led_init(void);

/*
 * 点亮一个led
 */
void low_system_led_on(int32_t led);

/*
 * 关闭一个led
 */
void low_system_led_off(int32_t led);

/*
 * 设置led打开时的亮度等级
 * 1是最亮,16是最暗
 */
void low_system_led_set_level(int32_t led, int32_t level);

//获取灯珠当前亮度值
int32_t low_system_led_get_level(void);

/*
 * 初始化按键
 */
void low_system_key_init(void);

/*
 * 采样按键输入值,这个函数直接采集此刻按键状态就行了，不需要做消抖处理
 * retval:
 *  一个bit对应一个按键，bit[x]==1,对应按键按下了，bit[x]==0,对应按键是放开的
 */
uint32_t low_system_key_load(void);

#define BAT_DISCHARGING     0
#define BAT_CHARGING        1
#define BAT_FULL            2
#define BAT_NOPRESENT       3

/*
*电池信息结构体
*id:区分多款电池的id
*model:电池型号
*temperature:电池温度
*capacity:电池电量
*status:充放电状态
*voltage:电池电压（充电时不准）
*/
typedef struct battery_info_struct
{
    int32_t id;      
    char *model;     
    float temperature;
    int32_t capacity;
    int32_t status;
    int32_t voltage;
    int32_t temperature_mv;
    int32_t status_mv;
}battery_info_t;

/*
 * 如果平台有能力提供电量信息的话就返回电量信息
 * 如果平台没有能力提供电量信息,就返回此刻的电压值,单位为mv
 * retval:
 *  -1: 获取电量失败
 *  >0: 返回电量百分比
 */
int32_t low_system_battery_capacity(void);

float low_system_battery_temperature(void);

/*
 * 返回电池的充电状态
 */
int32_t low_system_battery_status(void);

void low_system_get_pmu_status();

void low_system_bat_charge_enable(void);

void low_system_bat_charge_disable(void);

/*
 * 获取电池的所有信息
 */
int32_t low_system_battery_info_get(battery_info_t *info);

typedef void(*low_timer_cb)(int32_t ms);

/*
 * 定动定时器，应用层只需要1个定时器,精度要求为1ms
 * dur_ms:
 *  定时周期
 * cb:
 *  定时冋调回调函数
 * retval:
 *  0:ok
 * -1:failed
 */
int32_t low_system_timer_start(int32_t dur_ms, low_timer_cb cb);

/*
 * 关闭启动的定时器
 */
void low_system_timer_stop(void);

void low_system_timer_pause(void);

void low_system_timer_resume(void);

#define SYS_PWRSW_OFF  0
#define SYS_PWRSW_ON   1
/*
 * 如果系统开是拔动开关的情况下实现这个函数
 * retval:
 *  SYS_PWRSW_OFF
 *  SYS_PWRSW_ON
 */
int32_t low_system_pwr_switch_chk(void);

#define SYS_PWRBTN_REL  0
#define SYS_PWRBTN_PUS  1
/*
 * 如果系统开关是按钮的话,实现这个函数
 * retval:
 *  SYS_PWRBTN_REL
 *  SYS_PWRBTN_PUS
 */
int32_t low_system_pwr_button_chk(void);

/*
 * 进入待机状态,能立即恢复运行,取决于平台是否支持
 */
void low_system_pwr_standby(void);


#define SYS_SLEEP_SRC_TIMER         0           //唤醒源定时器
#define SYS_SLEEP_SRC_POWERKEY      1           //唤醒源电源键

//执行休眠的函数，目前只能在main函数（非子线程)中调用
void low_sys_pwr_sleep_do();

/*
 * 进入休眠状态,恢复的时候可以有一定的延时，取决于平台是否支持
 * sleep_s[in]:
 *  配置系统休眠时间
 * retval:
 *  SYS_SLEEP_SRC_TIMER / SYS_SLEEP_SRC_POWERKEY
 */
int32_t low_system_pwr_sleep(int32_t sleep_s);

/*
 * 系统关机,进入最低功耗状态，恢复的时候可以是重启或者上电
 */
void low_system_pwr_down(void);

/*
 * 系统重启
 */
void low_system_reboot(void);

uint32_t low_system_random_number(void);

uint32_t low_system_timestamp_get(void);

int32_t low_system_timestamp_set(uint32_t stamp);

int32_t low_system_get_storage_size(int64_t *sto_size, int64_t *usable_size);

//手动释放缓存
#define MEM_PAGE_CACHES  1
#define MEM_SLAB_CACHES  2
#define MEM_ALL_CACHES   3
/* 查看剩余内存
 * drop_level : 手动释放缓存, 0:不释放，其他见上面的宏定义
 * retval ： 剩余的内存，单位是kB
 */
int32_t low_system_get_mem_avaliable(int32_t drop_level);
#if UNIT_TEST
/* 查看可用内存
 * retval ： 可用的内存，单位是kB
 */
int32_t low_system_get_mem_available();
/* 查看可用Swap
 * retval ： 可用的Swap，单位是kB
 */
int32_t low_system_get_swap_available();
#endif
/*
 * 打印内存占用情况
 */
void low_system_show_mem_info(void);

#define SYS_GPIO_PIN0       0
#define SYS_GPIO_PIN1       1
/*
 * 控制一个gpio输出高电平
 * pin:
 */
void low_system_gpio_pin_high(int32_t pin);

/*
 * 控制一个gpio输出低电平
 * pin:
 */
void low_system_gpio_pin_low(int32_t pin);

int32_t low_system_check_poweron_mode();

typedef enum {
    SYSTEM_START_UP_TYPE_NORMAL = 1, // 正常启动
    SYSTEM_START_UP_TYPE_SCAN_KEY,  // 快扫启动
    SYSTEM_START_UP_TYPE_MAX,
} system_start_up_type_e;

system_start_up_type_e low_system_get_start_up_type(void);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif

