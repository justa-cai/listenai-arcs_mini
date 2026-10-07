/*
 * Gamepad: 模块内部共享接口 (discovery / ws_server / udp / input / ble_pad 之间)。
 */
#ifndef GAMEPAD_INTERNAL_H
#define GAMEPAD_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* gamepad_input.c -------------------------------------------------- */

/* 手柄按键事件入口。key 为协议键名 (up/down/left/right/a/b/select/start)。
 * 一次"按下"边沿翻译成一次 miniapp_button_click(); 方向键按住会周期性重复
 * (手柄上按住方向键要能连续移动)。返回 false 表示未知键名。 */
bool gamepad_input_key(const char *key, bool pressed);

/* UDP 全量位图 (§4.6, 弱网自愈语义 —— 最新状态即真相)。
 * 上升沿与 gamepad_input_key(pressed=true) 等价。 */
void gamepad_input_set_mask(uint16_t mask);

/* 断线/会话结束: 松开全部按键 (含停掉方向键重复), 不退出小应用
 * (WiFi 抖动 / APP 短暂重连不应中断使用) */
void gamepad_input_on_disconnect(void);

/* 显式命令 (§4.2): exit = 退出小应用; reset = 回到小应用首页 (等价 back 按键) */
void gamepad_input_cmd_exit(void);
void gamepad_input_cmd_reset(void);

/* 小应用状态 (供 welcome / state / announce 上报) */
void gamepad_get_game_state(bool *running, uint32_t *fps);

/* 手柄来源仲裁 —— BLE 直连优先, 网络手柄让位。
 * ble_pad 在连接/断开时置位; 网络侧 (WS/UDP) 的写入一律走 *_net_* 版本,
 * 让位期间它们整体被忽略 (不清键、不写位图), 从而不再破坏 BLE 的按住状态。 */
void gamepad_input_ble_set_active(bool active);
bool gamepad_input_ble_active(void);
bool gamepad_input_net_key(const char *key, bool pressed);
void gamepad_input_net_set_mask(uint16_t mask);
void gamepad_input_net_on_disconnect(void);

/* gamepad_util.c: 取本机 STA IP 点分字符串 (WiFi 未连返回 false) */
bool gamepad_util_get_local_ip(char *buf, uint32_t buf_len);

/* gamepad_discovery.c: 最近一次 UDP 发现探测的来源 IP (无则 false) */
bool gamepad_disc_peer_ip(char *buf, uint32_t buf_len);

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
