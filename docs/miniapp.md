# ARCS-MINI Lua 小应用

## 功能

小应用是由云端下发、在设备本地执行的 Lua 脚本。设备可根据产品配置提供：

- 全屏绘制矩形和文字。
- 接收功能键单击事件。
- 控制状态灯亮灭及闪烁。
- 通过扬声器播放指定频率和时长的蜂鸣声，使用系统音量。
- API 4 可按设备能力发起 HTTP 请求、处理 JSON 和请求文本播报。

从桌面进入小应用时停止当前语音输入和音乐播放，保留触发本次安装的交互响应 TTS；语音安装与远程推送安装均按此规则处理。
进入小应用或退出回桌面时结束当前交互，TTS 播完后不自动续聊，需重新唤醒，使新会话的 `start` 帧携带最新模式。
设备原有的单工、半双工或全双工模式不变，再次唤醒后的交互遵循原模式。运行中的小应用被另一个小应用替换时也结束当前交互，使下次 `start` 帧携带新应用身份。
运行期间的语音交互范围为重新生成、修改、切换和询问玩法；闹钟到点正常响铃提醒。
端侧限制无关工具和音乐播放；云端依据下文的 `start` 帧模式，在产生回复之前过滤无关意图。
功能键每次短按释放立即向小应用发送一次点击，脚本可根据点击间隔识别双击等操作；系统的多击快捷操作屏蔽，长按 3 秒返回桌面。
长按退出后继续按住不会触发关机；需先松开，再次长按才会按桌面规则关机。

运行期间保持正常电源状态，不进入空闲降亮度或休眠。自动关机仍沿用设备原有条件：
无用户活动累计 15 分钟、未接外部电源且电量不高于 30% 时，播放提醒后进入 10 秒关机倒计时。
按键和语音交互按原规则重新计时；脚本动画和蜂鸣声不视为用户活动。

## 内存与生命周期

小应用仅在 PSRAM 中运行，源码不写入 Flash 或 KV。
退出或重启后释放本地应用，再次使用需由云端重新下发。
API 3 起提供的 `storage` 可单独持久化少量应用数据，不保存源码。

替换应用时，候选版本和当前版本暂时并存。候选版本完成校验、启动回调和界面初始化后，
才替换当前版本；失败时保留仍驻留内存的旧应用。因此安装新应用时需留出两个 Lua 运行实例的内存。
相同版本复用仅适用于仍驻留内存的应用，不表示设备具有持久化应用库。

## MCP 接口

以下接口均使用标准 MCP `tools/list` 和 `tools/call` 机制，结果为
`{ "content": [{ "type": "text", "text": "..." }], "isError": false }`。
失败时 `isError` 为 `true`，`text` 提供简短错误说明。

### 查询能力：`ls.built_in.get_device_capabilities`

参数为空对象。成功结果的 `text` 是 UTF-8 JSON，包含：

- `schema_version`：能力结构版本，目前为 `2`。
- `firmware_info`：固件类型及构建版本。
- `hardware`：实际支持的屏幕、按键、音频、灯光及联网能力；`network` 为布尔值，表示具备联网能力，不表示当前在线。
- `runtime.lua_sdk`：启用小应用时提供 API 版本、源码字节上限和每个 Lua 实例的堆上限，以及 `storage` 的容量、有效期和写入间隔。API 4 还提供 `http`、`tts` 对象。

`runtime.lua_sdk.http` 包含 `methods`（字符串数组）、`max_pending`、`max_url_bytes`、
`max_headers`、`max_headers_bytes`、`max_body_bytes`、`max_response_bytes`、
`default_response_bytes`、`max_timeout_ms` 和 `default_timeout_ms`。
请求头字节数按每行 `名称: 值\r\n` 累计；超时单位为毫秒。
`runtime.lua_sdk.tts` 包含 `text_max_bytes`（UTF-8 字节）、`max_pending` 和 `timeout_ms`。

