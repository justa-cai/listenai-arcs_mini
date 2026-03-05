# LISA 驱动模型开发指南

> 本文档为AI辅助开发优化，提供清晰的模板和步骤指引

## 一、开发步骤总览

```
1. 创建驱动目录和文件 → 2. 定义设备API → 3. 实现硬件适配 → 4. 注册设备 → 5. 集成到构建系统
```

---

## 二、文件结构模板

为新设备 `xxx` 创建以下文件：

```
drivers/lisa_xxx/
├── CMakeLists.txt          # 构建配置
├── Kconfig                 # 配置选项
├── lisa_xxx.h              # 公共API头文件
└── lisa_xxx_arcs.c         # ARCS平台实现
```

---

## 三、关键代码模板

### 3.1 设备API头文件 (`lisa_xxx.h`)

```c
#pragma once
#include "lisa_device.h"

/* 设备配置结构体 */
typedef struct {
    // 设备特定配置参数
} lisa_xxx_config_t;

/* 设备API结构体 - 定义所有操作接口 */
typedef struct {
    int (*init)(lisa_device_t *dev, const lisa_xxx_config_t *config);
    int (*deinit)(lisa_device_t *dev);
    int (*read)(lisa_device_t *dev, void *buffer, uint32_t size);
    int (*write)(lisa_device_t *dev, const void *data, uint32_t size);
    // 根据设备类型添加其他接口...
} lisa_xxx_api_t;

/* 便捷inline包装函数 */
static inline int lisa_xxx_init(lisa_device_t *dev, const lisa_xxx_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_xxx_api_t *api = (lisa_xxx_api_t *)dev->api;
    return api->init ? api->init(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

static inline int lisa_xxx_read(lisa_device_t *dev, void *buffer, uint32_t size)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_xxx_api_t *api = (lisa_xxx_api_t *)dev->api;
    return api->read ? api->read(dev, buffer, size) : LISA_DEVICE_ERR_NOT_SUPPORT;
}
```

**关键点**：
- 使用 `lisa_device_t` 作为第一个参数
- 返回值使用统一错误码 (`LISA_DEVICE_OK` / `LISA_DEVICE_ERR_*`)
- 提供 `inline` 包装函数简化调用

---

### 3.2 平台实现文件 (`lisa_xxx_arcs.c`)

```c
#include "lisa_xxx.h"
#include "Driver_XXX.h"  // HAL头文件
#include <lisa_mutex.h>

#define LOG_TAG "lisa_xxx"
#include <lisa_log.h>

/* ===== 私有数据结构 ===== */
typedef struct {
    void *hal_handler;    // HAL句柄
    lisa_mutex_t *mutex;  // 互斥锁（可选）
    // 其他设备特定数据...
} lisa_xxx_priv_t;

/* ===== 设备实例 ===== */
static lisa_xxx_priv_t xxx0_priv;

/* ===== 参数检查辅助函数 ===== */
static inline int check_params_valid(lisa_device_t *dev, void *data)
{
    if (!lisa_device_is_initialized(dev) || !data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    return LISA_DEVICE_OK;
}

/* ===== API实现函数 ===== */
static int arcs_xxx_init(lisa_device_t *dev, const lisa_xxx_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_xxx_priv_t *priv = (lisa_xxx_priv_t *)dev->priv_data;
    
    // 调用HAL初始化
    if (XXX_Initialize(priv->hal_handler, config) != 0) {
        return LISA_DEVICE_ERR_IO;
    }
    
    return LISA_DEVICE_OK;
}

static int arcs_xxx_read(lisa_device_t *dev, void *buffer, uint32_t size)
{
    if (check_params_valid(dev, buffer) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_xxx_priv_t *priv = (lisa_xxx_priv_t *)dev->priv_data;
    
    // 调用HAL读取
    int ret = XXX_Read(priv->hal_handler, buffer, size);
    return (ret < 0) ? LISA_DEVICE_ERR_IO : ret;
}

/* ===== API实例 ===== */
static const lisa_xxx_api_t arcs_xxx_api = {
    .init = arcs_xxx_init,
    .deinit = arcs_xxx_deinit,
    .read = arcs_xxx_read,
    .write = arcs_xxx_write,
};

/* ===== 设备初始化函数 ===== */
static int arcs_xxx0_init(void)
{
    // 获取HAL句柄
    xxx0_priv.hal_handler = XXX0();
    if (!xxx0_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get XXX0 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    // 创建互斥锁（如需要）
    xxx0_priv.mutex = lisa_mutex_create();
    if (!xxx0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    // 调用HAL初始化
    if (XXX_Initialize(xxx0_priv.hal_handler, NULL, NULL) != 0) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(xxx0,                       // 设备名称（生成"xxx0"字符串）
                     &arcs_xxx_api,              // API指针
                     &xxx0_priv,                 // 私有数据指针
                     NULL,                       // 用户数据（可选）
                     arcs_xxx0_init,             // 初始化函数
                     LISA_DEVICE_PRIORITY_NORMAL); // 优先级
```

