#ifndef __SIP_SCREEN_H__
#define __SIP_SCREEN_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * sip_screen - LCD 画面回传（调试工具）。
 *
 * 把设备"此刻屏幕上实际显示的像素"抓成 RGB565，软件 JPEG 压缩后按
 * 自定义 UDP 分片协议发往主机（screen_sink 落盘查看），远程看屏。
 *
 * 抓帧点在 LVGL 层（lv_snapshot 重绘当前屏幕到离屏缓冲），必须跑在
 * UI 线程 —— 通过注册的 grab 回调实现：模型层把 snapshot 投递 worq.ui
 * 执行，完成后信号量放行。回调在 sip_screen 任务上下文被同步调用。
 */

/* 在 UI 线程完成抓帧：把当前屏幕写入 dst（RGB565，cap 字节），
 * 成功时填 w/h 并返回 true。 */
typedef bool (*sip_screen_grab_cb_t)(void *dst, uint32_t cap,
                                     uint16_t *w, uint16_t *h, void *user);

void sip_screen_set_grab_cb(sip_screen_grab_cb_t cb, void *user);

/* 启动/停止回传（port 0 = 默认 5070）。重复 start 只更新目标。 */
int sip_screen_start(const char *peer_ip, uint16_t port);
int sip_screen_stop(void);

#endif
