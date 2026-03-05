# LISA SNTP 组件

基于简单网络时间协议（SNTP）的时间同步客户端组件，为 ARCS 平台提供精确的网络时间获取功能，支持多服务器查询和高精度时间同步。

## 功能特性

- **SNTP 协议支持**: 完整支持 SNTP v4 协议，兼容标准 NTP 服务器
- **多服务器支持**: 支持配置多个 SNTP 服务器，提高时间同步可靠性
- **高精度时间**: 提供纳秒级精度的时间信息，满足高精度应用需求
- **自动容错**: 支持服务器自动切换，单个服务器故障时自动尝试备用服务器
- **DNS 解析**: 内置 DNS 解析功能，支持域名和 IP 地址两种服务器地址格式
- **超时控制**: 可配置的查询超时时间，适应不同网络环境
- **轻量级实现**: 基于 core_sntp_client 库，代码精简，资源占用低
- **时间转换**: 提供 Unix 时间戳格式，便于应用层处理和显示

## 配置选项

在 `prj.conf` 中启用组件：

```kconfig
CONFIG_LISA_SNTP=y     # 启用 SNTP 组件
```

## API 接口

### 时间同步接口

```c
int lisa_sntp_query(const char *servers[], int servers_cnt,
                   uint32_t timeout, struct lisa_sntp_time *time);
```

### 时间数据结构

```c
struct lisa_sntp_time {
    uint64_t sec;   // 自 1970-01-01 00:00:00 UTC 以来的秒数
    uint32_t nsec;  // 纳秒部分，范围 0-999999999
};
```

## 使用示例

### 基础时间同步示例

```c
#include "lisa_sntp.h"
#include "lisa_log.h"
#include "lisa_time.h"
#include <stdio.h>
#include <time.h>

int basic_sntp_example(void)
{
    // 1. 定义 SNTP 服务器列表
    const char *ntp_servers[] = {
        "ntp1.aliyun.com",     // 阿里云 NTP 服务器
        "ntp2.aliyun.com",     // 阿里云备用 NTP 服务器
        "ntp3.aliyun.com",     // 阿里云第三 NTP 服务器
        "pool.ntp.org",        // 全球 NTP 池
        "cn.pool.ntp.org"      // 中国 NTP 池
    };

    // 2. 定义时间结构体
    struct lisa_sntp_time network_time;

    // 3. 执行时间查询
    int result = lisa_sntp_query(ntp_servers, 5, 5000, &network_time);

    if (result == 0) {
        LISA_INFO("时间同步成功");

        // 4. 转换为可读格式
        time_t timestamp = (time_t)network_time.sec;
        struct tm *local_time = localtime(&timestamp);

        if (local_time) {
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

            LISA_INFO("网络时间: %s.%03u UTC", time_str, network_time.nsec / 1000000);
            printf("获取到的网络时间: %s.%03u\n", time_str, network_time.nsec / 1000000);
        }

        // 5. 显示原始时间戳
        LISA_INFO("时间戳: %llu 秒 %u 纳秒",
                 (unsigned long long)network_time.sec, network_time.nsec);

    } else {
        LISA_ERR("时间同步失败，错误代码: %d", result);
        return -1;
    }

    return 0;
}
```

### 自动时间同步服务示例

