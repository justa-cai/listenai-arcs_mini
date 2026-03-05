# cJSON 基础示例

## 功能说明

演示如何使用 cJSON 库进行 JSON 数据的创建、序列化和解析操作。本示例展示了 cJSON 的基本 API 使用方法，包括创建 JSON 对象、添加不同类型的数据、将对象转换为字符串、解析 JSON 字符串以及提取数据。

## 硬件连接

无需外部连接，cJSON 为纯软件 JSON 解析库。

## 示例内容

1. 创建 JSON 对象并添加字符串、数字和布尔值
2. 创建嵌套 JSON 对象
3. 将 JSON 对象转换为字符串格式
4. 解析 JSON 字符串
5. 从解析后的 JSON 对象中提取数据
6. 释放 JSON 对象和字符串内存

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
this is a cjson samples
Generated JSON:
{
	"name":	"John Doe",
	"age":	30,
	"is_student":	false,
	"info":	{
		"address":	"123 Main St",
		"zip_code":	12345
	}
}
Parsed JSON:
Name: John Doe
Age: 30
Is Student: false
Address: 123 Main St
Zip Code: 12345

```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 由于默认启用了自定义内存分配器（`CONFIG_CJSON_CUSTOM_INCLUDE=y`），输出中会显示内存分配和释放的调试信息（`cjson_custom_malloc`、`cjson_custom_realloc`、`cjson_custom_free`）
- 这些调试信息可以帮助了解 cJSON 库的内存使用情况

## 核心 API

| API | 说明 |
|-----|------|
| `cJSON_CreateObject()` | 创建 JSON 对象 |
| `cJSON_AddStringToObject()` | 向对象添加字符串值 |
| `cJSON_AddNumberToObject()` | 向对象添加数字值 |
| `cJSON_AddBoolToObject()` | 向对象添加布尔值 |
| `cJSON_AddItemToObject()` | 向对象添加子项（如嵌套对象） |
| `cJSON_Print()` | 将 JSON 对象转换为格式化的字符串 |
| `cJSON_Parse()` | 解析 JSON 字符串为 JSON 对象 |
| `cJSON_GetObjectItem()` | 从对象中获取指定键的项 |
| `cJSON_Delete()` | 删除 JSON 对象并释放内存 |

## 关键代码

```c
/* 创建 JSON 对象 */
cJSON *root = cJSON_CreateObject();
cJSON *info = cJSON_CreateObject();

/* 添加数据到 JSON 对象 */
cJSON_AddStringToObject(root, "name", "John Doe");
cJSON_AddNumberToObject(root, "age", 30);
cJSON_AddBoolToObject(root, "is_student", 0);
cJSON_AddItemToObject(root, "info", info);
cJSON_AddStringToObject(info, "address", "123 Main St");
cJSON_AddNumberToObject(info, "zip_code", 12345);

/* 将 JSON 对象转换为字符串 */
char *json_string = cJSON_Print(root);
if (json_string == NULL) {
    fprintf(stderr, "Failed to print JSON.\n");
    cJSON_Delete(root);
    return 1;
}

/* 解析 JSON 字符串 */
cJSON *parsed_root = cJSON_Parse(json_string);
if (parsed_root == NULL) {
    fprintf(stderr, "Failed to parse JSON.\n");
    free(json_string);
    cJSON_Delete(root);
    return 1;
}

/* 提取数据 */
cJSON *name = cJSON_GetObjectItem(parsed_root, "name");
cJSON *age = cJSON_GetObjectItem(parsed_root, "age");
cJSON *parsed_info = cJSON_GetObjectItem(parsed_root, "info");
cJSON *address = cJSON_GetObjectItem(parsed_info, "address");

/* 访问数据 */
printf("Name: %s\n", name->valuestring);
printf("Age: %d\n", age->valueint);

/* 释放内存 */
free(json_string);
cJSON_Delete(root);
cJSON_Delete(parsed_root);
```

## 配置说明

### 自定义内存分配器

本示例支持使用自定义内存分配器，通过以下配置启用：

- **`CONFIG_CJSON_CUSTOM_INCLUDE`**: 启用自定义内存分配器
- **`CONFIG_CJSON_CUSTOM_INCLUDE_PATH`**: 自定义头文件路径（默认：`"cjson_custom.h"`）

启用后，cJSON 将使用 `cjson_custom.c` 中定义的内存分配函数（`cjson_custom_malloc`、`cjson_custom_free`、`cjson_custom_realloc`）替代标准库的 `malloc`、`free`、`realloc`。

## 注意事项

1. **内存管理**: 使用 `cJSON_Print()` 返回的字符串必须使用 `free()` 释放，使用 `cJSON_Create*()` 创建的对象必须使用 `cJSON_Delete()` 释放
2. **返回值检查**: `cJSON_Print()` 和 `cJSON_Parse()` 可能返回 `NULL`，使用前必须检查返回值
3. **嵌套对象**: 使用 `cJSON_AddItemToObject()` 添加嵌套对象时，子对象会在父对象删除时自动删除，无需单独删除
4. **数据访问**: 从 `cJSON_GetObjectItem()` 获取的指针可能为 `NULL`（例如键不存在时），访问前必须检查指针有效性，否则可能导致程序崩溃
5. **字符串访问**: 访问 `cJSON` 对象的字符串值时，使用 `item->valuestring`。访问数字时，优先使用 `cJSON_GetNumberValue()` 或 `item->valuedouble`；注意 `item->valueint` 已被标记为废弃（DEPRECATED），不推荐使用
6. **线程安全**: cJSON 库本身不是线程安全的，在多线程环境中使用时需要额外的同步机制