**关键点**：
- 初始化函数返回 `LISA_DEVICE_OK (0)` 表示成功
- 使用 `lisa_device_is_initialized()` 检查设备状态
- 所有HAL错误转换为统一错误码
- 设备名称通过宏自动生成字符串

---

### 3.3 CMakeLists.txt

```cmake
if(CONFIG_LISA_XXX)

listenai_library_named(lisa_xxx)

listenai_include_directories(.)

listenai_library_sources(
    lisa_xxx_arcs.c
)

endif() # CONFIG_LISA_XXX
```

---

### 3.4 Kconfig

```kconfig
menuconfig LISA_XXX
    bool "Enable LISA XXX Driver"
    default n
    select LISA_DEVICE
    help
      Enable XXX device driver support.

if LISA_XXX

config LISA_XXX_DEBUG
    bool "Enable XXX driver debug"
    default n
    help
      Enable debug logging for XXX driver.

endif # LISA_XXX
```

---

## 四、集成到构建系统

### 4.1 更新顶层构建文件

**drivers/CMakeLists.txt**:
```cmake
add_subdirectory(lisa_xxx)
```

**drivers/Kconfig**:
```kconfig
rsource "lisa_xxx/Kconfig"
```

---

## 五、设备使用示例

```c
#include "lisa_xxx.h"

void app_main(void)
{
    // 1. 获取设备
    lisa_device_t *xxx = lisa_device_get("xxx0");
    if (!xxx || !lisa_device_ready(xxx)) {
        // 设备未就绪
        return;
    }
    
    // 2. 配置设备
    lisa_xxx_config_t config = {
        // 配置参数...
    };
    lisa_xxx_init(xxx, &config);
    
    // 3. 读写操作
    uint8_t buffer[128];
    int ret = lisa_xxx_read(xxx, buffer, sizeof(buffer));
    if (ret < 0) {
        // 错误处理
    }
}
```

---

## 六、统一错误码表

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `LISA_DEVICE_OK` | 0 | 成功 |
| `LISA_DEVICE_ERR_INVALID` | -1 | 无效参数 |
| `LISA_DEVICE_ERR_NOT_READY` | -9 | 设备未就绪 |
| `LISA_DEVICE_ERR_IO` | -10 | IO错误 |
| `LISA_DEVICE_ERR_RANGE` | -11 | 参数超范围 |
| `LISA_DEVICE_ERR_INIT_FAIL` | -5 | 初始化失败 |

---

## 七、开发检查清单

- [ ] 头文件包含 `lisa_device.h`
- [ ] API结构体第一个参数为 `lisa_device_t *dev`
- [ ] 使用统一错误码返回值
- [ ] 实现函数检查 `lisa_device_is_initialized()`
- [ ] 初始化函数返回 `LISA_DEVICE_OK` 表示成功
- [ ] 使用 `LISA_DEVICE_REGISTER` 宏注册设备
- [ ] 设备名称唯一且小写
- [ ] 提供 `inline` 包装函数
- [ ] 更新顶层 `CMakeLists.txt` 和 `Kconfig`
- [ ] 添加日志支持（LOG_TAG）

---

## 八、常见设备类型参考

| 设备类型 | 典型API | 参考实现 |
|---------|---------|---------|
| GPIO | configure, read_pin, write_pin | `lisa_gpio` |
| UART | init, read, write, set_baudrate | 待实现 |
| I2C | transfer, write, read | 待实现 |
| SPI | transfer, write_read | 待实现 |
| Timer | start, stop, get_count | 待实现 |

---

## 九、AI提示词模板

**用于生成新驱动时的提示词**：

```
请基于 LISA 驱动框架为 [设备名称] 创建驱动，包含以下功能：
- [功能1]
- [功能2]

设备名称: lisa_[xxx]
HAL接口: Driver_[XXX].h
主要操作: [初始化/读/写/配置等]

请生成完整的驱动代码，包括：
1. lisa_xxx.h (API定义)
2. lisa_xxx_arcs.c (平台实现)
3. CMakeLists.txt
4. Kconfig

参考模板: drivers/DRIVER_DEVELOPMENT_GUIDE.md
参考实现: drivers/lisa_gpio/
```

---

## 十、注意事项

1. **线程安全**：如设备可能被多线程访问，需在私有数据中添加互斥锁
2. **错误处理**：所有HAL调用必须检查返回值并转换为统一错误码
3. **设备命名**：使用小写字母和下划线，格式 `lisa_[设备类型]`
4. **优先级选择**：
   - 系统核心设备（时钟/电源）：`CRITICAL (0)`
   - 重要外设（UART/GPIO）：`HIGH (10)` 或 `NORMAL (50)`
   - 可选功能（传感器）：`LOW (90)`
5. **日志级别**：初始化用 `LOGI`，错误用 `LOGE`，调试用 `LOGD`

---

**文档版本**: v1.0  
**更新日期**: 2025-01-27
