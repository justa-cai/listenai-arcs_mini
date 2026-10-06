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

/* --- 对端 (PC 端 pad_gui) 地址 (romlib 模块调用) ---
 * 设备侧有两处能"确认" PC 端地址, 择优返回:
 *   ① UDP 发现探测的来源 (pad_gui 在探测里自报 "client":"...gui" 与 ROM API 端口);
 *   ② WebSocket 手柄会话的对端 IP。
 * 有了 ①, 即使从未建立过 WS 会话 (或 WS 对端是手机 APP), 也能定位 ROM 库服务。
 * 成功返回 true 并写入 ip_buf; *http_port 为对端自报的 ROM HTTP API 端口,
 * 未自报时为 0 (表示"用调用方的默认端口")。http_port 可传 NULL。 */
bool gamepad_get_server_addr(char *ip_buf, uint32_t ip_len, uint16_t *http_port);

/* 同上, 但在「无从得知」时会**主动广播探测** PC 端 ROM 服务
 * ({"t":"discover_server"} -> {"t":"server"}), 因此不依赖 pad_gui 先扫描过设备。
 * 会阻塞最多约 0.8s (仅在没有缓存时), 只能在工作线程调用。 */
bool gamepad_find_server_addr(char *ip_buf, uint32_t ip_len, uint16_t *http_port);

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_H */
