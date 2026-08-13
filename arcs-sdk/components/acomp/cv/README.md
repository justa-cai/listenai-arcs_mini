# 计算机视觉组件

## 简介

ACOMP CV (Computer Vision) 是 ARCS SDK 的计算机视觉组件，提供基于图像流的 OCR 文字识别、图像拼接和切割线检测功能。该组件支持多种图像格式输入，提供灵活的扫描模式配置和事件回调机制。

## 主要特性

- **OCR 文字识别**：支持基于图像流的文字识别，返回识别结果
- **图像拼接**：支持多帧图像拼接，返回拼接帧数据
- **切割线检测**：支持文档切割线检测
- **多种扫描模式**：支持正常扫描和快扫模式
- **左右模式配置**：支持左右手扫描方向设置
- **事件回调机制**：支持 OCR 结果、拼接帧、状态变更、帧处理完成等多种事件通知
- **图像流管理**：提供完整的图像流通道管理和缓冲区操作接口
- **图像保存回传**：支持 AP 核将处理后的图像（原始图、拼接图、切割线图）回传给 CP 核
- **跨核通信**：基于 IPC 机制实现图像流的跨核传输

## 图像保存数据结构

当 AP 核处理完图像后，可以通过 `CV_CB_EVENT_IMG_SAVE` 事件将图像数据回传给 CP 核：

```c
typedef struct {
    uint32_t img_addr;    /* 图像数据在共享内存中的地址 */
    uint16_t width;       /* 图像宽度（像素） */
    uint16_t height;      /* 图像高度（像素） */
    uint8_t  img_type;    /* 图像类型 */
} cv_img_save_info_t;
```

### 图像类型定义

| 类型 | 宏定义 | 说明 |
|------|--------|------|
| 0 | `CV_IMG_TYPE_RAW` | 原始图像 |
| 1 | `CV_IMG_TYPE_STITCH` | 拼接图像 |
| 2 | `CV_IMG_TYPE_CUTLINE` | 切割线图像 |

## 配置选项

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP` | 启用 ACOMP 组件 | n |
| `CONFIG_ACOMP_CV` | 启用 ACOMP 组件的 CV 组件 | n |

### 资源配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_CV_RES_CUTLINE_EMMC_ADDR` | 切割线模型 eMMC 地址 | 0x9b00000 |
| `CONFIG_ACOMP_CV_RES_CUTLINE_SIZE` | 切割线模型大小(字节) | 462320 |
| `CONFIG_ACOMP_CV_RES_OCR_EMMC_ADDR` | OCR 模型 eMMC 地址 | 0xa000000 |
| `CONFIG_ACOMP_CV_RES_OCR_SIZE` | OCR 模型大小(字节) | 2289296 |

## 快速开始

### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```kconfig
# 启用 ACOMP 组件
CONFIG_ACOMP=y

# 启用 ACOMP 组件的 CV 组件
CONFIG_ACOMP_CV=y

# 配置 CV 组件所需资源在 eMMC 的位置和大小
CONFIG_ACOMP_CV_RES_CUTLINE_EMMC_ADDR=0x9b00000
CONFIG_ACOMP_CV_RES_CUTLINE_SIZE=462320
CONFIG_ACOMP_CV_RES_OCR_EMMC_ADDR=0xa000000
CONFIG_ACOMP_CV_RES_OCR_SIZE=2289296
```

### 2. 初始化 CV 组件

在应用程序中初始化 CV 组件：

```c
#include "acomp.h"
#include "cv/acomp_cv.h"

int main(int argc, char **argv)
{
    // 初始化 ACOMP 框架
    acomp_init();
    
    // 初始化 CV 组件
    int ret = acomp_cv_init();
    if (ret != ACOMP_ERR_OK) {
        printf("CV init failed: %d\n", ret);
        return -1;
    }
    
    // 你的应用代码
    return 0;
}
```

### 3. 注册事件回调

使用事件回调函数接收 CV 处理结果：

```c
#include "cv/acomp_cv.h"

void cv_event_handler(uint32_t event, void *event_data,
                      uint32_t event_data_len, void *priv)
{
    if (event & CV_CB_EVENT_OCR_RESULT) {
        // OCR 识别结果
        printf("OCR result: %.*s\n", event_data_len, (char *)event_data);
    } else if (event & CV_CB_EVENT_STITCH_FRAME) {
        // 拼接帧数据
        printf("Stitch frame received, len: %d\n", event_data_len);
    } else if (event & CV_CB_EVENT_STATUS) {
        // 状态变更
        uint32_t status = *(uint32_t *)event_data;
        printf("CV status: %u\n", status);
    } else if (event & CV_CB_EVENT_FRAME_DONE) {
        // AP 处理完帧
        uint32_t fb_addr = *(uint32_t *)event_data;
        printf("Frame done, fb_addr: 0x%x\n", fb_addr);
    } else if (event & CV_CB_EVENT_IMG_SAVE) {
        // AP 回传处理后的图像
        cv_img_save_info_t *info = (cv_img_save_info_t *)event_data;
        printf("Image saved: type=%d, %dx%d, addr=0x%x\n",
               info->img_type, info->width, info->height, info->img_addr);
    }
}

