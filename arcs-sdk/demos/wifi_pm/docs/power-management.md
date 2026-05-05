# 低功耗技术方案

本文档详细说明 WiFi PM Demo 的低功耗实现，包括睡眠进入、唤醒恢复、时间补偿和连接保活的完整技术细节。

## 两层功耗控制

本方案采用 **WiFi 层 + 系统层** 两层协同的低功耗策略：

| 层级 | 模式 | 机制 | 功耗效果 |
|------|------|------|---------|
| WiFi 层 | `WIFI_PS_MODE_DTIM` (listen_interval=10) | AP 端缓存数据，设备仅在每 10 个 DTIM beacon 时唤醒射频接收 | 射频大部分时间关闭 |
| 系统层 | `PM_MODE_LIGHT_SLEEP` | CPU 核心域断电，RAM 通过 retention 保持，AON 域维持运行 | CPU 不消耗动态功耗 |

```c
// WiFi 层省电
wifi_sta_set_listen_itv(10);              // 每 10 个 DTIM 醒一次 (WiFi 初始化时设置)
wifi_ps_mode_set(WIFI_PS_MODE_DTIM);     // MRPC IPC → CP 核 (低功耗开启时设置)
net_enable_keep_alive();                  // 启动网络层 ARP 保活 (低功耗开启时设置)

// 系统层省电
pm_config_t config = {
    .mode = PM_MODE_LIGHT_SLEEP,
};
pm_set_config(&config);
```

### WiFi PS 的硬件实现

WiFi PS 通过 MRPC IPC 发送到 CP 核，由 `wifi_ps_hw.c` 控制底层硬件：

```
wifi_ps_mode_set(WIFI_PS_MODE_DTIM)
  → MRPC IPC → CP 核
    → wifi_ps_hw_suspend():
        ls_rf_suspend()          关闭射频收发器
        使能 PLF doze 唤醒中断    MAC 可在 DTIM 时唤醒 AON
    → wifi_ps_hw_resume():
        ls_rf_resume()           恢复射频
        等待 WF_STATE_CURR == 0  WiFi PMU 状态机完成
```

---

## 低功耗开启时机

低功耗的开启有严格前提——**WiFi 连接 + IP 获取 + MQTT 就绪** 三者缺一不可：

```
               WiFi 连接成功
                    │
               DHCP 获取 IP
                    │
           MQTT 连接 + 订阅 + 发布
                    │
          set_low_power_enabled(true)  ← 此时才开启低功耗
                    │
          ┌─────────┴──────────┐
          │                    │
  enable_wifi_power_save()  enable_system_low_power()
  (WiFi DTIM PS 模式)       (PM Light Sleep 模式)
```

WiFi 断连时立即关闭低功耗，重连成功后重新开启：

```c
// 断连事件处理
case EVENT_WIFI_DISCONNECT:
case EVENT_WIFI_STA_CONNECT_FAIL:
    set_low_power_enabled(false);   // 立即退出低功耗
    schedule_wifi_reconnect();      // 触发重连

// 重连成功后
if (mqtt_subscribe + mqtt_publish 均成功) {
    set_low_power_enabled(true);    // 重新进入低功耗
}
```

### `set_low_power_enabled()` 防重入

内部通过 `g_low_power_enabled` 标志防止重复开启/关闭：

```c
static void set_low_power_enabled(bool enable) {
    if (enable) {
        if (g_low_power_enabled) return;  // 已开启，跳过
        enable_wifi_power_save();
        enable_system_low_power();
        g_low_power_enabled = true;
    } else {
        if (!g_low_power_enabled) return; // 已关闭，跳过
        disable_wifi_power_save();
        disable_system_low_power();
        g_low_power_enabled = false;
    }
}
```

---

## 睡眠进入流程

睡眠**不由应用主动触发**，而是通过 FreeRTOS 的 tickless idle 机制自动进入：

