/*
 * Gamepad 按键 -> 小应用按键事件。
 *
 * 所有手柄来源 (WS 差分事件 / UDP 全量位图 / BLE 直连) 最终都汇到这里的
 * "按下集合" (s_pressed, 线上格式位图), 由本模块翻译成 miniapp_button_click()。
 *
 * 为什么需要翻译而不是直接转发:
 *   - 小应用的输入模型是「离散点击事件」而不是「位图」: 单击=下一个, 双击=确认。
 *     手柄的 A/B 键天然是"点击", 直接映射即可。
 *   - 方向键不同: 桌面上"按住右键连续走 6 个图标"是基本操作, 但按住不发事件。
 *     所以按下瞬间发一次, 之后按 GAMEPAD_REPEAT_MS 周期重复 —— 即键盘的
 *     "自动重复" 语义。
 *   - 方向键不同: 桌面上"按住右键连续走 6 个图标"是基本操作, 但按住不发事件。
 *     所以按下瞬间发一次, 之后按 GAMEPAD_REPEAT_MS 周期重复 —— 即键盘的
 *     "自动重复" 语义。
 *   - 确认键 (A / START) 直接发 function_double。小应用里"激活"就是这个 id:
 *     设备只有一个功能键, 单击已经被"下一个"占用, 所以它靠双击产生激活;
 *     手柄有独立按键, 按下即激活, 不需要也跟着双击。
 *
 * 线程: WS / UDP / BLE 三个任务都会调用, 状态用 volatile 位图 + 临界区保护,
 * 重复定时器由 FreeRTOS timer 任务回调触发。
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"
#include "lisa_log.h"

#include "miniapp.h"

#define TAG "gamepad"

/* 方向键自动重复: 首次按下后等 DELAY 再开始, 之后每 PERIOD 重复一次。
 * 200ms 约等于"桌面滑过一格"的手感; 再快会在列表里刹不住车。 */
#define GAMEPAD_REPEAT_DELAY_MS  420
#define GAMEPAD_REPEAT_PERIOD_MS 180

/* 键名 -> 按下集合位 (线上格式) */
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

/* 按下集合: 位在线 = 按着。所有来源共写。 */
static volatile uint16_t s_pressed = 0;

/* 方向键自动重复的状态 */
static volatile uint16_t s_repeat_mask = 0;   /* 正在重复的方向键 */
static volatile bool s_repeat_started = false;/* 是否已越过首延迟 */
static uint32_t s_repeat_periods = 0;

/* 小应用状态上报 */
static volatile bool s_app_running = false;
static volatile uint32_t s_app_fps = 0;

static TimerHandle_t s_repeat_timer = NULL;
static bool s_timer_failed = false;

/* ================================================================== */
/* 小应用状态                                                           */
/* ================================================================== */

static void gamepad_app_state_update(void)
{
    s_app_running = miniapp_is_active();
    s_app_fps = s_app_running ? GAMEPAD_FPS_REPORT : 0;
}

void gamepad_get_game_state(bool *running, uint32_t *fps)
{
    gamepad_app_state_update();
    if (running) {
        *running = s_app_running;
    }
    if (fps) {
        *fps = s_app_fps;
    }
}

/* ================================================================== */
/* 重复定时器                                                           */
/* ================================================================== */

static void gamepad_repeat_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    uint16_t mask = s_repeat_mask;

    if (mask == 0) {
        return;
    }
    if (!s_repeat_started) {
        /* 首个周期是"长按确认延迟", 到点才开始重复, 本身不发事件 */
        s_repeat_started = true;
        s_repeat_periods = 0;
        return;
    }
    s_repeat_periods++;

    /* 只重复方向; 确认键不做自动重复 (按住 A 不该连续进应用) */
    uint16_t dirs = (uint16_t)(mask & GAMEPAD_KEY_DIRS);
    if (dirs & GAMEPAD_KEY_UP)    (void)miniapp_button_click(MINIAPP_BUTTON_ID_UP);
    if (dirs & GAMEPAD_KEY_DOWN)  (void)miniapp_button_click(MINIAPP_BUTTON_ID_DOWN);
    if (dirs & GAMEPAD_KEY_LEFT)  (void)miniapp_button_click(MINIAPP_BUTTON_ID_LEFT);
    if (dirs & GAMEPAD_KEY_RIGHT) (void)miniapp_button_click(MINIAPP_BUTTON_ID_RIGHT);
}

