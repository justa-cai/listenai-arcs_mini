# 板型使用指南

本文档说明如何在项目中使用板型支持系统。

## 板型支持系统简介

板型支持系统负责管理不同硬件板卡的配置和初始化。系统采用模块化设计，将引脚复用（pinmux）配置与板级初始化逻辑分离，便于维护和扩展。

### 主要特性

- **模块化设计**：板级代码（board.c/h）与引脚配置（pinmux.c/h）分离
- **外部板型支持**：支持在 SDK 外部添加自定义板型，无需修改 SDK 源码
- **两级搜索机制**：优先搜索自定义路径，然后搜索 SDK 内置板型
- **简单易用**：通过 `build.sh` 命令行参数即可切换板型
- **配置管理**：通过 Kconfig 管理板型配置选项

## 快速开始

### 使用 SDK 内置板型

必须使用 `-DBOARD` 参数指定板型：

```bash
cd arcs-sdk

# 使用 arcs_mini 板型
./build.sh -S samples/helloworld -DBOARD=arcs_mini

# 使用 arcs_evb 板型
./build.sh -S samples/helloworld -DBOARD=arcs_evb
```

### 查看可用的 SDK 内置板型

```bash
ls boards/
```

查看各板型的详细说明，请参考对应板型目录下的 README.md 文件。

## 使用外部自定义板型

### 基本用法

使用 `-DBOARD` 和 `-DBOARD_SEARCH_PATH` 参数：

```bash
./build.sh -S samples/helloworld \
    -DBOARD=my_custom_board \
    -DBOARD_SEARCH_PATH=/path/to/my_boards
```

**说明：**
- `BOARD_SEARCH_PATH` **必须使用绝对路径**，例如 `/home/user/my_boards` 或 `~/my_boards`，避免相对路径带来的解析问题
- 例如：如果自定义板型目录为 `/path/to/my_boards/AAA_EVB`，则：
  - `-DBOARD=AAA_EVB`
  - `-DBOARD_SEARCH_PATH=/path/to/my_boards`
- **外部板型文件无需自行添加构建**

### 创建外部自定义板型

关于如何创建自定义板型的详细指南，请参考 [BOARD_TEMPLATE.md](BOARD_TEMPLATE.md)。

该指南包含：
- 完整的文件结构说明
- 接口规范和实现要求
- 详细的开发步骤
- 文件模板和示例

## 板型搜索机制

构建系统按以下优先级搜索板型：

1. **板型名称**（必需）
   - 必须通过 `-DBOARD=<board_name>` 参数指定
   - 不指定将导致构建失败

2. **外部自定义路径**（如果指定了 `BOARD_SEARCH_PATH`）
   - 搜索路径：`${BOARD_SEARCH_PATH}/${BOARD}/`
   - **使用绝对路径**（如 `/home/user/boards` 或 `~/boards`），避免相对路径在不同构建环境下的解析差异

3. **SDK 内置路径**（备用）
   - 搜索路径：`${ARCS_SDK_BASE}/boards/${BOARD}/`

4. **验证要求**
   - 板型目录必须存在
   - 板型目录下必须包含 `CMakeLists.txt` 文件

如果搜索失败，构建系统会给出详细的错误提示，说明搜索路径和解决方案。

## 板型文件结构

每个板型目录必须包含以下文件：

```
board_name/
├── CMakeLists.txt    # 板型构建脚本（必需）
├── board.h           # 板级接口声明（必需）
├── board.c           # 板级初始化实现（必需）
├── pinmux.h          # 引脚配置声明（必需）
├── pinmux.c          # 引脚配置实现（必需）
├── Kconfig           # 板型配置选项（必需）
└── README.md         # 板型说明文档（推荐）
```

详细的文件说明和模板请参考 [BOARD_TEMPLATE.md](BOARD_TEMPLATE.md)。

## 故障排查

### BOARD 未指定

**错误信息：**
```
BOARD is not specified!
```

**解决方案：**
必须使用 `-DBOARD` 参数指定板型：
```bash
./build.sh -S samples/helloworld -DBOARD=arcs_mini
```

### 板型未找到

**错误信息：**
```
Board 'xxx' not found!
```

**解决方案：**
1. 检查板型名称拼写是否正确
2. 确认板型目录存在：
   - SDK 内置板型：`ls boards/xxx`
   - 自定义板型：`ls ${BOARD_SEARCH_PATH}/xxx`
3. 如使用自定义板型，检查 `BOARD_SEARCH_PATH` 路径是否正确


### 板型目标重复定义

**错误信息：**
```
add_library cannot create target "module_boards" because another target
with the same name already exists. The existing target is a static library
created in source directory
```

**原因：**
使用外部自定义板型时，SDK已通过${BOARD_SEARCH_PATH}/${BOARD}将板型目录添加到构建系统中，无需外部构建额外添加，否则会出现重复定义目标的错误。

**解决方案：**
外部工程CMakeLists.txt中无需添加板型目录。

### 编译错误

**常见原因：**
1. 板型代码未实现必需的接口函数
2. 头文件路径配置错误
3. Kconfig 文件缺失或配置不正确

**解决方案：**
1. 确保实现了 `board_get_name()` 函数
2. 检查 CMakeLists.txt 中的头文件路径配置
3. 确保 Kconfig 文件包含必需的配置项

## 常见问题

### Q: 如何在不同板型之间快速切换？

只需修改 `build.sh` 的 `-DBOARD` 参数：

```bash
# 切换到 arcs_mini
./build.sh -S samples/helloworld -DBOARD=arcs_mini

# 切换到 arcs_evb
./build.sh -S samples/helloworld -DBOARD=arcs_evb
```

### Q: 可以为不同的项目使用不同的板型吗？

可以。每个项目的构建命令可以指定不同的板型：

```bash
# 项目 A 使用 arcs_mini
./build.sh -S samples/project_a -DBOARD=arcs_mini

# 项目 B 使用 arcs_evb
./build.sh -S samples/project_b -DBOARD=arcs_evb
```

### Q: 自定义板型是否会被覆盖？

不会。自定义板型存储在 SDK 外部，SDK 更新不会影响自定义板型。

### Q: 如何创建新的板型？

请参考 [BOARD_TEMPLATE.md](BOARD_TEMPLATE.md)，其中包含完整的开发指南和步骤说明。

