# UI框架迁移指南

## 新UI框架架构总结

### 核心特点
1. **完全业务分离的MVP架构**
   - **View层**：纯UI渲染，无业务逻辑（`info_view.c`, `lisa_ui_llm_primary.c`）
   - **Presenter层**：业务逻辑和生命周期管理（`info_presenter.c`, `home_presenter.c`）
   - **Model层**：数据接口（预留，暂未实现）

2. **导航系统**
   - 基于`lisa_ui_nav_scr`结构的页面管理
   - 标准生命周期：`open → show → pause → resume → close`
   - 通过ID进行页面导航

3. **组件继承体系**
   ```
   lv_obj (LVGL基础)
     ├─ lisa_ui_llm_base (基础组件：任务栏+容器)
     │    ├─ lisa_ui_llm_primary (主页面)
     │    └─ lisa_ui_info_view (信息/二维码页面)
     └─ lisa_ui_anim / lisa_ui_anim_ext (动画组件)
   ```

## 老UI分析

### 已有页面（`/apps/arcs-mini/groups/llm/launcher/pages/`）
1. **page_primary.c** (主页面)
   - emoji分阶段动画系统
   - WiFi/电量状态栏
   - 内容文本轮播
   - 犯困模式定时器

2. **page_info.c** (信息页面)
   - 二维码显示（BLE/网络）
   - 顶部/底部文本提示
   - 自动返回定时器

## 已完成迁移

### ✅ 完整设置系统（Settings System）

#### 1. 设置主页面（Setting Main Page）
**文件**: `apps-ui/apps/llm/views/setting_view.c/h`

**功能**:
- 列表形式的设置菜单
- 支持动态添加设置项
- 点击回调导航到子页面

**API接口**:
```c
lv_obj_t *lisa_ui_setting_view_create(lv_obj_t *parent);
void lisa_ui_setting_view_item_add(lv_obj_t *obj, const char *label, const char *icon_symbol);
void lisa_ui_setting_view_set_click_cb(lv_obj_t *obj, lisa_ui_setting_item_click_cb_t cb, void *user_data);
void lisa_ui_setting_view_clear(lv_obj_t *obj);
```

#### 2. WiFi设置页面（WiFi Settings）
**文件**: `apps-ui/apps/llm/views/wifi_view.c/h`

**功能**:
- WiFi开关控制
- 我的WiFi列表（已连接过）
- 其他WiFi列表
- 网络信号强度显示

#### 3. 音量设置页面（Volume Settings）
**文件**: `apps-ui/apps/llm/views/volume_view.c/h`

**功能**:
- 音量滑块控制（0-100%）
- 麦克风增益滑块控制
- 实时数值显示

#### 4. 关于页面（About Page）
**文件**: `apps-ui/apps/llm/views/about_view.c/h`

**功能**:
- 系统信息展示
- 版本号、编译日期
- 版权信息

#### 5. Presenter层
**文件**: `apps-ui/apps/llm/presenters/setting_presenter.c`

**功能**:
- 统一管理所有设置页面的导航
- setting_nav_scr - 主设置页面
- wifi_nav_scr - WiFi设置
- volume_nav_scr - 音量设置
- about_nav_scr - 关于页面

#### 6. 导航注册
- 在`lisa_ui_app.c`中注册所有设置页面
- 导航ID定义：
  - `LISA_UI_NAV_SCR_ID_SETTING`
  - `LISA_UI_NAV_SCR_ID_SETTING_WIFI`
  - `LISA_UI_NAV_SCR_ID_SETTING_VOLUME`
  - `LISA_UI_NAV_SCR_ID_SETTING_ABOUT`

#### 7. 资源迁移
已拷贝所有图标和emoji资源：
- **Emoji动画**: `/assets/emoji/` - angry, blink, heart, sad, sleepy, wakeup等
- **图标资源**: `/assets/lisa_ui_assets.c/h` - WiFi、电池、电量图标

### ✅ 信息/二维码页面（Info Page）

#### 1. View组件
**文件**: `apps-ui/apps/llm/views/info_view.c/h`

**功能**:
- 继承自`lisa_ui_llm_base`
- 包含顶部提示文本、QR图片、底部说明文本
- 垂直flex布局，居中对齐

**API接口**:
```c
lv_obj_t *lisa_ui_info_view_create(lv_obj_t *parent);
void lisa_ui_info_view_set_top_text(lv_obj_t *obj, const char *text);
void lisa_ui_info_view_set_bottom_text(lv_obj_t *obj, const char *text);
void lisa_ui_info_view_set_qr_image(lv_obj_t *obj, const void *src);
lv_obj_t *lisa_ui_info_view_get_qr_image(lv_obj_t *obj);
```

#### 2. Presenter
**文件**: `apps-ui/apps/llm/presenters/info_presenter.c`

**功能**:
- 管理info页面生命周期
- 10秒自动返回主页面定时器
- 预留数据接口对接点

#### 3. 导航注册
- 在`lisa_ui_app.c`中注册`info_nav_scr`
- ID: `LISA_UI_NAV_SCR_ID_WX_QRCODE`
- 支持从主页面设置按钮跳转

#### 4. 编译配置
- 已更新`views/CMakeLists.txt`
- 已更新`presenters/CMakeLists.txt`

## 页面导航流程