// 注册回调
int ret = acomp_cv_add_callback(
    CV_CB_EVENT_OCR_RESULT |
    CV_CB_EVENT_STITCH_FRAME |
    CV_CB_EVENT_STATUS |
    CV_CB_EVENT_FRAME_DONE |
    CV_CB_EVENT_IMG_SAVE,
    cv_event_handler,
    NULL
);
```

### 4. 启动 CV 服务

完整的启动流程：

```c
#include "cv/acomp_cv.h"

void start_cv_service(acomp_ipc_prepare_t *prepare)
{
    int ret;
    
    // 1. 就绪组件（传入 prepare 数据，初始化算法资源）
    ret = acomp_cv_prepare(prepare);
    if (ret != ACOMP_ERR_OK) {
        printf("CV prepare failed: %d\n", ret);
        return;
    }
    
    // 2. 启动 CV
    ret = acomp_cv_start();
    if (ret != ACOMP_ERR_OK) {
        printf("CV start failed: %d\n", ret);
        return;
    }
    
    printf("CV service started\n");
}

void stop_cv_service(void)
{
    int ret;
    
    // 1. 停止 CV
    ret = acomp_cv_stop();
    if (ret != ACOMP_ERR_OK) {
        printf("CV stop failed: %d\n", ret);
        return;
    }
    
    // 2. 清理资源
    ret = acomp_cv_cleanup();
    if (ret != ACOMP_ERR_OK) {
        printf("CV cleanup failed: %d\n", ret);
        return;
    }
    
    printf("CV service stopped\n");
}
```

### 5. 配置扫描参数

设置扫描模式和相关参数：

```c
#include "cv/acomp_cv.h"

void configure_cv(void)
{
    // 设置扫描模式
    acomp_cv_set_scan_mode(0);

    // 设置左右模式
    acomp_cv_set_lr_mode(0);

    // 设置屏幕高度
    acomp_cv_set_screen_height(240);

    // 设置启动类型（0=正常启动, 1=快扫启动）
    acomp_cv_set_boot_type(0);
}
```

### 6. 创建图像输入流并发送图像数据

使能 M2R (Master to Remote) 流通道，用于将图像数据发送给 AP 核：

```c
#include "cv/acomp_cv.h"

#define TX_STREAM_CH_INDEX    0
#define TX_STREAM_CH_NAME     "stream.cv_image"

void setup_tx_stream(void)
{
    // 1. 创建 M2R 流通道描述符
    acomp_stream_chn_create_desc_t desc = {
        .cname = TX_STREAM_CH_NAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,  // Master to Remote
        .index = TX_STREAM_CH_INDEX,
        .buffer_size = 320 * 240 * 2,   // 图像缓冲区大小
        .num_descs = 4,                  // 缓冲区描述符数量
        .kick_policy = 1,               // 自动触发
    };
    
    // 2. 使能流通道
    int ret = acomp_cv_stream_ch_enable(TX_STREAM_CH_INDEX, &desc);
    if (ret != ACOMP_ERR_OK) {
        printf("Failed to enable tx stream: %d\n", ret);
    }
}

void send_image_to_cv(uint8_t *image_data, uint32_t image_len)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    
    // 1. 分配发送缓冲区
    buffer = acomp_cv_stream_tx_buffer_alloc(TX_STREAM_CH_INDEX,
                                              &buf_size, &desc_idx);
    if (buffer && buf_size > 0) {
        // 2. 填充图像数据到缓冲区
        memcpy(buffer, image_data, image_len);
        
        // 3. 提交缓冲区发送
        acomp_cv_stream_tx_buffer_submit(TX_STREAM_CH_INDEX,
                                          buffer, image_len, desc_idx);
    }
}
```

## API 参考

### 生命周期管理

#### acomp_cv_init

```c
int acomp_cv_init(void);
```

**功能**：初始化 CV 组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_NOT_FOUND`：设备未找到

#### acomp_cv_prepare

```c
int acomp_cv_prepare(acomp_ipc_prepare_t *prepare);
```

