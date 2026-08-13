.. _labs_lnn:

LNN 实验室
==========

本专题介绍 ARCS SDK 中 LNN 相关的实验性推理能力，当前重点覆盖
Thinker 推理库、Luna 计算库以及在 ARCS AP Core 上运行的模型示例。

当前 LNN 支持建立在现有 ARCS SDK C 工程体系之上：

- 在 AP Core 固件中直接链接 Thinker 与 Luna 静态库
- 使用 Thinker 资源文件描述模型、张量、算子和运行期内存计划
- 使用 Luna 共享内存和 PSRAM 承载推理所需 workspace
- 提供 RGB565 测试图到模型输入 tensor 的预处理示例
- 提供模型资源独立烧录到外部 Flash 的参考流程

当前已提供 ``thinker_resnet18`` 和 ``thinker_resnet18_real`` 两个示例，分别用于
演示静态图片推理，以及 CP 摄像头实时预览配合 AP Thinker 推理的双核链路。

专题内容：

.. toctree::
   :maxdepth: 1

   usage
   thinker_resnet18/README
   thinker_resnet18_real/README
