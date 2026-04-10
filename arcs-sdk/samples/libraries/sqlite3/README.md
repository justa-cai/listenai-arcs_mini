# SQLite3 数据库操作示例

## 功能说明

演示如何在 ARCS SDK 环境中使用 SQLite3 数据库进行基本操作，包括数据库打开、加密查询以及与 LSFS 文件系统集成。

## 硬件连接

- **SDMMC0 接口**：连接 eMMC/SD 卡，用于存储数据库文件

## 示例内容

1. 初始化 SD 卡和 LSFS 文件系统
2. 挂载 FAT 文件系统到 `/SD:` 挂载点
3. 遍历根目录并列出数据库文件
4. 打开加密数据库文件
5. 执行 SQL 全文搜索查询并输出结果
6. 关闭数据库并卸载文件系统

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
SQLITE sample
/SD: mounted successfully
/SD:/ opendir: 0
25747456 test.db
26009600 test_csk_enc.db
25747456 test_wx_enc.db
0 dirs, 3 files.
Callback function called

SDK: WX_SQLITE3
DB: /SD:/test_wx_enc.db
SQL: select docid,type,audio_type,word_text from v_word WHERE word_text MATCH '小*时*不*识*月*，*呼*作*白*玉*盘*。'
+------------+---------+
| Operation  | Time(ms)  |
+------------+---------+
| Open      |    2    |
| Key       |  115    |
| Query     |  216    |
+------------+---------+
/SD: unmount success!
```

## 核心 API

| API | 说明 |
|-----|------|
| `sqlite3_open()` | 打开或创建 SQLite 数据库 |
| `sqlite3_key()` | 设置数据库加密密钥 |
| `sqlite3_exec()` | 执行 SQL 语句 |
| `sqlite3_close()` | 关闭数据库 |
| `lsfs_mount()` | 挂载文件系统 |
| `lsfs_unmount()` | 卸载文件系统 |

## 关键代码

```c
/* 挂载文件系统 */
lsfs_mount(&sdmmc_lsfs_mnt);

/* 打开加密数据库 */
sqlite3 *db;
sqlite3_open("/SD:/test_wx_enc.db", &db);
sqlite3_key(db, key, key_len);

/* 执行查询 */
sqlite3_exec(db, sql, callback, NULL, &err_msg);

/* 关闭数据库 */
sqlite3_close(db);
lsfs_unmount(&sdmmc_lsfs_mnt);
```

## 注意事项

1. **文件系统镜像**: 运行前需将包含数据库文件的 `fatfs_db.bin` 镜像烧录到 eMMC
2. **烧录顺序**: 先烧录文件系统镜像到 eMMC，再烧录应用固件到 Flash
3. **加密支持**: 数据库加密功能需要对应的加密模块支持，密钥需与数据库创建时一致
4. **内存需求**: SQLite3 运行需要较多内存，确保堆空间配置足够
5. **文件路径**: 数据库文件路径必须包含挂载点前缀，如 `/SD:/test.db`
