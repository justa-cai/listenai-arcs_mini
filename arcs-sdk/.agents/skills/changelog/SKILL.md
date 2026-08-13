---
name: changelog
description: Use when generating or updating CHANGELOG.md from git history, especially when related feature, fix, refactor, and follow-up commits should be merged into readable version-level release notes
---

# CHANGELOG 生成器

自动从 git 提交记录生成符合项目规范的 CHANGELOG.md。

核心原则：**CHANGELOG 是版本最终交付结果，不是 commit 列表翻译。**

## 执行步骤

1. **读取现有 CHANGELOG.md**
   - 解析最新版本号（如 0.1.1）
   - 识别最新版本的日期

2. **获取 git 提交记录**
   - 尝试查找最新的 git tag
   - 如果有 tag：获取从最新 tag 到 HEAD 的所有提交
   - 如果没有 tag：根据最新版本日期获取之后的所有提交
   - 使用命令：`git log --pretty=format:"%h|%s|%b|%ad" --date=short`

3. **先清洗，再按功能主题归并**
   - 从提交信息中提取 `type(scope): description` 格式
   - 先过滤或弱化以下提交，避免把噪音写进版本说明：
     - `Revert`：默认从结果中剔除；若 revert 抵消了之前的提交，则两者都不应出现在最终 changelog
     - 纯 submodule bump / pointer update / rebase pointer / merge 噪音：默认忽略，除非提交正文明确说明带来了用户可感知能力
     - 纯 docs / test / ci / style / chore：默认忽略；只有直接改变用户交付物、构建行为或版本能力时才保留
   - 不要直接按单条 commit 生成 changelog，必须先做“功能主题聚合”
   - 主题聚合优先级：
     1. 优先按真实功能主题归并，而不是机械按 scope 归并
     2. scope 相近或命名不一致时，允许人工合并为同一主题
     3. 常见可归并示例：`sample` / `samples`、`display` / `lisa_display`、`boot-adb` / `tinyusb-adb`、`modem` / `samples/modem`
   - 同一主题下如果出现以下连续提交，默认合并成一条最终描述，而不是拆成多条：
     - `feat` + 后续 `fix`
     - `feat` + `perf` / `refactor`
     - `feat` + 配套 `test` / `docs`
     - `sample` 新增 + sample 修复/调整
   - 归并后的描述应表达“版本最终交付了什么”，而不是“先做了什么、后来又修了什么”
   - 保留提交正文中的关键信息，但只吸收最终结果相关的内容

4. **将主题映射到 CHANGELOG section**
   - 询问用户新版本号（提供自动递增建议，如 0.1.1 → 0.1.2）
   - 使用当前日期（YYYY-MM-DD 格式）
   - 对每个主题只保留一条主归属，避免重复出现在多个 section
   - section 判定规则：
     - 主题以“全新能力、新组件、新示例、新接口”为主 → **Added**
     - 主题以“行为调整、重构、性能改进、兼容性更新”为主 → **Changed**
     - 主题仅包含独立 bugfix，且没有形成新的最终交付能力 → **Fixed**
   - 如果同一主题既有新增又有修复，默认优先写成一条 **Added** 或 **Changed**，在描述中吸收后续修复；不要同时在 **Added** 和 **Fixed** 各写一条

5. **生成新版本内容**
   - 生成格式：
   ```markdown
   ## [新版本号] - YYYY-MM-DD:

   - All changes since 上个版本号

   ### Changed:
     - scope: 变更说明

   ### Fixed:
     - scope: 修复说明

   ### Added:
     - scope: 新增说明

   ### Deprecated:

   ```
   - 写作时先决定“主题层级”，再落文字：
     - 小主题：一行写完
     - 中主题：主条目 + 2-4 个子项
     - 大主题（如 `samples`、`adb/boot-adb`、`display`）：必须使用主条目 + 子项，不能把多个子域压成一行
   - 对聚合主题优先按“读者理解方式”分组，而不是按 commit 顺序分组
   - `samples` 这类主题优先按领域拆组，如：
     - `network`
     - `adb`
     - `ota`
     - `display`
     - 只有确实无法归类时，才按 sample 名称平铺
   - 子项应表达一类相关能力，不要把多个无关点拼在同一句

6. **输出前去重审查**
   - 检查同一主题是否同时出现在多个 section
   - 检查是否仍残留“先新增、后修复”的重复表述
   - 检查是否把已被 revert 的内容写入 changelog
   - 检查条目是否面向版本读者，而不是面向提交历史读者
   - 检查是否存在“主条目过于抽象、子项过于拥挤”的排版问题

7. **预览和确认**
   - 显示生成的新版本内容
   - 询问用户是否插入到 CHANGELOG.md
   - 如果确认，将新内容插入到文件顶部（在 `# Change Log` 标题之后）

## 格式要求

- **缩进**：使用 2 空格缩进
- **分组**：相同功能主题的改动合并，必要时使用子列表
- **可读性优先**：不要在一行内堆叠过多并列项
- **拆行规则**：
  - 同一条目下如果并列项超过 3 个，必须改为子列表逐行展开
  - 如果一条描述同时包含多个子主题（如多类 samples、多类协议、多类设备），应拆成 2-4 个子项
  - 避免出现“新增 A、B、C、D、E、F”这类长串枚举；优先改写为主条目 + 子项
- **单行长度**：优先保持每条在一行内可快速扫读；过长时宁可增加子项，也不要硬塞进一句话
- **层级规则**：
  - 一行只表达 1 个主结论
  - 子项里最多放 2 个强相关实体；超过则继续拆分
  - 主条目应概括主题，不要把细节和结论混在同一层
- **聚合主题规则**：
  - `samples`、`examples`、`demos` 默认按场景或领域分组，不直接平铺名称列表
  - `drivers`、`display`、`network`、`boot/adb` 这类跨模块主题，优先写“能力类别”再写子项
- **可扫读性检查**：
  - 如果一行出现过多 `、`、`和`、`及`，应继续拆分
  - 如果一行出现 4 个以上独立能力名或示例名，应改写为子列表
- **排序**：
  - 部分顺序：Changed → Fixed → Added → Deprecated
  - 每部分内优先按主题重要性排序，其次再按名称排序
- **空行**：各部分之间保留空行
- **去重复**：同一主题在整个版本中只能出现一次，除非用户明确要求保留细粒度拆分

## 示例输出

```markdown
## [0.1.2] - 2025-01-14:

- All changes since 0.1.1

### Changed:
  - drivers/lisa_uart: 防止传输过程中重新配置

### Fixed:
  - drivers/lisa_gpio: 修复中断被重复触发的问题
  - drivers/lisa_flash: 修复边界检测

### Added:
  - boards: 新增rgb pinmux适配
  - components/new_feature: 新增功能组件
```

## 注意事项

- **不要**自动 git commit 生成的 CHANGELOG
- **只处理** CHANGELOG.md 文件，不修改其他文件
- 如果提交信息格式不规范，尽量智能提取关键信息
- 对于合并提交（Merge commit），可以忽略或提取实际改动
- 如果发现“新增 A，随后多次修复 A”，默认输出为一条关于 A 的最终版本能力说明
- 不要把同一主题拆散到 `Added`、`Changed`、`Fixed` 多个 section
- 不要机械逐条翻译 commit；先归并，再摘要
- 如果一个主题下包含大量示例或能力名称，必须分层排布，不能用一行长列表堆砌
- 如果生成后的条目读起来像“名词清单”，说明层级设计失败，需要重新组织
- 保持与现有 CHANGELOG.md 相同的格式风格
