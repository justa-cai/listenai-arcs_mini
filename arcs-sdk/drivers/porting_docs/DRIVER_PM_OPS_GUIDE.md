# LISA 驱动 system PM ops 接入指南

**对象**：要把基于 `lisa_device` 框架注册的外设驱动接入 `lisa_pm` `AUTO_LIGHT_SLEEP` 的驱动作者。

**前置阅读**：
- [`DRIVER_DEVELOPMENT_GUIDE.md`](./DRIVER_DEVELOPMENT_GUIDE.md) §3.2 —— 普通 `LISA_DEVICE_REGISTER` 用法与 init_fn 规则
- [`../../components/lisa_pm/README.md`](../../components/lisa_pm/README.md) —— `lisa_pm_system_ops_t` 契约、注册路径、PM 回调顺序与上下文限制
- [`../lisa_device/README.md`](../lisa_device/README.md) §5.5 —— `LISA_DEVICE_PM_ATTACH`、system PM 与 wakeup-source 能力

本文不是 spec，是规则手册：列硬约束、给统一模式、按外设类型分场景。新驱动接入前请逐条对照 §3 红线和 §5 自检清单。

参考实现：
- [`drivers/lisa_uart/lisa_uart_arcs.c`](../lisa_uart/lisa_uart_arcs.c) —— 同步传输型
- [`drivers/lisa_gpio/lisa_gpio_arcs.c`](../lisa_gpio/lisa_gpio_arcs.c) —— 无传输型
- [`system/console/console_uart.c`](../../system/console/console_uart.c) —— 系统基建合成 PM 设备

---

## 1 与应用层的契约

> wake 之后，driver **不保留** `configure()` / `set_callback()` / `rx_enable()` 等运行时配置；应用必须在唤醒后显式重新调用相应接口。

这条契约是后续所有边界的方向：

- driver 不替应用记忆 baudrate / DMA channel / callback / rx 缓冲等"非寄存器层"状态。
- 应用拿到 wake 事件（或确认 `lisa_pm` 进入过 sleep）后必须主动重配置。
- IO 在重配置之前应该明确报错（如 `LISA_DEVICE_ERR_NOT_READY`），不要看似工作但波特率错。

这条契约让 PM 路径的目标变得清晰：**把硬件拉回 `_init_fn` 出口形态即可，不需要在 PM 路径里恢复"用户视角的状态"**。

---

## 2 三回调语义快表

详细语义见 `components/lisa_pm/README.md` 的 System PM 设备接入章节。驱动作者最常用的两点：

| 回调 | 进入态 | 退出态 | 单句描述 |
|---|---|---|---|
| `check_idle` | 任意 | 任意（只读） | 报告"是否存在调用方正在等待完成的同步传输" |
| `prepare_suspend` | 已 / 未 `configure()` 任意态 | "_init_fn 尚未执行" 的逻辑等价态 | 把 device 拆回 pre-init |
| `resume_restore` | "pre-init" | "_init_fn 刚执行完，未 configure" | 跑完 _init_fn 的 HW 层动作 |

`check_idle` 返回值反语义：**非 0 = idle / 允许睡眠**，0 = busy / 阻塞。

`prepare_suspend` / `resume_restore` 返回 0=成功，<0=失败；失败不会短路链路但会被记录。

---

## 3 硬性约束（红线）

PM 三回调在 SoC HAL light sleep enter/exit 关键路径中执行，期间可能处于关中断或调度受限状态。在此上下文：

| 禁止 | 原因 |
|---|---|
| `lisa_mutex_create / lock / unlock` | 依赖 FreeRTOS 调度器 |
| `lisa_semaphore_create / delete / take / give` | 同上；`xQueueDelete` 内部也走调度路径 |
| `lisa_mem_alloc / lisa_mem_free / pvPortMalloc / vPortFree` | 堆操作内部要拿 heap mutex |
| 调用任何会 `taskYIELD()` 或 `vTaskDelay()` 的 API | 调度器在临界区无法调度 |
| `LISA_LOGI / D / E` 等日志 | 日志通道可能锁、串口可能正被 prepare_suspend 收掉 |