static void gamepad_repeat_restart(void)
{
    if (s_timer_failed) {
        return;
    }
    if (!s_repeat_timer) {
        s_repeat_timer = xTimerCreate("gp.repeat", pdMS_TO_TICKS(GAMEPAD_REPEAT_PERIOD_MS),
                                      pdTRUE, NULL, gamepad_repeat_timer_cb);
        if (!s_repeat_timer) {
            s_timer_failed = true;
            LISA_LOGW(TAG, "repeat timer create failed, direction keys will not auto-repeat");
            return;
        }
    }
    s_repeat_started = false;
    s_repeat_periods = 0;
    /* 先停再起: 周期从"最后一次按下方向"重新算, 含首延迟 */
    (void)xTimerStop(s_repeat_timer, 0);
    (void)xTimerChangePeriod(s_repeat_timer, pdMS_TO_TICKS(GAMEPAD_REPEAT_DELAY_MS), 0);
    (void)xTimerStart(s_repeat_timer, 0);
}

static void gamepad_repeat_stop(void)
{
    s_repeat_mask = 0;
    s_repeat_started = false;
    s_repeat_periods = 0;
    if (s_repeat_timer) {
        (void)xTimerStop(s_repeat_timer, 0);
    }
}

/* ================================================================== */
/* 按键翻译                                                             */
/* ================================================================== */

/* 确认键 (A / START): 一次按下 = 一次"激活"(function_double)。
 *
 * 小应用把 function_double 当作"激活" —— 桌面上是进入当前焦点应用, 应用内是
 * 执行当前焦点命令。设备功能键靠双击产生它(只有一个键, 单击已被"下一个"占用);
 * 手柄有独立的确认键, 按下即激活, 不再要求双击。 */
static void gamepad_confirm_pressed(void)
{
    (void)miniapp_button_click(MINIAPP_BUTTON_ID_FUNCTION_DOUBLE);
}

/* 一次按下边沿 -> 事件。 */
static void gamepad_apply_press(uint16_t added)
{
    /* 只在"按下边沿"打一条: 保活重发时 added 恒为 0, 不会刷屏。
     * 这行是手柄侧的唯一观测点 —— 有它说明"帧收到了、按键也翻出来了"，
     * 没它而 blepad status 的 frames 在涨，说明是键名/位映射的问题。 */
    LISA_LOGI(TAG, "press mask=0x%04X", (unsigned)added);

    if (added & GAMEPAD_KEY_UP)    (void)miniapp_button_click(MINIAPP_BUTTON_ID_UP);
    if (added & GAMEPAD_KEY_DOWN)  (void)miniapp_button_click(MINIAPP_BUTTON_ID_DOWN);
    if (added & GAMEPAD_KEY_LEFT)  (void)miniapp_button_click(MINIAPP_BUTTON_ID_LEFT);
    if (added & GAMEPAD_KEY_RIGHT) (void)miniapp_button_click(MINIAPP_BUTTON_ID_RIGHT);
    if (added & GAMEPAD_KEY_B)     (void)miniapp_button_click(MINIAPP_BUTTON_ID_BACK);
    if (added & (GAMEPAD_KEY_A | GAMEPAD_KEY_START)) gamepad_confirm_pressed();
    if (added & GAMEPAD_KEY_SELECT) (void)miniapp_button_click(MINIAPP_BUTTON_ID_SETTINGS);

    /* 方向键按住 -> 自动重复; 松开由 gamepad_apply_release 重算 */
    uint16_t dirs = (uint16_t)(added & GAMEPAD_KEY_DIRS);
    if (dirs) {
        s_repeat_mask = (uint16_t)(s_repeat_mask | dirs);
        gamepad_repeat_restart();
    }
}

