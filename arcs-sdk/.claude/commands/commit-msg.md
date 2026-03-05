根据当前的代码修改生成简洁的中文 git 提交信息。

执行以下步骤：

1. 并行运行 `git status` 和 `git diff --cached` 查看已暂存的改动
2. 如果有已暂存的改动（staged），只针对这些改动生成提交信息
3. 如果没有已暂存的改动，查看所有未暂存改动 (`git diff`)，并询问是否 add 所有改动
4. 分析修改内容，识别模块和改动类型
5. 生成提交信息，包含标题和详细说明
6. 询问用户是否创建 commit，如果确认则使用 `git commit -s` 添加个人签名

## 提交信息格式

```
<type>(<scope>): <简短标题>

<详细说明，2-3行，说明：
- 具体改动内容
- 改动原因
- 影响范围>
```

## Type 类型
- `feat`: 新功能
- `fix`: 修复问题
- `refactor`: 重构
- `perf`: 性能优化
- `docs`: 文档
- `style`: 格式
- `test`: 测试
- `chore`: 构建工具

## 要求
- **智能识别**：优先使用已 staged 的改动，如果没有则提示用户
- **标题**：一行，不超过50字符
- **详细说明**：2-3行，说明具体改动、原因和影响
- **scope**：具体模块名（如 lisa_uart, lisa_gpio）
- **提交命令**：
  - 如果有 staged 改动：使用 `git commit -s`
  - 如果没有 staged 改动且用户确认 add：先运行 `git add -A`，再 `git commit -s`
- **不要**添加 Claude Code 的 Co-Authored-By 信息
