/*
 * Gamepad 协议常量 (与 doc/gamepad-protocol.md 对应)。
 *
 * 这里的 KEY_* 位序是 **线上格式**: WS 的 k 事件用键名, UDP 的 8 字节帧用
 * 这个 16 位掩码, pad_gui 与手机端都照它实现, 所以不能改。它同时充当设备内部
 * 的"按下集合"表示 —— 消费端 (gamepad_input.c) 再把它翻译成小应用的 button_id。
 */
#ifndef GAMEPAD_PROTO_H
#define GAMEPAD_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#define GAMEPAD_WS_PORT_DEFAULT      38200
#define GAMEPAD_DISC_PORT_DEFAULT    38201
#define GAMEPAD_UDP_PORT_DEFAULT     38202
#define GAMEPAD_WS_PATH              "/gamepad"
#define GAMEPAD_PROTO_VERSION        1

/* UDP 按键帧 (8 字节, 大端): magic/ver/seq32/mask16 (协议 §4.6) */
#define GAMEPAD_UDP_FRAME_MAGIC      0xA5
#define GAMEPAD_UDP_FRAME_VER        0x01
#define GAMEPAD_UDP_FRAME_LEN        8

#define GAMEPAD_DEV_NAME             "arcs-mini"
#define GAMEPAD_DEV_BRAND            "ARCS"

/* 手柄位 (沿用原 NES 1P 布局: 线上格式, 不可改) */
#define GAMEPAD_KEY_A      (1U << 15)
#define GAMEPAD_KEY_B      (1U << 14)
#define GAMEPAD_KEY_SELECT (1U << 13)
#define GAMEPAD_KEY_START  (1U << 12)
#define GAMEPAD_KEY_UP     (1U << 11)
#define GAMEPAD_KEY_DOWN   (1U << 10)
#define GAMEPAD_KEY_LEFT   (1U << 9)
#define GAMEPAD_KEY_RIGHT  (1U << 8)

/* 方向键集合 (自动重复只对它们生效) */
#define GAMEPAD_KEY_DIRS   (GAMEPAD_KEY_UP | GAMEPAD_KEY_DOWN | \
                            GAMEPAD_KEY_LEFT | GAMEPAD_KEY_RIGHT)

/* 帧率上报值: 小应用 tick 恒为 20ms */
#define GAMEPAD_FPS_REPORT 50u

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_PROTO_H */
