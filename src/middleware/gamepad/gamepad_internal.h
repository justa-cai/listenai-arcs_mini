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

/* gamepad_input.c: 手柄来源仲裁 —— BLE 直连优先, 网络手柄让位。
 * ble_pad 在连接/断开时置位; 网络侧 (WS/UDP) 的写入一律走 *_net_* 版本,
 * 让位期间它们整体被忽略 (不清键、不写位图), 从而不再破坏 BLE 的按住状态。 */
void gamepad_input_ble_set_active(bool active);
bool gamepad_input_ble_active(void);
bool gamepad_input_net_key(const char *key, bool pressed);
void gamepad_input_net_set_mask(uint16_t mask);
void gamepad_input_net_reset(void);
void gamepad_input_net_on_disconnect(void);

/* gamepad_util.c: 取本机 STA IP 点分字符串 (WiFi 未连返回 false) */
bool gamepad_util_get_local_ip(char *buf, uint32_t buf_len);

/* gamepad_ws_server.c: 最近一次 WebSocket 手柄会话的对端 IP (无则 false) */
bool gamepad_ws_peer_ip(char *buf, uint32_t buf_len);

/* gamepad_discovery.c: 最近一次 UDP 发现探测的来源 IP。
 * 有自报为桌面 GUI 的来源时优先返回它, *is_gui 标志该来源是否自报为 GUI */
bool gamepad_disc_peer_ip(char *buf, uint32_t buf_len, bool *is_gui);

/* gamepad_discovery.c: 上述 GUI 来源自报的 ROM HTTP API 端口, 0 = 未自报 */
uint16_t gamepad_disc_gui_http_port(void);

/* gamepad_discovery.c: 设备主动广播 {"t":"discover_server"} 找 PC 端 ROM 服务,
 * 收到 {"t":"server"} 应答则记录并返回 true (0 超时用默认 800ms)。
 * 会阻塞调用者 (需 select 等待), 只能在工作线程里调。 */
bool gamepad_disc_find_server(uint32_t timeout_ms);

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
