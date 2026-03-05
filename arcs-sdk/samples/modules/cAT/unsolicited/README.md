# cAT AT 命令解析器无端请求示例

## 功能说明

演示 cAT 库的无端请求（Unsolicited Response）功能，即服务器主动向客户端发送未经请求的响应，用于异步事件通知和多条响应发送。

本示例模拟无线扫描操作，接收单个 `AT+START` 命令后，系统返回多条扫描结果，最后发送 OK。

## 硬件连接

- **UART1_TX (PB2)**: 传输数据，连接到 USB-UART 转接器的 RX
- **UART1_RX (PB3)**: 接收数据，连接到 USB-UART 转接器的 TX

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 示例内容

本示例演示以下功能：

1. **HOLD 状态返回**: 命令处理器返回 `CAT_RETURN_STATE_HOLD`，暂时不发送 OK
2. **多条异步响应**: 通过 `cat_trigger_unsolicited_read()` 主动发送多条无端响应
3. **异步事件处理**: 模拟后台扫描操作，逐条返回扫描结果
4. **HOLD 状态退出**: 扫描完成后调用 `cat_hold_exit()` 发送 OK 并退出 HOLD 状态
5. **模式切换**: 支持 WiFi (0) 和蓝牙 (1) 两种扫描模式

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb
```

## 预期输出

**系统启动输出：**

```
[I][cat_unsolicited] cAT AT Command Parser - Unsolicited Example
[I][cat_unsolicited] ============================================
[I][cat_uart] cAT UART adapter initialized on uart1 @ 115200 baud
[I][cat_unsolicited] AT command parser ready on uart1
[I][cat_unsolicited] Supported AT commands:
[I][cat_unsolicited]   +START - Start scanning (0=WiFi, 1=Bluetooth)
[I][cat_unsolicited]   +SCAN - Scan result record (unsolicited)
[I][cat_unsolicited]   #HELP - Print command list
[I][cat_unsolicited]   #QUIT - Quit the demo
[I][cat_uart] cAT UART task started
```

**发送 `AT+START=0` (WiFi 扫描) 后的输出：**

```
[I][cat_unsolicited] Mode set to: 0 (WiFi)
[I][cat_unsolicited] Starting WiFi scan...
[I][cat_unsolicited] Sending scan result 0: RSSI=-10, SSID=wifi1
[I][cat_unsolicited] Sending scan result 1: RSSI=-50, SSID=wifi2
[I][cat_unsolicited] Sending scan result 2: RSSI=-20, SSID=wifi3
[I][cat_unsolicited] Scan complete, sent 3 results
+SCAN: -10,"wifi1"
+SCAN: -50,"wifi2"
+SCAN: -20,"wifi3"
OK
```

**发送 `AT+START=1` (蓝牙扫描) 后的输出：**

```
[I][cat_unsolicited] Mode set to: 1 (Bluetooth)
[I][cat_unsolicited] Starting Bluetooth scan...
[I][cat_unsolicited] Sending scan result 0: RSSI=-20, SSID=bluetooth1
[I][cat_unsolicited] Scan complete, sent 1 results
+SCAN: -20,"bluetooth1"
OK
```

**发送 `AT+HELP` 后的输出：**

```
+HELP: +START(WR),+SCAN(TEST),#HELP(RUN),#QUIT(RUN)
OK
```

## 核心 API

| API | 说明 |
|-----|------|
| `cat_uart_adapter_init()` | 初始化 UART 适配器 |
| `cat_uart_adapter_get_io()` | 获取 IO 接口 |
| `cat_uart_adapter_get_mutex()` | 获取互斥锁接口（支持递归锁） |
| `cat_init()` | 初始化 AT 命令解析器 |
| `cat_trigger_unsolicited_read()` | 触发无端响应 |
| `cat_hold_exit()` | 退出 HOLD 状态 |
| `cat_uart_adapter_send_urc()` | 发送无端响应（备选） |

## 关键代码

**定义扫描结果数据：**

```c
/* 静态扫描结果 - 模拟 WiFi 和蓝牙 */
struct scan_results {
    int rssi;
    char ssid[16];
};

