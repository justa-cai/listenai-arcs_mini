#include "stdint.h"
#include "stdbool.h"

#define TAG "volume"

#include "lisa_log.h"
#include "lisa_kv.h"

#include "kv.h"
#include "voice_player_comm.h"
#include "service_volume.h"

/* ---- 配置 --------------------------------------------------------------- */

/*
 * 音量范围说明：
 *   DEFAULT_VOLUME       — 无 KV 记录时的默认音量（Kconfig 配置）
 *   DEFAULT_MIN_VOLUME   — 无 KV 记录时的默认最低音量下限（0–10）
 *   MIN_VOLUME_LIMIT     — 最低音量可设置的上限，防止用户把"最低"设得过高
 *                           导致无法静音（例如 min=80 时永远听不到安静模式）
 */
#define DEFAULT_VOLUME     CONFIG_VOICE_PLAYER_DEFAULT_VOLUME
#define DEFAULT_MIN_VOLUME 10
#define MIN_VOLUME_LIMIT   10

/* ---- 内部工具函数 ------------------------------------------------------- */

/**
 * @brief   从 KV 读取用户设定的最低音量下限
 * @return  最低音量值（0–MIN_VOLUME_LIMIT）
 */
static int get_min_volume(void)
{
    int min = DEFAULT_MIN_VOLUME;
    lisa_kv_get_int(KV_KEY_USER_MIN_VOLUME, &min);
    if (min < 0) min = 0;
    if (min > MIN_VOLUME_LIMIT) min = MIN_VOLUME_LIMIT;
    return min;
}

/**
 * @brief   在 min_volume–100 范围内钳位音量值
 */
static int clamp_volume(int volume)
{
    int min = get_min_volume();
    if (volume < min) volume = min;
    if (volume > 100) volume = 100;
    return volume;
}

/* ---- 公开 API ----------------------------------------------------------- */

/**
 * @brief   初始化音量服务
 *
 * @note    voice_player_comm 在 SYS_INIT 阶段已从 KV 恢复音量并通过异步线程应用，
 *          此处仅做 min_volume 约束的补校，确保启动时音量不低于用户设定的下限。
 */
void service_volume_init(void)
{
    int current = voice_player_get_system_volume();
    int clamped = clamp_volume(current);

    if (clamped != current) {
        LOGI("clamping init volume %d -> %d", current, clamped);
        voice_player_set_system_volume(clamped);
    }

    LOGI("volume init: %d (min: %d)", clamped, get_min_volume());
}

/**
 * @brief   设置音量（持久化到 KV 并异步应用）
 * @param   volume 音量值
 *
 * @note    钳位到 min_volume–100 后写入 KV，委托 voice_player_comm 异步应用到所有播放器。
 */
void service_volume_set(int volume)
{
    volume = clamp_volume(volume);
    lisa_kv_set_int(KV_KEY_USER_VOLUME, volume);
    voice_player_set_system_volume(volume);
    LOGI("volume set: %d", volume);
}

/**
 * @brief   获取当前系统音量
 * @return  当前音量值
 *
 * @note    直接返回 voice_player_comm 中维护的当前值，不读 KV。
 */
int service_volume_get(void)
{
    return voice_player_get_system_volume();
}

/**
 * @brief   临时设置音量（不持久化 KV）
 * @param   volume 音量值
 *
 * @note    用于 UI 滑块拖拽过程中的实时预览——用户松手后由 service_volume_set
 *          写入 KV 完成持久化。仍走异步线程，避免直接操作 player。
 */
void service_volume_set_temp(int volume)
{
    volume = clamp_volume(volume);
    voice_player_set_system_volume(volume);
    LOGI("volume temp: %d", volume);
}

/**
 * @brief   从 KV 恢复音量（覆盖当前值）
 *
 * @note    典型场景：取消 UI 滑块操作后恢复到之前的持久化值。
 */
void service_volume_restore_from_kv(void)
{
    int volume = DEFAULT_VOLUME;
    lisa_kv_get_int(KV_KEY_USER_VOLUME, &volume);
    voice_player_set_system_volume(clamp_volume(volume));
    LOGI("volume restored from kv: %d", volume);
}

/**
 * @brief   相对调节音量
 * @param   delta 变化量（正数增加，负数减少）
 */
void service_volume_adjust(int delta)
{
    service_volume_set(service_volume_get() + delta);
}
