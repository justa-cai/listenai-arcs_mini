# 二次开发文档（DEVELOPMENT.md）

面向在本仓库上做扩展开发的工程师。改动前先读完本文的**硬性约束**与扩展点契约。

## 架构总览

```
ws.py(协议层) ─▶ pipeline.py(每连接一个 SessionPipeline) ─▶ backend/(ModelBackend 抽象)
                    ├─ audio/   环形缓冲 + 多格式解码 + soxr 重采样（纯函数）
                    ├─ state/   双速率 EMA + 滞回状态机（纯函数）
                    └─ ThreadPoolExecutor → 共享模型单例（GPU）
app.py: FastAPI 工厂 + lifespan（executor/backend/会话表）+ /healthz
protocol/: pydantic 消息模型 + 错误码
```

分层纪律：`server/` 不懂音频，`pipeline.py` 不懂协议细节（回调注入），`backend/` 不懂 IO，
`audio/`、`state/` 无 IO 可独立单测。新代码保持该边界。

## 硬性约束（违反即缺陷，评审必拦）

1. **每个 WebSocket 连接必须有唯一且不变的凭证（`session_id`）**
   - 由服务端在 `accept()` 后生成：uuid4 全量 32 hex，并对在线会话表查重
     （`ws.py::_new_session_id`）；
   - **连接生命周期内不得重新赋值或更换**——重连 = 新凭证，旧凭证随之作废；
   - 凭证必须贯穿全链路：`welcome.session_id` 下发给客户端、`Connection.log`
     前缀、事件/结果关联、任何新增的监控/指标/消息扩展都必须携带它；
   - 会话表（`app.state.sessions`）以凭证为键，连接结束必须摘除（`finally` 兜底）。
2. **发送互斥**：对同一连接的 ws 发送只允许经 `Connection._send_lock` 串行
   （主循环、pipeline 消费 task、监控 task 三方并发，裸 `send` 会撕裂帧序）。
3. **seq 单调**：`welcome/result/event/pong/bye_ack` 占用序号且严格递增；`error` 不占。
4. **错误码只增不改**：新增错误用新码段（与 `protocol/errors.py` 注释一致），
   既有码的含义与 fatal 语义冻结（PROTOCOL.md 是对外契约）。
5. **推理不可在事件循环线程执行**：必须经 `run_in_executor`；共享 backend 单例
   只在 `eval()+inference_mode()` 下调用。
6. **协议变更必须升版本**：不兼容改动升 `PROTOCOL_VERSION` 主版本并在
   `_receive_hello` 校验；兼容性新增（新可选字段/新消息类型）升次版本。
7. 提交信息不加 AI 页脚、作者邮箱用项目 git config（见 CLAUDE.md）。

## 扩展点契约

### 换模型 / 加模型（OPERA、ICSD baseline、自训）

实现 `backend/base.py::ModelBackend` 三成员：

```python
def infer(self, pcm: np.ndarray, sr: int) -> dict[str, float]  # mono float32 → {类名: 概率}
def warmup(self) -> None
@property
def device(self) -> str
```

- `infer` 会被多线程并发调用（`infer_workers` 上限），必须线程安全；
- 返回的键必须与 `config.py::CLASS_MAP` 的协议类名一致；
- 在 `server/app.py::create_app(backend_factory=...)` 注入（测试同理，
  参考 `tests/conftest.py::FakeBackend`）。

### 加检测类

1. `config.py::CLASS_MAP` 增加协议类名 → AudioSet 标签名；
2. 若新底座索引不同于 `EXPECTED_LABEL_INDEX` 断言表，同步更新断言（防 CSV 错位）；
3. 概率分布与现类不同时，在 `class_thresholds` 配按类阈值（参考 baby_cry）；
4. 用 `scripts/eval_offline.py` 在正/负样本上标定阈值后再定默认值。

### 加消息类型 / 字段

- 服务端新增下发消息：在 `protocol/messages.py` 建 pydantic 模型并占用 seq；
- 客户端新增上行消息：在 `_handle_text` 分派，未知类型必须维持 4003 非致命语义；
- 同步更新 PROTOCOL.md 与 `tests/test_protocol_ws.py`。

## 测试要求

- 新功能必须带 `FakeBackend` 协议测试（无 GPU/权重依赖，确定性概率）；
- 真权重回归（`test_backend_real.py`）只做模型行为断言，不做协议断言；
- 提交前：`make lint && make test` 全绿。

## 已知坑位（历史教训，勿重复踩）

- `panns_inference` 的 `Cnn14.forward` 收裸张量，不是原 repo 的 `{"waveform":…}`；
- `panns_inference.config` import 时会 shell wget CSV——资产必须先由
  `scripts/prepare_assets.py` 预放置，服务端禁止自动下载；
- uvicorn 默认 `ws_max_size` 1MB，大音频帧会被静默断连——`main.py` 显式 4MB；
- Zenodo 权重下载经代理易断流，脚本已带 Range 续传。
