---
name: commit
description: Git 提交代码。使用 /commit 或当用户提到"提交"、"commit"时触发。只聚焦规范化 Git 提交、暂存范围、提交信息、submodule 顺序和推送顺序，不做代码审查。
---

# ARCS Git 提交

## 兼容约定

- 本技能同时适用于 Claude Code 和 Codex；不要依赖单一客户端专属工具名。
- 下文 `/xxx` 是触发示例；客户端不支持 slash 命令时，用对应自然语言请求即可。
- 需要用户选择或确认时，直接提出简短问题；如果当前环境提供专用提问工具，可使用该工具。
- 命令默认从仓库根目录执行；不要硬编码个人机器上的绝对路径。

## 使用方式

`/commit [范围]`

示例:
- `/commit` — 自动检测当前仓库变更并提交
- `/commit arcs-sdk` — 只提交 `arcs-sdk` 子模块
- `/commit --all` — 遍历所有 submodule 后提交主仓库
- `/commit "docs(agents): 更新提交规范"` — 使用用户指定的提交信息

## Submodule 仓库

| 名称 | 路径 | 说明 |
|------|------|------|
| arcs-sdk | `arcs-sdk/` | ARCS SDK（HAL、驱动、RTOS） |
| ebus | `modules/ebus/` | 事件总线模块 |
| lschat | `modules/lschat/` | 聆思大模型 SDK |
| lisaui | `modules/lisaui/` | LVGL UI 组件库 |
| **主仓库** | `.` | voiceassistant 工程 |

## 总原则

- **原子提交**：一个 commit 只表达一个清晰目的；无关改动分开提交或保留未提交。
- **显式暂存**：只 `git add` 本次提交相关路径，不使用 `git add -A`。
- **保护分支**：不在 `arcs-mini/main` / `main` / `master` 上提交。
- **仓库作者身份**：提交作者使用当前仓库 `git config user.name` / `git config user.email`，不在 skill 中改写作者。
- **先子模块后主仓库**：submodule 内部变更先在子模块提交，再提交主仓库中的 submodule 指针。
- **不做审查**：本技能不进行代码审查；需要审查时使用 `review` skill。

## 执行流程

### 1. 检查分支

```bash
git branch --show-current
```

- 如果当前分支是 `arcs-mini/main`、`main` 或 `master`，不要提交。
- 先阅读当前变更，根据修改内容创建主题分支，再继续提交：

```bash
git switch -c <type>/<short-topic>
```

分支命名建议：

| 类型 | 示例 | 适用场景 |
|------|------|----------|
| `feat/` | `feat/at-battery-status` | 新功能 |
| `fix/` | `fix/wifi-reconnect` | Bug 修复 |
| `docs/` | `docs/agents-commit-rules` | 文档/规则 |
| `ci/` | `ci/codex-review-comment` | CI 配置 |
| `chore/` | `chore/update-submodule` | 维护性变更 |

无法判断分支名时，先询问用户。

### 2. 检测变更范围

```bash
git status -s
git diff --stat
git submodule foreach --quiet 'if [ -n "$(git status -s)" ]; then echo "$sm_path"; fi'
```

处理规则：

- 用户指定范围时，只处理该范围。
- 如果只有主仓库普通文件变更，忽略无关 submodule dirty 状态。
- 如果存在编译产物、日志、临时文件或明显无关文件，先说明并排除。
- 如果没有任何可提交变更，告知用户“没有需要提交的内容”并结束。

### 3. 划分提交

根据 diff 判断是否需要拆分：

- 文档、CI、代码、资源、submodule 指针通常应分开提交。
- 同一功能的代码、配置和文档可以放在同一个提交中。
- 不确定是否拆分时，选择更小、更清晰的提交，或询问用户。

### 4. 暂存文件

主仓库：

```bash
git add <file1> <file2>
git diff --cached --stat
git diff --cached --name-status
```

Submodule：

```bash
git -C <submodule_path> add <file1> <file2>
git -C <submodule_path> diff --cached --stat
```

要求：

- 暂存后必须检查 staged 内容是否只包含本次提交范围。
- 不暂存未确认的大文件、生成文件、密钥、token、`.env`。
- 不用 `git add -A`；如果确需批量添加，使用明确路径或 pathspec。

### 5. 生成提交信息

使用 Conventional Commits 风格：

```text
<type>(<scope>): <subject>

<body>

Co-Authored-By: <当前模型和版本> <noreply@...>
```

字段规则：

