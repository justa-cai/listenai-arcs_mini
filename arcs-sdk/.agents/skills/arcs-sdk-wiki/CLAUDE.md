---
name: arcs-sdk-wiki
description: Use when building, updating, querying, linting, or fixing the Arcs SDK Wiki — a versioned, schema-driven wiki generated from Arcs SDK documentation materials. Covers four operations (ingest/query/lint/fix), full vs incremental builds, version management (versions.md / manifest.md / log.md), page types and frontmatter, quality rules per page type, and the validate.py / scope.py / diff.py / analyze_qa.py scripts. Trigger when the user mentions Arcs-SDK-wiki, asks to ingest a new SDK version, run lint/fix on the wiki, query the wiki, or work with summaries/entities/concepts/comparisons/matrices/guides/scenarios under wiki/<version>/.
compatibility: Designed for Codex (codex exec) and Claude Code; bundle is self-contained and runnable in CI pipelines.
allowed-tools: Read Glob Grep Bash Write Edit
---

# Arcs SDK Wiki — Schema

单 agent 操作规则。定义 wiki 的结构、页面格式、约定和所有操作流程。本目录是自包含 bundle，跟 SDK 仓库一起 ship 到 `.agents/skills/arcs-sdk-wiki/`。

---

## Execution Context

本 skill 是**纯 schema 与流程**，不假定调用方与路径。所有具体路径由调用方在 user prompt 里**字面注入**，schema 内部使用符号化路径：

| 符号 | 含义 | 谁来提供 |
|------|------|---------|
| `<version>` | 当前处理的 SDK 版本（如 `v0.1.7`） | 调用方 |
| `materials/<version>/` | 发布原料目录（只读归档） | 调用方注入绝对路径 |
| `docs/zh/` | 源文档目录（**实际的事实入口**，CI 模式下用它扫描） | 调用方注入绝对路径 |
| `wiki/<version>/` | 本次输出目录（**唯一可写**） | 调用方注入绝对路径 |
| `wiki/versions.md` | 全局版本注册表 | 调用方注入绝对路径 |

**调用入口**：

- **GitLab CI（生产）**：`.gitlab/scripts/ai_wiki_ingest.py` 调 `ai_common.call_ai()`，在 codex 镜像里跑 `codex exec`。CI 镜像内部已配好 codex config / auth；driver 会隔离 HOME，agent 看不到 GitLab token。
- **本地交互**：用户用 Claude Code 或 codex CLI 直接调，在 prompt 里注入路径。
- **本地批跑**：可手动执行 `python .gitlab/scripts/ai_wiki_ingest.py --version <version> ...`，复用同一份 driver。

**hard 约束**（无论谁调）：
- 只写指定的 `wiki/<version>/` 与同级 `versions.md`；不动其他 `wiki/<other_version>/`
- 原料目录（`materials/<version>/`、`docs/zh/`）严格只读
- `confidence` 字段不得自行升为 `high`

---

## Architecture

```
.agents/skills/arcs-sdk-wiki/  ← 本 bundle 在 SDK 仓库中的最终位置
├── CLAUDE.md                  ← 本文件（事实源 schema）
├── SKILL.md                   ← 软链 → CLAUDE.md（Claude Code skill 入口）
├── versions.md                ← 全局版本注册表
├── scripts/                   ← 确定性工具脚本（非 LLM）
│   ├── validate.py            ← frontmatter / wiki-link / index 一致性检查
│   ├── scope.py               ← 解析 doctrees → 输出 toctree 可达页面列表
│   ├── diff.py                ← 版本间源文件 diff
│   ├── classify.py            ← 源文件分类
│   ├── analyze_qa.py          ← QA 反馈分析
│   └── maintain_claude.py
├── tests/                     ← 脚本单元测试
└── wiki/                      ← Wiki 产出（按版本存放）
    └── <version>/
        ├── manifest.md
        ├── index.md
        ├── log.md
        ├── reports/
        ├── summaries/
        ├── entities/{chips,components,drivers,boards}/
        ├── concepts/
        ├── comparisons/
        ├── matrices/
        ├── guides/
        └── scenarios/
```

---

## Raw Sources

### 两类源 + 一类引用路径

