# 小智云端编译配置

## CMake 配置

需要在 `CMakeLists.txt` 中添加以下内容：

```cmake
# 小智云端协议
if(CONFIG_XIAOZHI_CLOUD)
    # 源文件
    list(APPEND XIAOZHI_SOURCES
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_tls.c
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_websocket.c
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_message.c
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_opus.c
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_audio.c
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_client.c
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi/xz_cloud.c
    )

    # 头文件路径
    list(APPEND XIAOZHI_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/cloud/xiaozhi
    )

    # 依赖库
    list(APPEND XIAOZHI_LIBS
        mbedtls
        mbedtls_x509
        mbedtls_ssl
        mbedtls_crypto
        opus
        cjson
    )
endif()
```

## prj.conf 配置

```
# 启用小智云端
CONFIG_XIAOZHI_CLOUD=y

# 启用 mbedTLS
CONFIG_MBEDTLS=y
CONFIG_MBEDTLS_TLS=y

# 启用 Opus
CONFIG_OPUS_DECODER=y

# 启用 cJSON
CONFIG_CJSON=y

# 网络配置
CONFIG_LWIP=y
CONFIG_WIFI=y
```

## 修改现有文件

### 1. src/app_client.c

```c
// 在文件开头添加
#ifdef XIAOZHI_CLOUD
#include "xz_cloud.h"
#endif

// 在 app_client_create 函数中
#ifdef XIAOZHI_CLOUD
    handle->cloud = xz_cloud_create(handle);
#endif

// 在 app_client_record 函数中
#ifdef XIAOZHI_CLOUD
    xz_cloud_audio(s_app_client->cloud, (const char*)s_record_rec_buf, LS_RECORD_ONE_CHNNEL_SIZE);
#endif
```

### 2. src/app_main.c (或 WiFi 事件处理文件)

```c
// WiFi 连接事件处理
#ifdef XIAOZHI_CLOUD
extern void xz_cloud_process_wifi_connected(xz_cloud_t);
#endif

void wifi_connected_handler(void) {
#ifdef LISTEN_CLOUD
    app_cloud_process_wifi_connected(client->cloud);
#elif defined(MY_CLOUD)
    jk_cloud_process_wifi_connected(client->cloud);
#elif defined(XIAOZHI_CLOUD)
    xz_cloud_process_wifi_connected(client->cloud);
#endif
}

// WiFi 断开事件处理
#ifdef XIAOZHI_CLOUD
extern void xz_cloud_process_wifi_disconnected(xz_cloud_t);
#endif

void wifi_disconnected_handler(void) {
#ifdef LISTEN_CLOUD
    app_cloud_process_wifi_disconnected(client->cloud);
#elif defined(MY_CLOUD)
    jk_cloud_process_wifi_disconnected(client->cloud);
#elif defined(XIAOZHI_CLOUD)
    xz_cloud_process_wifi_disconnected(client->cloud);
#endif
}
```

### 3. include/app_client.h

```c
// 在 cloud 成员定义处添加
#ifdef XIAOZHI_CLOUD
    xz_cloud_t cloud;
#endif
```

## 编译命令

```bash
# 清理
./build.sh -C

# 编译
./build.sh

# 或指定配置
./build.sh -t menuconfig
# 在 menuconfig 中启用 XIAOZHI_CLOUD
```

## 验证编译

```bash
# 检查符号
nm build/aiui | grep xz_

# 应该看到以下符号:
# xz_client_*
# xz_ws_*
# xz_msg_*
# xz_opus_*
# xz_audio_*
# xz_tls_*
# xz_cloud_*
```

## 常见编译问题

### 问题 1: 找不到 mbedTLS 头文件

```
fatal error: mbedtls/ssl.h: No such file or directory
```

解决方案:
```cmake
# 添加 mbedTLS 包含路径
include_directories(${ARCS_SDK_PATH}/modules/mbedtls/mbedtls-2.28.9/include)
```

### 问题 2: 找不到 opus_encoder.h

```
fatal error: opus_encoder.h: No such file or directory
```

解决方案:
```cmake
# 添加 Opus 编码器路径
include_directories(${ARCS_SDK_PATH}/modules/opusdec)
```

### 问题 3: 未定义的引用

```
undefined reference to `xz_client_create'
```

解决方案:
```cmake
# 确保源文件被添加到编译目标
target_sources(app PRIVATE ${XIAOZHI_SOURCES})
```

## 内存配置

小智云端协议的内存需求：

| 组件 | 内存 | 位置 |
|------|------|------|
| TLS 上下文 | ~50KB | SRAM |
| WebSocket 缓冲 | ~8KB | SRAM |
| Opus 编码器 | ~20KB | SRAM |
| 音频缓冲区 | ~4KB | SRAM |
| **总计** | **~82KB SRAM** + **~8MB PSRAM** |

如果内存不足，可以：
1. 减少 WS_BUFFER_SIZE
2. 使用 PSRAM 分配大缓冲区
3. 减少音频缓冲区大小

## 调试编译

```
# 启用调试符号
CONFIG_DEBUG=y
CONFIG_DEBUG_INFO=y

# 启用详细日志
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=n
```
