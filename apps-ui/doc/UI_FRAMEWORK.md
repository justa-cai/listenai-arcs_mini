# UI 框架文档（Views / Presenters / Models）

## 1. 目标与原则

本 UI 框架采用分层设计（MVP 风格），核心目标是：

- **View（视图层）**：只负责页面布局与控件渲染
- **Model（模型层）**：承载业务逻辑与数据状态
- **Presenter（主持层）**：负责 View 与 Model 的交互编排，并处理页面跳转

### 1.1 强制约束

- **views 中只存放页面布局相关内容，不存放业务逻辑**
- **业务逻辑存放于 models 中**
- **业务逻辑与 UI 交互通过 presenters 交换（包括页面跳转）**
- **日志采用英文日志，不要使用中文**
- **注释使用中文**

> 注意：本文档描述的是工程约定与推荐实践，具体接口命名以工程实际代码为准。

---

## 2. 目录结构与职责划分（建议）

以 `apps-ui/apps/llm/` 为例：

- `apps-ui/apps/llm/views/`
  - 页面布局、控件创建、样式设置
  - 对外提供“渲染接口”（例如 `set_title()`、`set_items()`）
  - 不直接调用业务模块、不访问硬件/云端、不做页面跳转决策

- `apps-ui/apps/llm/presenters/`
  - 处理 UI 事件（点击/手势/列表选择）
  - 调用 Model 执行业务
  - 将业务结果转成 View 可渲染的数据结构
  - 负责页面跳转、弹窗策略、提示音策略等

- `apps-ui/apps/llm/models/`
  - 业务逻辑与状态管理
  - 数据获取/缓存、异步任务、协议解析、设备能力调用
  - 对外提供与 UI 无关的能力接口（例如 `model_wifi_scan()`）

- `apps-ui/assets/`
  - 图片、字体、音频等资源

---

## 3. 推荐数据流

### 3.1 用户操作（UI → Presenter → Model → Presenter → View）

1. 用户点击/手势触发 View 回调
2. View 回调 **只上报事件** 给 Presenter（不做业务判断）
3. Presenter 调用 Model 执行业务
4. Model 返回结果（同步返回或异步回调/事件）
5. Presenter 调用 View 渲染接口刷新 UI

### 3.2 系统事件/异步结果（Model → Presenter → View）

1. 网络/云端/设备事件由 Model 接收
2. Model 更新内部状态
3. Model 通知 Presenter（回调/事件队列/消息机制）
4. Presenter 决定：刷新 UI / 跳转页面 / 提示错误

---

## 4. 页面生命周期（建议约定）

建议每个页面具备以下生命周期函数（名称按工程实际对齐）：

- `create()`
  - 创建控件树、初始化 View 私有数据
  - 绑定回调（回调中只上报 Presenter）

- `show()`
  - 页面显示时调用
  - Presenter 通常在此触发数据刷新（例如拉取列表、请求状态）

- `hide()`
  - 页面隐藏时调用
  - 停止动画/定时器（由 View 创建的必须由 View 清理）

- `destroy()`
  - 页面销毁时调用
  - 释放资源、注销回调、删除定时器

### 4.1 关键注意事项

- **LVGL 对象/定时器的创建与释放必须严格成对**
- View 不应把 LVGL 对象指针泄露给 Model
- Presenter/Model 的异步回调回到 UI 层前，需要确认页面仍然有效

---

## 5. View 层接口规范（建议）

View 对外暴露的函数建议为“纯渲染接口”，例如：

- `home_view_set_status_text(const char *text)`
- `wifi_view_set_list(const wifi_ap_item_t *items, size_t count)`
- `setting_view_set_loading(bool enable)`

约束：

- Presenter 不直接调用 `lv_label_set_text()` 去改布局细节，而是调用 View API
- View 不做业务判断（例如不判断 WiFi 是否已连接、不解析云端 JSON）

---

## 6. Model 层接口规范（建议）

Model 对外提供业务能力接口，不依赖 LVGL，不关心页面结构：

- `model_wifi_scan()`
- `model_wifi_connect(const char *ssid, const char *pwd)`
- `model_camera_start()`
- `model_camera_stop()`

Model 内部建议具备：

- 状态缓存（避免重复查询）
- 统一错误码/错误原因输出
- 异步结果通知机制（回调/事件）

---

## 7. Presenter 层职责（建议）

Presenter 主要负责：

- 接收 View 事件并转换成业务动作
- 调用 Model 并处理返回结果
- 控制 View 的 loading/empty/error 展示策略
- 页面跳转与导航（进入/返回/弹窗等）

禁止：

- 在 Presenter 中搭建复杂控件树（属于 View）
- 在 Presenter 中写硬件/协议操作（属于 Model）

---

## 8. 页面跳转（推荐做法）

- 页面跳转由 Presenter 统一发起
- 跳转参数使用结构体/枚举等轻量数据
- 避免跨页面传递 LVGL 对象指针

---

## 9. 线程与 UI 安全（必须明确）

- **LVGL 只能在 UI 线程操作**
- Model 若在后台线程工作，回调 Presenter 时需要通过队列/调度切回 UI 线程
- Presenter 调用 View API 前需保证当前处于 UI 线程

---

## 10. 日志与注释规范

- 日志：英文（例如 `LOGI("WiFi scan started")`）
- 注释：中文
- 错误提示文案的策略在 Presenter

---

## 11. 快速检查清单（Review Checklist）

- **View**
  - 是否只包含布局与渲染接口
  - 是否存在业务判断/网络调用/页面跳转（如有则需迁移）

- **Presenter**
  - 是否作为 UI 事件唯一入口
  - 是否负责跳转决策与交互策略

- **Model**
  - 是否承载业务状态与能力调用
  - 是否避免引用 LVGL 或 View 内部结构