使用 HTTP 需要 `hardware.network == true` 且 `runtime.lua_sdk.http` 存在；
使用 TTS 还需要扬声器及 `runtime.lua_sdk.tts`。接口或硬件能力缺失时不推断可用。
网络瞬时故障通过异步请求结果报告。

能力字段由实际产品配置决定。硬件存在不等于 Lua 可以访问该硬件；Lua 可用接口由
运行时 API 版本及固件配置确定。能力查询本身不表示设备支持小应用安装。

### 安装并启动：`ls.built_in.miniapp_install`

设备注册此工具即表示支持小应用安装。以下参数全部必填：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 小应用标识 |
| `version` | string | 不可变版本标识 |
| `name` | string | 小应用名称；协议保留，固件忽略此字段，不进行名称校验或保存 |
| `url` | string | 裸 Lua 文件的签名下载地址，有效期为 300 秒 |
| `size` | integer | 文件准确字节数，至少为 1，不超过设备公布的源码上限 |
| `hash` | string | 文件 MD5，32 位小写十六进制字符串 |

设备比较当前驻留应用的 `id`、`version`、`hash` 及文件大小。匹配时复用当前版本；
否则下载裸 Lua，验证长度及 MD5，并在 PSRAM 中启动候选版本。
最终结果在校验与启动完成后返回，使用原 MCP call ID 关联请求。

安装失败不替换仍可用的当前应用。调用超时或结果未知时，不自动重放整个安装请求。
不提供额外的安装状态或进度查询工具。

### 退出小应用：`ls.built_in.miniapp_exit`

退出当前小应用并返回桌面。无需参数，`arguments` 可省略或传空对象 `{}`：

```json
{"name":"ls.built_in.miniapp_exit","arguments":{}}
```

设备将退出请求交给小应用运行任务，返回 `miniapp exit requested`，随后释放实例并
返回桌面。退出会结束当前语音交互，下次唤醒恢复正常交互模式。没有小应用运行时
返回成功结果 `no miniapp is running`。`arguments` 为 `null`、非对象或含有任何
字段时均报错。退出不清除应用持久化存档，也不用于退出普通语音对话。

## 通过 ADB 本地调试

`CONFIG_MINIAPP_ADB_DEBUG` 默认启用，需要 CherryUSB ADB，
与完整的 `CONFIG_ADB_SYNC` 文件服务互斥。正常构建即可使用：

```sh
./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini
```

将对应板型的固件烧录到设备，等待启动完成、进入桌面后，每次上传一个 Lua 文件即可直接启动：

```sh
adb push ./miniapp.lua /miniapp/demo.lua
```

- `/miniapp/<id>.lua` 是虚拟入口，不需要文件系统。`id` 为 1–121 个 ASCII
  字母、数字、连字符或下划线；实际应用 ID 为 `local:<id>`。
- 相同路径对应同一个应用存档；每次上传都会重新启动，包括源码未改变时。
  版本号由源码 MD5 生成。本地应用与云端应用共用运行器，成功安装后替换当前实例。
- 上传成功表示脚本已通过校验并启动；语法或启动错误通过 `adb push` 返回，
  仍可用的旧实例会保留。启动成功后的运行错误沿用正常的小应用错误处理。
- 源码上限仍由设备配置决定，默认 64 KiB。源码仅保存在内存中，重启后不自动恢复；
  应用通过存储 API 保存的数据遵循原有的持久化和过期规则。
- 每条命令仅支持一个文件，不支持目录上传、拉取、列目录或其他文件路径。
  传输中断会释放接收缓存；收到完整文件并开始安装后，断开 ADB 不撤销安装。
- 接收缓存按源码大小在 PSRAM 中申请，安装任务使用临时的 4 KiB PSRAM 栈。
  Lua 实例仍受原有堆上限约束，替换时需容纳新旧两个实例；内存不足会返回失败。

不需要本地调试入口时，可通过 `-DCONFIG_MINIAPP_ADB_DEBUG=n` 关闭。

## Lua 基础接口

