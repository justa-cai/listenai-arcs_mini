# LISA 驱动开发快速参考

> 超精简模板，用于AI快速生成代码

## 文件清单

```
drivers/lisa_xxx/
├── lisa_xxx.h          # API定义
├── lisa_xxx_arcs.c     # 实现
├── CMakeLists.txt      # 构建
└── Kconfig             # 配置
```

---

## 完整代码模板

### `lisa_xxx.h`

```c
#pragma once
#include "lisa_device.h"

typedef struct {
    /* 配置参数 */
} lisa_xxx_config_t;

typedef struct {
    int (*operation)(lisa_device_t *dev, /* params */);
} lisa_xxx_api_t;

static inline int lisa_xxx_operation(lisa_device_t *dev, /* params */)
{
    if (!dev || !dev->api) return LISA_DEVICE_ERR_INVALID;
    lisa_xxx_api_t *api = (lisa_xxx_api_t *)dev->api;
    return api->operation ? api->operation(dev, /* params */) : LISA_DEVICE_ERR_NOT_SUPPORT;
}
```

### `lisa_xxx_arcs.c`

```c
#include "lisa_xxx.h"
#include "Driver_XXX.h"

#define LOG_TAG "lisa_xxx"
#include <lisa_log.h>

typedef struct {
    void *hal_handler;
    /* 其他数据 */
} lisa_xxx_priv_t;

static lisa_xxx_priv_t xxx0_priv;

static int arcs_xxx_operation(lisa_device_t *dev, /* params */)
{
    if (!lisa_device_is_initialized(dev)) return LISA_DEVICE_ERR_NOT_READY;
    lisa_xxx_priv_t *priv = (lisa_xxx_priv_t *)dev->priv_data;
    
    /* HAL调用 */
    int ret = HAL_Function(priv->hal_handler, /* params */);
    return (ret != 0) ? LISA_DEVICE_ERR_IO : LISA_DEVICE_OK;
}

static const lisa_xxx_api_t arcs_xxx_api = {
    .operation = arcs_xxx_operation,
};

static int arcs_xxx0_init(void)
{
    xxx0_priv.hal_handler = XXX0();
    if (!xxx0_priv.hal_handler) return LISA_DEVICE_ERR_INIT_FAIL;
    
    if (HAL_Initialize(xxx0_priv.hal_handler) != 0) return LISA_DEVICE_ERR_INIT_FAIL;
    
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(xxx0, &arcs_xxx_api, &xxx0_priv, NULL, arcs_xxx0_init, LISA_DEVICE_PRIORITY_NORMAL);
```

### `CMakeLists.txt`

```cmake
if(CONFIG_LISA_XXX)
listenai_library_named(lisa_xxx)
listenai_include_directories(.)
listenai_library_sources(lisa_xxx_arcs.c)
endif()
```

### `Kconfig`

```kconfig
menuconfig LISA_XXX
    bool "Enable LISA XXX Driver"
    default n
    select LISA_DEVICE

if LISA_XXX
# 其他选项...
endif
```

---

## 集成步骤

1. **drivers/CMakeLists.txt**: 添加 `add_subdirectory(lisa_xxx)`
2. **drivers/Kconfig**: 添加 `rsource "lisa_xxx/Kconfig"`

---

## 关键规则

| 项目 | 规则 |
|-----|------|
| 设备名 | 小写，格式 `lisa_xxx` |
| 返回值 | 成功=`LISA_DEVICE_OK (0)`，失败=负数 |
| 第一参数 | 必须是 `lisa_device_t *dev` |
| 状态检查 | `lisa_device_is_initialized(dev)` |
| 错误转换 | HAL错误 → `LISA_DEVICE_ERR_IO` |
| 注册宏 | `LISA_DEVICE_REGISTER(名称, API, 私有数据, NULL, 初始化函数, 优先级)` |
| 优先级 | CRITICAL(0)/HIGH(10)/NORMAL(50)/LOW(90) |

---

## AI生成提示词

```
基于 LISA 驱动框架创建 [设备] 驱动
- 设备: lisa_[xxx]
- 功能: [列举]
- HAL: Driver_[XXX].h

使用模板: drivers/DRIVER_TEMPLATE_QUICKREF.md
参考: drivers/lisa_gpio/

生成文件:
1. lisa_xxx.h
2. lisa_xxx_arcs.c  
3. CMakeLists.txt
4. Kconfig
```

---

**版本**: v1.0