允许的操作：

- 寄存器读写 / HAL 内部状态结构体读写。
- 短忙等（轮询 HAL 寄存器位）。
- 读 priv 中的 `volatile` 标志（busy / running / enabled）。
- 直接给 `volatile` 标志赋值。
- `memset` priv 的**业务字段子集**（不含 mutex / sem / 堆缓冲区句柄）。

`check_idle` 内部额外限制：**只读 priv**，不持锁、不调任何 HAL（HAL 状态此时不可靠）。

> 当前 `lisa_uart` 驱动在 `prepare_suspend` 里调用了 `lisa_semaphore_delete` 与 `lisa_mem_free` —— 这是**违规**的，仅靠"PM 入口的 `taskENTER_CRITICAL()` 已经停了所有任务、堆 mutex 实际不会被竞争"这一假设侥幸通过单板测试。新驱动**不要**复制这种写法；把 OS 资源生命周期与 device 对齐，PM 路径只读不动。

---

## 4 统一实现模式

### 4.1 init 拆三层

把现有的 `arcs_xxx_init` 拆成 OS 资源初始化、HAL 硬件初始化、framework 入口三层。HAL 那一层既作启动用，也作 `resume_restore` 用。

```c
/* 4.1.1 OS 资源 —— 仅 _init_fn 调用一次；调用上下文 = 任务级，可分配堆 */
static int arcs_xxx_init_resources(xxx_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) return LISA_DEVICE_ERR_INIT_FAIL;
    /* sem / 缓冲区 / 其它依赖调度器的资源 */
    return LISA_DEVICE_OK;
}

/* 4.1.2 HAL 硬件 —— 幂等；既给 _init_fn 用，也给 resume_restore 用 */
static int arcs_xxx_init_hw(xxx_priv_t *priv)
{
    if (XXX_Initialize(priv->hal_handler, xxx_event_cb, priv) != CSK_DRIVER_OK)
        return LISA_DEVICE_ERR_INIT_FAIL;
    if (XXX_PowerControl(priv->hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK)
        return LISA_DEVICE_ERR_INIT_FAIL;
    lisa_xxxn_pinmux();
    return LISA_DEVICE_OK;
}

/* 4.1.3 framework 入口 */
static int arcs_xxxn_init(void)
{
    memset(&xxxn_priv, 0, sizeof(xxxn_priv));
    xxxn_priv.hal_handler = XXXn();
    if (!xxxn_priv.hal_handler) return LISA_DEVICE_ERR_INIT_FAIL;

    int ret = arcs_xxx_init_resources(&xxxn_priv);
    if (ret) return ret;
    return arcs_xxx_init_hw(&xxxn_priv);
}
```

不变量：

- `_init_resources` 在 priv 整体 zero-init 之后调用，**只**创建 OS 对象、不写 HAL 寄存器；**只在 `_init_fn` 中调用一次**。
- `_init_hw` 不分配 OS 资源、不动 mutex；可被 `_init_fn` 和 `resume_restore` 重复调用，HAL 内部 `Uninitialize → Initialize` 必须幂等。
- priv 中的 mutex / sem 句柄一旦创建，**生命周期与设备相同，PM 路径只读不动**。

### 4.2 三回调骨架

按统一模式写，按外设差异（§6）裁剪：