| 路径 | 用途 | 备注 |
|------|------|------|
| `docs/zh/` | **事实入口**：agent 实际读取的源文件（md/rst） | CI 模式下用它扫描 |
| `materials/<version>/zh/` | 发布原料归档（只读，不作为读取入口） | 用于最终 publish 阶段打包 |
| `materials/<version>/output/zh/html/` | wiki 页面 `sources:` 字段的 canonical 引用路径 | 写 frontmatter 时拼接 |

### 原料不可变原则

**永远不修改原料目录下的任何文件。** 读取入口（`docs/zh/`）和归档（`materials/<version>/`）都视为只读。

### Public Scope

Wiki 原料范围 = 从 root `index` 通过 `toctree_includes` 可达的页面集合。

**本地模式**：可用 `scripts/scope.py` 解析 Sphinx 的 `environment.pickle` 精确计算可达页面：

```bash
python scripts/scope.py <path-to>/output/zh/doctrees/environment.pickle
```

**CI 模式**：driver 的 system prompt 明确**禁止解析 `doctrees/environment.pickle`**，agent 直接扫 `docs/zh/`，按 toctree 索引页判断规则跳过纯导航页。

不可达的 orphan 页面默认排除。`_static/api_doc/html/**`（Doxygen API 站）排除。

---

## Version Management

### versions.md 格式

```markdown
# 版本注册表

| 版本 | 状态 | 模式 | 源文件数 | wiki 页面数 | 最后更新 |
|------|------|------|---------|------------|---------|
| v0.1.5 | 完成 | 全量 | 225 | 361 | 2026-04-14 |
```

### manifest.md 格式

每版本一个，记录**当前状态**（不是过程日志）：

```yaml
---
version: v0.1.5
status: 完成
mode: 全量
source_count: 225
wiki_page_count: 361
baseline: v0.1.4
created: 2026-04-13
updated: 2026-04-14
---

## 变更摘要（相对 baseline）
- 新增源文件: 12
- 修改源文件: 8
- 删除源文件: 0
- 新增 wiki 页面: 23
- 更新 wiki 页面: 15

## Source → Page Mapping
| 源文件 | 影响的 wiki 页面 |
|-------|----------------|
| drivers/lisa_gpio/README.md | summaries/drivers-lisa-gpio-README.md, entities/drivers/lisa-gpio.md |

## Known Issues

## 待改进
```

**Source → Page Mapping** 是增量构建的关键：源文件变更时，直接查表确定需要更新的 wiki 页面。

### log.md 格式

append-only，所有操作追加写入，不修改已有条目：

```markdown
## [YYYY-MM-DD] <operation> | <summary>

<2-5 行说明：做了什么、影响了哪些页面>
```

operation 取值：`ingest`、`lint`、`fix`、`query`

---

## Source File Classification

### 源文件类型与处理方式

| 类型 | 特征 | 处理方式 |
|------|------|---------|
| 实质性文档 | 驱动/组件/系统文档，有正文内容 | 生成 summary + 更新相关 entity/concept |
| Sample README | 示例项目说明 | 生成 summary + 更新相关 entity 的 Related Samples |
| toctree 索引页 | 纯导航，正文 < 3 个有意义的句子 | **跳过，不生成 summary** |
| 工具文档 | 命令行工具用法 | 生成 summary + 可能生成 guide |
| 开发板文档 | 硬件规格和引脚分配 | 生成 summary + 更新 entities/boards/ |

### toctree 索引页判断规则

满足以下任一条件即为索引页，**跳过不生成 summary**：

1. 文件内容主要由 `toctree` 指令构成（RST `.. toctree::` 或 MD toctree 块）
2. 正文段落少于 3 个（不含标题、toctree 指令、include 指令）
3. 文件名为 `index_zh.rst`、`index.rst`、`index.md` 且无实质正文

**例外**：如果索引页有实质性导语（章节概述、架构说明），可生成 `type: overview` 的轻量页面。

---

## Wiki Structure

```
wiki/<version>/
├── manifest.md
├── index.md               ← 全局索引（query 时首先读取）
├── log.md                 ← 操作时间线（append-only）
├── reports/               ← lint 报告
├── summaries/             ← 每个实质性源文件一个 summary
├── entities/
│   ├── chips/
│   ├── components/
│   ├── drivers/
│   └── boards/
├── concepts/
├── comparisons/
├── matrices/
├── guides/
└── scenarios/
```

