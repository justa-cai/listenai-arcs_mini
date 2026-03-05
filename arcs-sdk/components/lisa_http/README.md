# LISA HTTP 组件

基于 HTTP 协议的客户端组件，为 ARCS 平台提供完整的 HTTP 通信功能，支持多种请求方法、同步和异步传输、文件下载、分块传输等功能。

## 功能特性

- **HTTP 方法支持**: 支持 GET、POST、PUT、PATCH、DELETE 等标准 HTTP 方法
- **传输模式**: 支持同步传输、分块传输和文件下载多种传输模式
- **HTTPS 兼容**: 自动将 HTTPS 转换为 HTTP 处理，简化安全连接使用
- **灵活配置**: 支持自定义头部、请求体、超时时间等配置选项
- **回调机制**: 提供灵活的数据接收回调函数，支持流式数据处理
- **内存管理**: 自动内存管理，支持动态分配和释放 HTTP 实例
- **错误处理**: 完善的错误代码体系，便于问题定位和处理
- **大文件支持**: 分块传输模式支持大文件下载，单个数据块最大 4096 字节

## 配置选项

在 `prj.conf` 中启用组件：

```kconfig
CONFIG_LISA_HTTP=y     # 启用 HTTP 组件
```

## API 接口

### 生命周期管理

```c
lisa_http_t *lisa_http_init(lisa_http_request_t *req);
lisa_http_err_e lisa_http_cleanup(lisa_http_t *ins);
```

### HTTP 请求接口

```c
lisa_http_err_e lisa_http_perform(lisa_http_t *ins);
lisa_http_err_e lisa_http_download(lisa_http_t *ins);
```

### 分块传输接口

```c
lisa_http_err_e lisa_http_perform_chunked(lisa_http_t *ins);
lisa_http_err_e lisa_http_perform_chunked_with_cb(lisa_http_t *ins,
                                                 int (*on_chunk)(lisa_http_data_t *));
```

## 使用示例

### 基础 HTTP GET 请求示例

```c
#include "lisa_http.h"
#include "lisa_log.h"

// HTTP 响应数据回调函数
static void http_response_callback(lisa_http_data_t *data)
{
    if (data && data->buf && data->len > 0) {
        LISA_INFO("收到 HTTP 响应数据，长度: %d", data->len);

        // 将数据转换为字符串并输出（假设是文本数据）
        char *response = lisa_mem_alloc(data->len + 1);
        if (response) {
            memcpy(response, data->buf, data->len);
            response[data->len] = '\0';
            LISA_INFO("响应内容: %s", response);
            lisa_mem_free(response);
        }
    }
}

int basic_http_get_example(void)
{
    // 1. 配置 HTTP 请求
    lisa_http_request_t request = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)"http://httpbin.org/get",
        .headers = NULL,  // 使用默认头部
        .body = NULL,
        .body_len = 0,
        .timeout = 10000,  // 10秒超时
        .user = NULL,
        .on_data = http_response_callback,
    };

    // 2. 创建 HTTP 实例
    lisa_http_t *http = lisa_http_init(&request);
    if (!http) {
        LISA_ERR("创建 HTTP 实例失败");
        return -1;
    }

    // 3. 执行 HTTP 请求
    lisa_http_err_e result = lisa_http_perform(http);
    if (result == LISA_HTTP_OK) {
        LISA_INFO("HTTP 请求执行成功");
    } else {
        LISA_ERR("HTTP 请求失败，错误代码: %d", result);
    }

    // 4. 清理资源
    lisa_http_cleanup(http);
    return (result == LISA_HTTP_OK) ? 0 : -1;
}
```

### HTTP POST 请求示例

