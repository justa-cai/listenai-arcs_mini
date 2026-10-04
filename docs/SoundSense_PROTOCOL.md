# WebSocket 流式声音检测协议 v1.0

端点 `ws://<host>:8000/v1/stream`。检测类：`snoring`（鼾声）、`baby_cry`（婴儿哭声）。

## 设计原则

- **客户端不用转格式**：服务端自适应客户端码流。采样率 8k–192k、1–8 声道、
  `pcm_u8 / pcm_s16le / pcm_s24le / pcm_s32le / pcm_f32le` 任选，hello 里如实声明即可，
  服务端统一转码（混音 + 重采样到模型采样率）。
- **帧即消息**：一条 WebSocket 消息 = 一段音频帧，边界即帧边界；建议 20–250ms/帧。
- **文本帧 = JSON 消息**，二进制帧 = PCM 音频。

## 消息流

```
Client                          Server
  │ ── hello (JSON, 10s 内) ──▶ │
  │ ◀───────── welcome ──────── │
  │ ── binary PCM 帧流 ───────▶ │
  │ ◀── result（每 hop 一条）── │
  │ ◀── event（状态翻转时）──── │
  │ ◀── ping / ── pong ──────▶ │
  │ ── bye ──▶ │ ◀─ bye_ack ── │ ──▶ Close 1000
```

## 消息定义

### hello（C→S，连接后第一条，超时 10s 关闭）

```json
{"type": "hello", "protocol_version": "1.0", "sample_rate": 44100, "channels": 1,
 "format": "pcm_s16le", "client": "myapp-1.2"}
```

| 字段 | 说明 |
|---|---|
| `session_id` | **本连接的唯一不变凭证**：服务端生成（uuid4 32hex、查重），连接生命周期内不变，重连即换新。客户端应保存并以此关联日志、结果与事件；服务端所有日志均携带该凭证 |
| `protocol_version` | 主版本必须与服务端一致（当前 `1.0`），否则 4000 |
| `sample_rate` | 客户端实际采样率，8000–192000，否则 4006 |
| `channels` | 声道数 1–8（多声道服务端自动混音），否则 4006 |
| `format` | `pcm_u8` / `pcm_s16le` / `pcm_s24le` / `pcm_s32le` / `pcm_f32le`，否则 4001 |
| `client` | 可选，客户端自述，仅记日志 |

二进制帧为交织（interleaved）小端 PCM，字节数必须是 `每样本字节数 × channels` 的整数倍，
否则 4005（非致命，该帧丢弃、连接保留）。

### welcome（S→C）

```json
{"type": "welcome", "protocol_version": "1.0", "session_id": "b7c1a2d3e4f50617",
 "seq": 1, "ts": 1760000000000,
 "config": {
   "target_sr": 32000, "window_s": 5.0, "hop_s": 1.0,
   "classes": [{"name": "snoring", "audioset_label": "Snoring"},
               {"name": "baby_cry", "audioset_label": "Baby cry, infant cry"}],
   "accepted_formats": ["pcm_f32le", "pcm_s16le", "pcm_s24le", "pcm_s32le", "pcm_u8"],
   "smoothing": {"ema_rise": 0.5, "ema_fall": 0.25},
   "state_machine": {"enter_confirm_frames": 2, "exit_confirm_frames": 3,
                     "min_event_duration_ms": 2000, "emit_policy": "confirmed",
                     "thresholds": {"snoring": {"enter": 0.45, "exit": 0.25},
                                    "baby_cry": {"enter": 0.30, "exit": 0.15}}},
   "heartbeat_interval_s": 30, "idle_timeout_s": 60}}
```

滞回阈值**按类标定**（ESC-50 上两类负样本均零误报、正样本分布不同：cry 整体偏低故阈值更低），
以 welcome 实际下发的 `thresholds` 为准。

### result（S→C，每 hop 一条；首个 result 在攒满一个 window 后）

```json
{"type": "result", "seq": 7, "ts": 1760000005210, "ts_audio_ms": 5040,
 "window": {"start_ms": 40, "end_ms": 5040},
 "probs": {"snoring": 0.812, "baby_cry": 0.008},
 "smoothed": {"snoring": 0.64, "baby_cry": 0.021},
 "states": {"snoring": "on", "baby_cry": "off"},
 "dropped_hops": 0}
```