```
主页面 (Home)
  ├─ 点击设置图标 → 设置主页面 (Setting)
  │                    ├─ WiFi → WiFi设置页面
  │                    ├─ Volume → 音量设置页面
  │                    └─ About → 关于页面
  │
  └─ （预留）其他导航入口 → 信息/二维码页面 (Info)
```

## 编译配置完成

### CMakeLists.txt更新
1. **views/CMakeLists.txt** - 添加所有view文件
   ```cmake
   info_view.c
   setting_view.c
   wifi_view.c
   volume_view.c
   about_view.c
   ```

2. **presenters/CMakeLists.txt** - 添加presenter
   ```cmake
   setting_presenter.c
   ```

3. **lisa_ui_app.c** - 注册所有导航
   ```c
   extern const struct lisa_ui_nav_scr setting_nav_scr;
   extern const struct lisa_ui_nav_scr wifi_nav_scr;
   extern const struct lisa_ui_nav_scr volume_nav_scr;
   extern const struct lisa_ui_nav_scr about_nav_scr;
   ```

## 待迁移内容

### 🔲 主页面增强（Home Page Enhancement）

#### 需要实现的功能
1. **Emoji动画系统**
   - 分阶段动画：enter → loop → exit
   - 动画配置数组（每种emoji的帧数、时长）
   - 状态管理（当前emoji、阶段、循环计数）
   - 适配`lisa_ui_anim_ext`组件

2. **状态栏动态更新**
   - WiFi连接状态
   - 电量显示（老版有，新版已移除）
   - 状态文本（"我在听..."、"思考中..."等）

3. **内容文本轮播**
   - 云端待机提示词轮换
   - 定时器管理
   - 文本队列

4. **特殊模式**
   - 犯困模式（30秒后切换到sleepy emoji）
   - 充电状态检测

### 🔲 设置页面（Setting Page - 可选）

如需要设置页面，需创建：
- `setting_view.c/h`
- `setting_presenter.c`
- 添加`LISA_UI_NAV_SCR_ID_SETTING`
- 注册到`lisa_ui_app.c`

### 🔲 动画资源迁移

需要将老UI的emoji图片资源迁移到新框架：
```
老路径: apps/arcs-mini/groups/llm/launcher/pages/private/anim_images.h
新路径: apps-ui/assets/ (需创建)
```

包含的emoji类型：
- BLINK (眨眼)
- LOVE (爱心)
- SAD (难过)
- ANGRY (生气)
- HAPPY (开心)
- CUTE (可爱)
- SLEEPY (犯困)
- WAIT (等待)
- BATTERY_CHANGE (充电动画)

## 编译和测试

### Linux模拟器
```bash
cd apps-ui/sim
cmake -B build && cmake --build build && ./build/lisa_ui_app_sim
```

### 操作演示
1. 启动后显示主页面（home）
2. 点击左上角设置图标 → 跳转到Info页面
3. Info页面显示10秒后自动返回主页面

## 数据接口对接（待实现）

### Model层设计建议
```c
// apps-ui/apps/llm/models/ui_model.h
typedef struct {
    bool wifi_connected;
    uint8_t battery_level;
    char status_text[32];
    lisa_ui_emoji_type_e current_emoji;
    char content_text[256];
    const void *qr_image_src;
} ui_model_data_t;

// 数据更新回调
typedef void (*ui_model_update_cb_t)(const ui_model_data_t *data, void *user_data);
```

### Presenter调用Model
```c
// 在presenter中监听数据变化
ui_model_register_callback(on_data_update, presenter_data);

// 回调中更新view
void on_data_update(const ui_model_data_t *data, void *user_data) {
    // 更新view显示
    lisa_ui_llm_primary_set_status_text(view, data->status_text);
    lisa_ui_llm_primary_set_emoji(view, data->current_emoji);
}
```

## 下一步工作优先级

1. **高优先级**：实现emoji动画系统（适配`lisa_ui_anim_ext`）
2. **中优先级**：迁移emoji图片资源
3. **中优先级**：实现主页面状态栏动态更新
4. **低优先级**：内容文本轮播功能
5. **低优先级**：犯困模式和特殊定时器

## 注意事项

### IDE Lint错误
当前IDE会报`lv_conf.h`找不到的错误，这是正常的：
- `lv_conf.h`在`portable/`目录下
- CMake编译时会正确处理包含路径
- 不影响实际编译和运行

### 代码风格
- 使用英文注释和日志
- View层不包含业务逻辑
- Presenter负责生命周期和业务
- 保持与现有代码风格一致

## 迁移完成标准

- [x] 所有页面View组件实现
  - [x] Info/QRCode页面
  - [x] Setting主页面
  - [x] WiFi设置页面
  - [x] Volume设置页面
  - [x] About页面
- [x] 所有Presenter实现并注册
  - [x] info_presenter
  - [x] setting_presenter（含4个子页面）
- [x] 资源完整拷贝
  - [x] Emoji动画资源（angry, blink, heart等）
  - [x] 图标资源（WiFi, 电池等）
- [x] 编译配置完成
  - [x] CMakeLists.txt更新
  - [x] 导航系统注册
- [ ] Emoji动画系统完整迁移（待实现）
- [ ] 模拟器测试通过
- [ ] 数据接口对接完成（业务层对接后）
- [ ] 页面导航流畅无bug

## 参考文档

- 新框架README: `apps-ui/README.MD`
- 老UI实现: `apps/arcs-mini/groups/llm/launcher/pages/`
- LVGL文档: https://docs.lvgl.io/
