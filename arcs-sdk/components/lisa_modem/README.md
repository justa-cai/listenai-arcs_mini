# LISA Modem

`lisa_modem` 为 ARCS 提供基于 AT modem 的蜂窝网络接入能力。当前默认支持 `ML307`，并可按配置启用 `EC801E` runtime。

## 当前架构

组件内部现在是三层轻量结构：

1. `lisa_modem_module`
   负责 modem 创建、驱动探测、默认实例管理，以及 socket 风格 API。
   同时持有轻量 socket record，包括 public sockfd、driver endpoint id、超时、TLS 使能和 cached peer。
2. `drivers/<modem>`
   每个 modem 自己维护真实 runtime 状态和 AT 事务，例如连接、收发、URC、预取任务、DNS。
3. `drivers/common`
   只放小型共用 helper，目前包括 `recv_ring` 轮询读取逻辑和共享网络状态枚举。

`modem_core`、`bearer_manager`、`tls_profile_manager` 已移除，不再保留单纯为转发存在的中间层。

## 默认驱动

默认配置下通常启用：

- `CONFIG_LISA_MODEM_DRIVER_ML307=y`

可选启用：

- `CONFIG_LISA_MODEM_DRIVER_EC801E=y`

## 配置

在 `prj.conf` 中启用：

```kconfig
CONFIG_LISA_MODEM=y
CONFIG_LISA_MODEM_DRIVER_ML307=y
```

如果需要通过 `lisa_net` 暴露 socket/netdev：

```kconfig
CONFIG_LISA_NET=y
CONFIG_SAL_USING_POSIX=y
```

## 公开接口

头文件：

```c
#include "lisa_modem_module.h"
```

核心生命周期接口：

```c
bool lisa_modem_module_init(const char *uart_dev);
bool lisa_modem_module_deinit(void);

lisa_modem_t *lisa_modem_create_uart(const char *uart_dev, uint32_t baudrate);
void lisa_modem_destroy(lisa_modem_t *modem);
```

主推 socket 风格接口：

```c
bool lisa_modem_dns_resolve(const char *domain, char *ip_addr, size_t size);

int  lisa_modem_socket_open(int domain, int type, int protocol);
bool lisa_modem_socket_connect(int sockfd, const struct sockaddr *addr, int addrlen);
int  lisa_modem_socket_close(int sockfd);
int  lisa_modem_socket_send(int sockfd, const void *data, size_t length, uint32_t timeout_ms);
int  lisa_modem_socket_sendto(int sockfd, const void *data, size_t length, int flags,
                              const struct sockaddr *dest_addr, int addrlen, uint32_t timeout_ms);
int  lisa_modem_socket_recv(int sockfd, void *buffer, size_t length, uint32_t timeout_ms);
int  lisa_modem_socket_recvfrom(int sockfd, void *buffer, size_t length, int flags,
                                struct sockaddr *src_addr, int *addrlen, uint32_t timeout_ms);

int lisa_modem_setsockopt(int sockfd, int level, int optname, const void *optval, int optlen);
int lisa_modem_ioctlsocket(int sockfd, long cmd, void *arg);
int lisa_modem_getpeername(int sockfd, struct sockaddr *addr, int *addrlen);
```

如果需要显式操作某个 modem 实例，而不是默认实例，可使用对应的 `*_on` 接口。

## 兼容接口

`lisa_modem_tcp_*` / `lisa_modem_udp_*` 旧接口仍然保留，但现在只是对 socket API 的兼容薄壳，主要用于历史调用点平滑迁移。

新的代码优先使用 `lisa_modem_socket_*`。

## Socket 语义

- public sockfd 与 driver endpoint id 已解耦，`lisa_modem_module` 负责映射。
- `setsockopt()` 当前只支持 `SOL_SOCKET` 下的 `SO_SNDTIMEO`、`SO_RCVTIMEO` 和 `SO_SSL_CONFIG`。
- `ioctlsocket()` 当前只支持 `FIONBIO`。
- `recv()` 只消费 driver 本地 ring buffer；实际 modem RX pull 由 dispatcher 调度。
- socket `SO_RCVTIMEO` / per-call `timeout_ms` 只控制当前 socket 等待语义，不再直接放大后台 RX pull 的 AT 事务超时。
- UDP 显式 `connect()` 会记住默认 peer，后续 `send()` 走该 peer。
- UDP `sendto()` 不会把最近一次目标地址当成长期 peer。
- `getpeername()` 返回模块层缓存的 peer 地址。
- TCP `recvfrom()` 在 driver 未上报源地址时，会回退到缓存 peer。

## 已验证行为

本轮重构至少回归了以下路径：

- TCP connect / send / recv / close
- UDP sendto / recvfrom
- `recvfrom src=` 来源地址打印
- `SO_RCVTIMEO`
- `SO_SNDTIMEO`
- `SO_SSL_CONFIG`
- `FIONBIO`
- dual-board sample build: `arcs_evb` / `arcs_mini`
- host regression: public sockfd 映射、sockaddr 转换、peer cache、wrapper guard

## 调试

AT 收发日志可通过 `at_client_set_debug()` 打开：

```c
at_client_set_debug(lisa_modem_get_client(lisa_modem_get_default()), true);
```

## 示例

参考：

- `samples/network/modem`

该示例覆盖：

- modem 初始化
- DNS 解析
- TCP HTTP 请求
- UDP NTP 请求
