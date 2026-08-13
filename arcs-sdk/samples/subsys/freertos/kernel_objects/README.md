# FreeRTOS kernel objects 示例

## 功能说明

该示例用于在实际板子上验证 FreeRTOS 常见内核对象和基础调度能力是否正常工作。示例不依赖外部外设，适合通过串口日志判断结果。

## 硬件连接

无需额外硬件连接，只需要连接开发板调试串口查看日志。

## 测试内容

1. Task 创建、优先级调度和任务删除
2. Queue FIFO 顺序和数据完整性
3. Binary semaphore 跨任务同步
4. Counting semaphore token 计数
5. Mutex 对共享计数器的互斥保护
6. Recursive mutex 嵌套加锁和解锁
7. Event group bit 同步
8. Software timer 回调执行
9. Direct-to-task notification 通知投递

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

也可以直接在 SDK 根目录执行：

```bash
./build.sh -C -S samples/subsys/freertos/kernel_objects -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

串口日志中应看到每个测试项的 PASS，以及最终 PASS 总结：

```text
I/freertos_test [...] === FreeRTOS kernel objects test START ===
I/freertos_test [...] PASS: task create and priority order
I/freertos_test [...] PASS: queue FIFO
I/freertos_test [...] PASS: binary semaphore
I/freertos_test [...] PASS: counting semaphore
I/freertos_test [...] PASS: mutex
I/freertos_test [...] PASS: recursive mutex
I/freertos_test [...] PASS: event group
I/freertos_test [...] PASS: software timer
I/freertos_test [...] PASS: task notification
I/freertos_test [...] === FreeRTOS kernel objects test PASS ===
```

若任一测试失败，会打印 `FAIL:` 日志，并以 `=== FreeRTOS kernel objects test FAIL ===` 结束。

## CI 判定

`sample.yaml` 使用 UART runner，并通过以下最终日志判断成功：

```text
=== FreeRTOS kernel objects test PASS ===
```

## 注意事项

- 本示例使用 FreeRTOS 原生 API，目标是直接验证内核对象行为。
- 本示例关闭异步日志，避免日志后台任务干扰 task 调度顺序和故障定位。
- 测试 task 在 `vTaskDelete(NULL)` 后不会从 task 入口函数返回，避免 RISC-V 软件中断延迟触发时跳转到 `configTASK_RETURN_ADDRESS`。
- 根据项目测试要求，涉及测试工程变更时，最终完成判定必须在实际板子上运行并抓取串口日志。