```c
static int32_t arcs_xxx_pm_check_idle(void *ctx)
{
    xxx_priv_t *priv = (xxx_priv_t *)ctx;
    if (priv == NULL) return 1;   /* 退化为允许，避免阻塞整个系统 */

    /* 仅读 priv 中的 volatile 忙标志 */
    if (priv->tx_busy)  return 0;
    if (priv->rx_busy)  return 0;
    return 1;
}

static int32_t arcs_xxx_pm_prepare_suspend(void *ctx)
{
    xxx_priv_t *priv = (xxx_priv_t *)ctx;
    if (priv == NULL || priv->hal_handler == NULL) return 0;

    /* 1) HAL 关电 + 去初始化：顺序 = PowerControl(OFF) → Uninitialize */
    XXX_PowerControl(priv->hal_handler, CSK_POWER_OFF);
    XXX_Uninitialize(priv->hal_handler);

    /* 2) 清应用级 configured 标记和 config 缓存（仅业务字段子集） */
    priv->configured = false;
    memset(&priv->current_config, 0, sizeof(priv->current_config));
    /* 通道型标志逐个清；callback 指针按需清 */

    /* 3) 绝不动 mutex / sem / 堆缓冲区句柄 */
    return 0;
}

static int32_t arcs_xxx_pm_resume_restore(void *ctx)
{
    xxx_priv_t *priv = (xxx_priv_t *)ctx;
    if (priv == NULL || priv->hal_handler == NULL) return 0;
    return arcs_xxx_init_hw(priv);
}

static const lisa_pm_system_ops_t arcs_xxx_pm_ops = {
    .check_idle      = arcs_xxx_pm_check_idle,
    .prepare_suspend = arcs_xxx_pm_prepare_suspend,
    .resume_restore  = arcs_xxx_pm_resume_restore,
};
```

### 4.3 HAL 调用顺序的两条死规则

| 时机 | 顺序 | 理由 |
|---|---|---|
| `prepare_suspend` | **先** `PowerControl(OFF)` **再** `Uninitialize` | `PowerControl(OFF)` 内部清 POWERED 后会校验 INITIALIZED；先 Uninitialize 会让 `PowerControl(OFF)` 报错且不会 disable IRQ |
| `resume_restore` | **先** `Initialize` **再** `PowerControl(FULL)` | `PowerControl(FULL)` 进入条件是 INITIALIZED=1；先调要么 early-return 不真正开外设时钟，要么报错 |

UART / I2C / SPI / SDMMC 等 ARM Driver-V2 风格的 HAL 都遵循这个 flag 状态机。GPIO HAL 形态略不同（无独立 PowerControl/POWERED flag），只调 `Initialize` / `Uninitialize` 即可。

### 4.4 注册：普通设备注册 + `LISA_DEVICE_PM_ATTACH`

推荐保持设备本体注册使用普通宏，PM 能力只在 `CONFIG_LISA_PM=y` 时单独声明：

```c
LISA_DEVICE_REGISTER_DEINIT(xxx0, &arcs_xxx_api, &xxx0_priv, NULL,
                            arcs_xxx0_init, arcs_xxx0_deinit,
                            LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);

#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(xxx0, &arcs_xxx_pm_ops, NULL, &xxx0_priv);
#endif
```

- `LISA_DEVICE_PM_ATTACH()` 的第一个参数必须与前面的设备注册名一致。
- 第二个参数是 `system_ops`，用于 system PM 的 idle 判断、睡前挂起和醒后基础恢复。
- 第三个参数是 `wakeup_ops`，用于设备作为唤醒源时的唤醒条件缓存和硬件 wakeup 启停；不作为唤醒源时填 `NULL`。
- `ctx` 显式传入，通常填 `&xxx0_priv`，`system_ops` 三回调收到的 `ctx` 就是 priv。
- `CONFIG_LISA_PM=n` 时 attach 宏为空，PM ops 符号不会被引用；驱动本体仍按普通设备注册方式可用。
- `LISA_DEVICE_REGISTER_PM()` / `LISA_DEVICE_REGISTER_PM_DEINIT()` 仍可编译，新驱动优先使用注册与 PM 能力分离的写法。

---

## 5 安全自检清单

每个驱动接入完成后，逐条勾掉：

