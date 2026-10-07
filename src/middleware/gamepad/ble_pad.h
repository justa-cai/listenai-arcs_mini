/*
 * ble_pad.h - CodexPad-S10 BLE 手柄（Central 角色）驱动
 *
 * 对接协议见 codex_pad_s10/INTEGRATION_GUIDE.zh-CN.md：
 *   扫描(名字前缀 CodexPad- + MFG 0xFFFF+"CodexPad" 头)
 *   → 连接(7.5~10ms/latency5/timeout1s)
 *   → GATT 发现 0xFFA0/0xFFA1
 *   → 写 CCCD 订阅 → 8 字节输入帧 [u32 按键 LE][LX][LY][RX][RY]
 *   → gamepad_input_set_mask() 并入共享位图（与 WS/UDP 手柄并存）
 *
 * 位图最终由 gamepad_input 翻译成小应用按键事件 (miniapp_button_click),
 * 与网络手柄共用同一条输入通路。
 *
 * 实现基于 SDK 官方 lisa_ble_client（GATT 客户端）+ lisa_ble_api（扫描/连接），
 * 线程模型对齐官方 central demo：BLE 回调（BT task 上下文）只投递事件，
 * 状态机与所有 lisa_ble_* / lisa_ble_client_* 调用在独立 app task 中执行。
 */
#ifndef BLE_PAD_H
#define BLE_PAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 启动驱动（幂等）：初始化 GATT 客户端 + 开始扫描。返回 0 成功。 */
int ble_pad_start(void);

/* 停止驱动：断连（若连着）+ 停扫 + 注销 GATT 客户端。 */
int ble_pad_stop(void);

bool ble_pad_is_running(void);

/* --- 状态查询（shell 用） --- */
typedef struct {
    bool running;
    bool connected;
    char peer_addr[18];     /* "XX:XX:XX:XX:XX:XX" */
    int8_t last_rssi;
    uint32_t frames;        /* 收到的输入帧数 */
    uint32_t scan_matches;  /* 广播命中次数 */
    uint8_t conidx;
    uint8_t last_buttons;   /* 调试: 最近帧按键低 8 位 */
    uint8_t last_lx, last_ly;
} ble_pad_status_t;

void ble_pad_get_status(ble_pad_status_t *out);

/* shell 子命令入口（同步，内部投递到 app task） */
void ble_pad_shell_cmd(const char *sub);

#ifdef __cplusplus
}
#endif

#endif /* BLE_PAD_H */
