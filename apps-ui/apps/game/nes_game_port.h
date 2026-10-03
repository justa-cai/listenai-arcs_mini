/*
 * NES 游戏 port 接口 (平台无关)。
 * PC 由 port/sim/nes_port_sim.c 实现 (SDL); 设备由 port/lisa/ 实现 (lisa_*)。
 */
#ifndef NES_GAME_PORT_H
#define NES_GAME_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "nes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 显示目标接入: 把 NES 帧缓冲挂到 lv_img (零拷贝/双缓冲由 port 内部决定) */
void nes_game_port_attach(nes_t *nes, lv_obj_t *img);
void nes_game_port_detach(void);

/* 主线程周期调用 (LVGL timer): 交换帧缓冲 + 采集输入 */
void nes_game_port_tick(void);

/* 模拟线程启停: start 后核心在独立线程 nes_run(), stop 阻塞至退出 */
int nes_game_port_start(nes_t *nes);
void nes_game_port_stop(void);

/* 模拟线程是否已退出 (Esc/quit 后为 true) */
bool nes_game_port_is_done(void);

/* 逻辑帧率 (fps 统计, 每 2s 刷新) */
uint32_t nes_game_port_get_logic_fps(void);

#ifdef __cplusplus
}
#endif

#endif /* NES_GAME_PORT_H */