| 字段 | 说明 |
|---|---|
| `ts` | 服务端 UTC epoch 毫秒 |
| `ts_audio_ms` | 流内时钟：hello 后首个样本 = 0；`window` 为本次推理窗口的流内区间 |
| `probs` | 模型原始多标签概率（本窗口） |
| `smoothed` | 双速率 EMA 后的平滑概率（上升 α=0.5 快、下降 α=0.25 慢） |
| `states` | 状态机对外状态 `off`/`on` |
| `dropped_hops` | 上一条 result 以来因过载丢弃的 hop 数（客户端推流过快或服务端过载时 >0） |

### event（S→C，状态翻转时）

```json
{"type": "event", "seq": 9, "ts": 1760000007413, "class": "snoring",
 "event": "class_start", "ts_audio_ms": 6040, "duration_ms": 2000, "peak_prob": 0.71}
{"type": "event", "seq": 21, "ts": 1760000018900, "class": "snoring",
 "event": "class_end", "ts_audio_ms": 17040, "duration_ms": 11000, "peak_prob": 0.94}
```

- **延迟确认（`emit_policy: "confirmed"`）**：`class_start` 在事件存续满
  `min_event_duration_ms` 后才发出，`ts_audio_ms` 回溯到真实起点（首个超阈帧）。
  短于该时长的抖动不产生事件（零误报，代价 ≈2s 通知延迟）。
- `class_end.duration_ms` 为事件真实持续；`peak_prob` 为整个事件期间平滑值峰值。
- **静默语义**：客户端停止发音频 = 没有 hop = 状态不推进、不会发 `class_end`，
  由 `idle_timeout_s` 兜底断连。

### ping / pong（双向）

```json
{"type": "ping", "ts": 1760000000000}
{"type": "pong", "ts": 1760000000000, "seq": 30}
```

服务端每 `heartbeat_interval_s` 发 ping；连续 `idle_timeout_s` 未收到客户端任何帧
（audio/pong/任意 JSON）→ error 4008 + Close 4408。

### error（S→C；`fatal: true` 则随后关闭连接）

```json
{"type": "error", "code": 4005, "message": "audio frame not aligned: ...", "fatal": false, "ref_seq": null}
```

### bye / bye_ack

```json
{"type": "bye"}                    // C→S 主动结束
{"type": "bye_ack", "seq": 31}     // S→C，随后 Close 1000
{"type": "bye", "reason": "server_shutdown"}  // S→C 停机广播，随后 Close 1001
```

## 序号规则

服务端每连接维护 `seq`，从 1 单调递增；`welcome / result / event / pong / bye_ack`
占用序号，`error` 不占用。客户端消息无需序号。

## 错误码与关闭码

| code | 含义 | fatal |
|---|---|---|
| 4000 | 协议版本不兼容 | 是 |
| 4001 | 不支持的 format | 是 |
| 4002 | hello 之前发送音频 | 是 |
| 4003 | 未知消息类型（含重复 hello） | 否，忽略 |
| 4004 | JSON 解析失败 | 否，忽略 |
| 4005 | 音频帧字节不对齐 | 否，丢该帧 |
| 4006 | sample_rate / channels 超界 | 是 |
| 4007 | 会话数超限（过载） | 是 |
| 4008 | 空闲超时（含 hello 超时） | 是 |
| 5000 | 服务端内部错误 | 是 |

WS Close code：`1000` 正常 bye；`1001` 服务端停机；`4400` 协议违规；
`4408` 空闲超时；`1011` 内部错误。

## 时间线示例（window=5s, hop=1s）

```
t=0.0s  连接，hello，welcome
t=0~5s  推音频（攒窗）
t=5.0s  result#1 (ts_audio=5000, window 0~5000ms)
t=6.0s  result#2 …
t=6~7s  若平滑值连续 2 帧 ≥0.5 且已持续 ≥2s → class_start（ts_audio 回溯到 ~5s）
…       持续 on
t=Xs    平滑值连续 3 帧 <0.3 → class_end（含 duration/peak）
```