```
  应用调用 vTaskDelay(300ms) / xQueueReceive(阻塞) / ...
                     │
              所有任务都阻塞
                     │
         FreeRTOS 调度空闲任务 (idle task)
                     │
      vPortSuppressTicksAndSleep()     ← PM 框架重写的 tickless idle 钩子
                     │
              pm_can_sleep() 检查:
              ① mode == PM_MODE_LIGHT_SLEEP?
              ② lock_bits == 0? (无 PM 锁)
              ③ GPIO 空闲 > 1 秒?
              ④ 所有注册设备的 check_idle() 返回 idle?
                     │
                全部通过 → pm_light_sleep()
                未通过 → 普通 __WFI() (浅睡眠)
```

### `pm_light_sleep()` 详细步骤

```
 ┌──────────────── 睡前准备 ────────────────────────┐
 │                                                   │
 │  1. SysTimer_Stop()            停止系统定时器      │
 │  2. __disable_irq()            关全局中断          │
 │  3. log_flush()                刷日志缓冲区        │
 │  4. eTaskConfirmSleepModeStatus()                 │
 │     └─ FreeRTOS 最后确认：若有新中断则放弃睡眠     │
 │  5. sleep_time = vrtc_get_time_us()               │
 │     └─ 用 AON 32kHz 定时器记录入睡时刻             │
 │  6. system_timer = SysTimer_GetLoadValue()        │
 │     └─ 记录 RISC-V mtimer 当前值                   │
 │                                                   │
 ├──────────── 保存上下文 (两级保存) ────────────────┤
 │                                                   │
 │  7. 遍历注册设备，调用 on_enter() 回调             │
 │     └─ WiFi: wifi_ps_hw_suspend()                 │
 │        └─ ls_rf_suspend() 关闭射频收发器           │
 │  8. pm_save_context()                             │
 │     └─ 保存 UART / GPADC / IOMux 外设寄存器       │
 │  9. BootClock_save()                              │
 │     └─ 保存 PLL / 时钟树配置                       │
 │                                                   │
 ├──────────── 配置唤醒条件 ─────────────────────────┤
 │                                                   │
 │ 10. vrtc_set_timer(deadline)                      │
 │     └─ 设置 AON 定时器作为自动唤醒源               │
 │ 11. pm_set_wakeup_entry(__light_sleep_entry)      │
 │     └─ 写 AON 寄存器:                              │
 │        REG_AON_DIG_RSVD0 = 0xAA  ("请跳转到RAM")  │
 │        REG_AON_DIG_RSVD1 = 唤醒入口函数地址        │
 │ 12. pm_set_wakeup_source()                        │
 │     └─ 配置唤醒源: WiFi / GPIO / AON Timer         │
 │ 13. HAL_PMU_ConfigDeepSleepMode(MODE2, WFI)       │
 │ 14. pm_ram_retention(0xDFF)                       │
 │     └─ 标记哪些 RAM bank 在睡眠中保持供电           │
 │                                                   │
 ├──────────── 执行挂起 (汇编) ──────────────────────┤
 │                                                   │
 │ 15. __idle_save():                                │
 │     ├─ 压栈保存 26 个 RISC-V 通用寄存器            │
 │     ├─ 将 SP 存入全局变量 __stack_store_repo       │
 │     └─ WFI  ← *** PMU 断电 CPU 核心域 ***         │
 │              ══════════════════                    │
 │              ║   芯片进入睡眠  ║                    │
 │              ══════════════════                    │
 └───────────────────────────────────────────────────┘
```

> **关键点**：此处的 `WFI` 不是普通的 Wait-For-Interrupt。因为之前已调用
> `HAL_PMU_ConfigDeepSleepMode(MODE2, HOLDENTRY_WFI)`，PMU 硬件会在执行 `WFI` 时
> **真正断电 CPU 核心域**，CPU 寄存器全部丢失。RAM 通过 retention 位保持，AON 域
> (32kHz RC 时钟 + AON 寄存器) 持续运行。

### 上下文保存的两个层级

| 层级 | 时机 | 保存内容 | 恢复方式 |
|------|------|---------|---------|
| 汇编级 | `__idle_save()` | 26 个 RISC-V 通用寄存器 (ra, tp, t0-t6, a0-a7, s0-s8) + SP | `__idle_restore()` 从栈恢复 |
| C 级 | `pm_save_context()` | UART、GPADC、IOMux 等外设寄存器 | `pm_restore_context()` 重新写入 |

---

## 唤醒恢复流程

唤醒源可以是：WiFi beacon、GPIO 引脚、AON 定时器到期。