### 文件名约定

- 小写，连字符分隔：`lisa-wifi.md`、`dual-core-architecture.md`
- summary 文件名来自源文件路径（斜杠和下划线替换为连字符）：
  - `drivers/lisa_gpio/README.md` → `summaries/drivers-lisa-gpio-README.md`

---

## Page Frontmatter

所有 wiki 页面必须有 frontmatter：

```yaml
---
title: 页面标题
type: summary | entity | concept | comparison | matrix | guide | scenario | overview
sources:
  - docs/output/zh/html/path/to/source.html
confidence: low | medium | high
created: YYYY-MM-DD
updated: YYYY-MM-DD
---
```

`sources:` 始终使用 `docs/output/zh/html/` 路径（canonical URL）。

### Confidence 分级规则

| 级别 | 条件 |
|------|------|
| `low` | 只有 1 个来源，或来源本身是索引/概述页 |
| `medium` | 2-3 个来源，无矛盾 |
| `high` | 4+ 个来源，无矛盾，**且经过人工确认** |

LLM 不得自行将页面升级为 `high`。

---

## Page Quality Rules

### Summary

**必须包含：**
- 至少 3 个具体事实（数字、函数名、配置项、限制条件）
- 涉及的实体/概念链接
- 可追溯到来源的结论

**禁止：**
- 原始 RST/MD 语法泄漏（如 `wakeup/README.md fd/README.rst`）
- 模板占位符句子（如"该文档主要提供能力说明、配置项和 API 用法"）
- 仅重述标题和章节名

### Entity

**必须包含：**
- 实质性概述（不得是模板占位符）
- 如有 Kconfig：列出真实配置项名称，不得跨驱动污染
- 如有 API：只列公共接口，不列实现内部函数
- Related Samples 节（如有相关示例）
- Evolution Log 节

**API 提取规则：**
- 只提取 README 中明确列为"API 接口"或"函数说明"的函数
- 不提取示例代码中用到的通用系统函数
- 不提取实现内部辅助函数

### Concept

**必须包含：**
- 来自至少 2 个不同源文件的信息（单源概念应合并到 entity）
- 具体数据点（数字、地址、限制）
- 涉及的实体表格
- Evolution Log 节

### Comparison

必须有表格，并明确说明选择标准和推荐场景。

### Matrix

必须说明列含义，并说明如何使用该表。不得成为无来源的数据堆砌。

### Guide

必须是可执行的步骤，包含必要的命令或配置。尽量提供验证方式。

### Scenario

必须给出明确结论（可行 / 有条件可行 / 不建议），并说明约束条件。

### Evolution Log

每个 entity 和 concept 页面在 Cross References 前包含：

```markdown
## Evolution Log
- v0.1.0 (3 sources): 初始创建
- v0.1.2 (4 sources): 新增 DMA 模式说明
- v0.1.5 (4 sources): 修正最大波特率为 921600
```

追加写入，不修改已有条目。

### 矛盾处理

发现矛盾时，**永远不静默覆盖**，使用：

```markdown
> [!conflict]
> [[summary-source-a]] 说 X，但 [[summary-source-b]] 说 Y。
> 待人工确认。
```

### 删除源文件处理

源文件在新版本中被删除时，不硬删除页面，在顶部添加：

```markdown
> [!tombstone]
> 该页面对应的源文件已在本版本删除。内容保留供历史参考。
```

---

## Operations

单 agent 支持四个操作：`ingest`、`query`、`lint`、`fix`

### Ingest

处理原料，构建或更新 wiki。

#### 全量 vs 增量判断

调用方在 prompt 中明确指定 mode（如"模式: 全量"）时以其为准。未指定时按以下规则：

满足以下任一条件 → **全量构建**：
- 该版本没有前一版本的 wiki 作为基线
- `diff.py` 报告变更文件 > 源文件总数的 40%
- 用户明确要求全量

否则 → **增量构建**：
- 只处理 `diff.py` 报告的新增/修改/删除文件
- 查 manifest 的 Source → Page Mapping，确定受影响的 wiki 页面
- 不动未受影响的页面

#### 全量构建流程

1. 运行 `scope.py` 获取可达页面列表：
   ```bash
   python scripts/scope.py "materials/<version>/output/zh/doctrees/environment.pickle"
   ```
