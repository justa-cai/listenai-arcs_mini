/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LSFS_H_
#define _LSFS_H_

#include <sys/types.h>
#include "fs_env/dlist.h"
#include "lsfs_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

/**
 * @brief 文件系统API
 * @defgroup file_system_api 文件系统API
 * @since 1.5
 * @version 1.0.0
 * @ingroup os_services
 * @{
 */

/**
 * @brief 目录条目类型枚举
 */
enum lsfs_dir_entry_type {
        /** 文件类型 */
        LSFS_DIR_ENTRY_FILE = 0,
        /** 目录类型 */
        LSFS_DIR_ENTRY_DIR
};

/**
 * @brief 文件系统类型枚举
 */
enum {
        /** FatFS文件系统 */
        LSFS_FATFS = 0,
        /** 外部文件系统 */
        LSFS_TYPE_EXTERNAL_BASE,
};

/** 将挂载的文件系统设置为只读 */
#define LSFS_MOUNT_FLAG_READ_ONLY BIT(1)



/**
 * @brief 文件系统挂载信息结构
 */
struct lsfs_mount_t {
        /** lsfs_mount_list链表中的条目 */
        sys_dnode_t node;
        /** 文件系统类型 */
        int type;
        /** 挂载点目录名（如："/fatfs"） */
        const char *mnt_point;
        /** 指向文件系统特定数据的指针 */
        void *fs_data;
        /** 指向后端存储设备的指针 */
        void *storage_dev;
        /* 下面的字段由文件系统核心填充 */
        /** 挂载点字符串长度 */
        size_t mountp_len;
        /** 指向挂载点文件系统接口的指针 */
        const struct lsfs_file_system_t *fs;
        /** 挂载标志 */
        uint8_t flags;
};

/**
 * @brief 接收文件或目录信息的结构体
 *
 * 在读取目录条目时用于获取文件或目录信息。
 */
struct lsfs_dirent {
        /**
         * 文件/目录类型（FS_DIR_ENTRY_FILE或FS_DIR_ENTRY_DIR）
         */
        enum lsfs_dir_entry_type type;
        /** 文件或目录名 */
        char name[MAX_FILE_NAME + 1];
        /** 文件大小（目录为0） */
        size_t size;
};

/**
 * @brief 接收卷统计信息的结构体
 *
 * 用于获取卷的总空间和可用空间信息。
 */
struct lsfs_statvfs {
        /** 最佳传输块大小 */
        unsigned long f_bsize;
        /** 分配单元大小 */
        unsigned long f_frsize;
        /** 文件系统大小（以f_frsize为单位） */
        unsigned long f_blocks;
        /** 空闲块数 */
        unsigned long f_bfree;
};

/**
 * @name lsfs_open打开和创建模式标志
 * @{
 */
/** 读模式标志 */
#define LSFS_O_READ       0x01
/** 写模式标志 */
#define LSFS_O_WRITE      0x02
/** 读写模式标志组合 */
#define LSFS_O_RDWR       (LSFS_O_READ | LSFS_O_WRITE)
/** 读写模式标志掩码 */
#define LSFS_O_MODE_MASK  0x03

/** 如果文件不存在则创建 */
#define LSFS_O_CREATE     0x10
/** 以追加模式打开/创建文件 */
#define LSFS_O_APPEND     0x20
/** 打开文件时清空文件 */
#define LSFS_O_TRUNC      0x40
/** 打开/创建标志掩码 */
#define LSFS_O_FLAGS_MASK 0x70

/** 打开标志掩码 */
#define LSFS_O_MASK       (LSFS_O_MODE_MASK | LSFS_O_FLAGS_MASK)
/**
 * @}
 */

/**
 * @name lsfs_seek whence参数值
 * @{
 */
#ifndef LSFS_SEEK_SET
/** 从文件开始位置查找 */
#define LSFS_SEEK_SET	0
#endif
#ifndef LSFS_SEEK_CUR
/** 从当前位置查找 */
#define LSFS_SEEK_CUR	1
#endif
#ifndef LSFS_SEEK_END
/** 从文件末尾位置查找 */
#define LSFS_SEEK_END	2
#endif
/**
 * @}
 */


static inline void lsfs_file_t_init(struct lsfs_file_t *fp)
{
	fp->filep = NULL;
	fp->mp = NULL;
	fp->flags = 0;
}

