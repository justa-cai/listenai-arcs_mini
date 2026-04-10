# 表情新增/修改指南（Emoji）

## 1. 目标

本文档说明在当前工程中：

- 新增一个表情资源（帧 PNG）
- 将资源接入到表情动画播放系统
- 使其可被 UI Presenter 主动播放
- 可选：加入 MCP 工具可选列表（让云端工具调用触发）

> 约束：日志使用英文；注释使用中文。

---

## 2. 现有链路概览

### 2.1 UI 播放入口（Presenter）

在 UI 层，表情播放通常由 Presenter 触发，例如：

- `apps-ui/apps/llm/presenters/home_presenter.c`
  - `show_emoji_anim(scr_data, "blink", 0);`
  - 内部会调用 `emoji_anim_get_by_name("blink")` 取得动画配置

### 2.2 动画配置与资源引用

- `apps-ui/apps/llm/presenters/emoji_anim.c`
  - 维护：
    - 帧数组（`static const void *xxx_frames[]`）
    - 动画配置（`lisa_ui_anim_ext_config_t`：enter/loop/exit）
    - 名称到动画配置映射（`emoji_configs[]`）
  - 资源符号来自：`lisa_ui_assets.h`

### 2.3 资源生成机制

资源文件放在：

- `apps-ui/assets/embed/`

生成脚本：

- `apps-ui/assets/assets.py`

生成产物（构建目录）：

- `${CMAKE_BINARY_DIR}/generated/assets/`
  - `*.c`（每个图片一个 C 文件）
  - `lisa_ui_assets.h`（声明 `LV_IMG_DECLARE(...)`）

生成规则：

- `assets.py` 会对相对路径做变量名转换：
  - `/`、`\`、`.`、`-` 都替换为 `_`
  - 例如：
    - `emoji/blink/blink_000.png` -> `emoji_blink_blink_000_png`

CMake 集成：

- `apps-ui/assets/CMakeLists.txt` 会在配置期与增量构建期执行 `assets.py`

---

## 3. 新增表情（推荐流程）

下面以新增一个表情 `puzzled` 为例（你可以替换为任意名称）。

### 3.1 添加 PNG 帧资源

1. 在 `apps-ui/assets/embed/` 下创建目录：

- `apps-ui/assets/embed/emoji/puzzled/`

2. 将帧 PNG 放入该目录，建议命名统一、可排序：

- `puzzled_000.png`
- `puzzled_001.png`
- ...

建议：

- 所有帧尺寸一致
- 透明背景（若需要）使用 PNG alpha

### 3.2 生成资源 C 文件与头文件

编译系统会自动生成，但在 Linux 模拟器下也可手动执行：

- 参考 `apps-ui/README.MD` 的命令：
  - `python3 ../assets/assets.py ../assets/embed build/generated/assets`

生成后你将获得：

- `build/generated/assets/lisa_ui_assets.h`
- 对应的 `emoji_puzzled_puzzled_000_png` 等符号

验证点：

- 在生成的 `lisa_ui_assets.h` 中能搜索到：
  - `LV_IMG_DECLARE(emoji_puzzled_puzzled_000_png);`

### 3.3 在 emoji_anim.c 注册帧与动画

文件：

- `apps-ui/apps/llm/presenters/emoji_anim.c`

步骤：

1. 新增帧数组（示例，按你的帧数量填写）：

- `static const void *emoji_puzzled_enter[] = { ... }`
- `static const void *emoji_puzzled_loop[] = { ... }`
- `static const void *emoji_puzzled_exit[] = { ... }`

2. 新增动画配置：

- `static const lisa_ui_anim_ext_config_t anim_ext_config_puzzled = { ... }`

说明：

- `enter.loop = 0` 表示进入段不循环
- `loop.loop = -1` 表示循环段无限循环
- `exit.loop = 0` 表示退出段不循环
- `default_delay` 为每帧默认时延（ms）

3. 在 `emoji_configs[]` 中添加映射项：

- `.name = "puzzled"`
- `.anim = &anim_ext_config_puzzled`

注意：

- `.name` 是对外使用的字符串（Presenter/MCP/云端都可能用）
- `emoji_anim_get_by_name()` 是按 `.name` 查找

### 3.4 在 Presenter 中触发播放

例如在 `home_presenter.c` 中直接调用：

- `show_emoji_anim(scr_data, "puzzled", 0);`

说明：

- 若 `emoji_anim_get_by_name("puzzled")` 找不到，会降级到 standby（并打印 error log）

---

## 4. 可选：加入 MCP 表情列表（让工具调用可选）

MCP 工具表情列表位置：

- `src/server/lschat_server/mcp_tool_emoji.c`

该文件维护：

- `emoji_list[] = { {"angry", "生气"}, ... }`

如果你希望 MCP 工具能选择新表情：

1. 增加一项：

- `{ "puzzled", "困惑" }`

2. 工具调用最终会发送：

- `voice_msg_pub(VOICE_MSG_CLOUD_MCP_EMOJI, emotion->valuestring, ...)`

随后由业务侧接收并触发 UI 更新。

注意：

- 这里的 `name_cn` 只是给工具描述用（中文），不影响动画播放逻辑

---

## 5. 常见问题排查

- **资源符号找不到**：
  - 检查 PNG 是否放在 `apps-ui/assets/embed/` 内
  - 检查生成的 `lisa_ui_assets.h` 是否包含对应 `LV_IMG_DECLARE(...)`
  - 检查文件名是否包含 `-`、`.` 等导致变量名变化（脚本会转换为 `_`）

- **能播放但显示空白**：
  - 检查图片尺寸是否为 0（生成脚本通过 PNG header 读取 w/h）
  - 检查 PNG 是否为有效 PNG

- **播放名称拼错**：
  - `emoji_anim_get_by_name()` 查不到会降级到 standby，并打 error log

---

## 6. 最小变更清单（Checklist）

- **资源**：`apps-ui/assets/embed/emoji/<name>/*.png`
- **生成**：确认 `lisa_ui_assets.h` 有 `LV_IMG_DECLARE(emoji_<...>_png)`
- **动画注册**：`apps-ui/apps/llm/presenters/emoji_anim.c`
  - 帧数组 + `anim_ext_config_xxx` + `emoji_configs[]` 增加条目
- **触发点**：Presenter 调用 `show_emoji_anim(..., "<name>", ...)`
- **可选**：`src/server/lschat_server/mcp_tool_emoji.c` 增加可选列表
