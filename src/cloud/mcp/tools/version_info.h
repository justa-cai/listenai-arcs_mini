#ifndef VERSION_INFO_H
#define VERSION_INFO_H

/**
 * @brief 获取设备固件版本信息
 * 
 * @return const char* 版本信息字符串
 */
const char* get_device_firmware_version(void);

/**
 * @brief 获取设备固件提交信息
 * 
 * @return const char* 提交信息字符串
 */
const char* get_device_firmware_commit(void);

#endif // VERSION_INFO_H