**功能**：就绪 CV 组件，将调用方提供的 prepare 数据发送给 AP 端初始化算法资源

**说明**：在调用 `acomp_cv_start()` 之前必须调用此函数

**参数**：
- `prepare`：调用方提供的 prepare 数据结构指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_cv_cleanup

```c
int acomp_cv_cleanup(void);
```

**功能**：复位 CV 组件，释放内存块及算法资源

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_cv_start

```c
int acomp_cv_start(void);
```

**功能**：启动 CV 组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_cv_stop

```c
int acomp_cv_stop(void);
```

**功能**：停止 CV 组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 参数配置

#### acomp_cv_set_scan_mode

```c
int acomp_cv_set_scan_mode(uint8_t scan_mode);
```

**功能**：设置扫描模式

**参数**：
- `scan_mode`：扫描模式值

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_cv_set_lr_mode

```c
int acomp_cv_set_lr_mode(uint8_t lr_mode);
```

**功能**：设置左右模式

**参数**：
- `lr_mode`：左右模式值

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_cv_set_screen_height

```c
int acomp_cv_set_screen_height(uint32_t screen_height);
```

**功能**：设置屏幕高度

**参数**：
- `screen_height`：屏幕高度（像素）

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_cv_set_boot_type

```c
int acomp_cv_set_boot_type(uint8_t boot_type);
```

**功能**：设置启动类型

**参数**：
- `boot_type`：启动类型
  - `0`：正常启动
  - `1`：快扫启动

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误

### 事件回调

#### acomp_cv_add_callback

```c
int acomp_cv_add_callback(uint32_t events, cv_event_cb_t cb, void *priv);
```

**功能**：添加事件回调函数

**参数**：
- `events`：事件位掩码，可同时注册多个事件
  - `CV_CB_EVENT_OCR_RESULT`：OCR 识别结果返回
  - `CV_CB_EVENT_STITCH_FRAME`：拼接帧数据返回
  - `CV_CB_EVENT_STATUS`：状态变更通知
  - `CV_CB_EVENT_STREAM_UPDATE`：数据流更新
  - `CV_CB_EVENT_FRAME_DONE`：AP 处理完帧通知
  - `CV_CB_EVENT_IMG_SAVE`：AP 回传 CV 处理后的图像
- `cb`：回调函数指针
- `priv`：回调函数的私有数据指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_cv_remove_callback

```c
int acomp_cv_remove_callback(cv_event_cb_t cb);
```

**功能**：移除回调函数

**参数**：
- `cb`：待移除的回调函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 图像流管理

#### acomp_cv_stream_ch_enable

```c
int acomp_cv_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);
```

**功能**：使能图像流通道

**参数**：
- `chn`：通道索引
- `desc`：通道描述符指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_CREATE_STREAM_FAILED`：创建流失败

#### acomp_cv_stream_ch_disable

```c
int acomp_cv_stream_ch_disable(int chn);
```

**功能**：禁用图像流通道

**参数**：
- `chn`：通道索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_cv_stream_tx_buffer_alloc

```c
void* acomp_cv_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx);
```

**功能**：分配 TX 流缓冲区用于向 AP 核发送图像数据

**参数**：
- `chn`：通道索引
- `len`：可用缓冲区长度指针（输出）
- `desc_idx`：描述符索引指针（输出）

**返回值**：缓冲区指针，失败返回 NULL

#### acomp_cv_stream_tx_buffer_submit

```c
int acomp_cv_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx);
```

**功能**：提交 TX 流缓冲区发送图像数据到 AP 核

**参数**：
- `chn`：通道索引
- `buffer`：缓冲区指针
- `len`：数据长度
- `desc_idx`：描述符索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

## 使用示例

### 完整示例

```c
#include "acomp.h"
#include "cv/acomp_cv.h"
#include "lisa_log.h"

#define TAG "cv_demo"

// 事件回调函数
void cv_event_handler(uint32_t event, void *event_data,
                      uint32_t event_data_len, void *priv)
{
    if (event & CV_CB_EVENT_OCR_RESULT) {
        LISA_LOGI(TAG, "OCR result: %.*s", event_data_len, (char *)event_data);
    } else if (event & CV_CB_EVENT_STITCH_FRAME) {
        LISA_LOGI(TAG, "Stitch frame received, len: %d", event_data_len);
    } else if (event & CV_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "CV status changed: %u", status);
    } else if (event & CV_CB_EVENT_FRAME_DONE) {
        uint32_t fb_addr = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "Frame done, fb_addr: 0x%x", fb_addr);
    } else if (event & CV_CB_EVENT_IMG_SAVE) {
        cv_img_save_info_t *info = (cv_img_save_info_t *)event_data;
        LISA_LOGI(TAG, "Image saved: type=%d, %dx%d",
                  info->img_type, info->width, info->height);
    }
}