static inline void lsfs_dir_t_init(struct lsfs_dir_t *dp)
{
	dp->dirp = NULL;
	dp->mp = NULL;
}
/**
 * @brief 打开或创建文件
 * 
 * @param fp 文件对象指针
 * @param file_name 文件名
 * @param flags 打开模式标志
 *        - LSFS_O_READ: 读模式
 *        - LSFS_O_WRITE: 写模式
 *        - LSFS_O_RDWR: 读写模式
 *        - LSFS_O_CREATE: 不存在则创建
 *        - LSFS_O_APPEND: 追加模式
 *        - LSFS_O_TRUNC: 打开时清空文件
 * 
 * @retval 0 成功
 * @retval -EBUSY 文件对象已被使用
 * @retval -EINVAL 无效的文件名
 * @retval -EROFS 只读文件系统
 * @retval -ENOENT 文件不存在
 */
int lsfs_open(struct lsfs_file_t *fp, const char *file_name, lsfs_mode_t flags);

/**
 * @brief 关闭文件
 * 
 * @param fp 文件对象指针
 * 
 * @retval 0 成功
 * @retval -ENOTSUP 文件系统不支持
 */
int lsfs_close(struct lsfs_file_t *fp);

/**
 * @brief 删除文件或目录
 * 
 * @param path 文件或目录路径
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -EROFS 只读文件系统
 */
int lsfs_unlink(const char *path);

/**
 * @brief 重命名文件或目录
 * 
 * @param from 源路径
 * @param to 目标路径
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -EROFS 只读文件系统
 */
int lsfs_rename(const char *from, const char *to);

/**
 * @brief 读取文件数据
 * 
 * @param fp 文件对象指针
 * @param ptr 数据缓冲区
 * @param size 要读取的字节数
 * 
 * @retval >0 实际读取的字节数
 * @retval -EBADF 无效的文件对象
 */
ssize_t lsfs_read(struct lsfs_file_t *fp, void *ptr, size_t size);

/**
 * @brief 写入文件数据
 * 
 * @param fp 文件对象指针
 * @param ptr 数据缓冲区
 * @param size 要写入的字节数
 * 
 * @retval >0 实际写入的字节数
 * @retval -EBADF 无效的文件对象
 */
ssize_t lsfs_write(struct lsfs_file_t *fp, const void *ptr, size_t size);

/**
 * @brief 设置文件系统簇映射表
 * 
 * @param fp 文件对象指针
 * @param size 映射表大小
 * 
 * @retval 0 成功
 * @retval -EBADF 无效的文件对象
 */
int lsfs_fastseek_link(struct lsfs_file_t * fp, uint32_t size);

/**
 * @brief 取消文件系统簇映射表
 * 
 * @param fp 文件对象指针
 * 
 * @retval 0 成功
 * @retval -EBADF 无效的文件对象
 */
int lsfs_fastseek_unlink(struct lsfs_file_t * fp);

/**
 * @brief 重新建设文件系统簇映射表
 * 
 * @param fp 文件对象指针
 * 
 * @retval 0 成功
 * @retval -EBADF 无效的文件对象
 */
int lsfs_fastseek_relinkmap(struct lsfs_file_t * fp);

/**
 * @brief 设置文件位置
 * 
 * @param fp 文件对象指针
 * @param offset 偏移量
 * @param whence 起始位置
 *        - LSFS_SEEK_SET: 文件开头
 *        - LSFS_SEEK_CUR: 当前位置
 *        - LSFS_SEEK_END: 文件末尾
 * 
 * @retval 0 成功
 * @retval -EBADF 无效的文件对象
 */
int lsfs_seek(struct lsfs_file_t *fp, off_t offset, int whence);

/**
 * @brief 获取当前文件位置
 * 
 * @param fp 文件对象指针
 * 
 * @retval >=0 当前位置
 * @retval -EBADF 无效的文件对象
 */
off_t lsfs_tell(struct lsfs_file_t *fp);

off_t lsfs_lsize(struct lsfs_file_t *fp);
#if CONFIG_FS_LARGE64_FILES
int lsfs_seek64(struct lsfs_file_t * fp, _off64_t offset, int whence);
_off64_t lsfs_tell64(struct lsfs_file_t * fp);
_off64_t lsfs_lsize64(struct lsfs_file_t *fp);
int lsfs_truncate64(struct lsfs_file_t *fp, _off64_t length);
#endif

/**
 * @brief 截断或扩展文件
 * 
 * @param fp 文件对象指针
 * @param length 新的文件长度
 * 
 * @retval 0 成功
 * @retval -EBADF 无效的文件对象
 */
