/*
 * Gamepad 按键状态: WS 线程写入, NES 游戏帧读取。
 *
 * 模型 (与 nes_port_sim.c 的短按锁存一致):
 *   - s_pressed: 持续按住集合 (WS 按下置位 / 抬起清位)
 *   - s_sticky:  锁存集合 (出现过按下的键置位; 游戏帧消费后清零)
 *   - 帧掩码 = s_pressed | s_sticky, 保证短于一个逻辑帧 (16.7ms) 的按键不丢。
 *
 * 线程安全: uint16 位图对齐写在该平台为单指令原子; 锁存清零只发生在
 * NES 游戏帧线程 (唯一消费者), WS 线程只做 OR 置位, 无丢失风险。
 * 退出标志同理 (单写者置位, 单读者消费)。
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "gamepad.h"
#include "gamepad_proto.h"
#include "lisa_log.h"

#define TAG "gamepad"

static volatile uint16_t s_pressed = 0;
static volatile uint16_t s_sticky = 0;
static volatile bool s_exit_req = false;
static volatile bool s_reset_req = false;
static volatile bool s_game_running = false;
static volatile uint32_t s_game_fps = 0;

typedef struct {
    const char *name;
    uint16_t mask;
} gamepad_key_map_t;

static const gamepad_key_map_t s_key_map[] = {
    { "up",     GAMEPAD_KEY_UP },
    { "down",   GAMEPAD_KEY_DOWN },
    { "left",   GAMEPAD_KEY_LEFT },
    { "right",  GAMEPAD_KEY_RIGHT },
    { "a",      GAMEPAD_KEY_A },
    { "b",      GAMEPAD_KEY_B },
    { "select", GAMEPAD_KEY_SELECT },
    { "start",  GAMEPAD_KEY_START },
};

#define GAMEPAD_KEY_MAP_COUNT (sizeof(s_key_map) / sizeof(s_key_map[0]))

/* WS 线程: 解析键名并更新状态; 返回 false 表示未知键名 */
bool gamepad_input_key(const char *key, bool pressed)
{
    if (!key) {
        return false;
    }
    for (uint32_t i = 0; i < GAMEPAD_KEY_MAP_COUNT; i++) {
        if (strcmp(s_key_map[i].name, key) == 0) {
            if (pressed) {
                s_pressed |= s_key_map[i].mask;
                s_sticky |= s_key_map[i].mask;
            } else {
                s_pressed &= (uint16_t)~s_key_map[i].mask;
            }
            return true;
        }
    }
    LISA_LOGW(TAG, "unknown key: %s", key);
    return false;
}

/* WS 线程: 手柄位图清零 (reset / 断线) */
void gamepad_input_reset(void)
{
    s_pressed = 0;
    s_sticky = 0;
}

/* UDP 线程: 全量位图覆盖 (§4.6, 弱网自愈语义 —— 最新状态即真相)。
 * 上升沿进锁存, 与 WS 差分事件的短按语义一致。 */
void gamepad_input_set_mask(uint16_t mask)
{
    uint16_t rising = (uint16_t)(mask & ~s_pressed);
    s_pressed = mask;
    s_sticky |= rising;
}

/* WS 线程: 请求退出游戏 */
void gamepad_input_set_exit(void)
{
    s_exit_req = true;
}

/* WS 线程: 请求复位游戏 (NES 主机 Reset, 设备重载当前 ROM) */
void gamepad_input_set_reset(void)
{
    s_reset_req = true;
}

/* NES 游戏帧: 读位图 (按住 + 锁存), 消费锁存 */
uint16_t gamepad_get_joypad_mask(void)
{
    uint16_t mask = (uint16_t)(s_pressed | s_sticky);
    s_sticky = 0;
    return mask;
}

/* NES 游戏帧: 消费退出请求 */
bool gamepad_consume_exit(void)
{
    bool req = s_exit_req;
    s_exit_req = false;
    return req;
}

/* NES 游戏帧: 消费复位请求 */
bool gamepad_consume_reset(void)
{
    bool req = s_reset_req;
    s_reset_req = false;
    return req;
}

/* 断线/会话结束时由 ws server 调用: 仅清位图防粘键, 不退出游戏
 * (WiFi 抖动 / APP 短暂重连不应中断游戏; 主动退出走 {"t":"cmd","c":"exit"}) */
void gamepad_input_on_disconnect(void)
{
    gamepad_input_reset();
}

/* ================================================================== */
/* 手柄来源仲裁: BLE 直连优先, 网络手柄让位                              */
/* ================================================================== */

/*
 * 按键有两条来源: BLE 直连 (ble_pad.c) 与网络 (WS 差分包 / UDP 全量位图)。
 * BLE 连上后网络侧必须整体让位, 否则网络侧会持续破坏 BLE 的按键状态:
 *   - WS 客户端一连上就 gamepad_input_reset();
 *   - UDP 通道 500ms 收不到帧就按"发送端失联"清零全部按键;
 *   - UDP 帧全量覆盖按下集合。
 * 而 BLE 手柄是事件驱动上报, 按住期间不发帧, 被清掉后没法自己写回来 ——
 * 表现就是"按住不放却断掉"。
 * 因此这里按来源分入口: 网络侧走 gamepad_input_net_* (让位时整体忽略),
 * BLE 侧继续用原名接口 (不受影响)。
 */
static volatile bool s_ble_active = false;

bool gamepad_input_ble_active(void)
{
    return s_ble_active;
}

void gamepad_input_ble_set_active(bool active)
{
    if (active == s_ble_active) {
        return;
    }

    s_ble_active = active;
    if (active) {
        /* 交棒给 BLE 前先清一次: 网络侧可能正按着键, 避免残留成"幽灵按键" */
        gamepad_input_reset();
        LISA_LOGI(TAG, "ble pad active, network gamepad input disabled");
    } else {
        LISA_LOGI(TAG, "ble pad inactive, network gamepad input enabled");
    }
}

bool gamepad_input_net_key(const char *key, bool pressed)
{
    if (s_ble_active) {
        return false;
    }
    return gamepad_input_key(key, pressed);
}

void gamepad_input_net_set_mask(uint16_t mask)
{
    if (s_ble_active) {
        return;
    }
    gamepad_input_set_mask(mask);
}

void gamepad_input_net_reset(void)
{
    if (s_ble_active) {
        return;
    }
    gamepad_input_reset();
}

void gamepad_input_net_on_disconnect(void)
{
    if (s_ble_active) {
        return;
    }
    gamepad_input_on_disconnect();
}

/* 游戏状态上报 */
void gamepad_notify_game_state(bool running, uint32_t fps)
{
    s_game_running = running;
    s_game_fps = fps;
}

void gamepad_get_game_state(bool *running, uint32_t *fps)
{
    if (running) {
        *running = s_game_running;
    }
    if (fps) {
        *fps = s_game_fps;
    }
}