```c
#include "lisa_http.h"
#include "lisa_log.h"
#include "cJSON.h"

// POST 请求响应回调函数
static void post_response_callback(lisa_http_data_t *data)
{
    if (data && data->buf && data->len > 0) {
        char *response = lisa_mem_alloc(data->len + 1);
        if (response) {
            memcpy(response, data->buf, data->len);
            response[data->len] = '\0';

            LISA_INFO("POST 响应: %s", response);

            // 解析 JSON 响应（如果需要）
            cJSON *json = cJSON_Parse(response);
            if (json) {
                // 处理 JSON 数据
                LISA_INFO("JSON 解析成功");
                cJSON_Delete(json);
            }

            lisa_mem_free(response);
        }
    }
}

int http_post_example(void)
{
    // 1. 准备 POST 数据（JSON 格式）
    cJSON *json_data = cJSON_CreateObject();
    if (!json_data) {
        LISA_ERR("创建 JSON 对象失败");
        return -1;
    }

    cJSON_AddStringToObject(json_data, "name", "LISA HTTP Client");
    cJSON_AddNumberToObject(json_data, "version", 1.0);
    cJSON_AddStringToObject(json_data, "message", "Hello from LISA");

    char *json_string = cJSON_PrintUnformatted(json_data);
    cJSON_Delete(json_data);

    if (!json_string) {
        LISA_ERR("JSON 序列化失败");
        return -1;
    }

    // 2. 配置 HTTP POST 请求
    lisa_http_request_t request = {
        .method = LISA_HTTP_POST,
        .url = (uint8_t *)"http://httpbin.org/post",
        .headers = (uint8_t *)"Content-Type: application/json\r\n",  // 设置 JSON 头部
        .body = json_string,
        .body_len = strlen(json_string),
        .timeout = 15000,  // 15秒超时
        .user = NULL,
        .on_data = post_response_callback,
    };

    // 3. 创建并执行 HTTP 请求
    lisa_http_t *http = lisa_http_init(&request);
    if (!http) {
        LISA_ERR("创建 HTTP 实例失败");
        free(json_string);
        return -1;
    }

    lisa_http_err_e result = lisa_http_perform(http);

    // 4. 清理资源
    lisa_http_cleanup(http);
    free(json_string);

    return (result == LISA_HTTP_OK) ? 0 : -1;
}
```

### 文件下载示例

```c
#include "lisa_http.h"
#include "lisa_log.h"
#include "lisa_fs.h"  // 假设有文件系统接口

// 文件下载上下文
typedef struct {
    FILE *file;
    uint32_t total_size;
    uint32_t downloaded_size;
} download_context_t;

// 文件下载回调函数
static void download_callback(lisa_http_data_t *data)
{
    download_context_t *ctx = (download_context_t *)data->user;

    if (ctx && ctx->file && data && data->buf && data->len > 0) {
        // 写入文件
        size_t written = fwrite(data->buf, 1, data->len, ctx->file);
        if (written == data->len) {
            ctx->downloaded_size += written;
            LISA_INFO("下载进度: %d/%d 字节 (%.1f%%)",
                     ctx->downloaded_size, ctx->total_size,
                     (float)ctx->downloaded_size * 100.0f / ctx->total_size);
        } else {
            LISA_ERR("文件写入失败");
        }
    }
}

int file_download_example(void)
{
    // 1. 准备下载上下文
    download_context_t download_ctx = {0};
    download_ctx.file = fopen("/tmp/downloaded_file.bin", "wb");
    if (!download_ctx.file) {
        LISA_ERR("无法创建下载文件");
        return -1;
    }

    download_ctx.total_size = 0;  // 初始不知道文件大小

    // 2. 配置下载请求
    lisa_http_request_t request = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)"http://httpbin.org/bytes/1024",  // 下载 1KB 数据
        .headers = NULL,
        .body = NULL,
        .body_len = 0,
        .timeout = 30000,  // 30秒超时
        .user = &download_ctx,
        .on_data = download_callback,
    };

    // 3. 执行下载
    lisa_http_t *http = lisa_http_init(&request);
    if (!http) {
        LISA_ERR("创建 HTTP 实例失败");
        fclose(download_ctx.file);
        return -1;
    }

    lisa_http_err_e result = lisa_http_download(http);

    // 4. 清理资源
    fclose(download_ctx.file);
    lisa_http_cleanup(http);

    if (result == LISA_HTTP_OK) {
        LISA_INFO("文件下载完成，总大小: %d 字节", download_ctx.downloaded_size);
    } else {
        LISA_ERR("文件下载失败，错误代码: %d", result);
    }

    return (result == LISA_HTTP_OK) ? 0 : -1;
}
```

