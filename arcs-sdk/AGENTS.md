# Project Agents

本仓库包含项目级 skill：`sdk-assistant-agent`、`gitlab-issue-pr-agent`、`gitlab-mr-review-followup-agent`。

当任务与 ARCS SDK / LISTENAI SDK 开发相关时，应优先加载并遵循：
- `.project/skills/sdk-assistant-agent/SKILL.md`
- `.project/skills/gitlab-issue-pr-agent/SKILL.md`（当任务来自本仓库 GitLab issue，且需要按 issue 建独立 branch / MR 时）
- `.project/skills/gitlab-mr-review-followup-agent/SKILL.md`（当任务进入 MR review 跟进、整改、thread 回复阶段时）

典型触发场景：
- 驱动开发与设备注册
- 编译、烧录、串口日志与构建排障
- 示例代码或 sample 生成
- SDK 代码审查
- SDK 文档编写与知识维护
- 处理 `cloud.listenai.com/CSKG836746/arcs-sdk/arcs-sdk` 的 issue 链接或 issue 编号
- 处理已创建 MR 的 review comment / discussion 跟进、整改与回复

加载后按该 skill 的路由规则选择对应 worker 与 references，避免在仓库中无目的地全量遍历。

## 测试类任务附加要求

当任务涉及新增或修改标准 test 工程、测试用例、`testcase.yaml`、CI 日志判定规则时，除静态检查和本地构建外，还必须满足以下要求后才能认为任务完成并提交代码：

- 必须在实际板子上运行对应测试工程，不能只停留在主机侧构建通过
- 必须抓取串口日志，并确认日志结果符合预期
- 若测试使用 Unity 或等价测试框架，必须以最终测试结果作为主要判定依据
- 回复和提交说明中应记录实际验证命令、板型、关键日志结论；若未完成实板验证，不能宣称该 issue 已完成