static void gamepad_apply_release(uint16_t removed)
{
    if (removed & GAMEPAD_KEY_DIRS) {
        s_repeat_mask = (uint16_t)(s_repeat_mask & ~removed);
        if ((s_repeat_mask & GAMEPAD_KEY_DIRS) == 0) {
            gamepad_repeat_stop();
        }
    }
}

/* ================================================================== */
/* 输入入口                                                             */
/* ================================================================== */

bool gamepad_input_key(const char *key, bool pressed)
{
    if (!key) {
        return false;
    }
    for (uint32_t i = 0; i < GAMEPAD_KEY_MAP_COUNT; i++) {
        if (strcmp(s_key_map[i].name, key) != 0) {
            continue;
        }
        uint16_t bit = s_key_map[i].mask;
        taskENTER_CRITICAL();
        uint16_t before = s_pressed;
        if (pressed) {
            s_pressed = (uint16_t)(before | bit);
        } else {
            s_pressed = (uint16_t)(before & ~bit);
        }
        taskEXIT_CRITICAL();

        if (pressed) {
            if (!(before & bit)) {
                gamepad_apply_press(bit);
            }
        } else if (before & bit) {
            gamepad_apply_release(bit);
        }
        return true;
    }
    LISA_LOGW(TAG, "unknown key: %s", key);
    return false;
}

/* UDP 全量位图: 最新状态即真相。上升沿按下、下降沿松开, 与差分事件等价。 */
void gamepad_input_set_mask(uint16_t mask)
{
    taskENTER_CRITICAL();
    uint16_t before = s_pressed;
    s_pressed = mask;
    taskEXIT_CRITICAL();

    uint16_t added = (uint16_t)(mask & ~before);
    uint16_t removed = (uint16_t)(before & ~mask);

    if (added) {
        gamepad_apply_press(added);
    }
    if (removed) {
        gamepad_apply_release(removed);
    }
}

void gamepad_input_on_disconnect(void)
{
    taskENTER_CRITICAL();
    s_pressed = 0;
    taskEXIT_CRITICAL();
    gamepad_repeat_stop();
}

/* {"t":"cmd","c":"exit"}: 直接退出小应用, 回原生 Home */
void gamepad_input_cmd_exit(void)
{
    LISA_LOGI(TAG, "cmd exit -> close miniapp");
    gamepad_input_on_disconnect();
    (void)miniapp_exit();
}

/* {"t":"cmd","c":"reset"}: 小应用没有 NES 的"主机复位"语义, 映射成"回首页"
 * (与 B 键一致) —— 卡在某个应用里时, 手柄/APP 都有一个确定性的脱身入口。 */
void gamepad_input_cmd_reset(void)
{
    LISA_LOGI(TAG, "cmd reset -> miniapp home");
    gamepad_input_on_disconnect();
    (void)miniapp_button_click(MINIAPP_BUTTON_ID_BACK);
}

/* ================================================================== */
/* 来源仲裁: BLE 直连优先, 网络手柄让位                                  */
/* ================================================================== */

/*
 * 按键有两条来源: BLE 直连 (ble_pad.c) 与网络 (WS 差分包 / UDP 全量位图)。
 * BLE 连上后网络侧必须整体让位, 否则网络侧会持续破坏 BLE 的按键状态:
 *   - WS 客户端一连上就清位图;
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
        gamepad_input_on_disconnect();
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

void gamepad_input_net_on_disconnect(void)
{
    if (s_ble_active) {
        return;
    }
    gamepad_input_on_disconnect();
}
