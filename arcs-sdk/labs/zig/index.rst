.. _labs_zig:

Zig 实验室
==========

本专题介绍 ARCS SDK 中新增的 Zig 语言支持，包括模块结构、构建接入方式、
`@import("arcs")` 暴露的能力，以及配套的 Zig 示例。

当前文档与构建适配版本为 **Zig 0.13.0**。如需安装方法、PATH 配置和版本自检命令，请优先阅读 `usage` 页面。

当前 Zig 支持建立在现有 ARCS SDK C 驱动、组件和 FreeRTOS 基础之上：

- 提供与 C API 一一映射的 bindings 层
- 提供更符合 Zig 习惯的 HAL 封装层
- 支持在 sample 工程中编译并链接 `.zig` 源文件
- 支持从 C 入口调用 Zig 导出函数，在现有工程体系中渐进接入 Zig

专题内容：

.. toctree::
   :maxdepth: 1

   usage
   samples/index