int lsfs_truncate(struct lsfs_file_t *fp, off_t length);

/**
 * @brief 同步文件数据到存储设备
 * 
 * @param fp 文件对象指针
 * 
 * @retval 0 成功
 * @retval -EBADF 无效的文件对象
 */
int lsfs_sync(struct lsfs_file_t *fp);

/**
 * @brief 创建目录
 * 
 * @param path 目录路径
 * 
 * @retval 0 成功
 * @retval -EEXIST 目录已存在
 * @retval -EROFS 只读文件系统
 */
int lsfs_mkdir(const char *path);

/**
 * @brief 打开目录
 * 
 * @param zdp 目录对象指针
 * @param path 目录路径
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -EBUSY 目录对象已被使用
 */
int lsfs_opendir(struct lsfs_dir_t *zdp, const char *path);

/**
 * @brief 读取目录项
 * 
 * @param zdp 目录对象指针
 * @param entry 目录项结构指针
 * 
 * @retval 0 成功或到达目录末尾
 * @retval -ENOENT 目录不存在
 */
int lsfs_readdir(struct lsfs_dir_t *zdp, struct lsfs_dirent *entry);

/**
 * @brief 关闭目录
 * 
 * @param zdp 目录对象指针
 * 
 * @retval 0 成功
 */
int lsfs_closedir(struct lsfs_dir_t *zdp);


int lsfs_mount(struct lsfs_mount_t *mp);
int lsfs_unmount(struct lsfs_mount_t *mp);

/**
 * @brief 获取文件或目录状态
 * 
 * @param path 文件或目录路径
 * @param entry 状态信息结构指针
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -ENOENT 文件或目录不存在
 */
int lsfs_stat(const char *path, struct lsfs_dirent *entry);

/**
 * @brief 获取当前工作目录
 * 
 * @param mountpath 挂载点路径
 * @param buf 用于存储路径的缓冲区
 * @param size 缓冲区大小
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -ENOENT 路径不存在
 */
int lsfs_getcwd(const char *mountpath, char *buf, size_t size);

/**
 * @brief 改变当前工作目录
 * 
 * @param pathname 新的当前工作目录路径
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -ENOENT 路径不存在
 */
int lsfs_chdir(const char *pathname);


/**
 * @brief 改变当前工作drive
 * 
 * @param pathname 新的drive路径
 * 
 * @retval 0 成功
 * @retval -EINVAL 无效的路径
 * @retval -ENOENT 路径不存在
 */
int lsfs_chdrive(const char *pathname);


int lsfs_statvfs(const char *path, struct lsfs_statvfs *stat);
/**
 * @brief 格式化文件系统
 * 
 * @param lsfs_type 文件系统类型
 * @param dev 设备名称
 * @param cfg 配置参数，NULL则使用默认配置
 * @param flags 格式化标志
 * 
 * @retval 0 成功
 * @retval <0 错误码
 */
int lsfs_mkfs(int lsfs_type, const char* dev, void *cfg, int flags);

/**
 * @brief 注册文件系统
 * 
 * @param type 文件系统类型
 * @param fs 文件系统接口指针
 * 
 * @retval 0 成功
 * @retval -EALREADY 文件系统已注册
 * @retval -ENOSCP 没有空间注册文件系统
 */
int lsfs_register(int type, const struct lsfs_file_system_t *fs);

/**
 * @brief 取消注册文件系统
 * 
 * @param type 文件系统类型
 * @param fs 文件系统接口指针
 * 
 * @retval 0 成功
 * @retval -EINVAL 文件系统未注册
 */
int lsfs_unregister(int type, const struct lsfs_file_system_t *fs);

/**
 * @brief 初始化LSFS文件系统框架
 * 
 * @retval 0 成功
 * @retval < 0 初始化失败
 */
int lsfs_init(void);

#if CONFIG_LSFS_SUPPORT_FREEZE
/**
 * @brief Freeze file system by closing all open files
 * 
 * This function is called when the system needs to power down.
 * It closes all currently open files without modifying the original
 * files and directories structure.
 * 
 * @param flags Reserved for future use, should be 0
 * 
 * @retval 0 Success
 * @retval <0 Error code if some files failed to close
 */
int lsfs_freeze_freeze(uint32_t flags);
#endif
/**
 * @}
 */


#ifdef __cplusplus
}
#endif

#endif /* _LSFS_H_ */
