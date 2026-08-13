---
name: create-skill
description: Use when creating a new skill in this repository. Enforces the canonical directory layout under .agents/skills/, writes SKILL.md with correct frontmatter, and sets up required symlinks in .claude/skills/ and .windsurf/skills/.
---

# 创建 Skill 目录规范

## 目录结构

每个 skill 统一放在 `.agents/skills/<skill-name>/` 下，并在两个 IDE 目录内各建一条软链接：

```
.agents/skills/
└── <skill-name>/               # skill 本体目录
    ├── SKILL.md                # 必须：skill 定义文件
    ├── references/             # 可选：延伸阅读 / worker 文件
    │   └── *.md
    └── scripts/                # 可选：可执行脚本
        └── *.sh

.claude/skills/
└── <skill-name> -> ../../.agents/skills/<skill-name>   # Claude Code 可见

.windsurf/skills/
└── <skill-name> -> ../../.agents/skills/<skill-name>   # Windsurf 可见
```

> **命名约定**：skill 目录名使用 kebab-case（全小写 + 连字符），与 `SKILL.md` 中的 `name` 字段保持一致。

---

## SKILL.md 格式

```markdown
---
name: <skill-name>
description: >
  一句话描述触发时机，以 "Use when…" 开头。
  可以多行，说清楚触发信号和适用范围。
compatibility: （可选）适用的工具 / 平台说明
allowed-tools: （可选）Read Glob Grep Bash …
---

# Skill 标题

（正文：工作流、规则、约束、示例……）
```

**必填字段**：`name`、`description`。  
**可选字段**：`compatibility`、`allowed-tools`。

---

## 执行步骤

创建新 skill 时，按顺序执行以下命令（将 `my-skill` 替换为实际名称）：

```bash
S=my-skill

mkdir -p .agents/skills/$S
# 写 SKILL.md（见上方格式）
ln -s ../../.agents/skills/$S .claude/skills/$S
ln -s ../../.agents/skills/$S .windsurf/skills/$S
```

---

## 检查清单

完成后逐项验证：

- [ ] `.agents/skills/<skill-name>/SKILL.md` 存在，frontmatter 含 `name` 和 `description`
- [ ] `.claude/skills/<skill-name>` 是指向 `../../.agents/skills/<skill-name>` 的软链接
- [ ] `.windsurf/skills/<skill-name>` 是指向 `../../.agents/skills/<skill-name>` 的软链接
- [ ] skill 名称在以上所有位置保持一致

验证命令：

```bash
S=my-skill
ls -la .agents/skills/$S/$S .claude/skills/$S .windsurf/skills/$S
```

---

## 注意事项

- 所有软链接均使用**相对路径**，不使用绝对路径，确保仓库跨机器可用。
- `references/` 内的文件不需要软链接，skill 的 `SKILL.md` 中用相对路径 `Read references/xxx.md` 即可。
- 创建完成后不需要修改 `CLAUDE.md`，Claude Code 会自动发现 `.claude/skills/` 下的 skill。