API 2 提供下述基础接口；API 3 增加系统时间与存档，API 4 增加 HTTP、JSON 与文本播报。
新版本保留旧接口，应用按目标 `runtime.lua_sdk.version` 和能力对象选择接口。

仅接受 UTF-8 Lua 文本，不接受字节码。默认源码上限 65,536 字节，每个实例堆上限
393,216 字节，以能力响应为准。`id`、`version` 最长 127 字节，按 UTF-8 字节计。

| 接口 | 参数与约束 |
| --- | --- |
| `app.api_version` | API 版本整数 |
| `app.width`、`app.height` | 启用屏幕时的画布尺寸，默认 240 × 240 |
| `screen.begin(rgb)` | 清空绘制列表并设置背景，颜色使用 `0xRRGGBB` |
| `screen.rect(x, y, w, h, rgb)` | 每帧最多 128 个矩形，尺寸为正，必须完全位于画布内 |
| `screen.text(text, x, y, rgb)` | 每帧最多 8 段文字，每段最多 63 字节，固定 16 像素字体 |
| `screen.present()` | 提交当前帧；新帧可合并，渲染约每 20 ms 更新一次 |
| `led.on("status")`、`led.off("status")` | 控制状态灯 |
| `led.blink("status", on_ms, off_ms)` | 亮、灭时长各为 10–5,000 ms |
| `buzzer.play(hz, ms)` | 100–5,000 Hz、20–3,000 ms；异步队列最多 4 项 |

未启用的硬件接口不注册相应 Lua 全局表。功能键未启用时不产生脚本按键回调，长按退出仍保留。

`on_tick(dt_ms)` 必须定义，正常调度间隔约 20 ms。可选回调为 `on_start()`、
`on_button_click(button_id)` 和 `on_exit()`，目前按钮 ID 为 `function`。
启用屏幕时，启动阶段（源码执行、`on_start`、第一次 `on_tick`）必须至少提交一帧。

```lua
local clicks = 0
local function draw()
  screen.begin(0x101820)
  screen.text("Clicks: " .. clicks, 16, 16, 0xffffff)
  screen.present()
end
function on_start() draw() end
function on_tick(dt_ms) end
function on_button_click(button_id)
  clicks = clicks + 1
  draw()
end
```

允许基础函数及 `table`、`string`、`math`、`utf8` 库。不开放文件系统、原始套接字、系统命令、动态加载、
协程、调试库、`pcall`、`xpcall`、`setmetatable` 或手动 GC。
Lua 执行受指令预算和检查点耗时限制；它不提供抢占 C 库函数的硬实时隔离。
源码执行默认预算为 500,000 条指令／150 ms，回调为 250,000 条指令／500 ms。回调时间预算允许偶发的数据处理超过 tick 间隔；不代表每帧可以持续占用这么长时间。
支持 FreeRTOS 运行统计的设备结合任务累计运行时间检查耗时，避免射频校准等任务抢占导致误终止。
统计在任务切换时更新，因此耗时检查是近似值；指令预算独立限制脚本自身的计算量。
超过预算、堆耗尽或回调异常时终止对应实例；候选启动失败保留当前实例。

蜂鸣使用系统音量；TTS 或闹钟播放期间跳过蜂鸣，闹钟或新 TTS 到来时中断当前蜂鸣。队列满时丢弃新增蜂鸣，不阻塞脚本，也不导致小应用退出。
闹钟期间暂停脚本 tick 和按键输入，提醒结束后继续原实例，按键优先用于闹钟操作。

## 系统时间与存档

API 3 保留 API 2 接口，增加以下接口，API 4 继续支持：

- `clock.now()`：UTC Unix 时间戳，整数秒。
- `clock.localtime()`：设备本地 `{year, month, day, hour, min, sec, wday, utc_offset}`；星期从 0（周日）开始，偏移单位为秒。
- 未校时均返回 `nil`，时钟类应用应显示等待状态并在后续 tick 重查。不要将系统启动的默认日期或 tick 累加值当作真实时间。
- `storage.load()`：读取当前安装包 ID 的存档表；不存在、过期或不可用时返回 `nil`。
- `storage.save(data[, ttl_seconds])`、`storage.clear()`：成功为 `true`，失败为 `false, reason`。

