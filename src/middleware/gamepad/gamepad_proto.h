/*
 * Gamepad 协议常量 (与 doc/gamepad-protocol.md 对应)。
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

/* NES 1P 手柄位 (arcs-sdk/demos/arcs/game/nes/src/game_nes_input.h) */
#define GAMEPAD_KEY_A      (1U << 15)
#define GAMEPAD_KEY_B      (1U << 14)
#define GAMEPAD_KEY_SELECT (1U << 13)
#define GAMEPAD_KEY_START  (1U << 12)
#define GAMEPAD_KEY_UP     (1U << 11)
#define GAMEPAD_KEY_DOWN   (1U << 10)
#define GAMEPAD_KEY_LEFT   (1U << 9)
#define GAMEPAD_KEY_RIGHT  (1U << 8)

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_PROTO_H */