```
 ┌──────────────── 硬件唤醒 ─────────────────────────┐
 │                                                    │
 │  唤醒事件 → PMU 重新上电 CPU 核心域                 │
 │  ROM 引导代码启动                                   │
 │       │                                            │
 │  Boot 阶段 (boot/src/main.c):                      │
 │  ap_startup_check():                               │
 │    读 REG_AON_DIG_RSVD0                            │
 │    ├─ == 0xAA (WAKEUP_ACT_JUMP_RAM)                │
 │    │   ├─ 条件满足时: 从 RSVD3 读出 CP 入口地址     │
 │    │   ├─ 写入 REG_N300_CP_RST_ADDR 设置 CP 复位   │
 │    │   ├─ 写 0xCAFE000A 触发 CP 核硬件复位          │
 │    │   └─ 进入 WFI 等待 (Boot 核工作结束)           │
 │    └─ == 0xFF (WAKEUP_ACT_JUMP_NONE)               │
 │        └─ 直接 WFI (不需要恢复)                     │
 │    (详见下方「Boot 阶段唤醒与 CP 复位」)             │
 │                                                    │
 ├──────── __light_sleep_entry (汇编) ────────────────┤
 │                                                    │
 │  1. 重初始化 RISC-V CSR 寄存器                      │
 │     (mtvt, mtvt2, mtvec, FPU 状态)                 │
 │  2. la sp, _sp    设置栈指针                        │
 │  3. 使能分支预测器 (BPU)                             │
 │  4. call pm_sleep_startup()                        │
 │                                                    │
 ├──────── pm_sleep_startup() (C 函数) ───────────────┤
 │                                                    │
 │  5.  清除 RAM 电源门控强制位                         │
 │  6.  EnableICache(), EnableDCache()                 │
 │  7.  flash_init()           重初始化 SPI Flash 控制器│
 │  8.  BootClock_restore()    恢复 PLL / 时钟树配置   │
 │  9.  从 Flash 按 .copy_table 拷贝代码段 → SRAM/ILM  │
 │  10. wakeup_cause = pm_get_wakeup_cause()           │
 │  11. HAL_PMU_ClearWakeUpCause()                     │
 │  12. irq_vectors_reinit()   恢复中断向量表           │
 │  13. call __idle_restore()                          │
 │                                                    │
 ├──────── __idle_restore (汇编) ─────────────────────┤
 │                                                    │
 │  14. 从 __stack_store_repo 恢复 SP                  │
 │  15. 从栈上恢复 26 个 RISC-V 通用寄存器             │
 │  16. ret → 返回到 pm_light_sleep() 中               │
 │      __idle_save() 之后的那一行代码                  │
 │                                                    │
 ├──────── 回到 pm_light_sleep() ─────────────────────┤
 │                                                    │
 │  17. pm_restore_context()                           │
 │      └─ 恢复 UART / GPADC / IOMux 外设寄存器       │
 │  18. wakeup_time = vrtc_get_time_us()               │
 │      gap = wakeup_time - sleep_time                 │
 │      └─ 计算实际睡眠时长 (微秒精度)                  │
 │  19. SysTimer_SetLoadValue(old + gap)               │
 │      └─ 补偿 RISC-V mtimer (追回睡眠时间)           │
 │  20. 遍历注册设备，调用 on_exit()/on_wake() 回调     │
 │      └─ wifi_ps_hw_resume()                         │
 │         └─ ls_rf_resume() 恢复射频                   │
 │         └─ 等待 WiFi PMU 状态机就绪                  │
 │  21. vTaskStepTick(complete_periods)                 │
 │      └─ 补偿 FreeRTOS tick 计数                     │
 │  22. SysTimer_Start()       重启系统定时器           │
 │  23. __enable_irq()         重新开启全局中断         │
 │                                                    │
 └────── FreeRTOS 恢复正常调度 ────────────────────────┘
```

### Boot 阶段唤醒与 CP 复位

#### 冷启动 vs 唤醒：两条路径

Boot 的 `boot.S` 在常规初始化（copy_table / data / bss）**之前**插入唤醒检查：

