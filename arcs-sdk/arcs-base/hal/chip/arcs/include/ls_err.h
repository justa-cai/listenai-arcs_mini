#ifndef __LS_ERR_H_
#define __LS_ERR_H_

#pragma once

#include <stdint.h>
#include <stddef.h>

typedef int ls_err_t;

#define LS_OK                      0
#define LS_FAIL                    -1

#define LS_ERR_WIFI_BASE           (-0x100)
#define LS_ERR_NET_IP_BASE         (-0x150)
#define LS_ERR_EVENT_BASE          (-0x200)
#define LS_ERR_COMMON_BASE         (-0x300)


// WIFI error code
#define LS_ERR_WIFI_NOT_CONNECT        (LS_ERR_WIFI_BASE - 1)
#define LS_ERR_WIFI_NOT_AX             (LS_ERR_WIFI_BASE - 2)
#define LS_ERR_WIFI_NOT_STA_MODE       (LS_ERR_WIFI_BASE - 3)
#define LS_ERR_WIFI_NOT_AP_MODE        (LS_ERR_WIFI_BASE - 4)
#define LS_ERR_WIFI_INVALID_RATE       (LS_ERR_WIFI_BASE - 5)
#define LS_ERR_WIFI_INVALID_COUNTRY_CODE       (LS_ERR_WIFI_BASE - 6)
#define LS_ERR_WIFI_DIS_NETWORK       (LS_ERR_WIFI_BASE - 7)
#define LS_ERR_WIFI_VIF_OPT           (LS_ERR_WIFI_BASE - 8)

// net ip error code
#define LS_NET_IP_ERR                  (LS_ERR_NET_IP_BASE - 1)
#define LS_NET_IP_ERR_VIFIDX           (LS_ERR_NET_IP_BASE - 2)
#define LS_NET_IP_ERR_NONETIF          (LS_ERR_NET_IP_BASE - 3)
#define LS_NET_IP_ERR_IFDOWN           (LS_ERR_NET_IP_BASE - 4)
#define LS_NET_IP_ERR_DHCPCUP          (LS_ERR_NET_IP_BASE - 5)
#define LS_NET_IP_ERR_DHCPCIP          (LS_ERR_NET_IP_BASE - 6)
#define LS_NET_IP_ERR_DHCPCABORT       (LS_ERR_NET_IP_BASE - 7)
#define LS_NET_IP_ERR_DHCPCRELEASE     (LS_ERR_NET_IP_BASE - 7)


// COMMON error code
#define LS_ERR_NOT_INIT            (LS_ERR_COMMON_BASE)
#define LS_ERR_PARAM               (LS_ERR_COMMON_BASE - 1)
#define LS_ERR_NOT_FOUND           (LS_ERR_COMMON_BASE - 2)
#define LS_ERR_OPEN                (LS_ERR_COMMON_BASE - 3)
#define LS_ERR_IN_PROGRESS         (LS_ERR_COMMON_BASE - 4)
#define LS_ERR_NO_MEM              (LS_ERR_COMMON_BASE - 5)
#define LS_ERR_TIMEOUT             (LS_ERR_COMMON_BASE - 6)
#define LS_ERR_STATE               (LS_ERR_COMMON_BASE - 7)
#define LS_ERR_TRY_AGAIN           (LS_ERR_COMMON_BASE - 8)
#define LS_ERR_NULL_PARAM          (LS_ERR_COMMON_BASE - 9)
#define LS_ERR_NOT_SUPPORT         (LS_ERR_COMMON_BASE - 10)
#define LS_ERR_BUSY                (LS_ERR_COMMON_BASE - 11)
#define LS_ERR_PATH                (LS_ERR_COMMON_BASE - 12)



#endif
