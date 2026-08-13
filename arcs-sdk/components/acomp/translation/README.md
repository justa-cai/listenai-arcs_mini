# 机器翻译组件

## 简介

ACOMP Translation 是 ARCS SDK 的机器翻译组件，提供基于文本输入的机器翻译功能。该组件支持中英文双向翻译（英译中、中译英），采用 Encoder-Decoder 架构，支持多组编码器和解码器模型，翻译结果通过事件回调返回。

## 主要特性

- **双向翻译**：支持英文到中文（EN→CN）和中文到英文（CN→EN）双向翻译
- **Encoder-Decoder 架构**：采用多组编码器（A-F）和解码器模型，支持复杂的翻译任务
- **结果类型配置**：支持设置翻译结果的返回类型
- **事件回调机制**：支持翻译结果返回和翻译状态通知
- **快捷 API**：提供 `acomp_translation_do_prepare()` / `acomp_translation_do_cleanup()` 快捷接口，简化资源管理
- **混合存储**：模型资源支持 Flash 和 eMMC 混合存储
- **跨核通信**：基于 IPC 机制实现文本和翻译结果的跨核传输

## 配置选项

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP` | 启用 ACOMP 组件 | n |
| `CONFIG_ACOMP_TRANSLATION` | 启用 ACOMP 组件的翻译组件 | n |

### 资源配置

翻译组件包含英译中（ENCN）和中译英（CNEN）两套模型，每套包含 6 个编码器（A-F）、1 个解码器和对应的词嵌入模型，所有模型资源存储在 eMMC 中。

**英译中 (ENCN) 模型资源**（仅列出部分编码器，C-F 配置项结构与 A/B 相同）:

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_TRANS_RES_ENCN_ENC_A_EMMC_ADDR` | 编码器 A eMMC 地址 | 0x0 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_ENC_A_SIZE` | 编码器 A 大小(字节) | 3306480 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_ENC_B_EMMC_ADDR` | 编码器 B eMMC 地址 | 0x328000 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_ENC_B_SIZE` | 编码器 B 大小(字节) | 3286864 |
| ... | 编码器 C-F 配置（各含 EMMC_ADDR 和 SIZE） | ... |
| `CONFIG_ACOMP_TRANS_RES_ENCN_DEC_EMMC_ADDR` | 解码器 eMMC 地址 | 0x131a000 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_DEC_SIZE` | 解码器大小(字节) | 4429216 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_DEC_EMB_EMMC_ADDR` | 解码器词嵌入 eMMC 地址 | 0x1754000 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_DEC_EMB_SIZE` | 解码器词嵌入大小(字节) | 1936384 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_ENC_EMB_EMMC_ADDR` | 编码器词嵌入 eMMC 地址 | 0x192d000 |
| `CONFIG_ACOMP_TRANS_RES_ENCN_ENC_EMB_SIZE` | 编码器词嵌入大小(字节) | 1936384 |

**中译英 (CNEN) 模型资源**：配置结构与 ENCN 相同，配置项前缀为 `CONFIG_ACOMP_TRANS_RES_CNEN_*`。

此外还需要 Flash 中的翻译词典（trandb）和传统词典（dict_trad）资源，通过 resmgr 获取。

## 快速开始

### 方式一：使用快捷 API（推荐）

快捷 API 封装了完整的初始化、资源加载和启动流程：

```c
#include "translation/acomp_translation.h"

void trans_event_handler(uint32_t event, void *event_data,
                         uint32_t event_data_len, void *priv)
{
    if (event & TRANS_CB_EVENT_RESULT) {
        // 翻译结果
        printf("Translation result: %.*s\n", event_data_len, (char *)event_data);
    } else if (event & TRANS_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        printf("Translation status: %u\n", status);
    }
}

void translation_quick_start(void)
{
    // 初始化 ACOMP 框架
    acomp_init();
    
    // 一键准备并启动翻译组件
    int ret = acomp_translation_do_prepare(trans_event_handler, NULL);
    if (ret != 0) {
        printf("Translation prepare failed: %d\n", ret);
        return;
    }
    
    // 提交翻译文本
    const char *text = "Hello, world";
    acomp_translation_translate(text, strlen(text));
    
    // 翻译结果将通过 TRANS_CB_EVENT_RESULT 回调返回
    
    // 清理
    acomp_translation_do_cleanup();
}
```

### 方式二：使用标准 API

#### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```kconfig
# 启用 ACOMP 组件
CONFIG_ACOMP=y

