# LISA 驱动开发文档中心

欢迎使用 LISA 驱动框架！本文档中心提供完整的驱动开发指南和工具。

---

## 🎯 我想做什么？

### 👨‍💻 我是开发者，想开发新驱动

➡️ 查看 **[完整开发指南](./DRIVER_DEVELOPMENT_GUIDE.md)**

包含：
- 详细开发步骤
- 完整代码模板
- 使用示例
- 最佳实践

### 🤖 我想用AI生成驱动代码

➡️ 查看 **[快速参考模板](./DRIVER_TEMPLATE_QUICKREF.md)**

特点：
- 超精简格式
- AI友好
- 即用即查
- 包含完整提示词

### ✅ 我想验证驱动是否规范

➡️ 使用 **[开发检查清单](./DRIVER_CHECKLIST.md)**

包含：
- 代码规范检查
- 构建系统集成
- 功能测试项
- 文档完整性

### 📚 我想了解框架详情

➡️ 阅读 **[设备框架文档](./lisa_device/README.md)**

内容：
- 框架设计理念
- 核心API说明
- 设备注册机制
- 调试支持

### 🔍 我想看实际驱动实现

➡️ 参考 **[GPIO驱动](./lisa_gpio/)** 或 **[PWM驱动示例](./lisa_pwm/)**

示例内容：
- GPIO：完整驱动代码、HAL层对接、中断处理、多实例支持
- PWM：构建脚本与公共头文件示例，演示最小可行驱动框架

---

## 📁 文档结构

```
drivers/
├── README.md                          # 本文件 - 文档中心导航
├── README_DEVELOPMENT.md              # 开发文档总览
├── DRIVER_DEVELOPMENT_GUIDE.md        # 完整开发指南 ⭐
├── DRIVER_TEMPLATE_QUICKREF.md        # 快速参考模板 ⭐
├── DRIVER_CHECKLIST.md                # 开发检查清单 ⭐
│
├── lisa_device/                       # 设备框架核心
│   ├── README.md                      # 框架详细说明
│   ├── lisa_device.h                  # 核心头文件
│   ├── lisa_device.c                  # 框架实现
│   ├── CMakeLists.txt
│   └── Kconfig
│
├── lisa_gpio/                         # GPIO驱动参考实现
│   ├── lisa_gpio.h                    # GPIO API定义
│   ├── lisa_gpio_arcs.c               # ARCS平台实现
│   ├── CMakeLists.txt
│   └── Kconfig
└── lisa_pwm/                          # PWM驱动文档示例
    ├── lisa_pwm.h                     # 公共API示例
    └── CMakeLists.txt                 # 构建脚本示例
```

---

## 🚀 快速开始

### 30秒了解如何开发新驱动

1. **创建目录**: `drivers/lisa_xxx/`
2. **创建文件**:
   - `lisa_xxx.h` - API定义
   - `lisa_xxx_arcs.c` - 实现
   - `CMakeLists.txt` - 构建
   - `Kconfig` - 配置
3. **编写代码**: 参考 [快速模板](./DRIVER_TEMPLATE_QUICKREF.md)
4. **注册设备**: 使用 `LISA_DEVICE_REGISTER` 宏
5. **集成构建**: 更新 `drivers/CMakeLists.txt` 和 `drivers/Kconfig`

### 使用AI生成驱动

复制以下提示词到AI助手：

```
请基于 LISA 驱动框架为 [设备名称] 创建驱动

设备信息：
- 设备名: lisa_[xxx]
- HAL接口: Driver_[XXX].h  
- 功能: [列举功能]

参考文档：
- 模板: /path/to/drivers/DRIVER_TEMPLATE_QUICKREF.md
- 示例: /path/to/drivers/lisa_gpio/

生成:
1. lisa_xxx.h
2. lisa_xxx_arcs.c
3. CMakeLists.txt
4. Kconfig
```

---

## 📋 核心概念速查

### 设备框架核心

| 概念 | 说明 |
|-----|------|
| `lisa_device_t` | 设备基类，所有设备的统一抽象 |
| `LISA_DEVICE_REGISTER` | 设备注册宏，自动注册到框架 |
| `lisa_device_get()` | 通过名称获取设备 |
| `lisa_device_ready()` | 检查设备是否就绪 |

### 错误码

| 错误码 | 值 | 用途 |
|--------|---|------|
| `LISA_DEVICE_OK` | 0 | 操作成功 |
| `LISA_DEVICE_ERR_INVALID` | -1 | 参数无效 |
| `LISA_DEVICE_ERR_NOT_READY` | -9 | 设备未就绪 |
| `LISA_DEVICE_ERR_IO` | -10 | IO错误 |

### 优先级

| 宏 | 值 | 适用设备 |
|----|---|---------|
| `CRITICAL` | 0 | 时钟、电源 |
| `HIGH` | 10 | UART、GPIO |
| `NORMAL` | 50 | 普通外设 |
| `LOW` | 90 | 传感器、LED |

---

## 🔧 开发工具链

### 必需工具
- CMake 3.20+
- Kconfig
- ARCS SDK

### 推荐IDE
- VSCode + C/C++ Extension
- CLion

---

## 📞 获取帮助

### 常见问题
- 查看 [README_DEVELOPMENT.md](./README_DEVELOPMENT.md) FAQ部分
- 参考 [检查清单](./DRIVER_CHECKLIST.md) 排查问题

### 联系方式
- 技术支持: support@listenai.com
- 问题反馈: 提交 Issue

---

## 📌 重要链接

| 文档 | 用途 | 优先级 |
|-----|------|--------|
| [DRIVER_DEVELOPMENT_GUIDE.md](./DRIVER_DEVELOPMENT_GUIDE.md) | 完整开发指南 | ⭐⭐⭐ |
| [DRIVER_TEMPLATE_QUICKREF.md](./DRIVER_TEMPLATE_QUICKREF.md) | 快速参考 | ⭐⭐⭐ |
| [DRIVER_CHECKLIST.md](./DRIVER_CHECKLIST.md) | 检查清单 | ⭐⭐ |
| [README_DEVELOPMENT.md](./README_DEVELOPMENT.md) | 文档导航 | ⭐⭐ |
| [lisa_device/README.md](./lisa_device/README.md) | 框架说明 | ⭐ |
| [lisa_gpio/](./lisa_gpio/) | 参考实现 | ⭐ |

---

## 🏆 最佳实践

1. **开发前**: 先阅读[完整指南](./DRIVER_DEVELOPMENT_GUIDE.md)了解框架
2. **开发中**: 参考[快速模板](./DRIVER_TEMPLATE_QUICKREF.md)和[GPIO实现](./lisa_gpio/)
3. **开发后**: 使用[检查清单](./DRIVER_CHECKLIST.md)验证代码质量
4. **AI辅助**: 提供[快速参考](./DRIVER_TEMPLATE_QUICKREF.md)给AI工具

---

**维护**: LISTENAI SDK Team  
**版本**: v1.0  
**更新**: 2025-01-27
