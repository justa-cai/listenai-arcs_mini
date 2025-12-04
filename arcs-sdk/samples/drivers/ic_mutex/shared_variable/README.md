# IC Mutex 共享变量示例

本示例演示 AP 和 CP 两个核使用核间互斥锁（IC Mutex）来互斥访问 SRAM 中的同一个共享变量，两个核分别对该变量进行自增操作。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **核间互斥锁**：使用 IC Mutex 实现双核互斥访问
- **共享变量访问**：AP 和 CP 核访问同一内存地址的变量
- **核间同步**：使用 ICFence 进行核间对象同步
- **核间消息**：使用 ICMessage 进行核间通信初始化
- **互斥自增**：两个核交替对共享变量自增

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
cd samples/drivers/ic_mutex/shared_variable/ap
./build.sh
```

构建成功后，会在 `ap/build` 目录下生成 `arcs.bin` 文件。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/ap.bin -C arcs
```

在 CP 子目录下执行构建脚本：

```bash
cd samples/drivers/ic_mutex/shared_variable/cp
./build.sh
```

构建成功后，会在 `cp/build` 目录下生成 `arcs.bin` 文件。

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
```

### 烧录地址配置

| 核心 | 烧录地址 | 固件文件 |
|------|----------|----------|
| AP 核 | 0x0 | ap/build/arcs.bin |
| CP 核 | 0xd00000 | cp/build/arcs.bin |

### IC Mutex 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 互斥锁类型 | `IC_MUTEX_TYPE_5` | 互斥锁类型 5 |
| 等待模式 | `IC_MUTEX_SLEEP_WAIT` | 睡眠等待模式 |
| ICFence ID | 1 | 核间同步对象 ID |

## 📋 代码解析

### 关键代码段

#### 1. AP 核 - 共享变量定义

```c
// 全局：核间资源
static IC_Mutex shared_ic_mutex;
static ICFenceHandle ic_fence_1;
volatile int gCounter = 0;  // AP 核定义共享变量
```

#### 2. CP 核 - 共享变量指针

```c
// 全局：核间资源
static IC_Mutex shared_ic_mutex;
static ICFenceHandle ic_fence_1;
volatile int *gPtrCounter = NULL;  // CP 核使用指针访问共享变量
```

#### 3. AP 核 - IC Mutex 初始化

```c
void ic_mutex_task(void *param)
{
    int ret;
    
    printf("ic_mutex_task enter...\n");
    
    // 初始化核间互斥锁
    IC_Mutex_init(&shared_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_TYPE_5);
    
    // 创建 ICFence 对象（id: 1）
    ic_fence_1 = ICFence_Creator_createObj(1);
    
    // 对象级的核间同步，确认对端已经就绪
    ICFence_syncWithRemote(ic_fence_1);
    
    // 通知对端共享变量的地址
    ICFence_notify(ic_fence_1, (uint32_t)(uintptr_t) &gCounter);
}
```

#### 4. CP 核 - 获取共享变量地址

```c
void ic_mutex_task(void *param)
{
    int ret;
    
    printf("ic_mutex_task enter...\n");
    
    // 初始化核间互斥锁
    IC_Mutex_init(&shared_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_TYPE_5);
    
    // 延时等待 AP 核的 ICFence 对象就绪
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 获取远程 ICFence 对象
    ic_fence_1 = (ICFenceHandle) IC_Proxy_getRemoteFence(1);
    
    // 对象级的核间同步，确认对端已经就绪
    ICFence_syncWithRemote(ic_fence_1);
    
    // 等待接收共享变量地址
    ICFence_wait(ic_fence_1, (uint32_t*) &gPtrCounter);
}
```

#### 5. AP 核 - 互斥访问共享变量

```c
while (1) {
    // 获取互斥锁
    ret = IC_Mutex_acquire(&shared_ic_mutex);
    
    // 对共享变量自增
    gCounter ++;
    printf("gCounter == %d\n", gCounter);
    
    // 释放互斥锁
    ret = IC_Mutex_release(&shared_ic_mutex);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

#### 6. CP 核 - 互斥访问共享变量

```c
while (1) {
    // 获取互斥锁
    ret = IC_Mutex_acquire(&shared_ic_mutex);
    
    // 对共享变量自增
    (*gPtrCounter) ++;
    printf("gCounter == %d\n", *gPtrCounter);
    
    // 释放互斥锁
    ret = IC_Mutex_release(&shared_ic_mutex);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

#### 7. AP 核 - 启动 CP 核

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

## 🔍 预期输出

### AP 核串口日志（PB02 引脚）

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 0
AP Hard ID: 0
boot cp from address: 0x30d00000
ic_message_init done!
ic_mutex_task enter...
gCounter == 1
gCounter == 3
gCounter == 5
gCounter == 7
gCounter == 9
gCounter == 11
gCounter == 13
gCounter == 15
gCounter == 17
gCounter == 19
gCounter == 21
gCounter == 23
gCounter == 25
gCounter == 27
gCounter == 29
gCounter == 31
...
```

### CP 核串口日志（PA03 引脚）

```
********Arcs SDK@V0.0.10-6-g6e971fb7-dirty-@v0.0.10********
Running on hart-id: 1
CP=======! Hard ID: 1
ic_message_init done!
ic_mutex_task enter...
gCounter == 2
gCounter == 4
gCounter == 6
gCounter == 8
gCounter == 10
gCounter == 12
gCounter == 14
gCounter == 16
gCounter == 18
gCounter == 20
gCounter == 22
gCounter == 24
gCounter == 26
gCounter == 28
gCounter == 30
...
```

输出说明：
- AP 核运行在 hart-id: 0
- CP 核运行在 hart-id: 1
- AP 核打印奇数值（1, 3, 5, 7, ...）
- CP 核打印偶数值（2, 4, 6, 8, ...）
- 两个核交替对共享变量自增，互斥访问保证了数据一致性

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

4. **IC Mutex 使用**：
   - `IC_Mutex_acquire()`：获取互斥锁
   - `IC_Mutex_release()`：释放互斥锁
   - 必须成对使用，避免死锁

5. **ICFence 同步**：
   - AP 核创建 ICFence 对象（Creator）
   - CP 核获取远程 ICFence 对象（Proxy）
   - 用于核间对象级同步

6. **共享变量地址传递**：
   - AP 核通过 `ICFence_notify()` 发送地址
   - CP 核通过 `ICFence_wait()` 接收地址

7. **内存配置**：
   - SRAM 基地址：0x20010000
   - 共享变量位于 SRAM 区域
   - 两个核都可访问该地址

8. **执行结果验证**：
   - 正确结果是两个核的打印值交替递增
   - AP 核打印奇数，CP 核打印偶数
   - 说明互斥锁正常工作