```text
# boot/src/boot.S
#if (BOOT_HARTID == 0) && (CONFIG_PM) && !defined(CFG_AMP_IPC)
    call ap_startup_check
#endif
```

其中 `call ap_startup_check` 发生在 `.data` 加载和 `.bss` 清零之前。

> 由于 `ap_startup_check()` 在全局变量初始化之前执行，它不能使用任何全局变量，
> 只能读写硬件寄存器（AON 寄存器在睡眠中保持供电，不会丢失）。

**冷启动时**，AON 寄存器为空，`ap_startup_check()` 直接 return，继续走到 `main()`：

```c
// boot/src/main.c — main()
IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = CONFIG_BOOT_CP_ENTRY;  // 固定值 0x30004000
IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;                // 触发 CP 核复位
```

冷启动时 CP 入口地址是**编译时常量** `CONFIG_BOOT_CP_ENTRY = 0x30004000`（Flash 中 App 起始地址）。

#### `ap_startup_check()` 详解

```c
// boot/src/main.c
void ap_startup_check(void)
{
    // ① 读 AON 寄存器判断唤醒类型
    if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_NONE)  // 0xFF
    {
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = 0;   // 清标志
        goto WFI_LOOP;                              // Boot 无事可做，直接休眠
    }
    else if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_RAM)  // 0xAA
    {
        // ② UART 唤醒场景的特殊判断
        if ((IP_AON_CTRL->REG_WAKEUP_ISR.all == 0) &&
            (IP_AON_CTRL->REG_AON_DIG_RSVD2.all == WAKEUP_ACT_JUMP_RAM))
        {
            // ③ 设置 CP 复位地址 = RSVD3 中保存的地址
            IP_CMN_SYS->REG_N300_CP_RST_ADDR.all =
                IP_AON_CTRL->REG_AON_DIG_RSVD3.all;

            // ④ 魔术字 0xCAFE000A 触发 CP 核硬件复位
            IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
        }
        goto WFI_LOOP;   // Boot 工作完成，进入 WFI 等待
    }

    return;  // RSVD0 为其他值 → 冷启动，继续执行 main()

WFI_LOOP:
    do { __WFI(); } while(1);  // Boot 核永久休眠
}
```

逐步拆解：

**第 ① 步 — 判断唤醒动作**：PM 框架在睡前通过 `pm_set_wakeup_entry()` 将 `0xAA`
写入 `RSVD0`。Boot 读到此值，即知这次是唤醒而非冷启动。

**第 ② 步 — UART 唤醒的额外条件**：UART 唤醒时 `REG_WAKEUP_ISR` 可能还未被
硬件置位，需通过 `RSVD2` 软件标志辅助确认。只有同时满足 `WAKEUP_ISR==0`
且 `RSVD2==0xAA` 时才走此分支。

**第 ③④ 步 — 设置 CP 复位地址并触发复位**：

| 寄存器 | 含义 |
|--------|------|
| `REG_N300_CP_RST_ADDR` | CP 核复位向量地址（硬件寄存器，CP 复位后从此地址开始执行） |
| `REG_SW_RESET_CP0` | 写入魔术字 `0xCAFE000A` 触发 CP 核硬件复位 |

#### `pm_set_wakeup_entry()` — 睡前写入 AON 寄存器

PM 框架根据编译配置走不同分支：

```c
// pm_impl.c — pm_set_wakeup_entry()
static void pm_set_wakeup_entry(uint32_t entry)
{
#if (BOOT_HARTID == 0)
    // ── 本 Demo 走此路径 (BOOT_HARTID=0, 无 CFG_AMP_IPC) ──
    IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_RAM;  // 0xAA
    IP_AON_CTRL->REG_AON_DIG_RSVD1.all = entry;                 // __light_sleep_entry
    // 注意：不写 RSVD2 和 RSVD3
#else
  #ifdef CFG_AMP_IPC
    // ── AMP IPC 模式 (双核独立运行) ──
    IP_AON_CTRL->REG_AON_DIG_RSVD2.all = WAKEUP_ACT_JUMP_RAM;
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = (uint32_t)pm_dead_loop; // CP 先进死循环
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = entry;                  // 真正的入口
  #else
    // ── 非 AMP, BOOT_HARTID!=0 ──
    IP_AON_CTRL->REG_AON_DIG_RSVD0.all = WAKEUP_ACT_JUMP_RAM;
    IP_AON_CTRL->REG_AON_DIG_RSVD1.all = (uint32_t)pm_dead_loop;
    IP_AON_CTRL->REG_AON_DIG_RSVD3.all = entry;
  #endif
#endif
}
```

