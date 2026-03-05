# Tone模块自动生成配置说明

## 概述

tone模块支持在编译时自动调用tone_tool脚本生成tone.h头文件，并将其拷贝到build目录供不同的app工程使用。

## 配置方法

### 1. 在app项目的CMakeLists.txt中配置tone_tool目录

```cmake
# 配置tone工具脚本目录（支持多版型）
set(TONE_TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tone_tool")
```

**说明：**
- `TONE_TOOL_DIR` 宏用于指定tone_tool脚本的位置
- 每个app工程可以有自己独立的tone_tool目录
- 默认路径为 `${CMAKE_SOURCE_DIR}/tone_tool`

### 2. tone_tool目录结构

```
apps/arcs-evb/tone_tool/
├── cmd/
│   └── tone_tool          # tone打包工具
├── ring/                  # 音频文件目录
│   ├── 000_xxx.mp3
│   ├── 001_xxx.mp3
│   └── ...
├── config.sh              # 配置文件
├── proc.sh                # 处理脚本
└── run.sh                 # 执行脚本
```

### 3. 编译流程

编译时会自动执行以下步骤：

1. 检测 `${TONE_TOOL_DIR}/run.sh` 是否存在
2. 执行 `cd ${TONE_TOOL_DIR} && ./run.sh`
3. 生成 `${TONE_TOOL_DIR}/ring/tone.h`
4. 拷贝到 `${CMAKE_BINARY_DIR}/generated/tone/tone.h`
5. 将生成目录添加到include路径

### 4. 生成的tone.h位置

- **源文件位置：** `apps/arcs-evb/tone_tool/ring/tone.h`
- **编译目录位置：** `build/generated/tone/tone.h`
- **包含路径：** 编译时会自动添加 `${CMAKE_BINARY_DIR}/generated/tone` 到include路径

### 5. 使用生成的tone.h

在代码中直接引用：

```c
#include "tone.h"

// 使用tone ID
uint16_t tone_id = TONE_ID_0;
```

## 多版型支持

不同的app工程可以有不同的音频资源：

```
apps/
├── arcs-evb/
│   ├── CMakeLists.txt          # set(TONE_TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tone_tool")
│   └── tone_tool/
│       └── ring/               # arcs-evb的音频文件
├── remote-ap/
│   ├── CMakeLists.txt          # set(TONE_TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tone_tool")
│   └── tone_tool/
│       └── ring/               # remote-ap的音频文件
```

每个工程编译时会使用各自的tone_tool生成对应的tone.h。

## Kconfig配置

可以通过Kconfig控制是否编译tone模块：

```conf
# 启用tone模块（默认）
CONFIG_MIDDLEWARE_TONE=y

# 禁用tone模块
CONFIG_MIDDLEWARE_TONE=n
```

## 编译日志

编译时会输出以下信息：

```
-- Found tone_tool at: /home/yjh/code/arcs/voiceassistant/apps/arcs-evb/tone_tool
-- tone.h will be generated at: /home/yjh/code/arcs/voiceassistant/build/generated/tone/tone.h
[ 10%] Generating tone.h using tone_tool from /home/yjh/code/arcs/voiceassistant/apps/arcs-evb/tone_tool
```

## 注意事项

1. **tone_tool目录必须包含run.sh脚本**
2. **run.sh必须有执行权限**：`chmod +x tone_tool/run.sh`
3. **音频文件必须放在ring/目录下**
4. **生成的tone.h会在每次编译时重新生成**
5. **如果tone_tool不存在，会使用现有的tone.h（警告模式）**

## 故障排查

### 问题1：找不到tone.h

**原因：** tone_tool脚本未执行或执行失败

**解决：**
1. 检查 `TONE_TOOL_DIR` 路径是否正确
2. 检查 `run.sh` 是否有执行权限
3. 手动执行 `cd apps/arcs-evb/tone_tool && ./run.sh` 测试

### 问题2：编译时tone.h内容不更新

**原因：** CMake缓存问题

**解决：**
```bash
rm -rf build
./build.sh
```

### 问题3：tone_tool执行失败

**原因：** 缺少依赖工具

**解决：**
```bash
# 安装id3v2工具
sudo apt-get install id3v2
```
