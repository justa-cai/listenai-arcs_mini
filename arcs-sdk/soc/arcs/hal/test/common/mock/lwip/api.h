#ifndef LWIP_API_H
#define LWIP_API_H

#include <stdint.h>
#include "lwip/tcpip.h"

#define NETCONN_EVT_RCVPLUS 1

struct netconn {
    int recvmbox;
    uint16_t recv_avail;
};

#define API_EVENT(conn, evt, len) do { (void)(conn); (void)(evt); (void)(len); } while (0)
#define SYS_ARCH_INC(var, inc) do { (var) = (uint16_t)((var) + (inc)); } while (0)

#define MEMP_NETBUF 1

void *memp_malloc(int type);
int sys_mbox_trypost(int *mbox, void *msg);

#endif