| 字段 | 规则 |
|------|------|
| `type` | 必须是 `feat` / `fix` / `refactor` / `docs` / `ci` / `test` / `build` / `chore` 之一 |
| `scope` | 可选，使用模块、app、目录或功能名，如 `agents`、`arcs-evb`、`review-ci` |
| `subject` | 简短描述做了什么，建议不超过 72 个字符，不以句号结尾 |
| `body` | 可选，说明原因、影响范围或关键决策；简单文档提交可省略 |
| `footer` | 必须包含表示当前协作模型和版本的 `Co-Authored-By` 签名 |

`fix` 类型提交必须包含正文，说明：

- 问题发生的边界：在哪些条件、配置、输入、设备或流程下会触发。
- 修复方法：改了什么逻辑、为什么能解决问题，以及是否影响其他场景。

示例：

```text
fix(wifi): 避免重连流程重复释放连接对象

问题边界：仅在 WiFi 连接超时后立即收到断连事件时触发，普通主动断开流程不受影响。
修复方法：在释放连接对象前检查状态并清空指针，避免超时回调和断连回调重复释放。

Co-Authored-By: <当前模型和版本> <noreply@...>
```

常用 type：

| type | 适用场景 |
|------|----------|
| `feat` | 新功能、新能力 |
| `fix` | 修复缺陷 |
| `refactor` | 不改变行为的重构 |
| `docs` | 文档、注释、agent/skill 说明 |
| `ci` | CI/CD、GitLab/GitHub 配置 |
| `test` | 测试用例、测试脚本 |
| `build` | 构建系统、依赖、工具链配置 |
| `chore` | 维护性改动、格式化、杂项 |

提交信息选择示例：

| 改动 | 提交信息 |
|------|----------|
| 修改 agent skill | `docs(agents): 完善提交技能规范` |
| 修复 GitLab 评论复用 | `ci: 修复 Codex Deep Review 评论复用` |
| 新增 AT 命令 | `feat(sunra-e-bike): 添加电池状态 AT 命令` |
| 修复构建配置 | `build(arcs-evb): 修正 Kconfig 依赖` |
| 更新 submodule 指针 | `chore: 更新 arcs-sdk submodule 引用` |

签名规则：
1. 提交作者使用当前仓库配置，可用 `git config user.name` 和 `git config user.email` 检查；缺失时先让用户配置 Git 作者身份。
2. `Co-Authored-By` 写当前实际协作模型和版本，例如 `Co-Authored-By: Codex GPT-5 <noreply@openai.com>`。
3. 用户提供的 `Co-Authored-By` 只有在能准确表示当前实际模型和版本时才使用；不要把用户个人身份写成协作者签名。
4. 不从 `.agents/local/AGENTS.local.md` 读取或保存 `Co-Authored-By` 作为默认值。

### 6. 执行提交

主仓库：

```bash
git commit -m "<type>(<scope>): <subject>" -m "Co-Authored-By: <当前模型和版本> <noreply@...>"
```

带正文：

```bash
git commit \
  -m "<type>(<scope>): <subject>" \
  -m "<body>" \
  -m "Co-Authored-By: <当前模型和版本> <noreply@...>"
```

Submodule：

```bash
git -C <submodule_path> commit -m "<message>" -m "Co-Authored-By: <当前模型和版本> <noreply@...>"
```

### 7. 提交顺序

1. 提交有变更的 submodule。
2. 回到主仓库，暂存并提交 submodule 指针变更。
3. 再提交主仓库普通文件；如果普通文件和 submodule 指针属于同一目的，也可以同一提交。
4. 如果只提交主仓库普通文件，不处理无关 submodule dirty 状态。

### 8. 提交后检查

```bash
git status -s
git log --oneline -1
```

输出汇总：

```markdown
## 提交完成

| 仓库 | Commit | 信息 |
|------|--------|------|
| 主仓库 | `abc1234` | docs(agents): 完善提交技能规范 |

未提交内容：<如有，列出并说明为何保留>
```

## 推送

仅在用户明确要求 push/推送时执行。

- 先推送有新提交的 submodule。
- 再推送主仓库。
- 任一 submodule 推送失败时停止后续推送，避免主仓库引用远端不存在的 submodule commit。

## 禁止事项

- 不主动 push。
- 不在 `arcs-mini/main`、`main` 或 `master` 分支上提交。
- 不使用 `git add -A`。
- 不使用 `git commit --amend`，除非用户明确要求。
- 不使用 `--no-verify`。
- 不回滚用户已有改动。
- 不把代码审查混入提交流程；需要审查时使用 `review` skill。
