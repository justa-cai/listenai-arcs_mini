/*
 * PC (sim) 端 autoconf 桩: SDK Kconfig 生成的 autoconf.h 在 PC 上不存在,
 * 这里提供与设备 demo 等价的 CONFIG_GAME_NES_* 宏, 供 nes_conf.h 消费。
 *
 * 注意: 不要打开 CONFIG_GAME_NES_CORE_PROFILE_ENABLE —— 会拉入 nmsis_core.h (RISC-V 专用)。
 */
#ifndef PC_AUTOCONF_H
#define PC_AUTOCONF_H

#define CONFIG_GAME_NES_AUDIO_ENABLE                1
#define CONFIG_GAME_NES_AUDIO_FAST_S16_COPY         1
#define CONFIG_GAME_NES_FRAME_SKIP                  1
#define CONFIG_GAME_NES_DRAW_FPS_TARGET             0
#define CONFIG_GAME_NES_PROFILE_LOG_MS              2000
#define CONFIG_GAME_NES_PERF_LOG_ENABLE             0
#define CONFIG_GAME_NES_CHR_PACKED_DYNAMIC_MAX_BYTES 0

#define CONFIG_GAME_NES_FAST_VISIBLE_RENDER         1
#define CONFIG_GAME_NES_FAST_PPU_PACKED_PATTERN     1
#define CONFIG_GAME_NES_FAST_CPU_BRANCH             1
#define CONFIG_GAME_NES_FAST_CPU_JMP_IDLE           1
#define CONFIG_GAME_NES_FAST_SKIP_SPRITES           1

#define CONFIG_GAME_NES_MAPPER_INFONES_ENABLE       1
#define CONFIG_GAME_NES_MAPPER_INFONES_DEFAULT_SET  1

#endif /* PC_AUTOCONF_H */