### 分块传输示例

```c
#include "lisa_http.h"
#include "lisa_log.h"

// 分块数据处理回调函数
static int chunk_callback(lisa_http_data_t *data)
{
    if (data && data->buf && data->len > 0) {
        LISA_INFO("收到数据块，大小: %d 字节", data->len);

        // 处理数据块（这里简单计算校验和）
        uint8_t checksum = 0;
        uint8_t *chunk_data = (uint8_t *)data->buf;
        for (int i = 0; i < data->len; i++) {
            checksum ^= chunk_data[i];
        }

        LISA_INFO("数据块校验和: 0x%02X", checksum);

        // 检查是否需要继续接收数据
        // 返回 0 继续，返回非 0 停止传输
        static int chunk_count = 0;
        chunk_count++;

        if (chunk_count >= 5) {
            LISA_INFO("接收到 5 个数据块，停止传输");
            return -1;  // 停止传输
        }
    }

    return 0;  // 继续接收数据
}

int chunked_transfer_example(void)
{
    // 1. 配置分块传输请求
    lisa_http_request_t request = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)"http://httpbin.org/stream/20",  // 流式数据接口
        .headers = NULL,
        .body = NULL,
        .body_len = 0,
        .timeout = 20000,  // 20秒超时
        .user = NULL,
        .on_data = NULL,  // 使用自定义回调，不需要这个
    };

    // 2. 创建 HTTP 实例
    lisa_http_t *http = lisa_http_init(&request);
    if (!http) {
        LISA_ERR("创建 HTTP 实例失败");
        return -1;
    }

    // 3. 执行分块传输（使用自定义回调）
    lisa_http_err_e result = lisa_http_perform_chunked_with_cb(http, chunk_callback);

    // 4. 清理资源
    lisa_http_cleanup(http);

    if (result == LISA_HTTP_OK) {
        LISA_INFO("分块传输完成");
    } else {
        LISA_ERR("分块传输失败，错误代码: %d", result);
    }

    return (result == LISA_HTTP_OK) ? 0 : -1;
}
```

### 多请求管理示例

```c
#include "lisa_http.h"
#include "lisa_log.h"

// 请求管理结构
typedef struct {
    lisa_http_t *http;
    char *name;
    bool completed;
    lisa_http_err_e result;
} http_request_t;

// 统一的响应处理函数
static void unified_response_handler(lisa_http_data_t *data)
{
    // 从 user_data 中获取请求管理结构
    http_request_t *req_info = (http_request_t *)data->user;

    if (req_info) {
        req_info->completed = true;

        if (data && data->buf && data->len > 0) {
            LISA_INFO("请求 '%s' 收到响应，长度: %d", req_info->name, data->len);
        } else {
            LISA_INFO("请求 '%s' 完成，无响应数据", req_info->name);
        }
    }
}

int multi_request_example(void)
{
    // 1. 定义多个请求
    http_request_t requests[3];
    const char *urls[] = {
        "http://httpbin.org/get",
        "http://httpbin.org/uuid",
        "http://httpbin.org/ip"
    };
    const char *names[] = {"GET请求", "UUID请求", "IP请求"};

    // 2. 创建并配置多个 HTTP 实例
    for (int i = 0; i < 3; i++) {
        lisa_http_request_t request = {
            .method = LISA_HTTP_GET,
            .url = (uint8_t *)urls[i],
            .headers = NULL,
            .body = NULL,
            .body_len = 0,
            .timeout = 10000,
            .user = &requests[i],
            .on_data = unified_response_handler,
        };

        requests[i].http = lisa_http_init(&request);
        requests[i].name = (char *)names[i];
        requests[i].completed = false;
        requests[i].result = LISA_HTTP_COMMON_ERR;

        if (!requests[i].http) {
            LISA_ERR("创建请求 '%s' 失败", names[i]);
        }
    }

    // 3. 执行所有请求
    for (int i = 0; i < 3; i++) {
        if (requests[i].http) {
            requests[i].result = lisa_http_perform(requests[i].http);
            LISA_INFO("请求 '%s' 执行结果: %d", names[i], requests[i].result);
        }
    }

    // 4. 等待所有请求完成（在实际应用中可能需要更复杂的同步机制）
    for (int i = 0; i < 3; i++) {
        if (requests[i].completed) {
            LISA_INFO("请求 '%s' 已完成", names[i]);
        }
    }

    // 5. 清理所有资源
    for (int i = 0; i < 3; i++) {
        if (requests[i].http) {
            lisa_http_cleanup(requests[i].http);
        }
    }

    return 0;
}
```

