#ifndef __LISA_EVS_CONFIG_H__
#define __LISA_EVS_CONFIG_H__

//client_id，iflyos平台分配
//https://device.iflyos.cn/product/device-info
//从设备控制台=>产品信息=>client_id 获取
#define PRODUCT_CLIENT_ID ("3244a1f2-a4a2-4a96-8457-d1228d66f787")

//正式集成的时候需要从设备中获取，不同的设备deviceid不一样，不能重复！！！
//https://device.iflyos.cn/product/device-id
//从上述链接导入设备白名单，并且确保每台设备device_id不一样，否则产生设备竞争，导致设备和账号异常。
#define PRODUCT_DEVICE_ID ("12347")

//ota_secret_id，iflyos平台分配
//https://device.iflyos.cn/product/auto-update 
//从设备控制台=>自动更新=>加密密钥 获取
#define PRODUCT_OTA_SECRET_ID ("5e38fe23-6194-4a25-bcf8-0bf65c30ad61")

#endif // __LISA_EVS_CONFIG_H__
