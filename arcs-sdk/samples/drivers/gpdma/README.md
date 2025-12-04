# GPDMA 示例

本示例演示如何使用 GPDMA（General Purpose DMA）进行内存到内存（M2M）数据传输。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **内存到内存传输**：使用 GPDMA 将数据从源缓冲区传输到目标缓冲区
- **Normal 模式**：使用 GPDMA 的 Normal 传输模式
- **事件回调机制**：注册并处理 GPDMA 传输完成事件
- **传输验证**：使用内存比较验证数据传输的正确性
- **动态内存分配**：使用 malloc/free 管理缓冲区

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/gpdma
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/gpdma -C
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/arcs.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具（如 minicom）查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### GPDMA 参数配置

| 参数 | 值 |
|------|-----|
| `CSK_GPDMA_VALIDATION_LENGTH` | 100 |
| `CSK_GPDMA_VALIDATION_WORD_LENGTH` | 4 |
| 通道（`dma_ch`） | `gp_dma_ch0` |
| 突发长度（`burst_len`） | `gpdma_burst_len_1spl` |
| 传输模式（`tfr_mode`） | `tfr_mode_m2m` |
| 采样单元（`sample_unit`） | `gpdma_sample_unit_word` |

### GPDMA 配置结构

```c
csk_gpdma_init_t gpdma_para = {
    .dma_ch = gp_dma_ch0,                   // GPDMA 通道 0
    .burst_len = gpdma_burst_len_1spl,      // 突发长度：1 采样
    .src_mode = address_mode_normal,        // 源地址模式：正常
    .dst_mode = address_mode_normal,        // 目标地址模式：正常
    .tfr_mode = tfr_mode_m2m,               // 传输模式：内存到内存
    .sample_unit = gpdma_sample_unit_word,  // 采样单元：字
    .src_inc_mode = inc_mode_increase,      // 源地址递增模式
    .dst_inc_mode = inc_mode_increase,      // 目标地址递增模式
    .prio_lvl = prio_mode_vhigh,            // 优先级：非常高
    .handshake = hs_none,                   // 握手：无
};
```

## 📋 代码解析

### 关键代码段

#### 1. 缓冲区分配

```c
// 分配源缓冲区和目标缓冲区
gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

// 初始化源缓冲区数据
uint32_t i = 0;
for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++) {
    *((uint32_t *)gpdma_src_buffer + i) = i * i;
}
```

#### 2. GPDMA 事件回调

```c
static volatile uint32_t GPDMA_Event = 0;

static void gpdma_normal_test_callback(uint32_t event, void *workspace)
{
    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE) {
        printf("GPDMA Normal mode trigger\n");
    }
    
    GPDMA_Event = CSK_GPDMA_EVENT_TRANSFER_DONE;
}
```

#### 3. GPDMA 初始化和配置

```c
// 定义 GPDMA 参数结构
csk_gpdma_init_t gpdma_para = { /* ... */ };

// 初始化 GPDMA
GPDMA_Initialize();

// 配置 GPDMA 并注册回调
GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);
```

#### 4. 启动 GPDMA 传输

```c
// 启动 Normal 模式传输
GPDMA_Start_Normal(gp_dma_ch0, 
                   (void *)gpdma_src_buffer, 
                   (void *)gpdma_dst_buffer, 
                   CSK_GPDMA_VALIDATION_LENGTH);

// 等待传输完成
while (!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));

GPDMA_Event = 0;
```

#### 5. 传输验证

```c
// 比较源缓冲区和目标缓冲区
int32_t ret = 0;
ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, 
             CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);

if (ret != 0) {
    printf("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error", 
           gpdma_src_buffer, gpdma_dst_buffer);
} else {
    printf("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\n",
           gpdma_src_buffer, gpdma_dst_buffer);
}
```

#### 6. 资源释放

```c
// 释放缓冲区
free(gpdma_src_buffer);
free(gpdma_dst_buffer);
printf("gpdma end\n");
```

#### 7. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! gpdma\n");
    
    // 使能全局中断
    enable_GINT();
    
    // 执行 GPDMA 内存到内存传输测试
    GPDMA_NormalMode_M2M_Word_Channel0_Test();
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! gpdma
GPDMA Normal mode trigger
[GPDMA][NORMAL][M2M][CH0][Word] source: 0x200132dc -> destination: 0x20013470 transfer success!!!
gpdma end
```

输出说明：
- `Hello, world! gpdma`：程序启动信息
- `GPDMA Normal mode trigger`：GPDMA 传输完成事件触发
- `[GPDMA][NORMAL][M2M][CH0][Word]...`：传输详情
  - `source: 0x200132dc`：源缓冲区地址
  - `destination: 0x20013470`：目标缓冲区地址
  - `transfer success!!!`：传输成功
- `gpdma end`：示例执行完成

## ⚠️ 注意事项

1. **GPDMA 与 DMA 的区别**：
   - GPDMA：General Purpose DMA，通用 DMA
   - DMA：标准 DMA
   - 两者配置和 API 可能略有不同

2. **传输大小**：
   - 传输长度：100 个 32 位字
   - 总字节数：100 × 4 = 400 字节

3. **地址模式**：
   - 源地址和目标地址都配置为正常模式（`address_mode_normal`）
   - 地址递增模式（`inc_mode_increase`）

4. **突发长度**：
   - 配置为 1 采样（`gpdma_burst_len_1spl`）
   - 较小的突发长度适合小数据量传输

5. **优先级**：
   - 配置为非常高优先级（`prio_mode_vhigh`）
   - 确保 DMA 传输及时完成

6. **全局中断**：
   - 使用前需要调用 `enable_GINT()` 使能全局中断
   - 否则事件回调可能无法触发

7. **内存管理**：
   - 使用 malloc 动态分配缓冲区
   - 使用完成后必须调用 free 释放内存
   - 避免内存泄漏

8. **传输验证**：
   - 使用 `memcmp()` 比较源和目标缓冲区
   - 返回 0 表示数据一致，传输正确
   - 返回非 0 表示数据不一致，传输错误

9. **源数据初始化**：
   - 源缓冲区初始化为 `i * i`
