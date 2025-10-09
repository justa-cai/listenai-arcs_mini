#ifndef _ZE__TRANSPORT_SSL_H_
#define _ZE__TRANSPORT_SSL_H_

#include "ze_tls.h"

typedef struct ze_transport_ssl
{
	security_client m_client_param;
	mbedtls_context *m_context;
	mbedtls_sock m_httpc_net_fd;
} ze_transport_ssl_t;


int ze_transport_ssl_connect(ze_transport_ssl_t *handle, int s, const struct sockaddr *name, int namelen, char *hostname);

void ze_transport_ssl_set_recvtimeout(ze_transport_ssl_t *handle, int timeout_ms);

int ze_transport_ssl_negotiate(ze_transport_ssl_t *handle, int s, char *hostname);

int ze_transport_ssl_send(ze_transport_ssl_t *handle, int s, char *buf, int len, int flags);

int ze_transport_ssl_recv(ze_transport_ssl_t *handle, int s, char *buf, int len, int flags);

int ze_transport_ssl_recv_pending(ze_transport_ssl_t *handle, int s);

int ze_transport_ssl_close(ze_transport_ssl_t *handle, int s);

#endif
