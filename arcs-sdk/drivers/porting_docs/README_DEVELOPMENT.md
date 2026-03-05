# LISA 驱动开发文档

本目录包含 LISA 驱动框架及相关驱动模块的开发文档。

## 📚 文档导航

### 🚀 新驱动开发

| 文档 | 用途 | 适用场景 |
|-----|------|---------|
| [**DRIVER_DEVELOPMENT_GUIDE.md**](./DRIVER_DEVELOPMENT_GUIDE.md) | 完整开发指南 | 人工开发者、学习框架 |
| [**DRIVER_TEMPLATE_QUICKREF.md**](./DRIVER_TEMPLATE_QUICKREF.md) | 快速参考模板 | AI生成代码、快速查阅 |

### 📖 框架说明

| 目录/文件 | 说明 |
|----------|------|
| [lisa_device/](./lisa_device/) | LISA 设备框架核心 |
| [lisa_gpio/](./lisa_gpio/) | GPIO 驱动参考实现 |

---

## 🎯 快速开始

### 对于人工开发者

1. 阅读 [DRIVER_DEVELOPMENT_GUIDE.md](./DRIVER_DEVELOPMENT_GUIDE.md) 了解完整流程
2. 参考 [lisa_gpio](./lisa_gpio/) 查看实际实现
3. 按照步骤创建新驱动

### 对于AI辅助开发

使用以下提示词让AI生成驱动代码：

```
请基于 LISA 驱动框架为 [设备名称] 创建驱动

设备信息：
- 设备名: lisa_[xxx]
- HAL接口: Driver_[XXX].h
- 功能需求: [列举功能]

参考文档:
1. /home/conor/work/01-arcs/arcs-sdk/drivers/DRIVER_TEMPLATE_QUICKREF.md
2. /home/conor/work/01-arcs/arcs-sdk/drivers/lisa_gpio/

请生成:
1. lisa_xxx.h (API定义)
2. lisa_xxx_arcs.c (ARCS平台实现)
3. CMakeLists.txt
4. Kconfig
```

---

## 📋 驱动开发核心要点

### 必须遵守的规则

1. **设备命名**: 小写字母+下划线，格式 `lisa_[设备类型]`
2. **返回值**: 成功返回 `LISA_DEVICE_OK (0)`，失败返回负数错误码
3. **API参数**: 第一个参数必须是 `lisa_device_t *dev`
4. **状态检查**: 实现函数中使用 `lisa_device_is_initialized(dev)`
5. **注册方式**: 使用 `LISA_DEVICE_REGISTER` 宏

### 文件结构

```
drivers/lisa_xxx/
├── lisa_xxx.h          # 设备API定义（公共头文件）
├── lisa_xxx_arcs.c     # ARCS平台实现
├── CMakeLists.txt      # 构建配置
├── Kconfig             # 配置选项
└── README.md           # 驱动说明（可选）
```

### 代码模板位置

- **完整模板**: [DRIVER_DEVELOPMENT_GUIDE.md](./DRIVER_DEVELOPMENT_GUIDE.md) 第三节
- **精简模板**: [DRIVER_TEMPLATE_QUICKREF.md](./DRIVER_TEMPLATE_QUICKREF.md)

---

## 🔗 相关资源

- **设备框架文档**: [lisa_device/README.md](./lisa_device/README.md)
- **GPIO驱动示例**: [lisa_gpio/](./lisa_gpio/)
- **错误码定义**: `lisa_device.h` 第24-36行
- **优先级定义**: `lisa_device.h` 第47-51行

---

## ❓ 常见问题

**Q: 如何选择设备优先级？**
- 系统核心设备（时钟/电源）: `LISA_DEVICE_PRIORITY_CRITICAL (0)`
- 重要外设（UART/GPIO）: `LISA_DEVICE_PRIORITY_HIGH (10)` 或 `NORMAL (50)`
- 可选功能（传感器/LED）: `LISA_DEVICE_PRIORITY_LOW (90)`

**Q: HAL错误如何转换？**
- HAL返回非0 → `LISA_DEVICE_ERR_IO`
- 参数无效 → `LISA_DEVICE_ERR_INVALID`
- 超出范围 → `LISA_DEVICE_ERR_RANGE`

**Q: 是否需要线程安全？**
- 如果设备可能被多线程访问，在私有数据中添加 `lisa_mutex_t *mutex`
- 在操作前后使用 `DEVICE_LOCK/UNLOCK` 宏

---

**维护者**: LISTENAI  
**更新时间**: 2025-01-27
