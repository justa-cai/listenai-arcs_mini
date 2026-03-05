# collections-c 基础示例

## 功能说明

演示如何使用 collections-c 库的 Array（动态数组）数据结构。本示例展示了如何创建数组、添加元素、使用销毁回调函数自动释放元素内存等基本操作。

collections-c 是一个 C 语言通用数据结构库，提供了数组、列表、哈希表、双端队列等多种数据结构。

## 硬件连接

无需外部连接，collections-c 为纯软件数据结构库。

## 示例内容

1. 创建动态数组（Array）
2. 初始化数组迭代器
3. 循环添加 10 个元素（0-9），每个元素动态分配内存
4. 使用销毁回调函数释放数组及其所有元素的内存
5. 打印完成信息

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
cc_custom_malloc(xx)
cc_custom_malloc(xx)
cc_custom_malloc(xx)
Destroying 0
Destroying 1
Destroying 2
Destroying 3
Destroying 4
Destroying 5
Destroying 6
Destroying 7
Destroying 8
Destroying 9
cc_custom_malloc(xx)
cc_custom_malloc(xx)
cc_custom_malloc(xx)
...
collections-c sample done
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 由于启用了自定义内存分配器（'CONFIG_SDK_MODULE_COLLECTIONS_C_CUSTOM_INCLUDE=y'）,输出中会显示内存分配和释放的调试信息（'cc_custom_malloc','cc_custom_free','cc_custom_calloc'）
- `Destroying X` 信息由销毁回调函数 `array_destroy_cb_handle` 输出，X 为元素值（0-9）
- 销毁顺序可能与添加顺序不同，取决于数组内部实现

## 核心 API

| API | 说明 |
|-----|------|
| `array_new()` | 创建新的动态数组 |
| `array_iter_init()` | 初始化数组迭代器 |
| `array_add()` | 向数组添加元素 |
| `array_destroy_cb()` | 销毁数组并调用回调函数处理每个元素 |
| `array_destroy()` | 销毁数组（不调用回调） |

## 关键代码

```c
/* 创建数组 */
Array *ar;
enum cc_stat st;
st = array_new(&ar);
assert(st == CC_OK);

/* 初始化迭代器 */
ArrayIter it;
array_iter_init(&it, ar);

/* 添加元素 */
for (uint8_t i = 0; i < 10; ++i) {
    uint32_t *n = malloc(sizeof(uint32_t));
    assert(n);
    *n = i;
    array_add(ar, n);
}

/* 销毁回调函数 */
void array_destroy_cb_handle(void *item)
{
    uint32_t *n = item;
    printf("Destroying %u\n", *n);
    free(n);
}

/* 销毁数组并释放元素内存 */
array_destroy_cb(ar, array_destroy_cb_handle);
```

## 配置说明

### 自定义内存分配器

本示例支持使用自定义内存分配器，通过以下配置启用：

- **`CONFIG_SDK_MODULE_COLLECTIONS_C_CUSTOM_INCLUDE`**: 启用自定义内存分配器
- **`CONFIG_SDK_MODULE_COLLECTIONS_C_CUSTOM_INCLUDE_PATH`**: 自定义头文件路径（默认：`"cc_custom_include.h"`）

启用后，collections-c 将使用 `cc_custom.c` 中定义的内存分配函数（`cc_custom_malloc`、`cc_custom_free`、`cc_custom_calloc`）替代标准库的 `malloc`、`free`、`calloc`。

本示例的自定义内存分配器使用外部 RAM（`exram_malloc`、`exram_free`、`exram_calloc`），适用于需要从特定内存区域分配的场景。


启用后，collections-c 将使用自定义的内存分配函数替代标准库函数。自定义内存分配器可以使用外部 RAM（`exram_malloc`、`exram_free`、`exram_calloc`）或其他内存管理方案，适用于需要从特定内存区域分配的场景。

## 注意事项

1. **内存管理**: 使用 `array_destroy_cb()` 时，回调函数负责释放每个元素的内存；如果元素不需要特殊处理，可以使用 `array_destroy()` 直接销毁数组
2. **返回值检查**: `array_new()` 和 `array_add()` 返回 `enum cc_stat` 状态码，应检查返回值是否为 `CC_OK`
3. **元素所有权**: 数组不拥有元素的所有权，销毁数组时不会自动释放元素内存，必须使用 `array_destroy_cb()` 并提供回调函数来释放元素
4. **迭代器使用**: 本示例初始化了迭代器但未使用，实际使用时可以通过迭代器遍历数组元素
5. **自定义分配器**: 如果使用自定义内存分配器，确保分配和释放函数配对使用，避免内存泄漏
6. **线程安全**: collections-c 库本身不是线程安全的，在多线程环境中使用时需要额外的同步机制

