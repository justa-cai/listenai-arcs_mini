/*
 * Gamepad ROM 动态加载: WS 推送的 ROM 暂存与生命周期管理。
 *
 * 数据流 (WS 线程):
 *   rom_begin(size)  -> 分配 PSRAM 暂存缓冲, 进入传输态
 *   rom_data(chunk)  -> 二进制帧追加 (累计须等于 size)
 *   rom_end()        -> 校验 iNES magic / crc32 / size -> 换入 staging
 *   rom_cancel()     -> 丢弃进行中的传输
 *
 * 消费 (LVGL 线程, nes_game_presenter):
 *   gamepad_rom_acquire(&buf,&size) -> 取当前 staged ROM, 返回代数 gen (0=无)
 *   gamepad_rom_staged_gen()        -> 只查代数 (tick 检测热重启用)
 *   gamepad_rom_retire(gen)         -> 声明 <=gen 的 staged 不再被核心引用
 *
 * 生命周期 (nes_load_rom 只取指针不拷贝, PRG/CHR 直接 XIP 进缓冲):
 *   - 当前 staging 一直有效, 直到被新 ROM 替换或设备重启;
 *   - 替换时若游戏在跑 (还在读旧缓冲), 旧缓冲成为 zombie,
 *     等 presenter restart/quit 后 retire() 释放; 没在跑则立即释放。
 */
#ifndef GAMEPAD_ROM_H
#define GAMEPAD_ROM_H

#include <stddef.h>
#include <stdint.h>

/* 与 nes_rom flash 分区容量对齐 (见 res/arcs-mini/partition_table.json) */
#define GAMEPAD_ROM_MAX_BYTES (1024U * 1024U)

/* --- WS 线程调用 (gamepad_ws_server.c) --- */
int  gamepad_rom_begin(uint32_t size, uint32_t crc32);
int  gamepad_rom_data(const uint8_t *data, uint32_t len);
int  gamepad_rom_end(const char **err_msg); /* 成功返回 0, 失败 *err_msg 指向静态错误串 */
void gamepad_rom_cancel(void);

/* --- LVGL 线程调用: 对外声明在 gamepad.h (nes_game_presenter 只 include 它) --- */

#endif /* GAMEPAD_ROM_H */
