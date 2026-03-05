---
name: sample-doc-review
description: 使用 Samples_Spec.md 规范对示例文档进行全面审查，生成详细审查报告
---

# 示例文档审查器

基于 `samples/Samples_Spec.md` 规范对 ARCS SDK 示例文档进行全面审查，检查结构、格式、内容和语言规范性。

## 快速使用

直接在对话中使用以下任一方式触发：

```
审查 samples/modules/sys_heap/README.md
检查 lisa_gpio 示例文档
审查 sys_heap
```

## 执行步骤

### 1. 读取规范文档

**必须先读取** `samples/Samples_Spec.md` 文件，获取完整的审查标准：
- 标准章节结构和顺序
- 各章节的详细规范
- 不同类型示例的差异要求
- 格式规范
- 语言规范
- AI 审查文档清单

### 2. 确定示例文档路径

- 如果用户提供了文档路径，直接使用
- 如果用户只提供了示例名称（如 "sys_heap"），在 `samples/` 目录下搜索对应的 README.md
- 显示找到的文档路径供用户确认

### 3. 读取并分析目标文档

- 读取目标 README.md 文件
- 根据路径自动识别示例类型：
  - `samples/drivers/devices/` → Devices
  - `samples/modules/` → Modules
  - `samples/network/` → Network
  - `samples/drivers/hal/` → HAL

### 4. 执行全面检查

严格按照 `Samples_Spec.md` 中的 **"AI 审查文档清单"** 部分逐项检查：

1. **结构检查**（10 项）
2. **格式检查**（9 项）
3. **内容检查**（8 项）
4. **语言检查**（6 项）
5. **类型特定检查**（根据示例类型）

### 5. 生成审查报告

使用 `Samples_Spec.md` 中定义的 **"审查报告格式"** 生成报告：

```markdown
# 文档审查报告

**文档路径**: {文件路径}
**示例类型**: {Devices/Modules/Network/HAL}
**审查日期**: {日期}

## 审查结果

**总体评分**: {通过/需修改/不合格}

## 问题清单

### 必需修改（阻塞问题）

{列出所有阻塞问题}

### 建议优化（非阻塞）

{列出所有优化建议}

## 符合规范项

{列出所有通过的检查项}

## 总结

{总体评价和建议}
```

## 评分标准

参考 `Samples_Spec.md` 的规范，但具体标准如下：

- **通过**：无阻塞问题，建议优化 ≤ 3 个
- **需修改**：有 1-3 个阻塞问题
- **不合格**：有 >3 个阻塞问题

### 阻塞问题定义

以下属于阻塞问题（必须修改）：

1. 缺少必需章节
2. 标题格式严重错误
3. 章节顺序严重混乱
4. 核心 API 章节缺失（Devices/Modules 类型）
5. 代码块未标注语言
6. API 表格格式错误

其他问题归为建议优化。

## 更多触发方式

### 完整路径
```
审查 samples/drivers/devices/lisa_uart/send_async_dma/README.md
帮我审查 http 示例文档是否符合规范
```

### 使用关键词
```
使用规范审查 sys_heap 示例
对 lisa_gpio 文档进行规范检查
验证 http 示例文档的规范性
```

## 使用示例

### 示例 1: 审查指定路径

```
用户：审查 samples/modules/sys_heap/README.md
AI：[读取规范] → [读取文档] → [识别类型: Modules] → [执行检查] → [生成报告]
```

### 示例 2: 根据名称查找

```
用户：审查 sys_heap 示例文档
AI：找到文档：samples/modules/sys_heap/README.md
    类型：Modules
    [开始审查...]
```

### 示例 3: 批量审查

```
用户：审查 samples/drivers/devices/lisa_gpio/ 下的所有文档
AI：找到 3 个文档：
    - output_basic/README.md
    - input_basic/README.md
    - interrupt/README.md
    [逐个审查...]
```

## 特别注意

1. **规范引用**：所有检查标准以 `Samples_Spec.md` 为准，不要使用过时规范
2. **类型识别**：必须正确识别示例类型，应用对应的特定检查
3. **详细定位**：问题描述要包含章节名称和行号（如果适用）
4. **具体建议**：提供可直接应用的修改示例
5. **RST 引用检查**：编译和烧录章节优先推荐使用 `.. include:: /sample_build.rst` 和 `.. include:: /sample_flash.rst`

## 特殊情况处理

- **HAL 示例**：允许使用 `📖示例说明` 替代 `功能说明`，在报告中注明即可
- **特殊编译参数**：如果示例需要特殊编译参数，允许直接写命令而非 RST 引用，但需说明原因
- **文档不存在**：提示用户并建议可能的路径
