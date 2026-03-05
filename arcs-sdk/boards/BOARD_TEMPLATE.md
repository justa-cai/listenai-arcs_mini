# 自定义板型开发指南

本文档提供创建自定义板型的完整指南，包括文件结构、接口规范、开发步骤和文件模板。

## 概述

创建自定义板型需要准备以下文件：

```
my_board/
├── CMakeLists.txt    # 板型构建脚本（必需）
├── board.h           # 板级接口声明（必需）
├── board.c           # 板级初始化实现（必需）
├── pinmux.h          # 引脚配置声明（必需）
├── pinmux.c          # 引脚配置实现（必需）
└── Kconfig           # 板型配置选项（必需）
```

## 开发步骤

### Step 1: 创建板型目录

```bash
# 创建板型目录
mkdir -p /path/to/my_boards/my_board
cd /path/to/my_boards/my_board
```

### Step 2: 创建 CMakeLists.txt

复制以下内容到 `CMakeLists.txt`：

```cmake
# ARCS BOARD SUPPORT
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

# 创建板型库
listenai_library_named(module_boards)

# 添加源文件
listenai_library_sources(
    pinmux.c
    board.c
)

# 添加头文件路径
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
```

### Step 3: 创建 board.h 和 board.c

**方法一：复制 SDK 内置板型作为起点**

```bash
cp $ARCS_SDK_BASE/boards/arcs_evb/board.h .
cp $ARCS_SDK_BASE/boards/arcs_evb/board.c .
```

然后修改文件中的注释和 `board_get_name()` 返回值为您的板型名称。

**方法二：使用下方的文件模板手动创建**

参考本文档后面的"文件模板"章节。

### Step 4: 使用 Pinmux 工具生成引脚配置

**重要：pinmux.h 和 pinmux.c 必须使用工具生成，不应手动编辑。**

#### 工具信息

- **工具地址**：https://tool.listenai.com/ls-pinmux-tool/
- **功能**：根据芯片型号和外设配置，自动生成引脚复用代码

#### 使用步骤

1. 访问 Pinmux 配置工具
2. 选择目标芯片型号
3. 配置项目中使用的外设（如 UART0、SPI0、I2C0 等）
4. 为每个外设配置引脚映射
5. 生成代码
6. 将生成的 `pinmux.h` 和 `pinmux.c` 保存到板型目录

#### 设备驱动集成

生成的 pinmux 函数会被对应的设备驱动自动调用：
- `lisa_uart0_pinmux()` - 由 lisa_uart 驱动的 uart0 设备初始化时调用
- `lisa_spi0_pinmux()` - 由 lisa_spi 驱动的 spi0 设备初始化时调用
- 其他外设依此类推

**注意事项：**
- 不要手动编辑生成的文件
- 如需修改引脚配置，应在工具中修改后重新生成

### Step 5: 创建 Kconfig 文件（必需）

创建 `Kconfig` 文件，必须包含以下两个配置项：

```bash
# Board Configuration
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

config BOARD_NAME
    string
    default "my_board"
    prompt "Board Name"
    help
        Board Name

config BOARD_MY_BOARD
    bool
    default y
    help
        Board is my_board
```

**重要提示：**
1. 将 `my_board` 替换为您的实际板型名称
2. 第二个配置项名称格式为 `BOARD_<板型名称大写>`
3. 例如板型名为 `arcs_evb`，则配置项为 `BOARD_arcs_evb`

**Kconfig 说明：**
- **必需配置项 1**：`BOARD_NAME` - 板型名称（string 类型）
- **必需配置项 2**：`BOARD_<BOARD_NAME_UPPER>` - 板型标识（bool 类型，默认 y）
- 构建系统会自动加载此文件并设置环境变量 `BOARD_KCONFIG_PATH`
- 外部板型和 SDK 内置板型享有相同的配置能力

### Step 6: 编译验证

```bash
cd $ARCS_SDK_BASE
./build.sh -S samples/helloworld \
    -DBOARD=my_board \
    -DBOARD_SEARCH_PATH=/path/to/my_boards
```

**重要提示：** 外部板型文件已通过编译变量指定构建路径，无需在工程中额外添加构建。

### Step 7: 检查构建日志

确认看到板型加载信息：

```
-- Target board: my_board
-- Board found (custom): my_board from /path/to/my_boards
-- Board loaded successfully: /path/to/my_boards/my_board
```

## 接口规范

### board.h / board.c - 标准接口

板型必须实现以下标准接口：

- `const char* board_get_name(void)` - 返回板型名称字符串

### pinmux.h / pinmux.c - 引脚复用配置

**重要说明：这两个文件由 Pinmux 配置工具生成，不应手动编辑。**

详见 Step 4 的说明。

## 文件模板

### board.h 模板

可参考：boards/arcs_evb/board.h

```c
// ARCS BOARD SUPPORT
//
// Copyright (c) 2025, LISTENAI
// SPDX-License-Identifier: Apache-2.0

#ifndef _BOARD_MY_BOARD_H_
#define _BOARD_MY_BOARD_H_

/**
 * @brief Get the board name
 *
 * @return const char* Board name string
 */
const char* board_get_name(void);

#endif // _BOARD_MY_BOARD_H_
```

### board.c 模板

可参考：boards/arcs_evb/board.c

```c
// ARCS BOARD SUPPORT
//
// Copyright (c) 2025, LISTENAI
// SPDX-License-Identifier: Apache-2.0

#include "board.h"

const char* board_get_name(void)
{
    return "my_board";
}
```

### Kconfig 模板

可参考：boards/arcs_evb/Kconfig

```kconfig
# Board Configuration
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

config BOARD_NAME
    string
    default "my_board"
    prompt "Board Name"
    help
        Board Name

config BOARD_MY_BOARD
    bool
    default y
    help
        Board is my_board
```

**注意：** 请将模板中的 `my_board` 和 `MY_BOARD` 替换为您的实际板型名称。

## 常见问题

### Q: 是否可以不创建 Kconfig 文件？

不可以。Kconfig 文件是必需的，必须包含 `BOARD_NAME` 和 `BOARD_<名称大写>` 两个配置项。

### Q: 可以手动编辑 pinmux.c 吗？

不建议。pinmux.h 和 pinmux.c 应该由 Pinmux 配置工具生成。如果需要修改引脚配置，应在工具中重新配置并生成新文件。

### Q: board.c 中还需要实现其他函数吗？

当前只需要实现 `board_get_name()` 函数。未来如果有更多板级初始化需求，可能会增加其他接口。