存档表最多 32 项，键为 1–32 UTF-8 字节字符串，值为有限数字、布尔值或最多 256 UTF-8 字节字符串；字符串不含 NUL，不支持嵌套。序列化总大小以 capabilities 中 `runtime.lua_sdk.storage.max_bytes` 为准。
当前实现每份最多 1,024 字节，最多 4 份存档；默认有效期 7 天、最长 30 天。同一存储槽至少间隔 10 秒写入，退出或清除不重置间隔。满额返回 `storage_full`，不会驱逐其他未过期应用的数据。原有分区表不变，仅使用固定数量的独立 KV 键。

有效期从保存成功算起，读取不续期，过期数据访问时回收。未校时暂不恢复或保存数据，已有存档不因此删除；显式清除仍可执行。常见失败包括 `clock_unavailable`、`rate_limited`、`too_large`、`invalid_data`、`invalid_ttl`、`storage_full`、`io_error`、`no_memory`、`unavailable`。参数种类错误会触发 Lua 异常。

启动候选阶段暂存写入，仅在替换成功后尝试落盘，落盘失败记日志；此阶段 `true` 仅表示接受暂存。正常运行阶段 `true` 表示写入成功。启动失败不会提交暂存修改，退出中的旧实例不能覆盖候选实例的存档。

应用自行兼容旧数据，不强制存档版本号。读取过存档的实例发生 Lua 异常时清除该 ID 的存档，异常退出禁止再保存；不自动重试，下次从空数据启动。底层存储故障可能使清除失败。存档只用于可丢失的进度、设置等信息。

## HTTP、JSON 与文本播报（API 4）

本节说明 API 4 接口契约。可用性和额度取自前述能力响应，不从 API 版本单独推断
HTTP 或 TTS 可用。联网请求异步执行，不应阻塞首帧绘制或 tick；先显示可操作的界面，
再根据结果更新状态。HTTPS 使用固件的 TLS 配置，接口不保证服务器证书校验。

### HTTP 与 JSON

| 接口 | 约定 |
| --- | --- |
| `http.request(options)` | `url` 必填；`method` 默认 GET，允许值见 `http.methods`；可选 `headers`、`body`、`timeout_ms`、`max_response_bytes` |
| `http.get(url[, options])` | GET 简写，options 可指定 headers、timeout_ms、max_response_bytes |
| `http.cancel(request_id)` | 取消当前实例的指定请求，返回是否成功取消 |
| `json.encode(value)` | Lua 值编码为 JSON；失败返回 `nil, reason` |
| `json.decode(text)` | JSON 解码为 Lua 值；失败返回 `nil, reason` |
| `json.null` | 表示 JSON null，避免以 nil 表示时丢失字段或数组元素 |

请求体仅用于 POST；请求头为字符串键值表，不允许 NUL 或换行。
省略超时及响应大小时分别使用能力中的 `default_timeout_ms` 和 `default_response_bytes`，
显式值不得超过 `max_timeout_ms` 和 `max_response_bytes`。不自动重试或跟随重定向。

请求返回数值 ID，结果交给可选的 `on_http_response(request_id, response)`。
未定义此回调时，运行时丢弃响应并释放请求额度，不终止应用。收到完整 HTTP
响应时提供 `{status, content_type, body}`，HTTP 4xx/5xx 也属于完整响应；传输失败时
提供 `{error = {code, message}}`。应用应分别检查传输错误、HTTP 状态和业务数据。

```lua
local pending
function on_button_click(button_id)
  if button_id == "function" and not pending then
    pending = http.get("https://example.com/data", {max_response_bytes = 1024})
  end
end
function on_http_response(id, response)
  if id ~= pending then return end
  pending = nil
  if response.error or response.status ~= 200 then return end
  local data, err = json.decode(response.body)
  if type(data) ~= "table" then return end
  -- 按服务约定检查业务字段后更新界面。
end
```