```c
#include "lisa_sntp.h"
#include "lisa_log.h"
#include "lisa_time.h"
#include "lisa_thread.h"
#include <stdio.h>
#include <stdbool.h>

// 时间同步服务配置
typedef struct {
    const char **servers;
    int server_count;
    uint32_t sync_interval;  // 同步间隔（秒）
    uint32_t timeout;        // 查询超时（毫秒）
    bool running;
    lisa_thread_t *sync_thread;
} sntp_sync_service_t;

// 预定义的可靠 NTP 服务器
static const char *reliable_ntp_servers[] = {
    "ntp1.aliyun.com",
    "ntp2.aliyun.com",
    "ntp3.aliyun.com",
    "ntp.ntsc.ac.cn",
    "cn.pool.ntp.org",
    "ntp.tencent.com"      // 腾讯云 NTP 服务器
};

// 时间同步线程函数
static void sntp_sync_thread(void *arg)
{
    sntp_sync_service_t *service = (sntp_sync_service_t *)arg;
    struct lisa_sntp_time network_time;

    LISA_INFO("SNTP 时间同步服务启动");

    while (service->running) {
        LISA_INFO("开始时间同步...");

        // 执行时间查询
        int result = lisa_sntp_query(service->servers, service->server_count,
                                   service->timeout, &network_time);

        if (result == 0) {
            // 转换为可读时间
            time_t timestamp = (time_t)network_time.sec;
            struct tm *local_time = localtime(&timestamp);

            if (local_time) {
                char time_str[64];
                strftime(time_str, sizeof(time_str),
                        "%Y-%m-%d %H:%M:%S", local_time);

                LISA_INFO("时间同步成功: %s.%03u",
                         time_str, network_time.nsec / 1000000);
                printf("[%s] 同步时间: %s.%03u UTC\n",
                       __func__, time_str, network_time.nsec / 1000000);
            }

            // 在实际应用中，这里可以调用系统时间设置函数
            // set_system_time(&network_time);

        } else {
            LISA_ERR("时间同步失败，错误代码: %d", result);
        }

        // 等待下次同步
        for (uint32_t i = 0; i < service->sync_interval && service->running; i++) {
            lisa_thread_delay(1000);  // 延迟 1 秒
        }
    }

    LISA_INFO("SNTP 时间同步服务停止");
}

// 启动时间同步服务
static sntp_sync_service_t* start_sntp_service(void)
{
    sntp_sync_service_t *service = lisa_mem_alloc(sizeof(sntp_sync_service_t));
    if (!service) {
        LISA_ERR("分配服务结构失败");
        return NULL;
    }

    // 配置服务参数
    service->servers = reliable_ntp_servers;
    service->server_count = sizeof(reliable_ntp_servers) / sizeof(reliable_ntp_servers[0]);
    service->sync_interval = 3600;  // 1小时同步一次
    service->timeout = 5000;        // 5秒超时
    service->running = true;

    // 创建同步线程
    service->sync_thread = lisa_thread_create("sntp_sync",
                                             sntp_sync_thread,
                                             service,
                                             LISA_THREAD_DEFAULT_STACK_SIZE,
                                             LISA_THREAD_DEFAULT_PRIORITY);

    if (!service->sync_thread) {
        LISA_ERR("创建同步线程失败");
        lisa_mem_free(service);
        return NULL;
    }

    LISA_INFO("SNTP 时间同步服务启动成功，同步间隔: %d 秒", service->sync_interval);
    return service;
}

// 停止时间同步服务
static void stop_sntp_service(sntp_sync_service_t *service)
{
    if (service) {
        service->running = false;

        if (service->sync_thread) {
            lisa_thread_join(service->sync_thread, LISA_THREAD_WAIT_FOREVER);
            lisa_thread_delete(service->sync_thread);
        }

        lisa_mem_free(service);
        LISA_INFO("SNTP 时间同步服务已停止");
    }
}

int auto_sync_service_example(void)
{
    LISA_INFO("启动自动时间同步服务示例");

    // 启动服务
    sntp_sync_service_t *service = start_sntp_service();
    if (!service) {
        return -1;
    }

    // 运行一段时间（模拟应用运行）
    LISA_INFO("服务运行中，将持续同步时间...");
    lisa_thread_delay(10000);  // 运行 10 秒

    // 停止服务
    stop_sntp_service(service);

    return 0;
}
```

### 多服务器容错示例

