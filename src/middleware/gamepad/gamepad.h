/*
 * Gamepad: WebSocket / BLE 手柄接入对外接口。
 *
 * 链路: APP --BLE 配网(已有)--> 设备入网
 *       APP --UDP 38201 发现--> 拿到设备 IP
 *       APP --WS 38200 /gamepad 或 UDP 38202--> 按键事件
 *       手柄 --BLE 直连(Central)--> 按键事件
 *       以上全部 → gamepad_input 翻译成小应用按键 (miniapp_button_click)
 *
 * 与小应用的关系: 本模块只产生"按键事件", 不关心小应用内部语义。键名到
 * button_id 的映射见 gamepad_input.c, button_id 取值见 apps/arcs-mini/miniapp/miniapp.h。
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

/* 启动 UDP 发现 + UDP 按键通道 + WebSocket server (幂等, 可重复调用) */
int gamepad_init(void);

/* 手柄此刻是否真的在手 (BLE 直连已订阅成功)。
 * 仅供能力上报判断"要不要列出方向/确认等按键": 网络手柄是会话期间才存在的,
 * 不在这里体现 —— 它连上就会发按键事件, 脚本收得到就行。 */
bool gamepad_pad_active(void);

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_H */
