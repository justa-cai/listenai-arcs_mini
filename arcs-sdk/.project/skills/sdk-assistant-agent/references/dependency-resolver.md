<!-- type: template -->
<!-- 开发规则: README.md#开发规则 -->

# 依赖链解析模板

输入：{module_name}（组件/模块名），{module_path}（从 Glob 发现的路径）

## Step 1: 发现直接依赖

并行执行：

1. `Read {module_path}/CMakeLists.txt` → 提取 `listenai_link_libraries` / `target_link_libraries` / `listenai_library_named` 依赖
2. `Glob {module_path}/Kconfig*` → 若存在，`Read` 并提取 `depends on` / `select` 声明
3. `Grep "#include" {module_path}/*.h` → 提取头文件引用中的跨模块依赖（忽略标准库头文件）

## Step 2: 搜索使用样例

1. `Glob "samples/**/*{module_name}*/"` → 找到使用该模块的现有样例
2. 若有样例：
   - `Read {sample_path}/prj.conf` → 提取完整 CONFIG 依赖链（这是最可靠的依赖来源）
   - `Read {sample_path}/src/main.c` → 提取初始化调用顺序
3. 若无样例 → 从 Step 1 的 CMake/Kconfig 依赖手动推导

## Step 3: 构建初始化序列

从样例 main.c 或依赖关系推导初始化顺序：

- 底层依赖先初始化（如 WiFi → 网络协议栈 → HTTP）
- 标注异步初始化（需等待回调后才能继续，如 `lisa_wifi_init` 是异步的）
- 标注自动初始化（通过 `LISA_DEVICE_REGISTER` 在启动时自动完成，无需手动调用）

## Step 4: 输出

返回三部分信息供调用方使用：

1. **prj.conf 必需配置**（从样例 prj.conf 提取，或从 Kconfig `depends on` 链推导）
2. **初始化代码序列**（从样例 main.c 提取，按依赖顺序排列）
3. **注意事项**（从 `references/experience/{module}.md` 加载，如存在）