# 启用 ACOMP 组件的翻译组件
CONFIG_ACOMP_TRANSLATION=y
```

#### 2. 初始化翻译组件

```c
#include "acomp.h"
#include "translation/acomp_translation.h"

int main(int argc, char **argv)
{
    // 初始化 ACOMP 框架
    acomp_init();
    
    // 初始化翻译组件
    int ret = acomp_translation_init();
    if (ret != ACOMP_ERR_OK) {
        printf("Translation init failed: %d\n", ret);
        return -1;
    }
    
    return 0;
}
```

#### 3. 注册事件回调

```c
#include "translation/acomp_translation.h"

void trans_event_handler(uint32_t event, void *event_data,
                         uint32_t event_data_len, void *priv)
{
    if (event & TRANS_CB_EVENT_RESULT) {
        printf("Translation result: %.*s\n", event_data_len, (char *)event_data);
    } else if (event & TRANS_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        printf("Translation status: %u\n", status);
    }
}

// 注册回调
int ret = acomp_translation_add_callback(
    TRANS_CB_EVENT_RESULT | TRANS_CB_EVENT_STATUS,
    trans_event_handler,
    NULL
);
```

#### 4. 准备资源并启动

```c
void start_translation_service(acomp_ipc_prepare_t *prepare)
{
    int ret;
    
    // 1. 就绪组件（传入资源配置）
    ret = acomp_translation_prepare(prepare);
    if (ret != ACOMP_ERR_OK) {
        printf("Translation prepare failed: %d\n", ret);
        return;
    }
    
    // 2. 启动翻译
    ret = acomp_translation_start();
    if (ret != ACOMP_ERR_OK) {
        printf("Translation start failed: %d\n", ret);
        return;
    }
    
    printf("Translation service started\n");
}
```

#### 5. 执行翻译

```c
void do_translate(const char *text)
{
    int ret = acomp_translation_translate(text, strlen(text));
    if (ret != ACOMP_ERR_OK) {
        printf("Translate failed: %d\n", ret);
    }
    // 翻译结果通过 TRANS_CB_EVENT_RESULT 回调返回
}
```

## API 参考

### 生命周期管理

#### acomp_translation_init

```c
int acomp_translation_init(void);
```

**功能**：初始化翻译组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：组件已初始化
- `ACOMP_ERR_NOT_FOUND`：设备未找到

#### acomp_translation_prepare

```c
int acomp_translation_prepare(acomp_ipc_prepare_t *prepare);
```

**功能**：就绪翻译组件，将资源配置发送给 AP 端

**参数**：
- `prepare`：资源配置数据结构指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_translation_start

```c
int acomp_translation_start(void);
```

**功能**：启动翻译组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_translation_stop

```c
int acomp_translation_stop(void);
```

**功能**：停止翻译组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_translation_cleanup

```c
int acomp_translation_cleanup(void);
```

**功能**：清理翻译组件资源，注销 IPC 回调并释放回调链表

**返回值**：
- `ACOMP_ERR_OK`：成功

### 快捷 API

#### acomp_translation_do_prepare

```c
int acomp_translation_do_prepare(trans_event_cb_t event_cb, void *cb_priv);
```

**功能**：一键准备并启动翻译组件

**说明**：封装了 init、回调注册、资源加载（eMMC + Flash resmgr）和 start 的完整流程。资源地址来自 Kconfig 配置。不会清理其他算法组件，调用方需自行协调。

**参数**：
- `event_cb`：事件回调函数（首次调用时注册，可为 NULL）
- `cb_priv`：回调函数的私有数据指针

**返回值**：
- `0`：成功
- 负值：错误

#### acomp_translation_do_cleanup

```c
int acomp_translation_do_cleanup(void);
```

**功能**：一键停止并清理翻译组件

**返回值**：
- `0`：成功

### 翻译控制

#### acomp_translation_translate

```c
int acomp_translation_translate(const char *text, uint32_t len);
```

**功能**：提交文本进行翻译

**参数**：
- `text`：待翻译的文本字符串
- `len`：文本长度（字节）

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：text 为 NULL 或 len 为 0
- `ACOMP_ERR_NO_MEM`：内存不足

#### acomp_translation_set_res_type

```c
int acomp_translation_set_res_type(int type);
```

**功能**：设置翻译结果类型

**参数**：
- `type`：结果类型值

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 事件回调

#### acomp_translation_add_callback

```c
int acomp_translation_add_callback(uint32_t events, trans_event_cb_t cb, void *priv);
```

**功能**：添加事件回调函数

**参数**：
- `events`：事件位掩码，可同时注册多个事件
  - `TRANS_CB_EVENT_RESULT`：翻译结果返回
  - `TRANS_CB_EVENT_STATUS`：翻译状态通知
- `cb`：回调函数指针
- `priv`：回调函数的私有数据指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_translation_remove_callback

```c
int acomp_translation_remove_callback(trans_event_cb_t cb);
```

**功能**：移除回调函数

**参数**：
- `cb`：待移除的回调函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

## 使用示例

### 完整示例

```c
#include "acomp.h"
#include "translation/acomp_translation.h"
#include "lisa_log.h"

