---
name: mcp
description: 为 ARCS-MINI 新增、修改或排查端侧 MCP 工具；新增工具强制使用无前缀普通名称，维护既有工具时保持原工具名不变。用户提到 MCP 工具、tools/list、tools/call、MCP_TOOL_DEFINE、工具描述、参数 schema、工具调用处理或 apps/arcs-mini/mcp-tools 下的代码时使用。
---

# ARCS-MINI MCP 工具开发

## 基准实现

以 `apps/arcs-mini/mcp-tools/mcp_tool_uart1.c` 为新增工具的首选示例。先读取该文件，再结合目标服务接口和同目录相近工具实现需求。

明确三个角色：

- `list` 回调定义工具描述和输入参数 schema。
- `call` 回调解析参数、执行实际动作并返回调用结果。
- `MCP_TOOL_DEFINE` 的第一个参数定义对外工具名；UART1 示例的工具名是 `uart1_send`。

## 工具命名

先判断任务是新增工具还是维护既有工具：

- 新增工具：必须使用无命名空间前缀的普通名称，推荐匹配 `[a-z][a-z0-9_]*`。
- 维护工具：保持 `MCP_TOOL_DEFINE` 中的原工具名完全不变，即使原名含 `ls.*`、`ls.buildin.*`、`ls.built_in.*` 或其他历史前缀。

新增工具的正确命名：

```c
MCP_TOOL_DEFINE(uart1_send, mcp_tool_uart1_list, mcp_tool_uart1_call);
```

新增工具禁止：

```c
MCP_TOOL_DEFINE(ls.uart1_send, ...);
MCP_TOOL_DEFINE(ls.buildin.uart1_send, ...);
MCP_TOOL_DEFINE(ls.built_in.uart1_send, ...);
```

不要因为同目录旧代码仍使用 `ls.*` 或 `ls.built_in.*` 就给新增工具复制其前缀。维护旧工具时也不要套用新增命名规则；除非用户明确要求迁移名称，否则禁止顺带重命名已有工具。

## 实现流程

1. 在 `apps/arcs-mini/mcp-tools/` 中查找职责相近的工具和可复用 service 接口。
2. 新建或修改 `mcp_tool_<功能>.c`，保持业务实现位于应用层，非必要不要修改 `arcs-sdk/`。
3. 实现 `list` 回调：
   - 使用 `mcp_tool_list_info_create_default(name, description)` 创建描述。
   - 清楚说明适用意图、非适用场景和参数语义。
   - 使用 `mcp_tool_info_add_property` 添加简单参数。
   - 需要 `enum`、范围等约束时，构造属性 JSON 并使用 `mcp_tool_info_add_json_property`。
   - 正确标记必填和可选参数。
4. 实现 `call` 回调：
   - 使用 `mcp_tool_call_args_get(args, "<参数名>")` 取参。
   - 在访问值之前检查参数存在性、cJSON 类型、空字符串、数值范围和枚举取值。
   - 调用目标 service 或业务接口，并检查返回值、部分成功和异常状态。
   - 使用 `mcp_tool_call_result_create(name)` 返回 MCP 结果；文本结果放入 `content` 数组，并准确设置 `isError`。
   - 未使用的 `id` 显式写 `(void)id;`。
5. 处理注册宏：
   - 新增工具：在文件末尾使用 `MCP_TOOL_DEFINE(<裸工具名>, <list回调>, <call回调>)` 注册。
   - 维护工具：保留现有 `MCP_TOOL_DEFINE` 的第一个参数，不修改工具名。
6. 把新增 `.c` 文件显式加入 `apps/arcs-mini/mcp-tools/CMakeLists.txt` 的 `listenai_library_sources(...)`；有 Kconfig 条件时放入对应条件块。

## 质量检查

- 确认 `list` 的 schema 与 `call` 实际解析的参数名、类型、必填性完全一致。
- 新增工具时，确认注册名不含 `.`，不以 `ls`、`ls.buildin`、`ls.built_in` 或其他命名空间开头。
- 维护工具时，对比修改前后的 `MCP_TOOL_DEFINE`，确认原工具名未发生变化。
- 检查所有 cJSON 分配失败路径，避免泄漏或重复释放所有权已转移的节点。
- 参数错误和业务失败应返回 `isError=true` 的可读结果；只有无法创建结果对象等底层失败才返回 `NULL`。
- 避免在日志或返回文本中无界拼接用户输入；使用定长缓冲区时检查截断风险。
- 检查 `CMakeLists.txt` 已包含新增源文件。
- 按 `.agents/skills/build/SKILL.md` 编译默认 `apps/arcs-mini` / `arcs_mini`，并根据改动风险决定是否按 `flash`、`run-log` skill 做设备验证。

## 交付说明

说明工具名、参数 schema、实际调用的业务接口、构建接入位置和验证结果。维护既有工具时，说明工具名保持不变。若未执行编译或设备验证，明确说明未验证项。