void cv_demo(void)
{
    int ret;
    
    // 1. 初始化 ACOMP 框架
    acomp_init();
    
    // 2. 初始化 CV 组件
    ret = acomp_cv_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "CV init failed: %d", ret);
        return;
    }
    
    // 3. 注册事件回调
    ret = acomp_cv_add_callback(
        CV_CB_EVENT_OCR_RESULT |
        CV_CB_EVENT_STITCH_FRAME |
        CV_CB_EVENT_STATUS |
        CV_CB_EVENT_FRAME_DONE |
        CV_CB_EVENT_IMG_SAVE,
        cv_event_handler,
        NULL
    );
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Add callback failed: %d", ret);
        return;
    }
    
    // 4. 构建 prepare 数据并就绪组件
    // （prepare 数据的构建取决于具体业务需求）
    // acomp_cv_prepare(prepare);
    
    // 5. 启动 CV
    ret = acomp_cv_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "CV start failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "CV started");
    
    // 6. 配置扫描参数
    acomp_cv_set_scan_mode(0);
    acomp_cv_set_lr_mode(0);
    acomp_cv_set_screen_height(240);
    acomp_cv_set_boot_type(0);
    
    // 7. 创建图像输入流
    acomp_stream_chn_create_desc_t desc = {
        .cname = "stream.cv_image",
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = 0,
        .buffer_size = 320 * 240 * 2,
        .num_descs = 4,
        .kick_policy = 1,
    };
    acomp_cv_stream_ch_enable(0, &desc);
    
    // 8. 发送图像数据（示例）
    // send_image_to_cv(...);
    
    // 9. 运行一段时间
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    // 10. 停止 CV
    acomp_cv_stop();
    LISA_LOGI(TAG, "CV stopped");
    
    // 11. 清理资源
    acomp_cv_cleanup();
    LISA_LOGI(TAG, "CV cleaned up");
}
```

## 注意事项

1. **初始化顺序**：必须先调用 `acomp_init()` 初始化 ACOMP 框架，再调用 `acomp_cv_init()`
2. **生命周期管理**：启动前必须调用 `acomp_cv_prepare()`，停止后应调用 `acomp_cv_cleanup()` 释放资源
3. **prepare 参数**：`acomp_cv_prepare()` 需要传入 `acomp_ipc_prepare_t` 结构指针，包含算法所需资源信息
4. **状态检查**：所有 API 都会进行状态检查，确保在正确的状态下调用相应的函数
5. **事件回调**：回调函数在组件内部线程中执行，应避免长时间阻塞操作
6. **内存管理**：组件使用动态内存分配（PSRAM），确保系统有足够的堆内存
7. **资源配置**：确保配置的模型地址和大小与实际资源匹配
8. **图像流管理**：使用图像流接口时，必须成对调用 alloc/submit
9. **跨核通信**：图像流基于 IPC 机制，注意跨核数据传输的延迟和同步问题
10. **双核协作**：本组件运行在 CP 核，需要 AP 核提供算法支持，确保 AP 核固件正确烧录

## 常见问题

**Q: CV 组件初始化失败怎么办？**

A: 检查以下几点：
- 确保 ACOMP 框架已正确初始化
- 检查系统是否有足够的堆内存（PSRAM）
- 确认 AP 核固件和算法模型已正确烧录

**Q: OCR 识别没有返回结果？**

A: 检查：
- 确认已正确注册 `CV_CB_EVENT_OCR_RESULT` 事件回调
- 检查图像数据格式和尺寸是否正确
- 确认 OCR 模型资源已正确配置
- 检查图像质量（光线充足、文字清晰）

**Q: 图像拼接效果不好怎么办？**

A: 建议：
- 确保相邻帧之间有足够的重叠区域
- 检查扫描速度是否过快
- 调整扫描模式和左右模式设置
- 确保图像质量良好

**Q: 如何切换正常扫描和快扫模式？**

A: 使用 `acomp_cv_set_boot_type()` 函数：
```c
// 正常启动
acomp_cv_set_boot_type(0);

// 快扫启动
acomp_cv_set_boot_type(1);
```

## 依赖项

- `ACOMP 框架`：算法组件框架
- `ACOMP Stream IPC`：图像流跨核通信
- `lisa_log`：日志系统
- `FreeRTOS`：实时操作系统
- `CV 算法库`：计算机视觉算法引擎
- `摄像头驱动`：图像采集硬件驱动
