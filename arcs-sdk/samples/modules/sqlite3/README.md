# SQLite3 示例

本示例展示了如何在 ARCS SDK 环境中使用 SQLite 数据库及其加密功能。主要演示了使用 SQLite3 进行基本数据库操作，并与 LSFS（轻量级存储文件系统）集成。

## 功能特点

- SQLite3 数据库操作（打开、查询）
- 数据库加密支持
- LSFS 安全文件存储集成


## 构建和运行

1. 编译：

```{eval-rst}
.. include:: /sample_build.rst
```

2. 烧录：

- 将生成的 arcs.bin 文件烧录到开发板子的 0x00 地址;

> cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 ./build/arcs.bin

- 将预置的带有数据库的文件系统镜像 fatfs_db.bin 烧录到 EMMC 的 0x00 地址;

> cskburn -s /dev/ttyACM0 -b 3000000 -C arcs --emmc 0x0 ./tools/image/fatfs_db.bin

3. 运行:
-  重启设备，可以看到一下日志信息：

    ```bash
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

## 附加说明

示例程序已经包含预先打包好的文件系统镜像 `fatfs_db.bin`，可以直接使用。如果需要修改文件系统内容，可以参考以下步骤：

### 1. 创建 FAT 文件系统镜像（可选）

使用 `makefs` 工具将数据库打包到 FAT 文件系统镜像中：

```bash
./makefs.sh
```
默认会在`image`目录下生成 `fatfs_db.bin` 文件。