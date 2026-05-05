<!-- type: meta -->
<!-- 开发规则: README.md#开发规则 -->

# SDK 开发助手系统全景图

AI 读此文件即可了解整个 skill 体系的结构、文件职责和加载时机，无需遍历目录。

---

## 文件角色定义

| 角色 | 说明 |
|------|------|
| `[ROUTER]` | 意图识别 + 模块识别 + 路由，自身不产生实质输出 |
| `[WORKER]` | 执行特定领域任务，产生代码/文档等输出 |
| `[TEMPLATE]` | 通用执行模板，由 worker 传入参数后执行 |
| `[EXPERIENCE]` | 按模块积累的使用经验，由 worker 按需加载 |
| `[KNOWLEDGE]` | 稳定的架构知识，按需添加和加载 |
| `[META]` | 关于 skill 系统本身的维护指令 |

---

## 入口层

| 文件 | 角色 | 职责 |
|------|------|------|
| `SKILL.md` | `[ROUTER]` | 意图识别 + 模块识别，路由到对应 worker |

---

## Worker 层

按需读取，不预加载。

| 文件 | 角色 | 职责 |
|------|------|------|
| `references/driver-dev.md` | `[WORKER]` | 识别目标设备，调用 driver-template 执行 |
| `references/build-debug.md` | `[WORKER]` | 编译构建、固件烧录、串口调试与故障诊断，并倡导可拆分的独立动作而非固定流程 |
| `references/sample-gen.md` | `[WORKER]` | 生成完整示例项目 |
| `references/code-review.md` | `[WORKER]` | SDK 代码审查（待实现） |
| `references/doc-writing.md` | `[WORKER]` | 文档编写/修复 |
| `references/evolution.md` | `[WORKER/META]` | 知识库维护：SDK 同步、健康检查、git 提取 |

## Template 层

| 文件 | 角色 | 职责 |
|------|------|------|
| `references/driver-template.md` | `[TEMPLATE]` | 驱动通用执行模板（Glob 动态发现资源） |
| `references/dependency-resolver.md` | `[TEMPLATE]` | 依赖链解析模板（从 CMake/Kconfig/样例推导模块依赖） |

---

## Experience 层

按模块一个文件，使用中持续积累。Worker 按需加载。

| 目录 | 角色 | 说明 |
|------|------|------|
| `references/experience/*.md` | `[EXPERIENCE]` | 如 `lisa_uart.md`、`lisa_audio.md`，每条经验带场景、日期、可选版本标记 |

---

## Knowledge 层

存放稳定的架构知识。初始为空，确认"无法从源码动态获取"后按需添加。

| 目录 | 角色 | 说明 |
|------|------|------|
| `references/knowledge/doc-standards.md` | `[KNOWLEDGE]` | SDK 文档编写统一规范（示例/模块/工具三类模板 + 通用格式规则） |
| `references/knowledge/*.md` | `[KNOWLEDGE]` | 其他按需添加 |

---

## Meta 层

仅在维护 skill 系统本身时使用。

| 文件 | 用途 |
|------|------|
| `references/design-principles.md` | skill 设计原则，编写/修改任何 skill 文件前必读 |
| `README.md` | 架构说明 + 开发规则（面向维护者） |
| `docs/` | 设计文档 |

---

## 关联 Skill

| Skill | 位置 | 职责 |
|-------|------|------|
| `experience-capture` | `<skills-root>/experience-capture/` | 经验判断、格式化、写入 |

---

## 路径说明

所有相对路径均以当前 skill 根目录为基准计算；在本仓库中即 `.project/skills/sdk-assistant-agent/`。
