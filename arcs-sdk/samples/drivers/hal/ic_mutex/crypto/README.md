# IC Mutex 加密模块示例

本示例演示 AP 和 CP 两个核使用核间互斥锁（IC Mutex）来互斥访问硬件加密模块，两个核分别对同样的输入数据进行 SHA256 计算。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **硬件加密模块互斥访问**：使用 IC Mutex 实现双核互斥访问硬件加密模块
- **SHA256 计算**：两个核分别进行 SHA256 哈希计算
- **核间消息初始化**：使用 ICMessage 进行核间通信初始化
- **结果一致性验证**：两个核对相同输入数据得到相同的 SHA256 结果

### 验证方法

- 正常情况：两个核输出相同的 SHA256 哈希值
- 禁用 IC Mutex（在 prj.conf 中添加 `CONFIG_ARCS_HAL_IC_MUTEX=n`）：两个核的加密结果会不一致

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看两个核的日志输出）
- 串口连接：
  - AP 核串口：PB02 引脚
  - CP 核串口：PA03 引脚

## 🚀 快速开始

### 1. 构建 AP 核固件

在 AP 子目录下执行构建脚本：

```bash
cd samples/drivers/ic_mutex/crypto/ap
./build.sh
```

构建成功后，会在 `ap/build` 目录下生成 `ap.bin` 文件。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/ap.bin -C arcs
```

在 CP 子目录下执行构建脚本：

```bash
cd samples/drivers/ic_mutex/crypto/cp
./build.sh
```

构建成功后，会在 `cp/build` 目录下生成 `helloworld.bin` 文件。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0xd00000 build/helloworld.bin -C arcs
```

### 4. 烧录 CP 核固件

将 CP 核固件烧录到 flash 的 0xd00000 位置：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0xd00000 cp/build/arcs.bin
```

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 5. 查看输出

烧录完成后，复位开发板，需要连接两个串口分别查看 AP 核和 CP 核的日志输出。

## 🔧 配置说明

### 项目配置

AP 核在 `ap/prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y

CONFIG_MEM_SRAM_BASE=0x20010000
CONFIG_MEM_SRAM_SIZE=0x00020000

CONFIG_MEM_PSRAM_BASE=0x28000000
CONFIG_MEM_PSRAM_SIZE=0x00800000

CONFIG_MEM_FLASH_BASE=0x30000000
CONFIG_MEM_FLASH_SIZE=0x00200000

CONFIG_ARCS_AP_CORE=y              # AP 核配置
CONFIG_CLOCK_INIT=y
CONFIG_PSRAM_INIT=y

CONFIG_ARCS_HAL_LSF=y              # LSF 硬件抽象层
CONFIG_IPC_LSF=y                   # LSF 核间通信

# CONFIG_ARCS_HAL_IC_MUTEX=n       # 注释掉表示启用 IC Mutex（默认）
```

### 烧录地址配置

| 核心 | 烧录地址 | 固件文件 |
|------|----------|----------|
| AP 核 | 0x0 | ap/build/ap.bin |
| CP 核 | 0xd00000 | cp/build/helloworld.bin |

## 📋 代码解析

### 关键代码段

#### 1. AP 核 - 主函数

```c
int main(int argc, char **argv)
{
    printf("AP Hard ID: %d\n", CONFIG_HARTID);
    
    // 启动 CP 核
    #define MEM_CP_FLASH_BASE  (0x30d00000)
    #define MEM_CP_FLASH_SIZE  (0x300000)
    printf("boot cp from address: 0x%x\n", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    
    // 初始化核间消息系统
    ic_message_init();
    printf("ic_message_init done!\n");
    
    // 创建 IC Mutex 任务
    xTaskCreate(ic_mutex_task, "ic_mutex_task", 4096, NULL, 6, NULL);
    
    return 0;
}
```

#### 2. AP 核 - IC Mutex 任务

```c
extern void ls_crypto_init(void);
extern void ls_crypto_sha256_test(void);

void ic_mutex_task(void *param)
{
    int ret;
    
    printf("ic_mutex_task enter...\n");
    
    // 初始化加密模块
    ls_crypto_init();
    
    while (1) {
        // 执行 SHA256 测试
        ls_crypto_sha256_test();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

#### 3. CP 核 - 主函数

```c
int main(int argc, char **argv)
{
    printf("CP=======! Hard ID: %d\n", CONFIG_HARTID);
    
    // 初始化核间消息系统
    ic_message_init();
    printf("ic_message_init done!\n");
    
    // 创建 IC Mutex 任务
    xTaskCreate(ic_mutex_task, "ic_mutex_task", 4096, NULL, 6, NULL);
    
    return 0;
}
```

#### 4. CP 核 - IC Mutex 任务

```c
extern void ls_crypto_init(void);
extern void ls_crypto_sha256_test(void);

void ic_mutex_task(void *param)
{
    int ret;
    
    printf("ic_mutex_task enter...\n");
    
    // 初始化加密模块
    ls_crypto_init();
    
    while (1) {
        // 执行 SHA256 测试
        ls_crypto_sha256_test();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 🔍 预期输出

### AP 核串口日志（PB02 引脚）

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 0
AP Hard ID: 0
boot cp from address: 0x30d00000
ic_message_init done!
ic_mutex_task enter...
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
...
```

### CP 核串口日志（PA03 引脚）

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 1
CP=======! Hard ID: 1
ic_message_init done!
ic_mutex_task enter...
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9 
...
```

输出说明：
- AP 核运行在 hart-id: 0
- CP 核运行在 hart-id: 1
- 两个核输出相同的 SHA256 哈希值：`78 5b 07 51 fc 2c 53 dc 14 a4 ce 3d 80 0e 69 ef 9c e1 00 9e b3 27 cc f4 58 af e0 9c 24 2c 26 c9`
- 说明互斥锁正常工作，硬件加密模块被正确互斥访问

## ⚠️ 注意事项

1. **双核固件烧录**：
   - 必须先烧录 AP 核固件（地址 0x0）
   - 再烧录 CP 核固件（地址 0xd00000）
   - 两个固件缺一不可

2. **串口日志分离**：
   - AP 核串口在 PB02 引脚
   - CP 核串口在 PA03 引脚
   - 需要连接两个串口分别查看日志

3. **CP 核启动**：
   - AP 核负责启动 CP 核
   - CP 核启动地址：0x30d00000（对应 flash 0xd00000）

4. **IC Mutex 验证**：
   - 正常情况：两个核输出相同的 SHA256 值
   - 禁用 IC Mutex（在 prj.conf 中添加 `CONFIG_ARCS_HAL_IC_MUTEX=n`）后，两个核的加密结果会不一致

5. **外部函数**：
   - `ls_crypto_init()`：初始化加密模块
   - `ls_crypto_sha256_test()`：执行 SHA256 测试

6. **循环执行**：
   - 两个核都在无限循环中执行 SHA256 计算
   - 每次计算后延时 10ms

7. **内存配置**：
   - SRAM 基地址：0x20010000
   - PSRAM 基地址：0x28000000
   - FLASH 基地址：0x30000000
