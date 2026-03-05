# LISA WDT 基础示例

## 功能说明

演示如何使用 LISA WDT 驱动的基本功能，配置看门狗为复位模式，定期喂狗防止系统复位。

看门狗采用两阶段超时机制：先触发中断（`int_timeout_ms`），再经过复位超时（`rst_timeout_ms`）后系统复位。示例中配置中断超时 300ms，复位超时 200ms，总复位时间 500ms。

## 硬件连接

无需外部连接，WDT 为芯片内部外设。

## 示例内容

1. 获取 WDT 设备
2. 配置看门狗参数（中断超时 300ms，复位超时 200ms）
3. 启动看门狗
4. 查询看门狗状态
5. 在主循环中每 300ms 定期喂狗，防止超时复位

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA WDT basic example ===
wdt0 device ready
WDT configured: int_timeout=300 ms, rst_timeout=200 ms (total=500 ms)
WDT started
WDT state: RUNNING
Feeding WDT every 300 ms
(持续运行中，定期喂狗...)
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 WDT 设备 |
| `lisa_wdt_setup()` | 配置看门狗超时参数 |
| `lisa_wdt_start()` | 启动看门狗 |
| `lisa_wdt_get_state()` | 查询看门狗状态 |
| `lisa_wdt_feed()` | 喂狗（刷新看门狗计数器） |

## 配置说明

### 超时参数配置

看门狗采用两阶段超时机制：

- **`int_timeout_ms`**：中断超时时间（300ms），超时后会触发中断
- **`rst_timeout_ms`**：复位超时时间（200ms），从中断超时后开始计算
- **总复位时间**：`int_timeout_ms + rst_timeout_ms = 500ms`

如果连续 300ms 未喂狗，会触发中断；如果再经过 200ms 仍未喂狗，系统将自动复位。

### 喂狗频率

示例中每 300ms 喂一次狗，小于总复位时间 500ms，确保在中断阶段前完成喂狗，避免触发复位。

## 注意事项

1. **喂狗频率**：喂狗间隔必须小于总复位时间（`int_timeout_ms + rst_timeout_ms`），建议小于中断超时时间以确保安全
2. **复位行为**：如果停止喂狗超过总复位时间（500ms），系统将自动复位，所有未保存数据会丢失
3. **测试复位功能**：如需验证复位功能，可注释掉喂狗代码，但会导致系统重启，请谨慎操作
4. **定时精度**：实际喂狗时间受任务调度影响，建议保留足够的时间余量
