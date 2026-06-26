---
name: review
description: 对 ARCS 工程代码进行专业审查。使用 /review 或当用户提到"审查"、"review"、"代码检查"时触发。
---

# ARCS 代码审查

## 兼容约定

- 本技能同时适用于 Claude Code 和 Codex；不要依赖单一客户端专属工具名。
- 下文 `/xxx` 是触发示例；客户端不支持 slash 命令时，用对应自然语言请求即可。
- 需要用户选择或确认时，直接提出简短问题；如果当前环境提供专用提问工具，可使用该工具。
- 命令默认从仓库根目录执行；不要硬编码个人机器上的绝对路径。

## 使用方式
`/review [范围]`

示例:
- `/review` — 审查当前分支相对于 arcs-mini/main 分支的所有改动
- `/review src/middleware/audio/` — 审查指定目录
- `/review --staged` — 审查暂存区的改动
- `/review --commit HEAD~3..HEAD` — 审查最近3次提交
- `/review path/to/file.c` — 审查单个文件

## 执行流程

### 第一步：确定审查范围

根据用户输入确定要审查的代码：

1. **默认（无参数）**：`git diff arcs-mini/main...HEAD --stat` 然后 `git diff arcs-mini/main...HEAD`
2. **暂存区（--staged）**：`git diff --cached`
3. **指定commit范围**：`git diff <range>`
4. **指定文件/目录**：直接读取文件内容

对于大文件（超过 500 行变更），分段审查。

### 第二步：加载审查上下文

1. 读取 `.clang-format` — 代码格式化规则
2. 读取相关 `prj.conf` — 配置约束
3. 如涉及消息代码，读取 `src/framework/pdk_msg.h` — PDK_MSG_ID 编码规则
4. 如涉及 ebus，读取 `modules/ebus/include/ebus/ebus.h` — ebus 接口

### 第三步：逐维度审查

#### 3.1 嵌入式安全性（最高优先级 - 严重）

- **缓冲区溢出**: memcpy/strcpy/snprintf 目标大小，循环索引越界，外部输入长度验证，柔性数组(items[0])大小计算
- **野指针/空指针**: 指针使用前 NULL 检查，malloc/pvPortMalloc 返回值检查，释放后置 NULL，回调指针验证
- **未初始化变量**: 局部变量赋值，结构体 memset/{0} 初始化
- **栈溢出**: 局部大数组(>256字节警告, >1KB严重)，递归终止条件，FreeRTOS 任务栈大小
- **整数溢出**: uint8_t/uint16_t 运算，有符号/无符号混合运算，移位宽度
- **中断安全**: ISR 中非安全函数调用，ISR 阻塞操作，volatile 声明，临界区配对

#### 3.2 并发安全 - FreeRTOS（高优先级 - 严重/警告）

- **竞态条件**: 共享变量互斥保护，读-修改-写原子性，双核共享资源 ic_mutex
- **死锁风险**: 多锁嵌套顺序一致性，超时机制，非递归锁重复获取
- **ISR 约束**: 禁用阻塞API(xSemaphoreTake/vTaskDelay/malloc)，仅用 FromISR 后缀API
- **信号量/互斥锁**: Take/Give 配对，同一任务获取释放，异常路径释放

#### 3.3 ebus 消息系统（高优先级 - 警告）

- **subscribe/unsubscribe 配对**: ebus_message_subscribe ↔ unsubscribe，pdk_msg_sub ↔ unsub
- **PDK_MSG_ID 编码**: 使用 PDK_MSG_ID(domain, evt) 宏，domain 高16位 + evt 低16位，域内无冲突
- **回调无阻塞**: ebus_chn_cb_t/pdk_msg_cb_t 中禁止耗时操作，需异步则用 pdk_invoke
- **消息生命周期**: pub 数据在订阅者处理完前有效，回调中需拷贝后续使用的数据
- **初始化顺序**: 订阅在消息发布之前完成

#### 3.4 编码规范（中优先级 - 建议）

- **命名**: snake_case + 模块前缀(app_/pdk__/listen_/evs_)，宏全大写，类型_t/_e后缀
- **格式**: .clang-format (LLVM, 4空格, Linux大括号, 120字符, InsertBraces:true, SortIncludes:Never)
- **CONFIG_***: 条件编译用 #ifdef CONFIG_XXX，必须在 Kconfig 中有定义
- **头文件**: #ifndef __FILENAME_H__ / #define / #endif 保护

#### 3.5 配置一致性（中优先级 - 警告）

- **Kconfig ↔ prj.conf**: CONFIG_XXX=y 在 Kconfig 中有定义，新选项在 prj.conf 中启用/关闭
- **#ifdef ↔ Kconfig**: 代码中使用的 CONFIG_ 宏在 Kconfig 中声明
- **CMakeLists.txt**: 新 .c 文件添加到构建，新 include 目录注册，条件编译一致
- **多核配置**: AP/CP 的 prj.conf 协调一致，共享外设配置不冲突

#### 3.6 性能（低优先级 - 建议）

- **PSRAM vs SRAM**: 大缓冲区放 PSRAM，频繁访问放 SRAM
- **DMA**: 大块传输使用 DMA，缓冲区对齐
- **DCACHE**: DMA 前 cache flush，DMA 后 cache invalidate

### 第四步：输出审查报告

```markdown
# 代码审查报告

## 概要
- **审查范围**: <描述>
- **变更文件数**: N
- **变更行数**: +X / -Y

## [严重] 必须修复
### S1: <问题标题>
- **文件**: `path/to/file.c:行号`
- **维度**: <审查维度>
- **问题**: <描述>
- **修复建议**: <含代码>

## [警告] 建议修复
### W1: <问题标题>
- **文件**: `path/to/file.c:行号`
- **修复建议**: <方案>

## [建议] 可以改进
### I1: <改进标题>
- **当前写法** → **建议写法**

## 亮点
- <好的实践>

## 总结
| 级别 | 数量 |
|------|------|
| 严重 | N |
| 警告 | N |
| 建议 | N |

**结论**: 通过 / 有条件通过 / 不通过
```

