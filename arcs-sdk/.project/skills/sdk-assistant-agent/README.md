# SDK Assistant Agent

ARCS SDK 开发助手，作为仓库内的项目级 skill 包使用。

## 架构概述

```
用户提问
  |
SKILL.md（路由器）— 识别领域 + 模块
  |
Worker（领域专家）— 按需加载 experience，动态读取 SDK 源码
  |
experience-capture（经验捕获）— 任务完成后评估是否有新经验
```

核心设计原则：
- **动态读源码为主**：API 细节从头文件和文档动态获取，不静态存放
- **经验静态沉淀**：踩坑记录和最佳实践按模块积累在 experience 文件中
- **独立于 SDK 版本**：通过 VERSION 文件识别 SDK 版本，experience 条目带版本标记

## 文件结构

```
.project/skills/sdk-assistant-agent/
|-- README.md                    # 本文件（架构 + 开发规则）
|-- SKILL.md                     # 路由器
|-- docs/                        # 设计文档
|-- references/
|   |-- index.md                 # 全景图
|   |-- design-principles.md     # 设计原则
|   |-- driver-dev.md            # Worker: 驱动开发
|   |-- driver-template.md       # Template: 驱动通用执行模板
|   |-- dependency-resolver.md   # Template: 依赖链解析模板
|   |-- build-debug.md           # Worker: 构建调试（独立覆盖编译、烧录、抓日志与诊断）
|   |-- sample-gen.md            # Worker: 示例生成
|   |-- code-review.md           # Worker: 代码审查（待实现）
|   |-- doc-writing.md           # Worker: 文档编写
|   |-- evolution.md             # Worker: 知识库维护
|   |-- knowledge/               # 稳定知识（初始为空）
|   +-- experience/              # 经验积累（按模块）
|       |-- lisa_uart.md
|       +-- ...

experience-capture/              # 独立 Skill：经验捕获
+-- SKILL.md
```

## 项目级加载方式

- `AGENTS.md`：供 Codex 在进入仓库时自动读取，并路由到本 skill
- `CLAUDE.md`：供 Claude Code 在进入仓库时自动读取，并转发到 `AGENTS.md`
- skill 主体路径：`.project/skills/sdk-assistant-agent/SKILL.md`

## 各层职责

| 层 | 文件 | 职责 | 变更频率 |
|----|------|------|----------|
| 路由层 | SKILL.md | 意图识别 + 模块识别 + 分发 | 新增模块时更新关键词表 |
| Worker 层 | references/*.md | 领域执行指令；`build-debug` 负责独立的编译、烧录、抓日志与诊断指引，强调经验与稳健而非固定流程 | 优化流程时更新 |
| Template 层 | driver-template.md, dependency-resolver.md | 驱动执行模板、依赖链解析模板 | 稳定，很少改 |
| Knowledge 层 | references/knowledge/*.md | 稳定架构知识 | 按需添加 |
| Experience 层 | references/experience/*.md | 踩坑和最佳实践 | 高频，日常使用中积累 |

## 开发规则

修改本项目文件时必须遵守以下规则：

### 通用规则

1. **每个文件顶部必须有类型声明**：
   ```markdown
   <!-- type: worker | knowledge | experience | driver-template | meta -->
   <!-- 开发规则: README.md#开发规则 -->
   ```

2. **新增文件必须在 index.md 中注册**

3. **Skill 文件是给 LLM 执行的程序，不是给人读的文档**
   - 每个 Step 包含具体工具调用
   - 用表格代替段落，用决策树代替散文
   - 详见 `references/design-principles.md`

### Driver-template 规则

4. **不写死路径** — 使用 Glob 动态发现
5. **不内嵌 API 知识** — 通过 Read 头文件获取
6. **新增驱动不需要修改任何文件** — 模板自动适配

### Experience 规则

7. **每条经验必须有"场景"和"日期"字段**
8. **版本标记格式**：`<= x.y.z` 或 `>= x.y.z`，仅在经验有版本限制时添加
9. **不存放可从源码读取的 API 说明**

### Knowledge 规则

10. **只添加"无法从源码动态获取的稳定知识"**
11. **添加前先确认**：这个信息能通过 Read/Glob/Grep 从 SDK 源码获取吗？如果能，不要添加

## 如何新增一个领域 Worker

1. 在 `references/` 下创建 `{domain}.md`
2. 添加文件头类型声明
3. 在 `SKILL.md` 的领域路由表中添加行
4. 在 `index.md` 的 Worker 层表格中注册
5. 更新本 README 的文件结构

## 校验

运行 `scripts/validate_skill.sh` 检查：
- index.md 是否包含所有实际文件
- experience 条目格式是否合规
- 新增文件是否已注册