2. 遍历 `materials/<version>/zh/` 中所有可达源文件
3. 对每个实质性源文件：
   - 读取源文件
   - 写 summary 页
   - 更新相关 entity/concept 页面
   - 必要时更新 comparison/matrix/guide/scenario
4. 跳过 toctree 索引页
5. 更新 `wiki/<version>/index.md`
6. 更新 `wiki/<version>/manifest.md`
7. 追加写入 `wiki/<version>/log.md`
8. 运行 `python scripts/validate.py wiki/<version>/`
9. **复查补强阶段**：不要在第 8 步通过后结束。继续在同一个 agent 会话中重新阅读
   `wiki/<version>/manifest.md`、`wiki/<version>/index.md` 和当前版本 `docs/zh/` 原料。
10. 查找并修正第一轮遗漏：
    - 已纳入源文件但 summary 过薄、只重述标题或缺少 API/Kconfig/命令/限制
    - 有多个相关源文件但缺少 entity/concept 汇总
    - 有 ≥3 个 sample 或跨模块主题但缺少 matrix/guide/scenario/comparison
    - manifest 中 Known Issues 可通过继续阅读原料解决的问题
    - index 未体现重要主题聚类或页面之间缺少必要交叉链接
11. 在当前 wiki 基础上继续补充和加深页面。能满足本文件质量规则的就写入；
    不能满足的继续记录到 `manifest.md` 的 Known Issues / 待改进,不得生成空页面。
12. 第二轮补强后重新清理空目录,更新 `index.md`、`manifest.md`、`log.md`
    和 `wiki/versions.md` 的计数。
13. 再次运行 `python scripts/validate.py wiki/<version>/`,并把最终校验结果记录到
    `manifest.md` 末尾。

#### 增量构建流程

1. 复制 baseline wiki 目录到本版本（包括 Synthesis 页面）
2. 运行 `diff.py` 比较 baseline 和当前版本的 `zh/`
3. 对每个变更文件：
   - 查 manifest 的 Source → Page Mapping
   - 更新对应 summary 页
   - 更新受影响的 entity/concept/matrix 页
   - 在受影响页面的 Evolution Log 中追加条目
4. 处理新增文件（生成新 summary，更新 index）
5. 处理删除文件（标记 tombstone）
6. 继承的 Synthesis 页面处理：
   - 涉及的 entity/summary 有变化 → 更新 Synthesis 页面中的对应数据
   - 涉及的功能在新版本被删除 → 标记为待审查，记入 manifest
   - 不主动删除继承的 Synthesis 页面
7. 更新 `manifest.md` 和 `log.md`
8. 运行 `validate.py`

#### 中断恢复

ingest 可能因 token 耗尽或其他原因中断。中断时：

1. 已完成的页面保留，不回滚
2. `manifest.md` 标记 `status: partial`
3. 在 manifest 中记录断点位置（最后处理的源文件路径）
4. 恢复时从断点继续，不重复处理已完成的文件

```yaml
---
status: partial
last_processed: drivers/lisa_spi/README.md
sources_processed: 87
sources_total: 225
---
```

### Query

1. 先读 `wiki/<version>/index.md` 定位相关页面
2. 再读具体页面获取证据
3. 综合回答
4. 不混入训练知识，除非明确标注"(note: not from wiki sources)"
5. 如果页面 confidence 为 `low`，回答中明确提示
6. 多个页面说法不一致时，显式指出冲突，不自行裁决
7. 涉及"当前 / 支持 / 推荐"时，必须绑定到具体版本

### Lint

健康检查。输出报告到 `wiki/<version>/reports/lint-YYYY-MM-DD.md`。

#### 机械检查

运行 `validate.py`，检查：
- frontmatter 格式与必填字段
- wiki 链接有效性
- index.md 与实际页面的一致性
- type 与目录的一致性

#### QA 反馈检查

如果调用方提供了 QA records API，运行 `analyze_qa.py`。该流程只做 HTTP GET 读取，不连接数据库、不修改 API 数据：

```bash
python scripts/analyze_qa.py wiki/<version>/ \
  --api-url <qa-records-api> \
  --version <version> \
  --output wiki/<version>/reports/qa-feedback-YYYY-MM-DD.md \
  --jsonl wiki/<version>/reports/qa-signals-YYYY-MM-DD.jsonl \
  --raw-json wiki/<version>/reports/qa-records-YYYY-MM-DD.json
```