static const struct scan_results results[2][3] = {
    /* WiFi 结果 */
    {
        { .rssi = -10, .ssid = "wifi1" },
        { .rssi = -50, .ssid = "wifi2" },
        { .rssi = -20, .ssid = "wifi3" }
    },
    /* 蓝牙结果 */
    {
        { .rssi = -20, .ssid = "bluetooth1" },
        { .rssi = 0, .ssid = "" },
        { .rssi = 0, .ssid = "" }
    }
};
```

**START 命令处理器（返回 HOLD 状态）：**

```c
/* AT+START=<mode> - 启动扫描 */
static cat_return_state start_write(const struct cat_command *cmd, const uint8_t *data,
                                     const size_t data_size, const size_t args_num)
{
    LOGI("Starting %s scan...", (mode == 0) ? "WiFi" : "Bluetooth");

    /* 重置扫描状态 */
    scan_index = 0;
    scan_completed = false;

    /* 加载第一条扫描结果 */
    load_scan_results(scan_index);

    /* 触发第一条无端响应 */
    cat_trigger_unsolicited_read(&at, &scan_cmd);

    /* 返回 HOLD - 暂不发送 OK，等待异步操作完成 */
    return CAT_RETURN_STATE_HOLD;
}
```

**SCAN 命令读取处理器（处理无端响应）：**

```c
/* 无端读取回调 - 每条扫描结果调用一次 */
static cat_return_state scan_read(const struct cat_command *cmd, uint8_t *data,
                                   size_t *data_size, const size_t max_data_size)
{
    int max = (mode == 0) ? 3 : 1;

    if (scan_completed) {
        return CAT_RETURN_STATE_OK;
    }

    LOGI("Sending scan result %d: RSSI=%d, SSID=%s", scan_index, rssi, ssid);

    scan_index++;

    /* 如果还有更多结果，触发下一条 */
    if (scan_index < max) {
        load_scan_results(scan_index);
        cat_trigger_unsolicited_read(&at, &scan_cmd);
        return CAT_RETURN_STATE_DATA_NEXT;  /* 继续下一条 */
    }

    /* 最后一条结果 - 完成后退出 HOLD 状态 */
    LOGI("Scan complete, sent %d results", scan_index);
    scan_completed = true;
    
    cat_hold_exit(&at, CAT_STATUS_OK);     /* 发送 OK 并退出 HOLD */
    return CAT_RETURN_STATE_DATA_OK;       /* 最后一条数据 */
}
```

**模式验证器：**

```c
/* MODE 变量的写入验证器 */
static int mode_write(const struct cat_variable *var, const size_t write_size)
{
    if (*(int *)var->data >= 2) {
        LOGE("Invalid mode: %d (must be 0 or 1)", *(int *)var->data);
        return -1;
    }
    LOGI("Mode set to: %d (%s)", *(int *)var->data,
         (*(int *)var->data == 0) ? "WiFi" : "Bluetooth");
    return 0;
}
```

**SCAN 命令定义：**

```c
/* 定义扫描结果变量 */
static struct cat_variable scan_vars[] = {
    {
        .type = CAT_VAR_INT_DEC,
        .data = &rssi,
        .data_size = sizeof(rssi),
        .name = "RSSI",
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = ssid,
        .data_size = sizeof(ssid),
        .name = "SSID",
        .access = CAT_VAR_ACCESS_READ_ONLY,
    }
};

/* SCAN 命令 - 仅用于无端响应，标记为 only_test */
static struct cat_command scan_cmd = {
    .name = "+SCAN",
    .description = "Scan result record",
    .read = scan_read,
    .var = scan_vars,
    .var_num = sizeof(scan_vars) / sizeof(scan_vars[0])
};
```

## 注意事项

1. **递归 Mutex**: 示例使用递归互斥锁支持无端事件的触发。在 HOLD 状态的命令处理中调用 `cat_trigger_unsolicited_read()` 时，需要重新获取锁

2. **HOLD 状态管理**:
   - 返回 `CAT_RETURN_STATE_HOLD` 使命令进入 HOLD 状态，暂停发送 OK
   - 返回 `CAT_RETURN_STATE_DATA_NEXT` 继续发送下一条响应
   - 返回 `CAT_RETURN_STATE_DATA_OK` 发送最后一条响应
   - 调用 `cat_hold_exit()` 退出 HOLD 状态并发送 OK

3. **无端响应流程**:
   ```
   1. 用户发送 AT+START=0
   2. start_write() 返回 HOLD，调用 cat_trigger_unsolicited_read()
   3. scan_read() 被调用发送第一条 +SCAN 结果
   4. 返回 DATA_NEXT，再次调用 cat_trigger_unsolicited_read()
   5. 重复直到最后一条结果
   6. 最后一条返回 DATA_OK 并调用 cat_hold_exit()
   7. 发送 OK 并退出 HOLD 状态
   ```

4. **线程安全**: UART 任务中所有 cAT 操作都受递归互斥锁保护，可安全调用 `cat_trigger_unsolicited_read()`

5. **响应变量**: 无端响应中的变量值必须在回调被调用前设置正确（如 `rssi` 和 `ssid`）

6. **错误处理**: 如果参数验证失败（mode >= 2），回调返回 -1 会导致命令返回 ERROR

## 命令参考

| 命令 | 说明 | 示例 |
|------|------|------|
| `AT+START=<mode>` | 开始扫描 (0=WiFi, 1=Bluetooth) | `AT+START=0` → 多条 +SCAN + OK |
| `AT+SCAN=?` | 显示 SCAN 格式 | `AT+SCAN=?` → `+SCAN: RSSI(INT),SSID(STRING)` |
| `AT#HELP` | 列出所有命令 | `AT#HELP` → 命令列表 |
| `AT#QUIT` | 退出示例 | `AT#QUIT` → `OK` |

## 扩展建议

1. **实时扫描**: 替换静态结果数据，接收来自实际 WiFi/蓝牙 驱动的扫描结果

2. **扫描进度**: 返回 DATA_NEXT 时可添加进度信息（如 "Scan 30% complete"）

3. **超时处理**: 在 HOLD 状态下实现扫描超时机制，防止命令无限期挂起

4. **并发操作**: 支持在扫描过程中处理其他 AT 命令（使用任务安全机制）

5. **结果缓存**: 存储扫描结果供后续查询，实现增量式结果返回

## 无端响应原理

无端响应是一种经典的 AT 命令扩展模式，允许设备在不进行直接查询的情况下主动通知主机事件发生。

**典型应用场景：**
- 信号强度实时通知 (Signal Strength Unsolicited Result Code)
- 来电提示 (Incoming Call Notification)
- 短信接收通知 (SMS Reception Notification)
- 网络状态变化 (Network Status Change)
- 外设事件通知 (Peripheral Events)

本示例通过模拟扫描结果演示了这一模式的完整实现。
