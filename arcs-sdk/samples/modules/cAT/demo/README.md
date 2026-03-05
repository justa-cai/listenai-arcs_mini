# cAT AT 命令解析器高级示例

## 功能说明

演示 cAT 库的高级功能，包括多种变量类型、变量写入回调通知，以及复杂的命令处理。

本示例展示如何处理不同的数据类型（整数、十六进制地址、字符串、二进制数据）以及通过回调函数实时获取变量更新通知。

## 硬件连接

- **UART1_TX (PB2)**: 传输数据，连接到 USB-UART 转接器的 RX
- **UART1_RX (PB3)**: 接收数据，连接到 USB-UART 转接器的 TX

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 示例内容

本示例演示以下高级功能：

1. **多种变量类型**: INT_DEC（整数）、UINT_DEC（无符号整数）、NUM_HEX（十六进制）、BUF_STRING（字符串）、BUF_HEX（二进制数据）
2. **变量写入回调**: 当变量被更新时，自动调用相应的回调函数通知应用
3. **多参数命令**: 处理包含多个参数的复杂 AT 命令
4. **命令读写处理**: 支持命令的写入（设置）和读取（查询）操作
5. **参数验证**: 演示如何通过回调函数验证参数的有效性

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb
```

## 预期输出

**系统启动输出：**

```
[I][cat_demo] cAT AT Command Parser - Demo Example
[I][cat_demo] =====================================
[I][cat_uart] cAT UART adapter initialized on uart1 @ 115200 baud
[I][cat_demo] AT command parser ready on uart1
[I][cat_demo] Supported AT commands:
[I][cat_demo]   +GO - Go to position (x, y) with message
[I][cat_demo]   +SET - Set speed, hex address and hex buffer
[I][cat_demo]   #TEST - Run test
[I][cat_demo]   #HELP - Print command list
[I][cat_demo]   #QUIT - Quit the demo
[I][cat_uart] cAT UART task started
```

**发送 `AT+GO=10,20,"hello"` 后的输出：**

```
[I][cat_demo] x variable updated internally to: 10
[I][cat_demo] y variable updated internally to: 20
[I][cat_demo] msg variable updated 5 bytes internally to: <hello>
[I][cat_demo] <+GO>: x=10 y=20 msg=hello @ speed=0
[I][cat_demo] <bytes>: 00000000
OK
```

**发送 `AT+SET=100,0x1234,AABBCCDD` 后的输出：**

```
[I][cat_demo] speed variable updated internally to: 100
[I][cat_demo] adr variable updated internally to: 0x1234
[I][cat_demo] bytes_buf variable updated 4 bytes internally to: AABBCCDD
[I][cat_demo] <+SET>: SET SPEED TO = 100
OK
```

**发送 `AT+SET?` 后的输出：**

```
+SET: 100,0x1234,AABBCCDD
OK
```

**发送 `AT#HELP` 后的输出：**

```
+HELP: +GO(WR),+SET(RW),#TEST(RUN),#HELP(RUN),#QUIT(RUN)
OK
```

## 核心 API

| API | 说明 |
|-----|------|
| `cat_uart_adapter_init()` | 初始化 UART 适配器 |
| `cat_uart_adapter_get_io()` | 获取 IO 接口 |
| `cat_uart_adapter_get_mutex()` | 获取互斥锁接口 |
| `cat_init()` | 初始化 AT 命令解析器 |
| `cat_uart_adapter_start_service()` | 启动后台处理任务 |
| `cat_service()` | 处理一次 AT 命令（自动调用） |

## 关键代码

**定义多种变量类型：**

```c
/* GO 命令的变量：整数、二进制 */
static uint8_t x;
static uint8_t y;
static char msg[16];

static struct cat_variable go_vars[] = {
    {
        .type = CAT_VAR_UINT_DEC,           /* 无符号十进制 */
        .data = &x,
        .data_size = sizeof(x),
        .write = x_write,                    /* 写入回调 */
        .name = "x",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &y,
        .data_size = sizeof(y),
        .write = y_write,
        .name = "y",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_STRING,         /* 字符串缓冲区 */
        .data = msg,
        .data_size = sizeof(msg),
        .write = msg_write,
        .name = "msg",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    }
};

/* SET 命令的变量：整数、十六进制地址、二进制 */
static int32_t speed;
static uint16_t adr;
static uint8_t bytes_buf[4];

static struct cat_variable set_vars[] = {
    {
        .type = CAT_VAR_INT_DEC,            /* 有符号十进制 */
        .data = &speed,
        .data_size = sizeof(speed),
        .write = speed_write,
        .name = "speed",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_NUM_HEX,            /* 十六进制数字 */
        .data = &adr,
        .data_size = sizeof(adr),
        .write = adr_write,
        .name = "address",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_HEX,            /* 十六进制缓冲区 */
        .data = &bytes_buf,
        .data_size = sizeof(bytes_buf),
        .write = bytesbuf_write,
        .name = "buffer",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    }
};
```

