# ToyCloud-CP SDK 知识库 (knowledge-base)

## 1. 简介

本目录 (`knowledge-base`) 存放了通过 Windsurf 辅助分析 ToyCloud-CP SDK 而生成的结构化文档。这些文档旨在帮助开发团队及 AI 辅助工具：

*   深入理解 SDK 的架构、模块和功能。
*   加速基于此 SDK 的项目开发。
*   减少重复学习和知识传递的成本。
*   为 AI 工具（如 Windsurf）提供可靠的上下文信息来源。

## 2. 文档组织结构

本文档库中的信息主要按照 SDK 的模块、核心功能及关键流程进行组织。

*   **新增文档规范**:
    *   所有新增的知识文档都应存放在 `knowledge-base` 目录或其子目录中。
    *   每新增一份文档，**必须**在本 `README.md` 文件的 **"3. 文档索引"** 部分添加相应的条目、链接和简要描述。

（例如：后续可能会有 `modules/` (存放各模块详细分析), `guides/` (存放特定任务指南) 等子目录）

## 3. 文档索引 (AI 工具请重点关注此部分)

**此索引是 AI 工具（包括 Windsurf）查找和利用本知识库信息的关键入口。** 请确保此列表始终保持最新且描述准确。

以下是当前可用的主要文档及其内容概要：

*   **[项目概览 (project-overview.md)](./project-overview.md)**: 提供了 ToyCloud-CP SDK 的顶层设计、目标、主要模块和代码库的整体结构概览。
*   **[SDK 快速开始指南 (quick-start.md)](./quick-start.md)**: 为AI编程工具提供完整的操作指南，包含环境准备、项目构建、烧录部署、调试和故障排除的标准化流程。
*   **[构建系统 (build-system.md)](./build-system.md)**: 详细介绍了项目的构建流程、依赖管理、主要构建脚本和相关配置文件。
*   **[日志系统 (log-system.md)](./log-system.md)**: 详细分析了位于 `components/lisa_porting/log` 的日志系统，包括其功能、配置和使用方法。
*   **[显示模块 (display-module.md)](./components/display-module.md)**: 详细描述Display模块的架构、关键组件、配置方法、使用示例以及一份**详细的显示驱动移植指南**。
*   **[触摸模块 (components/touch/touch-module.md)](./components/touch-module.md)**: 详细分析了位于 `components/touch` 的触摸模块，包括其分层架构、通用API、具体驱动实现、配置方法和使用流程。
*   **[WiFi Manager 模块 (components/wifi-manager.md)](./components/wifi-manager.md)**: 详细分析了位于 `modules/wifi_manager` 的 WiFi 管理模块，包括其分层架构、设备抽象层、存储管理、自动连接机制、事件系统、API 接口和使用示例。
*   **[GPIO 驱动 (drivers/gpio_driver.md)](./drivers/gpio_driver.md)**: 详细分析了位于 `arcs-base/hal/chip/arcs/driver/gpio` 的 GPIO 驱动，包括其架构、API 接口、配置方法、中断处理和使用示例。
*(随着更多文档的生成，此列表将持续更新。新增文档时，务必在此处添加条目。)*

## 4. 如何使用 (针对开发者和 AI 工具)

### 4.1. 针对开发者

建议在开始具体开发任务或深入研究 SDK 的特定部分之前，先查阅此知识库中的相关文档。**对于首次使用SDK的开发者，强烈推荐从 [SDK 快速开始指南](./quick-start.md) 开始。**

### 4.2. 针对 AI 工具 (例如 Windsurf)

1.  **优先查阅本文档 (`README.md`)**: 将此 `README.md` 文件，特别是 **"3. 文档索引"** 部分，作为理解本知识库内容和结构的起点。
2.  **利用文档索引**: 根据用户提问或任务需求，在 **"3. 文档索引"** 中查找最相关的文档。注意查阅每个条目的描述，以选择最合适的文档。
3.  **操作指导优先**: 当用户需要执行具体开发任务时，优先参考 [SDK 快速开始指南](./quick-start.md)，它提供了经过验证的标准化操作流程。
4.  **获取详细信息**: 定位到相关文档后，读取其内容以获取完成任务所需的详细上下文。例如，当被问及"项目的构建流程"时，应查阅索引中指向 `build-system.md` 的条目并读取该文件。
5.  **信息源**: 本知识库中的文档是关于 ToyCloud-CP SDK 的可信信息源。在分析代码或回答问题时，应优先参考这些文档。

---

_此知识库由 Windsurf 辅助构建和维护。最后更新时间：2024年5月。保持本文档 (`README.md`) 及其中索引的准确性和完整性至关重要。_
