/*
 * Gamepad: WebSocket 手柄接入对外接口。
 *
 * 链路: APP --BLE 配网(已有)--> 设备入网
 *       APP --UDP 38201 发现--> 拿到设备 IP
 *       APP --WS 38200 /gamepad--> 按键事件 -> gamepad_input 位图
 *       NES 每逻辑帧经 gamepad_get_joypad_mask() 读取。
 *
 * 协议规范: doc/gamepad-protocol.md
 */
#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 启动 UDP 发现服务 + WebSocket server (幂等, 可重复调用) */
int gamepad_init(void);

/* --- NES 游戏帧调用 (nes_port_lisa.c) --- */

/* 当前手柄位图: 持续按住 + 短按锁存 (游戏帧消费后清锁存)。
 * 位定义见 arcs-sdk/demos/arcs/game/nes/src/game_nes_input.h (1P 高 8 位)。 */
uint16_t gamepad_get_joypad_mask(void);

/* 消费一次退出请求 (WS {"t":"cmd","c":"exit"}); 有请求返回 true 且清除标志 */
bool gamepad_consume_exit(void);

/* 消费一次复位请求 (WS {"t":"cmd","c":"reset"}, NES 主机 Reset 语义:
 * 设备重载当前 ROM); 有请求返回 true 且清除标志 */
bool gamepad_consume_reset(void);

/* --- 游戏状态上报 (game presenter 调用, 供 WS state 报文) --- */
void gamepad_notify_game_state(bool running, uint32_t fps);

/* --- ROM 动态加载 (game presenter 调用, 见 doc/gamepad-protocol.md §4.6) ---
 * nes_load_rom 只取指针不拷贝, staged 缓冲在游戏运行期间必须保持有效,
 * 由 gamepad_rom 模块按代数 (gen) + zombie 机制管理生命周期。 */

/* 取当前 staged ROM; 返回代数 (0=无推送过, *buf/*size 不动)。
 * 调用即声明「本代正被核心加载」, 替换它前必须先 retire。 */
uint32_t gamepad_rom_acquire(const uint8_t **buf, uint32_t *size);

/* 只查当前代数 (tick 里检测有新 ROM 需要热重启) */
uint32_t gamepad_rom_staged_gen(void);

/* 声明 <=gen 的 staged 不再被核心引用; 释放等待中的旧缓冲 (zombie) */
void gamepad_rom_retire(uint32_t gen);

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_H */