**变量写入回调函数：**

```c
/* 当 x 变量被写入时自动调用 */
static int x_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("x variable updated internally to: %u", x);
    /* 可在此处触发硬件更新、参数验证等 */
    return 0;  /* 返回 0 表示成功，-1 表示失败 */
}

/* 当 msg 变量被写入时自动调用 */
static int msg_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("msg variable updated %zu bytes internally to: <%s>", write_size, msg);
    return 0;
}

/* 当 bytes_buf 变量被写入时自动调用 */
static int bytesbuf_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("bytes_buf variable updated %zu bytes internally to: %02X%02X%02X%02X",
         write_size, bytes_buf[0], bytes_buf[1], bytes_buf[2], bytes_buf[3]);
    return 0;
}
```

**命令处理器：**

```c
/* AT+GO=<x>,<y>,<msg> 的处理器 */
static cat_return_state go_write(const struct cat_command *cmd, const uint8_t *data,
                                  const size_t data_size, const size_t args_num)
{
    LOGI("<%s>: x=%d y=%d msg=%s @ speed=%d",
         cmd->name,
         x, y, msg, (int)speed);
    LOGI("<bytes>: %02X%02X%02X%02X",
         bytes_buf[0], bytes_buf[1], bytes_buf[2], bytes_buf[3]);

    return CAT_RETURN_STATE_OK;
}

/* AT+SET=<speed>,<addr>,<buf> 的处理器 */
static cat_return_state set_write(const struct cat_command *cmd, const uint8_t *data,
                                   const size_t data_size, const size_t args_num)
{
    LOGI("<%s>: SET SPEED TO = %d", cmd->name, (int)speed);
    return CAT_RETURN_STATE_OK;
}

/* AT+SET? 的读取处理器 */
static cat_return_state set_read(const struct cat_command *cmd, uint8_t *data,
                                  size_t *data_size, const size_t max_data_size)
{
    /* cAT 会自动格式化变量并输出 */
    return CAT_RETURN_STATE_DATA_OK;
}
```

## 注意事项

1. **变量写入回调**: 回调函数在变量被 cAT 解析器更新时自动调用，返回 0 表示成功，-1 表示失败

2. **多种变量类型支持**:
   - `CAT_VAR_INT_DEC`: 有符号整数（十进制）
   - `CAT_VAR_UINT_DEC`: 无符号整数（十进制）
   - `CAT_VAR_NUM_HEX`: 十六进制数字（如 0x1234）
   - `CAT_VAR_BUF_STRING`: 字符串缓冲区（带双引号，如 "hello"）
   - `CAT_VAR_BUF_HEX`: 二进制数据（十六进制格式，如 AABBCCDD）

3. **命令参数验证**: 使用 `args_num` 检查接收的参数个数是否匹配预期

4. **字符串处理**: 字符串变量使用双引号，支持转义字符（\\, \", \n）

5. **十六进制数据**: BUF_HEX 类型支持任意二进制数据，以十六进制表示

6. **错误处理**: 回调函数返回 -1 会导致命令返回 ERROR

## 命令参考

| 命令 | 说明 | 示例 |
|------|------|------|
| `AT+GO=<x>,<y>,<msg>` | 移动到位置并显示消息 | `AT+GO=10,20,"hello"` |
| `AT+SET=<speed>,<addr>,<buf>` | 设置速度、地址和缓冲区 | `AT+SET=100,0x1234,AABBCCDD` |
| `AT+SET?` | 查询 SET 变量 | `AT+SET?` → `100,0x1234,AABBCCDD` |
| `AT#TEST` | 运行测试 | `AT#TEST` → `OK` |
| `AT#HELP` | 列出所有命令 | `AT#HELP` → 命令列表 |
| `AT#QUIT` | 退出示例 | `AT#QUIT` → `OK` |