#### 本 Demo 的实际唤醒路径

本 Demo 配置为 `BOOT_HARTID=0`、未定义 `CFG_AMP_IPC`，走最简路径：

```
 芯片唤醒 → ROM 启动 → Boot (boot.S _start)
                           │
                   call ap_startup_check()
                           │
                   读 RSVD0 == 0xAA
                           │
                   检查 UART 条件:
                     WAKEUP_ISR==0 && RSVD2==0xAA ?
                           │
                        不满足 ← (本路径未写 RSVD2)
                           │
                   goto WFI_LOOP  ← Boot 核休眠，不启动 CP
                                     (本 Demo 中 Boot 不负责恢复 CP)

 (同时) ROM 根据 RSVD1 跳转到 __light_sleep_entry
                           │
                   pm_sleep_startup()
                           │
                   ... 恢复 Cache / Flash / Clock / RAM ...
                           │
                   __idle_restore() → 返回 pm_light_sleep()
```

在本 Demo 的非 AMP IPC 模式下，`pm_sleep_startup()` 中恢复 CP 的代码被条件编译排除：

```c
// pm_impl.c — pm_sleep_startup() 中
#if defined(CFG_AMP_IPC) && (BOOT_HARTID == 0)
    // ── 仅 AMP IPC 模式下执行 ──
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif
```

#### 不同场景下 CP 复位地址的来源

```
                    ┌─ 冷启动 ──→ CONFIG_BOOT_CP_ENTRY (0x30004000)
                    │              由 boot main() 直接写入
                    │
  CP 复位地址来源 ──┤
                    │              ┌─ 非 AMP (本Demo): Boot 不负责恢复 CP
                    └─ 唤醒恢复 ──┤
                                   └─ AMP IPC 模式:
                                        ├─ Boot 中: 从 RSVD3 读 (UART 唤醒场景)
                                        └─ pm_sleep_startup() 中: 从 RSVD4 读
```

无论哪条路径，底层硬件操作始终是同一对寄存器：

```c
IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = <目标地址>;    // 设定 CP 复位向量
IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;        // 魔术字触发 CP 硬件复位
```

#### AON 寄存器通信协议汇总

| AON 寄存器 | 本 Demo 睡前写入 | AMP IPC 模式睡前写入 | 唤醒时读取方 | 含义 |
|-----------|-----------------|---------------------|-------------|------|
| `RSVD0` | `0xAA` | `0xAA` | Boot | 唤醒动作标志 |
| `RSVD1` | `__light_sleep_entry` | `pm_dead_loop` | ROM / Boot | AP 核唤醒入口 |
| `RSVD2` | (不写) | `0xAA` | Boot | UART 唤醒辅助标志 |
| `RSVD3` | (不写) | `pm_dead_loop` | Boot | CP 核临时入口 (死循环等待) |
| `RSVD4` | (不写) | `__light_sleep_entry` | pm_sleep_startup | CP 核真正唤醒入口 |

AMP IPC 模式下 CP 先跳到 `pm_dead_loop` (死循环)，等 AP 核在 `pm_sleep_startup()`
中完成硬件恢复后，再将 `RSVD4` 中的真正入口写入 `REG_N300_CP_RST_ADDR` 并触发复位，
确保 CP 核不会在硬件未就绪时执行应用代码。

---

## 时间补偿机制

CPU 断电期间，RISC-V mtimer 和 FreeRTOS tick 均已停止，但 AON 域的 VRTC
(Virtual RTC) 基于 32kHz RC 振荡器持续计时。唤醒后需要补偿：

```
        sleep_time ──────────────── wakeup_time
            │                            │
  VRTC:   ──┤  AON 32kHz 持续计时        ├──  → 计算 gap (µs)
            │                            │
  mtimer: ──┤  已停止 (CPU 断电)          ├──  → 手动 += gap
            │                            │
  FreeRTOS: ┤  tick 停止                  ├──  → vTaskStepTick(gap / tick_ms)
```

