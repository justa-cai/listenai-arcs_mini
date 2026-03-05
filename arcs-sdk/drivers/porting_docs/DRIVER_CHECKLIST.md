# LISA 驱动开发检查清单

> 用于验证新驱动是否符合框架规范

## ✅ 代码实现检查

### 头文件 (`lisa_xxx.h`)

- [ ] 包含 `#include "lisa_device.h"`
- [ ] 定义设备配置结构体 `lisa_xxx_config_t`
- [ ] 定义设备API结构体 `lisa_xxx_api_t`
- [ ] API结构体中所有函数第一参数为 `lisa_device_t *dev`
- [ ] 提供 `inline` 包装函数
- [ ] 包装函数检查空指针 (`!dev || !dev->api`)
- [ ] 包装函数返回 `LISA_DEVICE_ERR_NOT_SUPPORT` 当API未实现

### 实现文件 (`lisa_xxx_arcs.c`)

- [ ] 包含设备头文件和HAL头文件
- [ ] 定义 `LOG_TAG` 宏
- [ ] 包含 `<lisa_log.h>`
- [ ] 定义私有数据结构 `lisa_xxx_priv_t`
  - [ ] 包含 `void *hal_handler` 字段
  - [ ] 如需线程安全，包含 `lisa_mutex_t *mutex`
- [ ] 创建静态设备实例 `static lisa_xxx_priv_t xxx0_priv;`
- [ ] 实现API函数
  - [ ] 使用 `lisa_device_is_initialized(dev)` 检查设备状态
  - [ ] 参数验证返回 `LISA_DEVICE_ERR_INVALID`
  - [ ] HAL错误转换为统一错误码
- [ ] 定义API实例 `static const lisa_xxx_api_t arcs_xxx_api`
- [ ] 实现初始化函数 `arcs_xxx0_init()`
  - [ ] 获取HAL句柄
  - [ ] 检查HAL句柄有效性
  - [ ] 创建互斥锁（如需要）
  - [ ] 调用HAL初始化
  - [ ] 成功返回 `LISA_DEVICE_OK`
  - [ ] 失败返回 `LISA_DEVICE_ERR_INIT_FAIL`
- [ ] 使用 `LISA_DEVICE_REGISTER` 宏注册设备
  - [ ] 设备名称小写且唯一
  - [ ] API指针指向正确的API实例
  - [ ] 私有数据指针正确
  - [ ] 初始化函数指针正确
  - [ ] 优先级合理（使用预定义宏）

### 构建文件

#### `CMakeLists.txt`
- [ ] 使用 `if(CONFIG_LISA_XXX)` 条件编译
- [ ] 调用 `listenai_library_named(lisa_xxx)`
- [ ] 调用 `listenai_include_directories(.)`
- [ ] 添加源文件 `listenai_library_sources(lisa_xxx_arcs.c)`
- [ ] 结束条件 `endif()`

#### `Kconfig`
- [ ] 使用 `menuconfig LISA_XXX` 定义主选项
- [ ] 设置 `default n`
- [ ] 添加 `select LISA_DEVICE` 依赖
- [ ] 提供 `help` 说明
- [ ] 使用 `if LISA_XXX ... endif` 包裹子选项

### 集成到构建系统

- [ ] 在 `drivers/CMakeLists.txt` 中添加 `add_subdirectory(lisa_xxx)`
- [ ] 在 `drivers/Kconfig` 中添加 `rsource "lisa_xxx/Kconfig"`

---

## ✅ 编码规范检查

### 命名规范
- [ ] 设备名: `lisa_[设备类型]` (小写)
- [ ] 配置结构: `lisa_xxx_config_t`
- [ ] API结构: `lisa_xxx_api_t`
- [ ] 私有数据: `lisa_xxx_priv_t`
- [ ] 静态实例: `xxx0_priv`, `xxx1_priv`
- [ ] 初始化函数: `arcs_xxx0_init()`
- [ ] API实现函数: `arcs_xxx_operation()`

### 返回值规范
- [ ] 成功: `LISA_DEVICE_OK (0)`
- [ ] 参数错误: `LISA_DEVICE_ERR_INVALID`
- [ ] 设备未就绪: `LISA_DEVICE_ERR_NOT_READY`
- [ ] IO错误: `LISA_DEVICE_ERR_IO`
- [ ] 超范围: `LISA_DEVICE_ERR_RANGE`
- [ ] 初始化失败: `LISA_DEVICE_ERR_INIT_FAIL`

### 日志规范
- [ ] 定义 `LOG_TAG` 为 `"lisa_xxx"`
- [ ] 初始化成功使用 `LISA_LOGI`
- [ ] 初始化失败使用 `LISA_LOGE`
- [ ] 调试信息使用 `LISA_LOGD`

---

## ✅ 功能测试检查

### 基本功能
- [ ] 设备能够成功初始化
- [ ] 设备能够被 `lisa_device_get()` 获取
- [ ] `lisa_device_ready()` 返回正确状态
- [ ] API函数能正确执行
- [ ] 错误处理正确（参数检查、HAL错误）

### 多实例测试（如适用）
- [ ] 多个设备实例能够独立工作
- [ ] 设备名称不冲突

### 线程安全测试（如适用）
- [ ] 多线程访问不会导致数据竞争
- [ ] 互斥锁正确使用

---

## ✅ 文档检查

- [ ] 创建 `README.md` 说明驱动用途和使用方法
- [ ] 在头文件中添加函数注释（Doxygen格式）
- [ ] 说明配置选项的含义

---

## 📊 检查结果

- **通过项**: _____ / _____
- **检查时间**: __________
- **检查人**: __________

---

## 🔗 参考文档

- [DRIVER_DEVELOPMENT_GUIDE.md](./DRIVER_DEVELOPMENT_GUIDE.md) - 完整开发指南
- [DRIVER_TEMPLATE_QUICKREF.md](./DRIVER_TEMPLATE_QUICKREF.md) - 快速参考
- [lisa_gpio/](./lisa_gpio/) - 参考实现

---

**版本**: v1.0  
**更新**: 2025-01-27
