#ifndef __ALARM_HANDLER_H__
#define __ALARM_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "alarm.h"
#include "alarm_types.h"

/**
 * @brief 初始化闹钟处理模块
 * @param user_callback 用户回调函数，闹钟触发时调用
 */
void alarm_handler_init(ls_alarm_user_callback_t user_callback);

/**
 * @brief 异步处理闹钟触发
 * @param fired_ts 触发的闹钟时间戳
 * @param cloud_id 云端闹钟 ID
 * @param now_ts 当前时间戳
 * @return 0 投递成功，非 0 投递失败
 */
int alarm_process_triggered_async(uint64_t fired_ts, uint64_t cloud_id, int64_t now_ts);

/**
 * @brief 处理用户单击稍后提醒
 * 单击：停止当前响铃，等待 snooze interval 后再次响铃
 */
void alarm_handle_snooze(void);

/**
 * @brief 处理用户双击关闭闹钟
 * 双击：停止闹钟，如果是循环闹钟则生成下一个实例
 */
void alarm_handle_stop_and_next(void);

/**
 * @brief 检查是否有活动的闹钟（包括待触发和snooze中）
 * @return true 有活动闹钟，false 没有活动闹钟
 */
bool alarm_has_active_alarms(void);

/**
 * @brief 指定 cloud_id 的闹钟被外部删除时，同步清理对应的 snooze 状态
 * @param cloud_id 云端闹钟 ID
 */
void alarm_handler_cancel_snooze_by_cloud_id(uint64_t cloud_id);

/**
 * @brief 获取当前响铃闹钟的snooze剩余次数
 * @return -1 没有snooze或查询失败，>=0 表示剩余次数
 */
int alarm_handler_get_snooze_remaining_count(void);

#ifdef __cplusplus
}
#endif

#endif // __ALARM_HANDLER_H__
