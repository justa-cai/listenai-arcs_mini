# Framework Layer Test Baseline

本文档梳理 ARCS SDK 当前测试体系中适用于 Framework 层的标准 test 工程约定与可复用公共资产，不额外设计新的测试框架或 `testcase.yaml` schema。

## 标准 test 工程定义

当前 SDK 中，一个可被 guardian 和现有构建流程稳定识别的标准 test 工程，至少需包含如下文件：

```text
test/<domain>/<name>/
├── CMakeLists.txt
├── Kconfig
├── prj.conf
├── testcase.yaml
└── test_*.c
```

其中各文件职责如下：

- `CMakeLists.txt`：通过 `find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})` 接入 SDK 构建体系、调用 `listenai_add_executable(${PROJECT_NAME})`，并链接 Unity 及测试源文件。
- `Kconfig`：统一引用 `$ARCS_BASE/Kconfig` 配置树，不在 Framework 层测试工程中自定义新的测试框架入口。
- `prj.conf`：声明测试所需最小的 `CONFIG_*` 组合，并启用 `CONFIG_TEST_UNITY=y`。
- `testcase.yaml`：作为 guardian 的执行入口，沿用现有字段表达 build-only 或真板执行，不新增字段。
- `test_*.c`：包含测试主程序、测试分组与辅助函数，并以 Unity 最终结果判定通过。

## testcase.yaml 约定

Framework 层真板测试默认沿用如下表达：

```yaml
tests:
  test.<domain>.<name>:
    programmer: arcs
    runner: uart
    log_analyzer: Unity
```

约束如下：

- `tests.<id>` 必须全仓唯一，推荐 `test.framework.<module>` 或 `test.driver.<module>` 风格。
- 真板执行场景统一使用 `programmer: arcs` 和 `runner: uart`。
- 以 Unity 汇总结果作为 CI 判定依据时，设置 `log_analyzer: Unity`。
- 仅当只需验证构建、无需真板运行时才加 `build_only: true`。
- 不在 Framework 层测试工程中提前设计新的 `runner`、`builder`、`programmer`、`log_analyzer` 字段。

## Framework 层公共约定

Framework 层测试工程应统一遵循以下组织方式：

1. 测试工程目录置于 `test/` 下，保持与现有 `driver`、`components`、`posix` 的分类一致。
2. 测试主程序负责初始化被测模块运行环境、顺序调用测试分组，并输出 Unity 汇总结果。
3. 公共辅助函数限于本测试工程目录内的 `test_common.*` 文件，优先就近复用，避免提前抽象全局库。
4. 真板测试默认要求可编译、可烧录，并通过串口输出完整 Unity 结果。
5. 若模块依赖启动阶段或注册表顺序，应通过真实初始化链路验证，避免自造脱离 SDK 的执行器。

## 可复用公共资产

当前 SDK 中，Framework 层补测试时可直接参考或复用的资产如下：

| 路径 | 作用 | 复用建议 |
| --- | --- | --- |
| `test/components/simple_box/` | 最小 Unity 真板测试工程形态 | 参考 `testcase.yaml`、`CMakeLists.txt` 和主测试入口组织方式 |
| `test/driver/lisa_device/` | 多文件拆分的标准驱动测试工程 | 参考 `test_common.*`、主程序调用多个测试分组的组织方式 |
| `test/common/mock_lisa_gpio/` | 通用 mock 组件示例 | 模块确有外设依赖时，优先复用已有 mock 风格 |
| `test/driver/vaddr_remap/boot/testcase.yaml` | 多 case `testcase.yaml` 写法 | 一个目录下有多个测试 case 时参考 |

## 标准工程参考形态

Framework 层新增测试工程时推荐采用如下目录结构：

```text
test/framework/<module>/
├── CMakeLists.txt
├── Kconfig
├── prj.conf
├── testcase.yaml
├── test_<module>.c
├── test_common.c
└── test_common.h
```

说明：

- `test_<module>.c` 负责 `UNITY_BEGIN()`、`RUN_TEST(...)` 及测试分组调度
- `test_common.*` 负责共享夹具、桩函数与公共断言
- 模块测试若天然分主题，可拆成多个 `test_<module>_<topic>.c`
- 目录命名紧贴被测模块，不引入新的抽象层级
