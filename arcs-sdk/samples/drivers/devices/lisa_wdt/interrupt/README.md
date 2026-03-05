# LISA WDT 中断模式示例

## 功能说明

演示如何使用 LISA WDT 驱动的中断模式，通过超时回调函数处理看门狗超时事件。

底层采用两阶段超时机制：中断阶段超时（`int_timeout_ms`）触发回调，复位阶段超时（`rst_timeout_ms`）后系统复位。应用层可在回调中执行恢复操作或重新喂狗，避免系统复位。

### 新特性

- **中断通知机制**：超时通过中断回调通知应用层，而非直接复位
- **灵活的恢复策略**：回调中可执行数据保存、状态恢复或重新喂狗等操作
- **两阶段超时保护**：中断阶段提供恢复机会，复位阶段作为最后保护
- **适合关键任务场景**：可在超时回调中执行关键操作，提高系统可靠性

## 硬件连接

无需外部连接，WDT 为芯片内部外设。

## 使用场景

适用于需要在看门狗超时前执行恢复操作的场景，如保存关键数据、记录错误日志、执行故障恢复等。通过中断回调机制，在系统复位前提供处理窗口。

## 示例步骤

1. 获取 WDT 设备
2. 配置看门狗参数（中断超时 1000ms，复位超时 500ms）
3. 设置超时回调函数
4. 启动看门狗
5. 在主循环中前 5 次正常喂狗（每 800ms）
6. 停止喂狗，等待超时中断触发
7. 超时后通过回调函数处理（打印信息，后续会触发复位）

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA WDT interrupt example ===
wdt0 device ready
WDT configured: int_timeout=1000 ms, rst_timeout=500 ms (total=1500 ms)
WDT timeout callback registered
WDT started
WDT state: RUNNING
Feeding WDT every 800 ms for 5 times, then stop to trigger timeout
WDT fed (1/5)
WDT fed (2/5)
WDT fed (3/5)
WDT fed (4/5)
WDT fed (5/5)
Stopped feeding WDT, waiting for timeout interrupt (should occur after 1000 ms)...
(等待约 1 秒后)
WDT callback triggered - Resetting
(再等待约 0.5 秒后系统复位)
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 WDT 设备 |
| `lisa_wdt_setup()` | 配置看门狗超时参数 |
| `lisa_wdt_set_callback()` | 设置超时回调函数 |
| `lisa_wdt_start()` | 启动看门狗 |
| `lisa_wdt_get_state()` | 查询看门狗状态 |
| `lisa_wdt_feed()` | 喂狗（刷新看门狗计数器） |

## 中断模式说明

看门狗中断模式的工作原理：

- **两阶段超时机制**：
  - 中断阶段：连续 `int_timeout_ms`（1000ms）未喂狗，触发中断并调用回调函数
  - 复位阶段：从中断触发后开始，再经过 `rst_timeout_ms`（500ms）仍未处理，系统自动复位
- **回调触发时机**：中断阶段超时时立即在中断上下文中调用回调函数
- **恢复机会**：回调函数中可执行数据保存、状态恢复或重新喂狗，避免系统复位
- **总复位时间**：`int_timeout_ms + rst_timeout_ms = 1500ms`

## 关键代码

```c
/* 配置看门狗：中断超时 1s，复位超时 0.5s */
lisa_wdt_config_t config = {
    .int_timeout_ms = 1000,  /* 中断超时时间 1s */
    .rst_timeout_ms = 500,   /* 复位超时时间 0.5s */
};
lisa_wdt_setup(wdt_dev, &config);

/* 设置超时回调函数 */
lisa_wdt_set_callback(wdt_dev, wdt_timeout_callback, NULL);

/* 启动看门狗 */
lisa_wdt_start(wdt_dev);

/* 定期喂狗 */
lisa_wdt_feed(wdt_dev);
```

## 配置说明

### 超时参数配置

- **`int_timeout_ms`**：中断超时时间（1000ms），超时后触发中断回调
- **`rst_timeout_ms`**：复位超时时间（500ms），从中断触发后开始计算
- **总复位时间**：`int_timeout_ms + rst_timeout_ms = 1500ms`

### 回调函数实现

```c
void wdt_timeout_callback(void *user_data)
{
    /* 注意：此函数在中断上下文中执行 */
    printf("WDT callback triggered - Resetting\n");
    /* 可在此执行：数据保存、错误记录、重新喂狗等操作 */
}
```

## 注意事项

1. **中断上下文限制**：回调函数在中断上下文中执行，应尽量简短快速，避免执行耗时操作或阻塞操作
2. **喂狗频率**：正常运行时，喂狗间隔应小于中断超时时间（1000ms），示例中使用 800ms 确保安全
3. **恢复策略**：在超时回调中可以重新喂狗以继续运行，也可以执行关键操作后允许复位
4. **复位时间窗口**：中断触发后有 `rst_timeout_ms`（500ms）的处理窗口，需在此时间内完成关键操作
5. **并发控制**：如果回调中重新喂狗，应确保喂狗操作正确完成，避免重复触发中断
