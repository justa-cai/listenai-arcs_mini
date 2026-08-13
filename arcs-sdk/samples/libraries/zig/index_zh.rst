.. _samples_libraries_zig:

Zig 示例总览
============

概述
----

本页是 `samples/libraries/zig/` 目录下 Zig 示例的分类入口，
用于汇总各个 Zig sample 的用途和跳转路径，而不是单个示例的 README 文档。

这些示例展示了如何通过 `@import("arcs")` 在现有 ARCS SDK 工程中使用日志、GPIO、音频、WiFi 以及基础系统能力。

其中 `binding_test`、`functional_test`、`display_test` 已迁移到 `test/zig/`，
作为验证型工程维护，不再出现在 samples 文档索引中。

使用前提
--------

构建这些示例前，请先确认开发机已安装 **Zig 0.13.0**，并可通过 `zig version` 正常访问。
更完整的安装、PATH 配置和版本验证说明，请参见 `labs/zig/usage.rst`。

样例分组
--------

- **入门示例**: `helloworld`、`blinky`
- **功能演示**: `audio_play`、`wifi_demo`
- **验证测试**: 详见 `test/zig/` 下的 `binding_test`、`functional_test`、`display_test`

.. toctree::
    :maxdepth: 1

    helloworld/README.md
    blinky/README.md
    audio_play/README.md
    wifi_demo/README.md
