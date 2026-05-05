<!-- type: driver-template -->
<!-- 开发规则: README.md#开发规则 -->

# 驱动通用执行模板

输入：{driver_name}（由 driver-dev worker 传入，如 gpio、uart、audio）

## 资源发现

并行执行：

1. `Read VERSION` — 确认当前 SDK 版本
2. `Glob drivers/lisa_{driver_name}/**` — 发现头文件、实现、文档、Kconfig
   - 若无匹配，尝试 `Glob drivers/{driver_name}/**`
3. `Glob samples/**/lisa_{driver_name}/**` — 发现示例
   - 若无匹配，尝试 `Glob samples/**/{driver_name}/**`
4. `Glob boards/*/pinmux.*` — 板级 pinmux 配置（如涉及引脚）

## 依赖发现

若用户的需求涉及跨模块功能（如驱动 + 网络、驱动 + 显示）：
- 按 `references/dependency-resolver.md` 流程获取完整依赖链
- 将依赖链的 prj.conf 配置和初始化代码纳入生成范围

## 知识加载

- `Read references/experience/lisa_{driver_name}.md`（如存在）— 过滤当前版本适用的条目

## 执行

1. 分析发现的资源（头文件获取 API 签名，文档获取配置说明，示例获取用法参考）
2. 结合用户需求，判断缺失的必要信息
3. 只对无法从资源推断的信息向用户确认（如具体引脚号、设备实例）
4. 执行用户请求的任务

## 经验捕获

任务完成后，评估是否有值得记录的经验（非显而易见的根因、用户纠正、隐含约束等）。如有，调用 `experience-capture` skill。