#define TAG "trans_demo"

void trans_event_handler(uint32_t event, void *event_data,
                         uint32_t event_data_len, void *priv)
{
    if (event & TRANS_CB_EVENT_RESULT) {
        LISA_LOGI(TAG, "Translation result: %.*s", event_data_len, (char *)event_data);
    } else if (event & TRANS_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "Translation status: %u", status);
    }
}

void translation_demo(void)
{
    int ret;
    
    // 1. 初始化 ACOMP 框架
    acomp_init();
    
    // 2. 一键准备并启动翻译组件
    ret = acomp_translation_do_prepare(trans_event_handler, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "Translation prepare failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "Translation started");
    
    // 3. 英译中
    const char *en_text = "Hello, how are you?";
    ret = acomp_translation_translate(en_text, strlen(en_text));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Translate failed: %d", ret);
    }
    
    // 4. 等待翻译结果（通过回调返回）
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 5. 中译英
    const char *cn_text = "你好世界";
    ret = acomp_translation_translate(cn_text, strlen(cn_text));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Translate failed: %d", ret);
    }
    
    // 6. 等待翻译结果
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 7. 清理
    acomp_translation_do_cleanup();
    LISA_LOGI(TAG, "Translation cleaned up");
}
```

## 注意事项

1. **初始化顺序**：必须先调用 `acomp_init()` 初始化 ACOMP 框架
2. **快捷 API 推荐**：推荐使用 `acomp_translation_do_prepare()` 快捷 API，自动处理 20 项模型资源的加载
3. **模型资源**：翻译组件需要大量模型资源（ENCN 和 CNEN 各 9 项 + 词典 2 项），确保 eMMC 和 Flash 中的资源正确烧录
4. **算法协调**：快捷 API 不会清理其他算法组件（如 CV、XTTS），调用方需自行管理算法生命周期
5. **异步结果**：翻译结果通过 `TRANS_CB_EVENT_RESULT` 事件回调异步返回
6. **事件回调**：回调函数在组件内部线程中执行，应避免长时间阻塞操作
7. **内存管理**：组件使用动态内存分配，确保系统有足够的堆内存
8. **跨核通信**：文本和翻译结果基于 IPC 机制传输，注意跨核数据传输的延迟

## 常见问题

**Q: 翻译组件初始化失败怎么办？**

A: 检查以下几点：
- 确保 ACOMP 框架已正确初始化
- 检查系统是否有足够的堆内存
- 确认 AP 核固件已正确烧录

**Q: 翻译没有返回结果？**

A: 检查：
- 确认已正确注册 `TRANS_CB_EVENT_RESULT` 事件回调
- 确认翻译文本非空
- 检查模型资源是否正确加载（eMMC 和 Flash 中的所有资源）

**Q: 翻译结果不准确怎么办？**

A: 建议：
- 检查输入文本是否符合模型支持的语言
- 尝试调整翻译结果类型（`acomp_translation_set_res_type()`）
- 确保所有模型资源版本一致

**Q: 翻译速度很慢怎么办？**

A: 建议：
- 减少每次翻译的文本长度
- 确保 AP 核没有被其他算法任务占用
- 检查 eMMC 读取速度是否正常

## 依赖项

- `ACOMP 框架`：算法组件框架
- `resmgr`：Flash 资源管理器（用于加载翻译词典）
- `lisa_log`：日志系统
- `lisa_mem`：内存管理
- `FreeRTOS`：实时操作系统
- `翻译算法库`：Encoder-Decoder 翻译算法引擎