## HTTP 方法说明

### 支持的 HTTP 方法

| 方法 | 用途 | 请求体 | 适用场景 |
|------|------|--------|---------|
| `LISA_HTTP_GET` | 获取资源 | 不支持 | 数据查询、信息获取 |
| `LISA_HTTP_POST` | 提交数据 | 支持 | 数据创建、表单提交 |
| `LISA_HTTP_PUT` | 更新资源 | 支持 | 完整资源更新 |
| `LISA_HTTP_PATCH` | 部分更新 | 支持 | 部分资源更新 |
| `LISA_HTTP_DELETE` | 删除资源 | 不支持 | 资源删除 |

### 传输模式对比

| 模式 | 同步性 | 适用场景 | 特点 |
|------|--------|---------|------|
| `lisa_http_perform()` | 同步 | 小数据量、简单请求 | 简单易用，等待完整响应 |
| `lisa_http_download()` | 同步 | 文件下载 | 优化的文件传输 |
| `lisa_http_perform_chunked()` | 异步 | 大数据流处理 | 内部回调，分块处理 |
| `lisa_http_perform_chunked_with_cb()` | 异步 | 自定义数据处理 | 用户控制数据处理流程 |

## 错误代码说明

| 错误代码 | 说明 | 可能原因 | 解决方法 |
|---------|------|---------|---------|
| `LISA_HTTP_OK` | 操作成功 | - | - |
| `LISA_HTTP_COMMON_ERR` | 一般错误 | 参数错误、内存不足 | 检查参数和内存状态 |
| `LISA_HTTP_PARAM_ERROR` | 参数错误 | URL 为空、回调函数为 NULL | 检查请求配置 |
| `LISA_HTTP_NET_ERR` | 网络错误 | 连接失败、超时 | 检查网络连接和超时设置 |

## 使用注意事项

1. **参数验证**: 创建 HTTP 实例前必须验证 URL 和回调函数不为 NULL
2. **内存管理**: 请求体数据在请求执行期间必须保持有效
3. **HTTPS 处理**: 组件会自动将 HTTPS URL 转换为 HTTP 处理
4. **超时设置**: 根据网络环境和数据大小合理设置超时时间
5. **回调函数**: 回调函数应尽量简短，避免阻塞网络处理线程
6. **分块大小**: 分块传输时单个数据块最大支持 4096 字节
7. **资源清理**: 请求完成后必须调用 `lisa_http_cleanup()` 释放资源
8. **错误处理**: 所有 API 调用都应检查返回值并处理错误情况
9. **URL 长度**: URL 最大长度为 1023 字符（包含结束符）
10. **请求体大小**: 请求体最大长度为 1024 字节
11. **线程安全**: 每个 HTTP 实例独立工作，但多线程使用时需要注意资源同步
12. **文件操作**: 文件下载时需要确保文件系统可用且有写入权限

## 文件说明

- `lisa_http.h` - HTTP 组件头文件，包含 API 接口和数据结构定义
- `lisa_http.c` - HTTP 组件实现文件，基于 HTTPCUsr_api 实现
- `Kconfig` - 组件配置选项定义文件
- `CMakeLists.txt` - 组件构建配置文件