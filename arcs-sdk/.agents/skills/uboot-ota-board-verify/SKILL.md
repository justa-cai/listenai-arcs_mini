---
name: uboot-ota-board-verify
description: Deprecated in arcs-sdk. Full uboot OTA board verification moved with the standalone uboot application project.
---

# U-Boot OTA Board Verify

此技能不再在 arcs-sdk 内执行。

arcs-sdk 只保留 APP 侧 `boot_api` 示例；recovery boot、OTA 包解析、ADB recovery 和完整板级 OTA 闭环验证由独立 uboot 应用工程维护。需要完整 Flash/TF OTA 板测时，请在独立 uboot 工程中运行对应验证流程。
