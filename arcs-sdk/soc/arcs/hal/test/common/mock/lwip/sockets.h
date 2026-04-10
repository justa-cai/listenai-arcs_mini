#ifndef LWIP_SOCKETS_H
#define LWIP_SOCKETS_H

#include <stddef.h>
#include <stdint.h>

typedef unsigned int socklen_t;

#define PF_INET 2
#define SOCK_DGRAM 2
#define SOL_SOCKET 1
#define SO_CONNINFO 2

int lwip_socket(int domain, int type, int protocol);
int lwip_getsockopt(int sock, int level, int optname, void *optval, socklen_t *optlen);
int lwip_close(int sock);

int socket(int domain, int type, int protocol);
int getsockopt(int sock, int level, int optname, void *optval, socklen_t *optlen);
int close(int sock);

#endif
