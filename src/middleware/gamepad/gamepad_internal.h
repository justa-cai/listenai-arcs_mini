/*
 * Gamepad: WS server 内部共享接口 (discovery / ws_server / input 之间)。
 */
#ifndef GAMEPAD_INTERNAL_H
#define GAMEPAD_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* gamepad_input.c */
bool gamepad_input_key(const char *key, bool pressed);
void gamepad_input_reset(void);
void gamepad_input_set_mask(uint16_t mask);   /* UDP 全量位图 (§4.6) */
void gamepad_input_set_exit(void);
void gamepad_input_set_reset(void);
void gamepad_input_on_disconnect(void);
void gamepad_get_game_state(bool *running, uint32_t *fps);

/* gamepad_util.c: 取本机 STA IP 点分字符串 (WiFi 未连返回 false) */
bool gamepad_util_get_local_ip(char *buf, uint32_t buf_len);

/* gamepad_udp.c: 当前 UDP 会话收包统计 (PC 端算丢包率用);
 * 无活跃 peer 返回 false */
bool gamepad_udp_get_stats(uint32_t *last_seq, uint32_t *rx_frames);

/* 获取设备 ID 字符串 (无则 "unknown") */
const char *gamepad_util_get_device_id(void);

/* 获取固件版本字符串 (sdk_version) */
const char *gamepad_util_get_fw_version(void);

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_INTERNAL_H */
