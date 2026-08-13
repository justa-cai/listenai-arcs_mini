<!-- type: meta -->
<!-- 开发规则: README.md#开发规则 -->

# Skill 设计原则

> 本文件定义 sdk-assistant-agent 体系中所有 skill 的设计规范。
> **编写新 skill 或迭代现有 skill 前必须遵循本原则。**

---

## 核心认知：Skill 是给 LLM 执行的程序，不是给人读的文档

这是一切原则的出发点。Skill 文件的读者是 LLM，不是人类开发者。
这意味着：写 skill 的方式更接近写代码，而不是写说明书。

---

## 原则一：指令必须可执行，映射到具体工具调用

**反模式（人类文档写法）：**
> "在生成代码前，参考现有的类似驱动"

**正确写法（LLM 程序写法）：**
> `Glob drivers/lisa_gpio/**` → 读取 `.h` 文件 → 提取 vtable 结构 → 作为生成模板

每一步要明确：**用哪个工具、传什么参数、从结果中提取什么**。

---

## 原则二：用活的数据源替代静态知识

Skill 文件中的静态 API 说明会过时，且占用宝贵的上下文窗口。

**正确做法：**
- API 细节 → 指令 `Glob drivers/lisa_{xxx}/** → Read 头文件和文档`
- 实现模式 → 指令 `Glob/Grep codebase 中的现有实现`
- 配置选项 → 指令 `Read 对应的 Kconfig 文件`

**规则：** Skill 文件只记录"去哪里找"，不记录"找到的内容是什么"。
知识文件（`references/knowledge/*.md`）记录经过验证的稳定知识，但 Skill 文件本身不内嵌 API。

---

## 原则三：决策树替代散文指引

**反模式：**
> "根据设备类型选择合适的参考实现"

**正确写法：**
```
IF 传感器类  → Glob drivers/lisa_adc/**
IF 通信接口  → Glob drivers/lisa_uart/** 或 drivers/lisa_spi/**
IF 存储设备  → Glob drivers/lisa_flash/**
IF 显示设备  → Glob drivers/lisa_display/**
ELSE         → 向用户确认设备类型
```

分支条件要具体、互斥、可判断。

---

## 原则四：Token 效率 — 密度优于完整性

Skill 文件每次调用都会加载到上下文，每个多余的词都有代价。

**实践规则：**
- 用表格代替列表，用列表代替段落
- 删除所有对 LLM 执行没有影响的解释性文字
- checklist 项只写**可验证的条件**，不写无法判断的"应该"
- 子 skill 的 SKILL.md 不重复父级已有的通用知识

---

## 原则五：质量检查项必须可机器判断

**反模式：**
> "- [ ] 代码风格符合规范"

**正确写法：**
> "- [ ] 每个 vtable 函数都有对应的 `static inline` 包装，包装内含 `!dev || !dev->api` NULL 检查"
> "- [ ] `Kconfig` 中存在 `depends on LISA_DEVICE` 行"
> "- [ ] `CMakeLists.txt` 中使用 `listenai_library_sources_ifdef` 而非原生 `target_sources`"

每条 checklist 项要能通过读文件或 Grep 来确认。

---

## 原则六：先读资源，再判断缺什么

不要在 skill 文件中预定义"必须询问的参数列表"——这样的列表会过时，且替代了 LLM 本该做的判断。

**正确流程：**
1. 先并行加载资源（头文件、在线文档、示例）
2. 结合用户描述和资源内容，判断哪些信息无法推断
3. 只针对缺失的、无法推断的信息向用户确认

**可以推断，不必询问：** 能从头文件枚举、示例目录名、文档配置表中直接读出的信息。

**必须询问：** 硬件相关的具体值（引脚号、设备实例、目标地址等），以及资源中存在多种选项而用户未明确的场景。

---

## 原则七：行为与知识分离

| 文件类型 | 存放内容 | 举例 |
|----------|----------|------|
| `SKILL.md`（入口/Router） | 意图识别、模块识别、路由指令 | 领域路由表 + 模块关键词表 |
| `references/driver-template.md` | 驱动通用执行模板（Glob 动态发现） | 资源发现 + 知识加载 + 执行步骤 |
| `references/{worker}.md` | 领域 Worker 执行指令 | build-debug、sample-gen 等 |
| `references/index.md` | 系统全景图 | 所有文件角色与加载时机 |
| `references/knowledge/*.md` | 稳定的架构知识（按需添加） | 初始为空 |
| `references/experience/*.md` | 按模块积累的经验 | 踩坑记录、最佳实践 |

**规则：** 执行文件引用知识文件，不内嵌知识内容。

---

## 原则八：子 Skill 的调用协议

父级 SKILL.md 通过 "读取 `[路径]` 执行" 调用 worker 文件：
- 功能 worker：`读取 references/build-debug.md 执行`
- 驱动设备 worker：`读取 references/drivers/lisa_uart.md 执行`

Worker 文件必须满足：

1. **自包含**：执行所需的上下文来源于工具调用，不依赖父级传递的隐式状态
2. **单一职责**：一个 worker 解决一类问题，不承担路由职责
3. **输出明确**：结尾声明产出物（生成了哪些文件、回答了什么问题）

---

## 原则九：迭代友好的结构

Skill 文件应便于局部更新：
- 每个 Step 是独立的逻辑单元，修改一个 Step 不影响其他 Step
- 决策树的分支条件写在同一处，不散落在多个地方
- 新增功能通过新增 Step 或新增分支，而非修改现有逻辑

---

## 自检清单（编写/修改 skill 后使用）

在提交 skill 变更前，逐项确认：

**通用检查（所有 skill 文件）**
- [ ] 每个 Step 都包含至少一个具体工具调用（非仅文字描述）
- [ ] 没有内嵌可以通过 WebFetch/Grep/Read 获取的 API 信息
- [ ] 所有决策分支的判断条件是明确的
- [ ] 文件中没有纯解释性的段落（对 LLM 执行无影响的内容）
- [ ] worker 不依赖父级的隐式状态

**功能 Worker 专项（build-debug.md、sample-gen.md 等）**
- [ ] 质量 checklist 项都可以通过读文件或 Grep 来验证
- [ ] 执行结尾声明了产出物（生成了哪些文件、回答了什么问题）

**驱动模板专项（driver-template.md）**
- [ ] 使用 Glob 动态发现，不写死路径
- [ ] 不内嵌 API 知识
- [ ] 包含 experience 文件加载步骤

**经验文件专项（references/experience/*.md）**
- [ ] 每条经验有"场景"和"日期"字段
- [ ] 版本标记格式合法（如 `<= 0.1.3` 或 `>= 0.2.0`）
- [ ] 无纯 API 说明（应通过 Read 头文件获取）
