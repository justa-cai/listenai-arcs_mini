# FlexLayout 基础示例

## 功能说明

演示如何使用 FlexLayout 库进行 Flexbox 布局计算。本示例展示了如何创建布局节点、设置子节点尺寸、执行布局计算和打印布局结果。

FlexLayout 是 Flexible Box 布局算法的 C 语言实现，遵循 CSS Flexbox 标准算法，支持 margin/padding、flex-direction、wrap、align-items、justify-content 等特性。

## 硬件连接

无需外部连接，FlexLayout 为纯软件布局计算库。

## 示例内容

1. 创建根节点（root）
2. 创建两个子节点（child1、child2）
3. 设置子节点的宽度和高度为 50
4. 将子节点添加到根节点
5. 执行布局计算（使用未定义的约束尺寸）
6. 打印布局结果
7. 递归释放所有节点内存

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
II/elog            [1034:42:44.158 1 elog_async] EasyLogger V2.2.99 is initialStart flexlayout sample
{
  result-padding: ize success.
0.0, nan, 0.0, nan
  children: [
    {
      width: 50.0
      height: 50.0
      result-x: nan
      result-padding: 0.0, nan, 0.0, nan
    }
    {
      width: 50.0
      height: 50.0
      result-x: nan
      result-padding: 0.0, nan, 0.0, nan
    }
  ]
}

```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- `Start flexlayout sample` 为示例开始提示
- `Flex_print()` 会打印节点的布局信息，包括样式、结果和子节点信息（具体格式取决于 `FlexPrintOptions` 参数）

## 核心 API

| API | 说明 |
|-----|------|
| `Flex_newNode()` | 创建新的布局节点 |
| `Flex_setWidth()` | 设置节点宽度 |
| `Flex_setHeight()` | 设置节点高度 |
| `Flex_addChild()` | 向节点添加子节点 |
| `Flex_layout()` | 执行布局计算 |
| `Flex_print()` | 打印节点布局信息 |
| `Flex_freeNodeRecursive()` | 递归释放节点及其所有子节点 |

## 关键代码

```c
/* 创建根节点 */
FlexNodeRef root = Flex_newNode();

/* 创建子节点并设置尺寸 */
FlexNodeRef child1 = Flex_newNode();
Flex_setWidth(child1, 50);
Flex_setHeight(child1, 50);
Flex_addChild(root, child1);

FlexNodeRef child2 = Flex_newNode();
Flex_setWidth(child2, 50);
Flex_setHeight(child2, 50);
Flex_addChild(root, child2);

/* 执行布局计算 */
/* FlexUndefined 表示未定义的约束尺寸，scale 为缩放因子 */
Flex_layout(root, FlexUndefined, FlexUndefined, 1);

/* 打印布局结果 */
Flex_print(root, FlexPrintDefault);

/* 释放所有节点 */
Flex_freeNodeRecursive(root);
```

## 布局参数说明

### Flex_layout 参数

- **`constrainedWidth`**: 约束宽度，使用 `FlexUndefined` 表示未定义
- **`constrainedHeight`**: 约束高度，使用 `FlexUndefined` 表示未定义
- **`scale`**: 缩放因子，通常为 1.0

### Flex_print 选项

- **`FlexPrintDefault`**: 默认打印选项
- **`FlexPrintStyle`**: 打印样式信息
- **`FlexPrintResult`**: 打印布局结果
- **`FlexPrintChildren`**: 打印子节点信息
- **`FlexPrintHideUnspecified`**: 隐藏未指定的属性

## 注意事项

1. **内存管理**: 使用 `Flex_freeNodeRecursive()` 可以递归释放节点及其所有子节点；如果只需要释放单个节点，使用 `Flex_freeNode()`
2. **节点所有权**: 子节点会被父节点管理，释放父节点时会自动处理子节点（使用 `Flex_freeNodeRecursive()` 时）
3. **布局计算**: `Flex_layout()` 必须在添加完所有子节点后调用，才能正确计算布局
4. **约束尺寸**: 使用 `FlexUndefined` 表示未定义的约束，布局算法会根据内容自动计算
5. **节点属性**: 可以通过 `Flex_set*` 系列函数设置节点的各种属性（方向、对齐方式、边距等）
6. **扩展特性**: FlexLayout 支持扩展属性，如 `spacing`（元素间距）、`line-spacing`（行间距）、`fixed`（固定定位）等

