/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LSFS_FS_SYS_H_
#define _LSFS_FS_SYS_H_

#include "lsfs_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 文件系统接口结构体
 */
struct lsfs_file_system_t {
        /**
         * @name 文件操作
         * @{
         */
        /**
         * 根据给定的标志打开或创建文件
         *
         * @param filp 要打开/创建的文件
         * @param lsfs_path 文件路径
         * @param flags 打开/创建文件的标志
         * @return 成功返回0，失败返回负的错误码
         */
        int (*open)(struct lsfs_file_t *filp, const char *fs_path,
                    lsfs_mode_t flags);
        /**
         * 读取指定字节数的数据
         *
         * @param filp 要读取的文件
         * @param dest 目标缓冲区
         * @param nbytes 要读取的字节数
         * @return 成功返回读取的字节数，失败返回负的错误码
         */
        ssize_t (*read)(struct lsfs_file_t *filp, void *dest, size_t nbytes);
        /**
         * 写入指定字节数的数据
         *
         * @param filp 要写入的文件
         * @param src 源缓冲区
         * @param nbytes 要写入的字节数
         * @return 成功返回写入的字节数，失败返回负的错误码
         */
        ssize_t (*write)(struct lsfs_file_t *filp,
                                        const void *src, size_t nbytes);
        
        int (*fastseek_link)(struct lsfs_file_t * fp, uint32_t size);
        int (*fastseek_unlink)(struct lsfs_file_t * fp);
        int (*fastseek_relinkmap)(struct lsfs_file_t * fp);
        /**
         * 将文件位置移动到文件中的新位置
         *
         * @param filp 要移动的文件
         * @param off 相对于whence指定位置的偏移量
         * @param whence 文件中的位置。可能的值：SEEK_CUR, SEEK_SET, SEEK_END
         * @return 成功返回文件中的新位置，失败返回负的错误码
         */
        int (*lseek)(struct lsfs_file_t *filp, off_t off, int whence);
        /**
         * 获取文件当前位置
         *
         * @param filp 要获取当前位置的文件
         * @return 成功返回文件中的当前位置，失败返回负的错误码
         */
        off_t (*tell)(struct lsfs_file_t *filp);
        ssize_t (*size)(struct lsfs_file_t *filp);
#if CONFIG_FS_LARGE64_FILES
        int (*lseek64)(struct lsfs_file_t *filp, _off64_t off, int whence);
        _off64_t (*tell64)(struct lsfs_file_t *filp);
        int64_t (*size64)(struct lsfs_file_t *filp);
        int (*truncate64)(struct lsfs_file_t *filp, _off64_t length);
#endif
        /**
         * 截断/扩展文件到新的长度
         *
         * @param filp 要截断/扩展的文件
         * @param length 文件的新长度
         * @return 成功返回0，失败返回负的错误码
         */
        int (*truncate)(struct lsfs_file_t *filp, off_t length);
        /**
         * 刷新打开文件的缓存
         *
         * @param filp 要刷新的文件
         * @return 成功返回0，失败返回负的错误码
         */
        int (*sync)(struct lsfs_file_t *filp);
        /**
         * 刷新关联的流并关闭文件
         *
         * @param filp 要关闭的文件
         * @return 成功返回0，失败返回负的错误码
         */
        int (*close)(struct lsfs_file_t *filp);
        /** @} */

        /**
         * @name 目录操作
         * @{
         */
        /**
         * 打开由路径指定的现有目录
         *
         * @param dirp 要打开的目录
         * @param fs_path 目录路径
         * @return 成功返回0，失败返回负的错误码
         */
        int (*opendir)(struct lsfs_dir_t *dirp, const char *fs_path);
        /**
         * 读取目录条目
         *
         * @param dirp 要读取的目录
         * @param entry 存储目录条目信息的缓冲区
         * @return 成功返回0，失败返回负的错误码
         */
        int (*readdir)(struct lsfs_dir_t *dirp, struct lsfs_dirent *entry);
        /**
         * 关闭目录
         *
         * @param dirp 要关闭的目录
         * @return 成功返回0，失败返回负的错误码
         */
        int (*closedir)(struct lsfs_dir_t *dirp);
        /** @} */

        /**
         * @name 文件系统操作
         * @{
         */
        /**
         * 挂载文件系统
         *
         * @param mountp 挂载点信息
         * @return 成功返回0，失败返回负的错误码
         */
        int (*mount)(struct lsfs_mount_t *mountp);
        /**
         * 卸载文件系统
         *
         * @param mountp 挂载点信息
         * @return 成功返回0，失败返回负的错误码
         */
        int (*unmount)(struct lsfs_mount_t *mountp);
        /**
         * 创建目录
         *
         * @param mountp 挂载点信息
         * @param path 目录路径
         * @return 成功返回0，失败返回负的错误码
         */
        int (*mkdir)(struct lsfs_mount_t *mountp, const char *path);
        /**
         * 删除文件
         *
         * @param mountp 挂载点信息
         * @param path 文件路径
         * @return 成功返回0，失败返回负的错误码
         */
        int (*unlink)(struct lsfs_mount_t *mountp, const char *path);
        /**
         * 重命名文件
         *
         * @param mountp 挂载点信息
         * @param from 原文件路径
         * @param to 新文件路径
         * @return 成功返回0，失败返回负的错误码
         */
        int (*rename)(struct lsfs_mount_t *mountp, const char *from,
                     const char *to);
        /**
         * 获取文件状态
         *
         * @param mountp 挂载点信息
         * @param path 文件路径
         * @param entry 存储文件状态的缓冲区
         * @return 成功返回0，失败返回负的错误码
         */
        int (*stat)(struct lsfs_mount_t *mountp, const char *path,
                   struct lsfs_dirent *entry);

        /**
         * 获取当前工作目录
         *
         * @param mountp 挂载点信息
         * @param buf 用于存储路径的缓冲区
         * @param size 缓冲区大小
         * @return 成功返回0，失败返回负的错误码
         */
        int (*getcwd)(struct lsfs_mount_t *mountp, char *buf, size_t size);

        /**
         * 改变当前工作目录
         *
         * @param mountp 挂载点信息
         * @param pathname 新的当前工作目录路径
         * @return 成功返回0，失败返回负的错误码
         */
        int (*chdir)(struct lsfs_mount_t *mountp, const char *pathname);

        /**
         * @brief 改变当前驱动器
         * 
         * @param pathname 新的当前驱动器路径
         * @return 成功返回0，失败返回负的错误码
         */
        int (*chdrive)(const char *pathname);

        /**
         * 获取文件系统状态
         *
         * @param mountp 挂载点信息
         * @param path 文件系统路径
         * @param stat 存储文件系统状态的缓冲区
         * @return 成功返回0，失败返回负的错误码
         */
        int (*statvfs)(struct lsfs_mount_t *mountp, const char *path,
                      struct lsfs_statvfs *stat);
        /**
         * 格式化设备为指定的文件系统类型
         * 仅当启用CONFIG_FILE_SYSTEM_MKFS时可用
         *
         * @param mountp 挂载点信息
         * @return 成功返回0，失败返回负的错误码
         */
        int (*mkfs)(const char* dev, void *cfg, int flags);

        /** @} */
};

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _LSFS_FS_SYS_H_ */
