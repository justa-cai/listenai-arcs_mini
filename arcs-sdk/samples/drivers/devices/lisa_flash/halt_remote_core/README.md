# LISA Flash 双核环境读写示例

## 功能说明

本示例演示在双核环境下使用 LISA Flash 驱动，通过启用 `CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE` 配置，在 Flash 操作期间自动停止远端核心，确保 Flash 访问的安全性和数据完整性。

## 硬件连接

无需外部连接。AP 核作为 Boot Core 启动并引导 CP 核，CP 核执行 Flash 操作时通过 IPC 机制自动停止和恢复 AP 核。

## 示例步骤

1. AP 核启动并引导 CP 核，双核通过 IPC 建立通信
2. CP 核获取 Flash 设备并查询设备信息
3. CP 核擦除 Flash 指定区域（自动通过 IPC 停止 AP 核）
4. CP 核写入测试数据到 Flash（自动停止 AP 核）
5. CP 核读取 Flash 数据并验证完整性
6. 循环执行多轮测试验证稳定性

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**CP 核输出（Master）：**

```
========================================
=== LISA Flash Dual-Core Example ===
===      (CP Core - Master)        ===
========================================

1. Getting flash device 'flash0'...
   Flash device 'flash0' is ready and auto-initialized at 0x...

=== Flash Device Information ===
Write block size: 1 bytes
Erase value: 0xFF
...

=== Flash Read/Write Test (Iteration 1) ===
Using page size: 4096 bytes

2. Erasing flash at offset 0x100000, size 4096 bytes...
   [CP Core] Halting remote core (AP) during erase...
   Erase successful
   [CP Core] Remote core (AP) resumed

3. Writing 256 bytes to flash at offset 0x100000...
   [CP Core] Halting remote core (AP) during write...
   Write successful
   [CP Core] Remote core (AP) resumed

4. Reading 256 bytes from flash at offset 0x100000...
   Read successful

5. Verifying data...
   Verification successful! All 256 bytes match.

=== Test Iteration 1 Completed Successfully ===
...
```

**AP 核输出（Remote）：**

```
========================================
=== LISA Flash Dual-Core Example ===
===      (AP Core - Remote)        ===
========================================

[remote_main] CP core booted.
[remote_main] IPC remote initialized.
[remote_main] ipc_utils_before_halt_by_peer_core
[remote_main] ipc_utils_after_resume_by_peer_core
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 Flash 设备实例 |
| `lisa_flash_get_parameters()` | 获取 Flash 参数信息 |
| `lisa_flash_erase()` | 擦除 Flash 区域（自动停止远端核心） |
| `lisa_flash_write()` | 写入 Flash 数据（自动停止远端核心） |
| `lisa_flash_read()` | 读取 Flash 数据 |

## 关键代码

```c
/* Flash 设备已自动初始化，直接获取 */
lisa_device_t *flash = lisa_device_get("flash0");

/* 擦除（自动通过 IPC 停止 AP 核） */
lisa_flash_erase(flash, offset, page_size);

/* 写入（自动通过 IPC 停止 AP 核） */
lisa_flash_write(flash, offset, data, size);

/* 读取 */
lisa_flash_read(flash, offset, buffer, size);
```

## 配置说明

```kconfig
CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE=y  # 启用双核 Flash 保护
```

## 注意事项

1. **双核协同**：AP 核作为 Boot Core 负责启动 CP 核，CP 核执行 Flash 操作
2. **自动停止**：Flash 擦除和写入操作时会自动通过 IPC 停止远端核心
3. **地址对齐**：擦除操作的地址和大小必须按照页面大小对齐
4. **写入前擦除**：大多数 Flash 设备在写入前需要先擦除目标区域
5. **一键构建**：主工程 CMake 自动编译两个核心的固件