- [ ] `_init_hw` 不调用 `lisa_mutex_create` / `lisa_semaphore_create` / `pvPortMalloc` / `lisa_mem_alloc`
- [ ] `prepare_suspend` 不释放 priv 中的 mutex / sem 句柄
- [ ] `prepare_suspend` 不调用 `lisa_mem_free`（若有正在使用的 DMA buffer，应在 `check_idle` 阶段视为忙）
- [ ] `prepare_suspend` 内 `memset` 只清业务字段子集，不覆盖 mutex / sem / 堆缓冲区句柄字段
- [ ] `resume_restore` 不重建 OS 资源
- [ ] `check_idle` 内只读 priv 字段，不持锁、不调 HAL
- [ ] `_init_hw` 对同一 priv 重复调用不泄漏（HAL Uninitialize 已先行）
- [ ] HAL 调用顺序遵循 §4.3 死规则
- [ ] `CONFIG_LISA_PM=y` 和 `=n` 两套 prj.conf 都能构建
- [ ] 应用样例已说明 wake 后必须重 `configure()`

---

## 6 按外设类型分场景

### 6.1 同步传输型（uart / i2c / spi / sdmmc / qspilcd）

priv 含 `tx_busy / rx_busy / configured / current_config`。

- `check_idle`：`!tx_busy && !rx_busy` 才允许睡眠。**循环 RX 常驻能力例外**：如果 driver 支持类似 `rx_circ_buf->enabled` 的"持续接收"模式，将这个标志从 busy 排除 —— 否则系统一旦开启循环 RX 永远无法睡。是否阻塞睡眠由上层 `lisa_pm_lock_acquire/release` 自行表达。
- `prepare_suspend`：`PowerControl(OFF)` → `Uninitialize` → 清 `configured / current_config / callback / tx_busy / rx_busy`。
- `resume_restore`：`Initialize(handler, isr_cb, priv)` → `PowerControl(FULL)` → 重写 pinmux。

部分总线只有"single-shot"模式，没有循环传输概念，按 `tx_busy / rx_busy` 处理即可。

### 6.2 无传输型（gpio）

无 tx/rx 概念。

- `check_idle`：永远 1。
- `prepare_suspend`：仅 `GPIO_Uninitialize`（关时钟门 / disable IRQ / `register_ISR(NULL)`），**不动 priv**。
- `resume_restore`：调 `_init_hw`，重 `GPIO_Initialize` + pinmux 回写。
- `priv->irq_info[]` 中的 callback 指针跨睡眠保留：HAL 状态已丢，应用 wake 后必须重 `configure_irq` 才会再次收到中断；保留 callback 指针只是数据，无副作用。

### 6.3 通道型（pwm / adc）

priv 中含 `channels[N].configured` / `channels[N].enabled`。

- `check_idle`：任一 `channels[i].enabled == true` 即返回 0。
- `prepare_suspend`：把所有 `channels[i].configured / enabled` 清零；HAL 层关闭通道输出（`HAL_GPT_DisablePWM` 等）；`PowerControl(OFF) + Uninitialize`（若 HAL 提供）。
- `resume_restore`：调 `_init_hw`。

ADC 的同步转换路径通常持 mutex，`check_idle` 读不到 mutex 状态，要在 priv 加 `volatile bool conversion_in_flight`，转换入口置 1、出口置 0。

### 6.4 always-on 域（rtc / aon_timer）

硬件 retention，业务无感。

- `check_idle`：永远 1。
- `prepare_suspend / resume_restore`：空回调或最小占位。本批不做 deep sleep 路径。

### 6.5 系统基建（console_uart 这种自身没"应用循环"重 configure 的）

参考 [`system/console/console_uart.c`](../../system/console/console_uart.c) 的实现：

- 在 init 时缓存配置到 file-scope 静态。
- 注册一个仅用于 PM 的合成 `lisa_device`（api / priv 均传 NULL，init_fn 返回 0）。
- 注册时用 `LISA_DEVICE_PRIORITY_LOWEST`，配合 lisa_pm device 链 head-插入 / 复制时再 head-插入的两次反转，确保该合成设备的 `resume_restore` 排在它依赖的硬件 device（CRITICAL / NORMAL 优先级）之后调度。
- `resume_restore` 内调用 `lisa_xxx_configure(dev, &cached_config)` 让 console / 类似无主组件在 wake 后自动重配，不依赖应用代码。