```c
#include "lisa_sntp.h"
#include "lisa_log.h"
#include <stdio.h>

// 测试不同服务器的响应情况
int server_reliability_test(void)
{
    // 1. 定义不同地区的服务器列表
    const char *global_servers[] = {
        "pool.ntp.org",          // 全球 NTP 池
        "time.nist.gov",         // 美国国家标准技术研究院
        "ntp1.aliyun.com",       // 阿里云
        "ntp.tencent.com",       // 腾讯云
        "ntp.ntsc.ac.cn",        // 中国科学院国家授时中心
        "time.apple.com"         // 苹果时间服务器
    };

    const char *invalid_servers[] = {
        "invalid.server.com",    // 无效域名
        "192.168.1.999",         // 无效IP
        "ntp.nonexistent.org"    // 不存在的服务器
    };

    struct lisa_sntp_time test_time;

    LISA_INFO("开始服务器可靠性测试...");

    // 2. 测试有效服务器
    LISA_INFO("测试有效 NTP 服务器:");
    for (int i = 0; i < sizeof(global_servers) / sizeof(global_servers[0]); i++) {
        LISA_INFO("测试服务器: %s", global_servers[i]);

        int result = lisa_sntp_query(&global_servers[i], 1, 3000, &test_time);

        if (result == 0) {
            time_t timestamp = (time_t)test_time.sec;
            struct tm *local_time = localtime(&timestamp);

            if (local_time) {
                char time_str[64];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);
                printf("  ✓ 服务器 %s 响应正常: %s.%03u\n",
                       global_servers[i], time_str, test_time.nsec / 1000000);
            }
        } else {
            printf("  ✗ 服务器 %s 响应失败，错误代码: %d\n",
                   global_servers[i], result);
        }
    }

    // 3. 测试无效服务器（应该失败）
    LISA_INFO("测试无效服务器（预期失败）:");
    for (int i = 0; i < sizeof(invalid_servers) / sizeof(invalid_servers[0]); i++) {
        LISA_INFO("测试服务器: %s", invalid_servers[i]);

        int result = lisa_sntp_query(&invalid_servers[i], 1, 1000, &test_time);

        if (result != 0) {
            printf("  ✓ 预期失败: 服务器 %s 无效，错误代码: %d\n",
                   invalid_servers[i], result);
        } else {
            printf("  ? 意外成功: 服务器 %s 本应无效但返回了时间\n",
                   invalid_servers[i]);
        }
    }

    // 4. 测试多服务器容错
    LISA_INFO("测试多服务器容错机制:");
    const char *mixed_servers[] = {
        "invalid.server.com",   // 第一个无效
        "ntp1.aliyun.com",      // 第二个有效
        "invalid2.server.com",  // 第三个无效
        "ntp.tencent.com"       // 第四个有效
    };

    int result = lisa_sntp_query(mixed_servers, 4, 8000, &test_time);

    if (result == 0) {
        time_t timestamp = (time_t)test_time.sec;
        struct tm *local_time = localtime(&timestamp);

        if (local_time) {
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);
            printf("  ✓ 容错机制正常: 从有效服务器获取时间 %s.%03u\n",
                   time_str, test_time.nsec / 1000000);
        }
    } else {
        printf("  ✗ 容错机制异常: 所有服务器都失败\n");
    }

    return 0;
}
```

## 常用 SNTP 服务器

### 国内服务器

| 服务器地址 | 提供商 | 特点 |
|-----------|--------|------|
| `ntp1.aliyun.com` | 阿里云 | 稳定可靠，国内访问快 |
| `ntp2.aliyun.com` | 阿里云 | 阿里云备用服务器 |
| `ntp3.aliyun.com` | 阿里云 | 阿里云第三服务器 |
| `ntp.tencent.com` | 腾讯云 | 腾讯云提供的服务器 |
| `ntp.ntsc.ac.cn` | 中科院 | 国家授时中心，权威性高 |
| `cn.pool.ntp.org` | NTP Pool | 中国区域服务器池 |

### 国际服务器

| 服务器地址 | 提供商 | 特点 |
|-----------|--------|------|
| `pool.ntp.org` | NTP Pool | 全球服务器池，自动选择 |
| `time.nist.gov` | NIST | 美国国家标准技术研究院 |
| `time.apple.com` | Apple | 苹果公司时间服务器 |
| `time.cloudflare.com` | Cloudflare | CDN 提供商，全球分布 |

## 返回值说明

| 返回值 | 说明 | 处理建议 |
|--------|------|---------|
| 0 | 成功 | 正常处理获取到的时间 |
| >0 | 警告 | 可能时间精度受影响，但仍然可用 |
| <0 | 错误 | 时间同步失败，需要重试或更换服务器 |

## 使用注意事项

1. **网络连接**: 确保设备能够访问互联网或指定的 NTP 服务器
2. **防火墙设置**: 确保防火墙允许 UDP 123 端口的出站连接
3. **服务器选择**: 优先选择地理位置较近的服务器以提高响应速度
4. **超时设置**: 根据网络环境合理设置超时时间，建议 3-10 秒
5. **服务器数量**: 建议配置 2-5 个服务器以提高可靠性
6. **同步频率**: 避免过于频繁的时间同步，建议至少间隔 1 分钟
7. **时间精度**: SNTP 提供纳秒级精度，但实际精度受网络延迟影响
8. **DNS 解析**: 支持域名和 IP 地址，域名需要 DNS 解析能力
9. **错误处理**: 必须检查返回值并处理失败情况
10. **内存管理**: `lisa_sntp_time` 结构由调用者分配和管理
11. **线程安全**: 函数是线程安全的，可以在多线程环境中调用
12. **时区转换**: 返回的时间为 UTC 时间，需要根据应用需求进行时区转换

## 文件说明

- `lisa_sntp.h` - SNTP 组件头文件，包含 API 接口和数据结构定义
- `lisa_sntp.c` - SNTP 组件实现文件，基于 core_sntp_client 库
- `Kconfig` - 组件配置选项定义文件
- `CMakeLists.txt` - 组件构建配置文件