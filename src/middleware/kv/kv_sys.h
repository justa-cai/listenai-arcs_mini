#ifndef __KV_SYS_H__
#define __KV_SYS_H__

#define KV_KEY_SYS_WAKEWORD "sys.wakeword"

/* 系统 OTA 死循环防护：
 * - pending_pkg: APP 写入 uboot 请求前记录的服务端 package_id，重启回来后
 *   用来识别"上次尝试的是哪一个具体包"；成功或失败都会清空
 * - failed_pkg:  pending_pkg 对应的尝试被 boot 判失败时，落黑名单；下一轮
 *   服务端仍返回同一 package_id 就直接跳过。用 package_id 而非
 *   version_number 是为了在服务端把坏包替换成同版本新包（新 id）时能
 *   自动放行重试 */
#define KV_KEY_SYS_OTA_PENDING_PKG "sys.ota.pending_pkg"
#define KV_KEY_SYS_OTA_FAILED_PKG  "sys.ota.failed_pkg"

#endif
