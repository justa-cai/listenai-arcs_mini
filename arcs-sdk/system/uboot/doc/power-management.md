# Power Guard 与开关机状态机设计文档

## 版本信息

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-04 | 整理 stage0/stage1/app 三阶段电源管理状态机；记录 AON SW reset 的 cascade 范围限制与 POWER_EN AON IOMUX 锁定方案 |
| 1.1 | 2026-04 | boot_info bit 重排避免与 boot_control_store BOOT_MODE/UPGRADE_STATUS 冲突；stage1 OTA 重启统一切 CMN reset 并通过 resume_normal_boot 防误触发 charging_wait；gate 在 req+ota_pending 同时置位时跳过 HARD_REQ 兜底；guard 移除无意义且会卡死的 watchdog feed；stage0 OTA pending 续跑检查仅在 USB 供电时执行 |
| 1.2 | 2026-04 | stage0 OTA pending 检查改用本地 `__boot_ramcode__` 实现，只读 magic+version；移除 USB 供电限制（电池冷启动也能触发 OTA 续跑） |
| 1.3 | 2026-04 | stage1 触发 CMN reset 前统一 lock latch 到 AON IOMUX force-output（封装在 main.c `main_reboot_cmn()` 内），避免电池场景 GPIO 外设复位窗口里 latch 失 drive 导致设备掉电 |

---

## 1. 概述

### 1.1 硬件背景

板载电源拓扑为 PMOS latch 配合 USB VBUS 双供电源结构。POWER_KEY (PB)
按下接地触发开机；PWR_LOCK (GPIO) 输出高电平维持 PMOS 导通，使 PB 释放
后 VCC 仍保持；VBUS 提供独立于 PMOS 的备选 VCC 来源。

由此引出的约束：

1. 冷启动来源仅有 PB 按下后由 stage0 及时拉高 PWR_LOCK，或 USB 插入提
   供 VBUS 两种。
2. 电池模式关机通过拉低 PWR_LOCK 实现，VCC 立即跌落。
3. USB 接入时拉低 PWR_LOCK 无效，VBUS 仍维持 VCC，需要进入"假关机自旋
   等待 PB 长按"状态或等待用户拔出 USB。
4. 复位过程中若 PWR_LOCK GPIO 出现失驱窗口（CMN reset 复位 GPIO 外设），
   电池模式下 Q1 栅极电容可能在 stage0 重新拉高 PWR_LOCK 之前放电，导
   致 VCC 跌落。

power guard 子系统的目的是用一致的方式覆盖以上所有组合。

涉及到的 GPIO（默认配置，可通过 Kconfig 重定义）：

| 信号 | 默认引脚 | 方向 | 有效电平 | 用途 |
|---|---|---|---|---|
| POWER_KEY | PB4 | 输入 | active-low | guard 长按计时；stage1 charging UI 轮询 |
| PWR_LOCK | PB3 | 输出 | active-high | guard latch_set —— 拉高维持 PMOS 导通 |
| USB_DET | PB0 | 输入 | active-high | guard 区分电池/USB 启动 |
| LED | PB1 | 输出 | active-low | guard 期间熄灭，确认启动 app 时点亮（CONFIG_BOOT_POWER_GUARD_LED） |

### 1.2 三阶段总览

```
+---------------------------+
|  stage0  boot_f           |  裸寄存器 + mcycle 计时
|  - POR / 软复位识别        |  __boot_ramcode__ 全部驻留 SRAM
|  - 入口 PB+USB 组合判定    |  无 FreeRTOS / 驱动 / 显示
|  - wait_pb_long_press 自旋|
|  - shutdown_req 假关机    |
|  - stage_gate 路由        |
+-------------|-------------+
              |
   has_app_intent ? ──── no ──→  jump to AP image (normal boot)
              |
             yes
              ↓
+---------------------------+
|  stage1  boot_s           |  FreeRTOS + display + 驱动
|  (recovery_basic 等)      |
|  - charging_wait UI       |
|  - recovery shell + ADB   |
|  - OTA lifecycle          |
+-------------|-------------+
              |
        complete / handoff
              ↓
+---------------------------+
|  app  CP / 业务固件        |  power_manager + uboot_*_api
|  - power_shutdown         |
|  - power_reboot_soft      |
|  - uboot_shutdown_request |
+---------------------------+
```