**前提条件**：被调用的 `lisa_xxx_configure(...)` 在该 cached_config 下不会触发 `lisa_mem_alloc` / `lisa_semaphore_create` 之类的违规调用。`lisa_uart_configure` 默认 config 没有循环 RX buffer 时是安全的；启用 `rx_buf_config` 时**不**安全，需要另外的机制。

### 6.6 复杂子系统（i2s / audio / dvp / rgb）

通常 priv 已经有 `started / running / record_running` 等运行标志。

- `check_idle`：任一 stream 在跑就返回 0。
- `prepare_suspend`：先停 stream（HAL `Stop`），再 `Uninitialize`，最后清缓存配置。
- `resume_restore`：调 `_init_hw`。
- 注意 **GPDMA 共享**：dvp / rgb / qspilcd 都可能调 `GPDMA_Initialize/Uninitialize`，重复 init 须 HAL 内部幂等；如不幂等需引入 reference counting，留作 follow-up。

### 6.7 看门狗（wdt）

- `check_idle`：WDT 已启动即返回 0（阻塞睡眠，避免 WDT 在 sleep 中误触发 reset）。
- `prepare_suspend`：停 WDT、清 `configured = false`、`PowerControl(OFF)`。
- `resume_restore`：调 `_init_hw`，应用层负责 wake 后重新 `configure(timeout)` + `start()`。

---

## 7 已知风险与缓解

| 风险 | 缓解 |
|---|---|
| HAL `Uninitialize` 不幂等 | `_init_hw` 中用 "try-Uninitialize-then-Initialize" 模式包裹；崩溃时单驱动回滚 |
| busy 标志没覆盖 ISR 路径 | 实板验证阶段加 `LISA_LOGD` 跟踪 `check_idle` 返回值；不通过即新增 `volatile` 字段 |
| GPDMA 多用户重复 Init/Uninit 冲突 | 本批先依赖 HAL 既有行为；观测到冲突再引入 GPDMA reference counting |
| 应用 wake 后未重 `configure()` 导致外设静默 | driver 在 IO API 入口检查 `priv->configured`，返回 `NOT_READY` 让缺陷可见；`samples/subsys/lisa_pm/wifi_ps` 提供正确范例 |
| PM 临界区里调堆/sem（如现 lisa_uart）| 未来重构把 OS 资源生命周期与 device 对齐；新驱动一律按本规则写 |

---

## 8 验证

### 8.1 主机侧构建

每个驱动改完，**两套都过**：

- 至少一个开 `CONFIG_LISA_PM=y` 的工程（推荐 `samples/subsys/lisa_pm/wifi_ps` 或 `test/components/lisa_pm`）。
- 至少一个不开 `CONFIG_LISA_PM` 的对应驱动样例（如 `samples/drivers/devices/lisa_xxx/...`）。

确认 `CONFIG_LISA_PM=n` 时 PM 符号未被引用、驱动行为与改之前等价。

### 8.2 实板验证

`arcs_mini` 上跑 `samples/subsys/lisa_pm/wifi_ps`：

1. 触发 `LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP` 真睡眠（确认 `lisa_pm_get_stats()` 中 `sleep_count > 0`）。
2. wake 后业务侧重新 `configure()` + 重做一次典型 IO，确认数据正常。
3. 串口日志中**不应**出现 `mutex error` / `assert failed` / `hardfault` 字样。
4. 重复 100+ 次睡眠 / 唤醒循环，确认无内存泄漏、无外设 hang。

### 8.3 日志判定

提交说明中至少要记录：

- 验证命令（build + flash 命令）
- 板型（如 `arcs_mini`）
- 关键日志结论（sleep_count、重 configure 后业务恢复证据）

未实板验证不得宣称该 issue 已完成。

---

## 9 接入顺序建议

按已有的设计推进：

1. UART（已完成，统一模式参考实现）
2. GPIO（已完成，验证最简场景）
3. I2C / SPI / PWM / ADC（同类基础外设）
4. hwtimer / wdt / rtc（与时钟域相关）
5. audio / i2s / dvp / qspilcd / rgb（复杂子系统，注意 GPDMA 共享）
6. sdmmc（存储类）

每组完成一次 commit，便于回滚。