示例仅展示请求流程，应用还需定义基础生命周期回调；实际 URL 和数据格式由服务提供方确定。
JSON 对象使用字符串键，数组使用从 1 开始的连续整数键；不支持循环引用、稀疏或混合键表、
函数和非有限数值。JSON false 是合法结果，不能用 `if not value` 判断解析失败。

### 文本播报

`tts.speak(text)` 接收非空 UTF-8 纯文本，使用设备当前发音人与系统音量，返回播报 ID。
文本长度及请求额度取自 `runtime.lua_sdk.tts`。结果通过可选的
`on_tts_result(speech_id, result)` 回调交付；需要显示播报状态或处理失败时应实现回调。
未定义此回调时，播报仍正常执行，结果被忽略并释放请求额度，不终止应用。

- `completed`：音频播放完毕。
- `failed`：请求或播放失败，可附带 `error = {code, message}`。
- `interrupted`：被唤醒、闹钟等系统行为中断。

对话占用时异步返回 `failed`，错误原因为 `busy`；不抢占对话，也不自动排队。
`tts.cancel(speech_id)` 只取消当前实例的对应播报，不能停止对话或闹钟音频。

### 异步生命周期

请求 ID 用于匹配结果和取消，不代表请求成功。普通参数错误、额度超限和繁忙通过结果
回调报告；回调在 Lua 任务中串行执行，每个请求最多交付一次最终结果。
成功取消后不再回调，已完成或重复取消返回 false。

候选启动阶段的请求只在安装成功后发出；候选失败不能产生网络或播报副作用。
闹钟冻结期间不执行 Lua 回调，恢复后交付已完成的结果；待交付结果仍占用请求额度。
退出或替换实例会取消其请求，旧结果不会交付给新实例。

## 小应用语音模式

设备在每个 `start` 帧生成时读取当前运行状态。小应用活跃时添加：

```json
{
  "action": "start",
  "params": {
    "nlu_properties": {
      "custom": {
        "mode": "miniapp",
        "miniapp": {"id": "<app-id>", "version": "<app-version>"}
      }
    }
  }
}
```

退出或运行异常结束后，后续 `start` 帧不携带小应用的 `mode` 和 `miniapp` 字段。
断线重连后的会话同样读取当前状态，不复用上一次会话的模式。
安装失败并保留旧应用时，继续携带 `miniapp`。

云端收到此模式后，只处理重新生成、修改、切换和询问玩法；应在产生业务 TTS 或调用工具前
过滤播放音乐、闲聊及其他无关意图。本期不把“掷骰子”“向左转”等语音变成脚本操作事件。
端侧同时拒绝除能力查询、安装和退出小应用以外的 MCP 调用，拦截音乐播放；主动推送 TTS 正常播放。
已设闹钟仍由本地调度触发，不受 MCP 白名单影响。

`mode` 是会话开始时的快照，不会追溯修改已发出的 `start` 帧。
模式切换或替换活动应用时停止本地上行并清理待续聊状态，保留当前回复 TTS；再次唤醒后按新模式开始会话。
长按退出和运行异常退出均结束当前交互，避免回到桌面后继续沿用小应用模式。
实体退出会使正在下载的旧安装失效。若云端在退出之后才新发出旧会话的安装调用，
现有六参数协议无法单独判断其是否过期，云端还需根据会话取消状态停止下发。

## 验证范围

实现需验证参数类型及长度边界、下载中断、错误长度或 MD5、内存不足、Lua 语法及回调错误、
无限循环、界面打开失败、相同版本复用、替换失败保留旧应用，以及退出后的资源释放。
设备验证还需覆盖单击与长按、全屏显示、蜂鸣音量、音乐停止、相关语音交互、无关意图拦截、TTS 保留及闹钟响铃。