---

## 2. 状态载体：boot_info @ REG_AON_DIG_RSVD4

跨阶段、跨复位的一次性意图保存在 AON 域 RSVD4 寄存器（32 bit），声明
见 [boot_config.h:59](../src/boot_config.h#L59)：

```c
struct boot_info {
    uint32_t reboot_cnt: 8;             // bits 0-7
    uint32_t recover_reason: 8;         // bits 8-15
    uint32_t ota_pending: 1;            // bit 16，BOOT_MODE bit 0 别名
    uint32_t shutdown_req: 1;           // bit 17
    uint32_t upgrade_status: 2;         // bits 18-19，boot_control_store UPGRADE_STATUS 共址
    uint32_t charging_wait: 1;          // bit 20
    uint32_t resume_normal_boot: 1;     // bit 21
    uint32_t reserved: 7;
    uint32_t handshake_timeout: 1;      // bit 29
    uint32_t boot_wdt: 1;               // bit 30
    uint32_t req: 1;                    // bit 31
};
```

bits 16-19 与 [boot_control_store_*.c](../src/boot_control_store_flash.c) 的
BOOT_MODE / UPGRADE_STATUS 字段共址，是历史 OTA 入口设计带来的；新增字段
（charging_wait / resume_normal_boot 等）必须避开这两个区域，对应
boot_control_store 的 `BOOT_MODE_MASK` (= 0x1) 与 `UPGRADE_STATUS_MASK`
(= 0x3) 也按枚举实际范围收紧，互不越界。

各字段的写入方与消费方：

| 字段 | 含义 | 写入方 | 消费方 |
|---|---|---|---|
| `req` | gate 路由开关：进入 stage1 的总入口 | guard / app / WDT | gate（进入 stage1 时立即清除） |
| `recover_reason` | stage1 应当展示的 recovery 类型 | guard / gate / app | stage1 main_task |
| `ota_pending` | OTA handoff 待处理（与 BOOT_MODE_UPDATE 共址） | app `uboot_ota_start` 触发的 `boot_ota_handoff_request_upgrade_reboot` / boot_control_store_set_mode | gate / stage1 OTA handler |
| `shutdown_req` | app 请求 stage0 进入假关机自旋 | app `uboot_shutdown_request` / stage1 charging_wait 超时 | guard 入口 |
| `upgrade_status` | OTA 进行状态，boot_control_store 内部用 | boot_control_store API | boot_control_store API（boot_info 视图不消费） |
| `charging_wait` | 冷启动插 USB，stage1 应当展示充电提示 | guard | gate / stage1 main_task |
| `resume_normal_boot` | stage1 已确认开机意图，guard 跳过冷启动判定 | stage1 charging_wait 内 PB 长按 / stage1 OTA 完成前 | guard warm 分支（消费后清除） |
| `boot_wdt` | CP_WDT 触发标记 | reset_cause_apply | gate transient_recovery 转 RECOVER_REASON |
| `handshake_timeout` | AP/CP 握手超时标记 | reset_cause_apply | 同上 |
| `reboot_cnt` | AP_SW_WDT 累计重启计数 | reset_cause_apply | 累计达阈值进 recovery |

`recover_reason` 自身不触发 stage1，必须配合 `req=1`。第一次进入 stage1
后 gate 清除 `req`、保留 `recover_reason` 供 main_task 使用，确保后续复
位不会重复进入 recovery。

### 2.1 持久化备份：flash NVS

boot_info 位于 AON 域，POR 与 AON SW reset 都会将其清零。需要跨此类复
位保留的状态（典型为 OTA 中途被硬复位）由 `boot_ota_request` 在 flash
NVS 中保存。stage0 在 boot_info 无相关字段置位时回退检查 flash NVS（实
现见 §3.3 关于 PSRAM 段重定位的说明）。

---

## 3. 复位类型与硬件语义

### 3.1 SoC 复位入口

REG_SYSRST_STATUS 位定义见 `pmu_rstsrc_t`（POR=0、AON=1、CP_WDT=16、
CMN=17、CP_SW=18、AP_SW_WDT=19）。

| 复位入口 | 触发方式 | AON 寄存器 | POR_STATUS | 涉及域 | sysrst 位 |
|---|---|---|---|---|---|
| POR | VCC 重新上电 | 清零 | 置 1 | 全部 | bit 0 |
| AON SW reset | `IP_AON_CTRL->REG_AON_SW_RESET = 0xCAFE000A` | 清零 | 0 | AON 状态机为主，cascade 范围较窄 | bit 1 |
| CP_WDT | CP 看门狗超时 | 保留 | 0 | 见 reset_route_init | bit 16 |
| CMN SW reset | `__HAL_PMU_WholeChip_RST_ENABLE()` | 保留 | 0 | CMN/AP/CP（显式 cascade） | bit 17 |
| CP_SW | CP 软复位 | 保留 | 0 | CP | bit 18 |
| AP_SW_WDT | AP 看门狗超时 | 保留 | 0 | 见 reset_route_init | bit 19 |

### 3.2 AON SW reset 与 CMN SW reset 的差异

`sys_platform_sw_full_reset()` 内部写 `REG_AON_SW_RESET`，行为接近"软
POR"：清零 AON 寄存器（含 boot_info）、复位 AP 核与取指、不显式触发
CMN/CP 子系统 cascade reset、POR_STATUS 不被置位。部分外设（SPI flash
controller、USB device controller、cache 状态等）可能保留 AON SW reset
之前的运行态。

CMN SW reset 显式 cascade 到 CMN/AP/CP 三个域，flash controller 跟着干
净复位。AON SW reset 已不在业务路径中使用（参见 §3.4）。

### 3.3 stage0 外设访问限制

stage0 (boot_f) 没有任何外设 init 阶段，只能用 boot ROM 留下的默认状
态。这给 stage0 加了几条硬限制：

1. **不要写 CP_WDT 寄存器**：stage0 没有任何 watchdog 在跑（`boot_watchdog_init`
   在 stage1 main 里才调），本来也不需要 feed；纯 POR 后控制器还在
   default 状态，写 `wdt_hw->REG_WREN.all` 会卡死 AHB 总线。guard 内部
   不调用 `boot_watchdog_feed()`。
2. **stage0 调用的代码必须落在 `.boot_f.ramcode` 段（SRAM）**：SDK 通过
   `listenai_code_relocate` 把绝大多数 system 函数放进 `.psram.text` 段
   （PSRAM 地址 0x2830xxxx），运行时由 stage1 入口 `scatload_psram` 从
   flash 拷贝到 PSRAM。stage0 在 scatload 之前执行，PSRAM 内容未初始化，
   跳进任意 `.psram.text` 函数都会跑飞 / 死循环。具体表现：直接调用
   `boot_ota_request_has_pending()` 链路（has_pending → ota_request_load
   → control_store_load → read_store → calculate_crc32）会挂在第一个
   PSRAM 跳转上。需要在 stage0 检查 OTA pending 的场景必须在
   `boot.c` 内手写一份 `__boot_ramcode__` 实现（见
   `stage0_ota_has_pending`），只走 `boot_flash_read`（其本身在
   `.boot_f.ramcode`）。
3. **flash 数据访问通过 `boot_flash_read`**：boot ROM 配的 flash 控制器
   支持 XIP 取指；非 XIP 数据读用 `boot_flash_read`（在 `.boot_f.ramcode`
   段），实测对纯电池 / USB 冷启动均稳定。
4. **PSRAM 已在 guard 之前 init**（`PSRAM_Initialize` 在 boot_f 入口
   段调用），但 `.psram.text` 段内容还没有，仍然受限于第 2 条。
5. **GPIO 通过 `iomux_to_gpio` 现拉现用**，每次 `read_pin` / `write_pin`
   会重写 IOMUX，安全。

### 3.4 复位类型选用规则

| 调用方 | 复位入口 | 理由 |
|---|---|---|
| stage1 内部清状态后 reboot（OTA 完成 / charging_wait 失败 / handshake 失败） | `__HAL_PMU_WholeChip_RST_ENABLE()` | 保留 boot_info 供下一轮使用，CMN cascade 范围完整 |
| app 任意 reboot | `power_reboot_soft()` → `sys_arch_reboot(SOFT)` | 同上，且包含 POWER_EN AON 锁定窗口 |
| app 软关机 | `uboot_shutdown_request()` | 置位 `shutdown_req` 后 CMN reset，进入 guard 假关机自旋 |
| recovery shell `reboot` | `__HAL_PMU_WholeChip_RST_ENABLE()` | 同上；不再区分 hard / soft |
| 不应使用 | `sys_platform_sw_full_reset()` (AON SW reset) | 仅历史接口与 stage0 自身的 CP 跳板保留；业务代码不应直接调用 |

---

## 4. Stage0 (boot_f / power_guard) 状态机

入口位于 [boot_power_guard.c:209](../src/boot_power_guard.c#L209)，全部
位于 ramcode 段，仅使用裸寄存器访问。

### 4.1 决策流程

```
                         boot_power_guard_run
                                  │
                          led_set(0)
                                  │
                  ┌───────────────┴───────────────┐
                  ▼                               ▼
         shutdown_req == 1                    其它
                  │                               │
        latch_set(0) + 清除标志                   │
        等待 PB 释放                              │
        wait_pb_long_press()                      │
                                                  ▼
                        is_cold = (sysrst & POR) != 0
                        has_app_intent = req | ota_pending |
                                         boot_wdt | handshake_timeout |
                                         resume_normal_boot |
                                         (recover_reason != NONE)
                                                  │
                  ┌───────────────────────────────┼───────────────────┐
                  ▼                               ▼                   ▼
        !is_cold OR has_app_intent      cold + USB + PB         cold + USB + !PB
        (warm reset / 有意图)            (PB+USB 组合手势)        (关机态插 USB)
                  │                               │                   │
        latch_set(1) + led_set(1)        mark_recovery()       charging_wait=1
        清除 resume_normal_boot          (req=1+reason=HARD_REQ)  latch_set(0)
        return → gate                    return → gate           return → gate
                                                                       │
                                                                       ▼
                                                                cold + 其它
                                                                (PB 启动)
                                                                       │
                                                                wait_pb_long_press
                                                                ─ recovery: mark_recovery
                                                                ─ normal:   led_set(1)
```

### 4.2 wait_pb_long_press 行为

入口先 `latch_set(0)`（PB 不达标时立即关机）。检测到 PB 按下后采样
USB 状态作为 `usb_at_entry`，进入 3 秒计时循环：

- PB 释放 → 退出循环，回到外层等待。
- `usb_at_entry == 0` 且期间 USB 插入 → recovery 手势，立即退出。
- 计时满 → 根据是否进入 recovery 决定 latch 处理。

recovery 路径不主动 latch，原因是 recovery 模式以 USB 持续在插为前提，
VCC 由 VBUS 维持，用户拔出 USB 自动完成关机。

### 4.3 状态总表

| 复位类型 | boot_info | PB | USB | 动作 |
|---|---|---|---|---|
| 冷启动 | - | 0 | 0 | 不可达（无 VCC 源） |
| 冷启动 | - | 0 | 1 | charging_wait=1 → stage1 接管 |
| 冷启动 | - | 1 | 0 | wait_pb_long_press 计时 |
| 冷启动 | - | 1 | 1 | mark_recovery 直接进 recovery |
| 软复位 | shutdown_req=1 | - | - | latch(0) + 等 PB 释放 + 长按计时 |
| 软复位 | has_app_intent | - | - | latch(1) + led(1) + return → gate |
| 软复位 | 全 0 | - | - | 同上（warm 分支默认路径） |

---

## 5. Stage_gate 路由

[boot_stage_gate.c:62](../src/boot_stage_gate.c#L62) 决定 guard 退出后
是否进入 stage1，按以下优先级：

1. `req=1`：调用 transient_recovery 将 boot_wdt / handshake_timeout 转
   为对应 RECOVER_REASON。若两者都没置位但 `ota_pending=1`，认为是
   OTA 路由意图，跳过 HARD_REQ 兜底（避免 stage1 误渲染 recovery UI 而
   不进 OTA handler）；其它情况兜底 HARD_REQ。清除 req，进入 stage1。
2. `ota_pending=1`：进入 stage1 OTA handler。
3. `charging_wait=1`：进入 stage1 charging UI。
4. boot_config 中的持久 recovery 标记非零：写入 HARD_REQ 后进入 stage1。
5. flash NVS 中存在未完成的 `boot_ota_request`（`stage0_ota_has_pending`
   只读 magic+version，详见 §3.3）：进入 stage1 续跑 OTA。
6. 其它：跳转 AP image 进入 normal boot。

---

## 6. Stage1 (main_task) 的电源相关行为

实现见 [main.c](../src/main.c) `main_task`，电源相关分支有两个。

### 6.1 charging_wait 处理

见 [main.c:281](../src/main.c#L281)。初始化 display、显示充电提示、3 秒
倒计时同时以 100ms 粒度轮询 PB。计时期间 PB 长按 3 秒 → 置位
`resume_normal_boot`；否则 → 置位 `shutdown_req`。最后通过 CMN reset
让 stage0 接管。

设计原因：guard 入口已 `latch_set(0)`，charging UI 期间设备依靠 VBUS
维持 VCC。开机与关机两条路径在 stage0 已分别由 warm 分支与假关机自旋
实现，stage1 通过一次 CMN reset 复用 stage0 既有逻辑，避免重复实现。
此处必须使用 CMN reset 而非 AON SW reset，以保留刚写入的字段。

### 6.2 OTA 路径

stage1 OTA handler ([main.c:215](../src/main.c#L215) `boot_ota_try_handle_update`)
有三处会触发重启：

1. **stale handoff 清理**：boot_info 上有残留 ota_pending 但 flash NVS
   不在 UPDATE 模式 → 清除 ota_pending 后 reset。
2. **lifecycle UPDATED**：OTA 成功完成 → 清除 ota_pending 后 reset 回
   app。
3. **lifecycle FAILED**：OTA 失败 → 清除 ota_pending、置位 req=1（让
   下一轮进 recovery） → reset。

三处都改用 `__HAL_PMU_WholeChip_RST_ENABLE()` (CMN reset)，因为 stage1
此时 display + ADB + USB 全开，AON SW reset 仅复位 AP 核而不 cascade
CMN/CP，可能让外设处于不一致状态（参见 §3.2）。

UPDATED / stale handoff 这两条路径在 reset 前还要置 `resume_normal_boot=1`
—— 因为本芯片上 CMN reset 也会顺手置 POR_STATUS，guard 看到 POR=1 +
boot_info 全 0 会判成"冷启动 + USB + 无 PB"误进 charging_wait 路径。
`resume_normal_boot=1` 让 has_app_intent=true，guard 走 warm 分支直接
return 给 gate。

FAILED 路径不需要 resume_normal_boot：req=1 本身就让 has_app_intent=true。

---

## 7. App 侧公共 API

### 7.1 `uboot_shutdown_request()`

定义见 [api/uboot_power_api.c](../api/uboot_power_api.c)。置位
`shutdown_req` 后做 CMN reset。`power_shutdown()` 在检测到 USB 插入时
调用此函数；纯电池场景下 `power_shutdown` 直接拉低 PWR_LOCK 触发 VCC
跌落。

### 7.2 `sys_arch_reboot(SOFT)`

定义见 arcs-sdk/soc/arcs/common/sys_reboot.c。当 boot 同时启用
OTA + POWER_GUARD feature 时，函数会先通过 AON IOMUX force-output 将
POWER_EN 锁定为高电平，再走 CMN reset 路径；否则按照 type 走原本的
soft / hard 分支。

POWER_EN AON force-output 的必要性：CMN reset 复位 GPIO 外设，常规 GPIO
output 在 stage0 重新接管之前存在数毫秒的失驱窗口。电池模式下 Q1 栅极
电容无法在该窗口内维持，导致 VCC 跌落。AON 域的 IOMUX force-output 不
受 CMN reset 影响，可贯穿整个窗口维持 PB3 输出。

`power_init()` 在 app 启动早期需清除该 force-output 并切回常规 GPIO 控
制；否则后续 `power_shutdown` 与 stage0 `latch_set` 都将无法控制该引
脚。

### 7.3 `power_reboot_soft()`

委托给 `sys_arch_reboot(SYS_REBOOT_SOFT)`。应用代码触发 reboot 应统一
调用此函数，不应直接调用 `sys_platform_sw_full_reset()` 或写入复位寄存
器。

---

## 8. 端到端场景

### 8.1 冷启动：纯电池长按开机

用户按 PB → Q1 导通 → VCC 上电 → guard `is_cold=true`、PB=1、USB=0 →
进入 wait_pb_long_press。PB 持续按住满 3 秒 → return false → guard 设置
latch + LED → gate 看到 boot_info=0 → normal boot。

### 8.2 冷启动：USB 插入触发

用户插入 USB → VBUS 上电 → guard `is_cold=true`、PB=0、USB=1 → 置位
`charging_wait`、`latch_set(0)`、return 给 gate → stage1 显示充电提示。

3 秒倒计时期间 PB 长按 3 秒 → 置 `resume_normal_boot` → CMN reset →
guard warm 分支 → normal boot。

3 秒到达且 PB 未长按 → 置 `shutdown_req` → CMN reset → guard 进入假关
机自旋。

### 8.3 软关机：电池模式

`power_shutdown()` 检测到 USB 未插 → `power_latch_set(false)` → PWR_LOCK
拉低 → VCC 跌落。

### 8.4 软关机：USB 插入中

`power_shutdown()` 检测到 USB 在插 → `uboot_shutdown_request()` →
`shutdown_req=1` + CMN reset → guard 进入 shutdown_req 分支 →
`latch_set(0)` 后等待 PB 释放并进入 `wait_pb_long_press`。

后续 PB 长按 3 秒 → normal boot；用户拔出 USB → VCC 跌落。

### 8.5 OTA 升级（USB 或电池均成立）

app 调 `uboot_ota_start_from_flash`：写 flash NVS（mode=UPDATE + 请求
记录） + 置 boot_info `req=1, ota_pending=1`。然后 `power_reboot_soft()`
→ `sys_arch_reboot(SOFT)` → CMN reset，**boot_info 保留**。

guard 看到 has_app_intent=true → warm 分支 → return。gate 命中 `req=1`
分支：因 `ota_pending=1` 跳过 HARD_REQ 兜底，清 req 后进入 stage1。
stage1 `boot_ota_try_handle_update` 检测 mode=UPDATE，跑
lifecycle handler 烧入。

完成后清 ota_pending + 置 resume_normal_boot=1 → CMN reset。回到
stage0 时 boot_info 仅剩 resume_normal_boot 一位，guard 走 warm 分支
（有 app_intent）+ 消费 resume_normal_boot → return。gate 全 0 → 跳到
AP image。

### 8.6 OTA 中途断电（电池 vs USB 续起）

app 触发 OTA → CMN reset → stage1 OTA 跑到一半电池断电 → cold POR。
boot_info 全 0（POR 清 AON），但 flash NVS 里 `boot_ota_request` 仍
是 UPDATE 模式。后续视用户怎么唤起设备而定：

- **接电池冷启动 + 长按 PB**：guard 走 wait_pb_long_press，gate fall
  through 到 `stage0_ota_has_pending` 检查（§3.3 本地实现，不依赖
  PSRAM），命中 → 进 stage1 OTA handler 续跑。OTA 完成后正常回 app。
- **接 USB 触发冷启动**：guard 走 charging_wait → stage1 入口
  resume_normal_boot → CMN reset 回 stage0 → 第二次 stage0 同样命中
  `stage0_ota_has_pending` → 进 stage1 OTA handler 续跑。
- 任一路径下，若 AP 镜像因半截烧录损坏到 `boot_default_app_is_valid`
  失败的程度，会被 `boot_default_app_prepare_recovery` 标
  `RECOVER_REASON_APP_INVALID`，gate 优先进 recovery UI 提示用户走
  ADB 救援；OTA pending 检查作为 normal-boot 路径的兜底。

### 8.7 Recovery 模式 reboot

recovery shell 命令执行 → `__HAL_PMU_WholeChip_RST_ENABLE()` → CMN
reset 保留 AON。guard 看到 `recover_reason` 仍为 HARD_REQ（首次进入
recovery 时被填入，gate 进入 stage1 时未清除），但 `req` 已被 gate 清
除 → has_app_intent=true → warm 分支 → return。gate 检查 req / ota_pending /
charging_wait / persistent recovery 全 0 → normal boot。

未使用 AON SW reset 的原因见 §3.2：cascade 范围窄，外设可能保留先前状态。

---

## 9. 实施约束

1. **保留意图时使用 CMN reset**：CMN reset 不复位 AON，boot_info 得以
   保留。AON SW reset 等同软 POR，会清除 boot_info，需要依赖 flash NVS
   作为持久化备份。
2. **业务代码统一使用 `power_reboot_soft` / `uboot_shutdown_request`**：
   不应直接调用 `sys_platform_sw_full_reset` 或写入复位寄存器。
3. **POWER_EN 在 CMN reset 期间必须有源**：电池模式下 Q1 栅极一旦失驱
   即跌落 VCC。`sys_arch_reboot` 已封装 AON IOMUX force-output 锁定，
   调用即可。
4. **App 启动早期必须清除 AON IOMUX force-output**：上一轮 reboot 留下
   的 force-output 不清除将导致常规 GPIO 控制失效。`power_gpio_init`
   已处理。
5. **stage1 向 stage0 传递信息只能写 boot_info**：`charging_wait` /
   `resume_normal_boot` / `shutdown_req` 均使用此机制。写入后通过 CMN
   reset 让 stage0 接管。
6. **历史 `sys_platform_sw_full_reset` 不应在新代码中使用**：stage1
   OTA 三个出口已从 AON SW reset 切到 CMN reset；唯一保留的合法用途是
   stage0 内部的 CP 跳板。
7. **PB 长按阈值在 stage0 / stage1 / app 三处均为 3 秒**，修改时三处需
   同步。
8. **stage0 不能调任何依赖 stage1 init 的外设访问**：参见 §3.3 的硬限制
   清单（无 WDT feed、有限的 flash 数据访问等）。
9. **boot_info 加新字段必须避开 bits 16-21**：这段是
   boot_control_store BOOT_MODE / UPGRADE_STATUS 的共址区域，越界会被
   OTA API 写到导致误触发。新字段加在 reserved 段（bits 22-28），并
   按需收紧 boot_control_store mask。

---

## 10. 配置项参考

stage0 power guard：`CONFIG_BOOT_POWER_GUARD`、
`CONFIG_BOOT_POWER_GUARD_LED`、`CONFIG_BOOT_POWER_GUARD_KEY_*`、
`CONFIG_BOOT_POWER_GUARD_LATCH_*`、`CONFIG_BOOT_POWER_GUARD_USB_*`、
`CONFIG_BOOT_POWER_GUARD_LED_*`、`CONFIG_BOOT_POWER_GUARD_HOLD_TIME_MS`、
`CONFIG_BOOT_POWER_GUARD_SAMPLE_INTERVAL_MS`。注意 `CONFIG_BOOT_CHECK_POWER_KEY`
是旧版本的 GPIO recovery 检测路径，启用 POWER_GUARD 时需关闭。

App / SDK 侧：`CONFIG_BOOT_FEATURES_API` 必须启用，`sys_arch_reboot`
依赖它识别 OTA + POWER_GUARD 组合。

---

## 11. 相关源码索引

| 文件 | 作用 |
|---|---|
| [src/boot_power_guard.c](../src/boot_power_guard.c) | stage0 状态机主体 |
| [src/boot_stage_gate.c](../src/boot_stage_gate.c) | guard 之后的路由决策 |
| [src/boot_reset_cause.c](../src/boot_reset_cause.c) | 将 sysrst_status 转换为 boot_info 字段 |
| [src/boot.c](../src/boot.c) | stage0 入口、stage_gate 调用、stage0 本地 OTA pending 检查 |
| [src/boot_config.h](../src/boot_config.h) | `struct boot_info` 布局定义 |
| [src/boot_control_store_flash.c](../src/boot_control_store_flash.c) / [api/boot_control_store_lisa_flash.c](../api/boot_control_store_lisa_flash.c) | OTA 控制存储 BOOT_MODE / UPGRADE_STATUS 写入（与 boot_info 共址需保持 mask 收紧） |
| [src/main.c](../src/main.c) | stage1 charging_wait / OTA / recovery main_task |
| [src/boot_recovery_shell_cmds.c](../src/boot_recovery_shell_cmds.c) | recovery shell `reboot` 命令 |
| [api/uboot_power_api.c](../api/uboot_power_api.c) | `uboot_shutdown_request` |
| arcs-sdk/soc/arcs/common/sys_reboot.c | `sys_arch_reboot` 与 POWER_EN 锁定 |
| src/middleware/power/power_manager.c (业务侧) | `power_shutdown` / `power_reboot_soft` 入口 |
