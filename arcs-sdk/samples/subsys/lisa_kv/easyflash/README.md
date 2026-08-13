# LISA KV EasyFlash 示例

## 功能说明

演示如何使用 LISA KV 组件的 EasyFlash 后端保存和读取键值数据。示例覆盖 `int`、`string`、`bool` 和 `blob` 四种常用数据类型，并展示读取动态内存后的释放方式。

## 硬件连接

无需外部连接，示例使用芯片内部 Flash 上的 EasyFlash ENV 区域保存 KV 数据。

## 示例内容

1. 调用 `lisa_kv_init()` 初始化 EasyFlash 后端。
2. 使用 `lisa_kv_set_int()` 和 `lisa_kv_get_int()` 保存和读取整型数据。
3. 使用 `lisa_kv_set_string()` 和 `lisa_kv_get_string()` 保存和读取字符串。
4. 使用 `lisa_kv_set_bool()` 和 `lisa_kv_get_bool()` 保存和读取布尔值。
5. 使用 `lisa_kv_set_blob()` 和 `lisa_kv_get_blob()` 保存和读取二进制数据。
6. 使用 `lisa_kv_free()` 释放读取接口返回的动态内存，并使用 `lisa_kv_del()` 删除示例 key。

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```text
[I][lisa_kv_sample] === LISA KV EasyFlash Example ===
[I][lisa_kv_sample] lisa_kv_init ok
[I][lisa_kv_sample] set int ok
[I][lisa_kv_sample] get int ok
[I][lisa_kv_sample] int value: 1234
[I][lisa_kv_sample] set string ok
[I][lisa_kv_sample] get string ok
[I][lisa_kv_sample] string value: hello easyflash kv
[I][lisa_kv_sample] set bool ok
[I][lisa_kv_sample] get bool ok
[I][lisa_kv_sample] bool value: true
[I][lisa_kv_sample] set blob ok
[I][lisa_kv_sample] get blob ok
[I][lisa_kv_sample] blob value len: 5
[I][lisa_kv_sample] dump EasyFlash env after write:
[I][lisa_kv_sample] demo keys deleted
[I][lisa_kv_sample] === LISA KV EasyFlash Example completed ===
```

`lisa_kv_dump()` 会打印 EasyFlash ENV 中的当前内容，具体输出会随设备上已有 ENV 数据变化。

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_kv_init()` | 初始化 KV 后端，EasyFlash 模式下会调用 `easyflash_init()` |
| `lisa_kv_set_int()` | 写入整型值 |
| `lisa_kv_get_int()` | 读取整型值 |
| `lisa_kv_set_string()` | 写入字符串 |
| `lisa_kv_get_string()` | 读取字符串，返回内存需要用 `lisa_kv_free()` 释放 |
| `lisa_kv_set_bool()` | 写入布尔值 |
| `lisa_kv_get_bool()` | 读取布尔值 |
| `lisa_kv_set_blob()` | 写入二进制数据 |
| `lisa_kv_get_blob()` | 读取二进制数据，返回内存需要用 `lisa_kv_free()` 释放 |
| `lisa_kv_del()` | 删除指定 key |
| `lisa_kv_dump()` | 打印当前 KV 后端内容 |

## 关键代码

```c
if (lisa_kv_init() != 0) {
    return -1;
}

lisa_kv_set_int("kv.demo.int", 1234);

int value = 0;
if (lisa_kv_get_int("kv.demo.int", &value) == 0) {
    /* 使用 value */
}

char *string_value = NULL;
lisa_kv_set_string("kv.demo.str", "hello easyflash kv");
if (lisa_kv_get_string("kv.demo.str", &string_value) == 0) {
    /* 使用 string_value */
    lisa_kv_free(string_value);
}

uint8_t blob[] = { 0x11, 0x22, 0x33 };
lisa_kv_set_blob("kv.demo.blob", blob, sizeof(blob));

uint8_t *blob_value = NULL;
int blob_len = 0;
if (lisa_kv_get_blob("kv.demo.blob", &blob_value, &blob_len) == 0) {
    /* 使用 blob_value/blob_len */
    lisa_kv_free(blob_value);
}
```

## 配置说明

示例默认使用 EasyFlash 后端：

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_FLASH=y
CONFIG_LISA_KV=y
CONFIG_LISA_KV_TYPE_EF=y
CONFIG_LS_EF_START_ADDR=0x30100000
CONFIG_LS_EF_ENV_AREA_SIZE=8192
CONFIG_LS_EF_ERASE_MIN_SIZE=4096
```

`CONFIG_LISA_KV_TYPE_EF` 会选择 EasyFlash 模块，EasyFlash 本身依赖 LISA Flash，因此示例显式打开 `CONFIG_LISA_FLASH` 和 `CONFIG_LISA_DEVICE`。

### EasyFlash 存储区域

| 配置项 | 示例值 | 说明 |
|--------|--------|------|
| `CONFIG_LS_EF_START_ADDR` | `0x30100000` | EasyFlash ENV 起始地址 |
| `CONFIG_LS_EF_ENV_AREA_SIZE` | `8192` | ENV 区域大小，单位为字节 |
| `CONFIG_LS_EF_ERASE_MIN_SIZE` | `4096` | Flash 最小擦除粒度，单位为字节 |

上述地址和大小来自 `modules/EasyFlash/Kconfig` 的默认值，本示例显式写入 `prj.conf`，便于根据实际 Flash 分区调整。量产工程需要确认该区域不与 boot、app、OTA、资源分区或其他持久化数据重叠。

## 注意事项

1. **后端选择**：本示例使用 `CONFIG_LISA_KV_TYPE_EF`，不需要文件系统 mount；如果切换到 LSFS 后端，需要先完成文件系统初始化。
2. **内存释放**：`lisa_kv_get_string()` 和 `lisa_kv_get_blob()` 返回的内存必须用 `lisa_kv_free()` 释放。
3. **key 长度**：EasyFlash ENV key 长度受 `CONFIG_LS_EF_ENV_NAME_MAX` 限制，默认最大 32 字节。
4. **数据清理**：示例只删除自身使用的 `kv.demo.*` key，不调用 `lisa_kv_clear()`，避免清空设备上的其他 KV 数据。
5. **持久化影响**：EasyFlash 写入会占用 Flash ENV 区域，量产应用应规划好 ENV 区域地址和大小。