提取信号：
- 差评会话（rating=poor）→ 对应 wiki 页面标记为"待改进"
- 无法回答的问题（no_answer）→ wiki 覆盖缺口，需补充页面
- HTML 回源会话（html_llm）→ wiki 摘要深度不足，需补强 summary
- 高频重复问题 → 触发 Synthesis 页面生成（FAQ/Guide/Scenario）

记录规则：
- `qa-records-YYYY-MM-DD.json` 完整保留 API 原始响应、查询参数和抓取时间
- `qa-signals-YYYY-MM-DD.jsonl` 只保存维护所需的结构化信号
- 报告会排除明显 OpenAPI 占位样例（如 `version=string` 且问题为 `string`），但原始 JSON 仍完整保留

#### 语义检查

检查：
- 矛盾（需人工确认）
- 过时信息（新版本已取代旧声明）
- 孤页（无入站链接）
- 缺失页面（被提及但无独立页面）
- 缺失交叉引用
- 数据缺口（覆盖薄弱的主题）
- 模板占位符未替换
- API 提取错误（通用函数混入）
- QA 反馈缺口（来自 `analyze_qa.py` 报告）

#### Synthesis 识别规则

lint 阶段同时识别 Synthesis 补充机会并记入报告：

| 类型 | 触发条件 |
|------|---------|
| Matrix | 某领域有 ≥3 个 sample 但没有对应 matrix |
| Guide | 某任务涉及 ≥2 个步骤且跨 ≥2 个 entity |
| Comparison | 两个 entity 功能重叠或用户可能混淆 |
| Scenario | ≥2 个功能组合在实际使用中常见 |
| FAQ/Guide | 同一主题被用户问 ≥3 次（来自 `analyze_qa.py` 高频问题报告） |

Synthesis 页面的内容**必须从已有 wiki 页面中提取和聚合**，不得凭空编写。每个 Synthesis 页面的 `sources:` 必须列出所有数据来源。

### Fix

根据 lint 报告执行修复。

**可自动修复：**
- 孤页 → 添加合理入站链接
- 缺失交叉引用 → 添加双向链接
- 缺失页面 → 创建 stub 页面
- 模板占位符未替换 → 重写该段落
- index.md 遗漏 → 补充索引项
- QA 反馈：摘要深度不足 → 重新读取源文件，补充 summary 中遗漏的细节
- QA 反馈：高频问题无对应页面 → 创建 Synthesis 页面

**不得自动修复：**
- 矛盾内容
- Kconfig/API 错误
- 过时声明
- 需要架构判断的 comparison/scenario 结论

这些问题必须先报告给用户，由用户确认后再修复。

---

## Conventions

- 所有 wiki 文件为 markdown（`.md`）
- 使用 Obsidian 风格链接：`[[page-name]]`
- 语言：匹配源文件语言（中文文档用中文）
- 文件名使用英文 slug，不用中文文件名
- 默认不创建空页面；stub 仅在 lint 明确发现缺失页时创建
- `index.md` 只做导航，不做证据
- `log.md` 只存摘要和指针，不存长篇详情
- 详情写入 `manifest.md` 或 `reports/*.md`

---

## Script Roles

### validate.py

机械检查工具。保留并持续使用。

### scope.py

范围计算工具。默认使用 `environment.pickle`；如果失败，fallback 到源文件目录扫描。

### diff.py

版本差异工具。用于判断全量/增量，并生成增量更新的文件列表。

### analyze_qa.py

QA 反馈分析工具。从文档助手 QA records API 提取差评、未覆盖、回源、高频问题等信号，输出 `reports/qa-feedback-YYYY-MM-DD.md`、`reports/qa-signals-YYYY-MM-DD.jsonl` 和 `reports/qa-records-YYYY-MM-DD.json`，供 lint / fix / maintenance 流程消费。

---

## Future Improvements

- `validate.py` 改为真正的 YAML frontmatter 解析
- `validate.py` 增加 `sources:` 列表校验
- `scope.py` 增加更安全的非 pickle fallback
- `diff.py` 改进 moved/reorganized 文件检测
- 后续可增加 `outputs/` 层，持久化高价值 query 结果