### VRTC 工作原理

VRTC 是构建在 AON 硬件定时器之上的软件计时层：

- 硬件基础：AON 32kHz RC 振荡器（约 32768 Hz）
- 中断周期：每 ~20 秒触发 `vrtc_isr()`，更新 `vrtc_reg->sec/usec`
- 频率校准：通过 `IRQ_RCCAL_DONE_VECTOR` 中断持续校正 RC 频率偏差
- 精度换算：`freq_fact = (1000000 << 10) / freq` 固定点 Hz→µs 因子

通过这一机制，上层应用的 `vTaskDelay()`、超时等待等时间相关操作**完全无感**。

---

## 连接保活策略

三层保活确保 MQTT 长连接在频繁睡眠中不中断：

| 层级 | 机制 | 周期 | 说明 |
|------|------|------|------|
| 射频层 | WiFi DTIM PS | listen_interval × DTIM | 设备周期性醒来接收 beacon，维持 WiFi 关联 |
| 网络层 | ARP Keep-Alive | 30 秒 | `net_enable_keep_alive()` 发送免费 ARP，防止路由器清除 ARP 表项 |
| 应用层 | MQTT Keep-Alive | 120 秒 | coreMQTT 的 PINGREQ/PINGRESP，维持 Broker 连接 |

### ARP Keep-Alive 详解

设备在 Light Sleep 中可能几十秒不发包，路由器可能清除其 ARP 表项，导致后续数据无法送达。

ARP 保活已从 PM 层移至网络层管理，在开启/关闭 WiFi 省电模式时联动控制：

```c
// 开启 WiFi 省电时同步启动 ARP 保活
enable_wifi_power_save() {
    wifi_ps_mode_set(WIFI_PS_MODE_DTIM);
    net_enable_keep_alive();    // 启动网络层 ARP 保活 (30s 周期)
}

// 关闭 WiFi 省电时同步停止 ARP 保活
disable_wifi_power_save() {
    wifi_ps_mode_set(WIFI_PS_MODE_OFF);
    net_disable_keep_alive();   // 停止网络层 ARP 保活
}
```

保活周期由 `NET_KEEP_ALIVE_PERIOD` 宏定义（默认 30000ms，定义在 `net_al.h`）。

免费 ARP 的作用：
1. 维持路由器 ARP 缓存中的设备 MAC↔IP 映射
2. 保持 NAT 表项不超时
3. 确保 MQTT Broker 的下行数据能正确路由到设备

---

## PM 框架核心数据结构

### `pm_config_t`

```c
typedef struct {
    pm_mode_t        mode;         // PM_MODE_ACTIVE 或 PM_MODE_LIGHT_SLEEP
    pm_clock_level_t clock_level;  // 时钟降频级别
    uint16_t         auto_mode;    // 自动模式
    uint16_t         dbg_level;    // 调试日志级别
} pm_config_t;
```

### `pm_sleep_config_t` — 睡眠参数配置

```c
typedef struct {
    uint32_t              wakeup_src_mask;  // 唤醒源位掩码
    uint32_t              time_us;          // 定时器唤醒时间 (µs)
    uint64_t              gpio_mask;        // GPIO 唤醒引脚掩码
    pm_gpio_wakeup_mode_t gpio_mode;        // GPIO 唤醒电平模式
    uint32_t              retention_bits;   // RAM bank 保持位掩码
} pm_sleep_config_t;
```

### `pm_handler_ops_t` — 设备/钩子 PM 操作回调

```c
typedef int32_t (*pm_handler_func_t)(uint32_t sleep_time_us, void *arg);

typedef struct {
    int32_t (*check_idle)(pm_mode_t mode);  // 检查是否允许睡眠
    pm_handler_func_t on_enter;              // 睡前回调
    pm_handler_func_t on_exit;               // 唤醒后回调
    pm_handler_func_t on_wake;               // 唤醒时回调
} pm_handler_ops_t;
```

设备通过 `pm_device_register()` 注册此操作集，参与 PM 的睡眠/唤醒协商。任何一个设备的 `check_idle()` 返回 0 (busy)，系统就不会进入 Light Sleep。
