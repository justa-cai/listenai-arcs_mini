# CAE 资源目录

当前 sample 直接调用原生 CAE 算法库，需要烧录两个资源：

- `cae_aes.bin`: CAE AES 模型资源，示例来源为 `/home/hiker/res_manage_tool/res_unpk/cae_aes.bin`。
- `cae_ffw0_100.json`: CAE 参数 JSON，已随 sample 提供。

如果 `cae_aes.bin` 变化，需要同步修改 `CONFIG_ACOMP_CAE_RES_CAE_AES_LENGTH`。
如果 `cae_ffw0_100.json` 变化，需要同步修改 `CONFIG_ACOMP_CAE_RES_CAE_JSON_LENGTH`。
