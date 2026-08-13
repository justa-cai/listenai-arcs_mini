.. _labs_zig_samples:

Zig 示例总览
============

当前仓库已经提供一组基于 `@import("arcs")` 的 Zig 示例，覆盖入门、外设控制、
音频、网络以及绑定验证等典型场景。

入门示例
--------

- `helloworld`：验证 Zig 代码可编译、链接并从 C 入口正常执行
- `blinky`：通过 Zig HAL 打开 GPIO 并控制板载 LED 闪烁

功能演示
--------

- `audio_play`：生成正弦波 PCM 数据并调用 Audio HAL 播放
- `wifi_demo`：初始化 WiFi 并扫描周围 AP 列表

验证与测试
----------

- `display_test`：位于 `test/zig/display_test`，用于显示设备能力获取、填色和亮度调节验证
- `binding_test`：位于 `test/zig/binding_test`，校验 Zig bindings 与 C 结构体布局是否一致
- `functional_test`：位于 `test/zig/functional_test`，对设备、线程、同步、定时器、GPIO、Flash、UART 等能力进行功能性验证

完整样例导航位于：

.. toctree::
   :maxdepth: 1

   /samples/libraries/zig/index_zh
