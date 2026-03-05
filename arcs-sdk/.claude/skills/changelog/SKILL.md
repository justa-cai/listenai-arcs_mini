---
name: changelog
description: 根据 git 提交记录生成或更新 CHANGELOG.md 文件，自动分类提交并按语义化版本管理
---

# CHANGELOG 生成器

自动从 git 提交记录生成符合项目规范的 CHANGELOG.md。

## 执行步骤

1. **读取现有 CHANGELOG.md**
   - 解析最新版本号（如 0.1.1）
   - 识别最新版本的日期

2. **获取 git 提交记录**
   - 尝试查找最新的 git tag
   - 如果有 tag：获取从最新 tag 到 HEAD 的所有提交
   - 如果没有 tag：根据最新版本日期获取之后的所有提交
   - 使用命令：`git log --pretty=format:"%h|%s|%b|%ad" --date=short`

3. **解析和分类提交**
   - 从提交信息中提取 `type(scope): description` 格式
   - 按 type 分类：
     - `feat` → **Added** 部分
     - `fix` → **Fixed** 部分
     - `refactor`, `perf`, `style` → **Changed** 部分
     - `chore`, `docs`, `test` → 根据内容判断或忽略
   - 按 scope 分组相同类型的改动
   - 保留提交的详细说明（body）中的关键信息

4. **生成新版本内容**
   - 询问用户新版本号（提供自动递增建议，如 0.1.1 → 0.1.2）
   - 使用当前日期（YYYY-MM-DD 格式）
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

5. **预览和确认**
   - 显示生成的新版本内容
   - 询问用户是否插入到 CHANGELOG.md
   - 如果确认，将新内容插入到文件顶部（在 `# Change Log` 标题之后）

## 格式要求

- **缩进**：使用 2 空格缩进
- **分组**：相同 scope 的改动合并，使用子列表
- **排序**：
  - 部分顺序：Changed → Fixed → Added → Deprecated
  - 每部分内按 scope 字母排序
- **空行**：各部分之间保留空行

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
- 保持与现有 CHANGELOG.md 相同的格式风格
