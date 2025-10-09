#include "ze_transport_ssl.h"
#include <string.h>
#include <mbedtls/platform.h>

extern const char mbedtls_test_cas_pem[];
extern const size_t mbedtls_test_cas_pem_len;

#define HTTPC_CUSTOM_CAS_PEM          mbedtls_test_cas_pem
#define HTTPC_CUSTOM_CAS_PEM_LEN      mbedtls_test_cas_pem_len

// static security_client client_param;
// mbedtls_context *g_pContext = NULL;
// mbedtls_sock g_httpc_net_fd = {.fd = -1};

int ze_transport_ssl_connect(ze_transport_ssl_t *handle, int s, const struct sockaddr *name, int namelen, char *hostname)
{
	int ret = 0;
	// HC_DBG(("Https:connect.."));
	struct sockaddr *ServerAddress = (struct sockaddr *)name;
	int net_fd = s;
	/* Init client context */
	mbedtls_context *pContext = (mbedtls_context *)mbedtls_init_context(0);
	if (!pContext || !ServerAddress)
		return -1;
	handle->m_context = pContext;
	handle->m_httpc_net_fd.fd = -1;

	memset(&(handle->m_client_param), 0, sizeof(security_client));

	// HC_DBG(("https: config defaults certs.."));
	handle->m_client_param.pCa = (char *)HTTPC_CUSTOM_CAS_PEM;
	handle->m_client_param.nCa = HTTPC_CUSTOM_CAS_PEM_LEN;
	

	int verify_mode = MBEDTLS_SSL_VERIFY_NONE;
	if ((ret = mbedtls_config_context(pContext, (void *)&(handle->m_client_param), verify_mode)) != 0) {
		// HC_ERR(("https: config failed.."));
		mbedtls_deinit_context(handle->m_context);
		handle->m_context = NULL;
		return -1;
	}

	if ((ret = mbedtls_connect(pContext, (mbedtls_sock *)&net_fd, ServerAddress, namelen, hostname)) != 0) {
		// HC_ERR(("https: connect failed.."));
		mbedtls_deinit_context(handle->m_context);
		handle->m_context = NULL;
		return -1;
	}
	// HC_DBG(("Https:connect ok.."));

	return ret;
}

void ze_transport_ssl_set_recvtimeout(ze_transport_ssl_t *handle, int timeout_ms)
{
	if (handle->m_context && timeout_ms > 0) {
		mbedtls_set_recv_timeout(handle->m_context, timeout_ms);
	}
}

int ze_transport_ssl_negotiate(ze_transport_ssl_t *handle, int s, char *hostname)
{
	int ret = 0;
	handle->m_httpc_net_fd.fd = s;
	// HC_DBG(("Https:negotiate.."));
	if ((ret = mbedtls_handshake(handle->m_context, &(handle->m_httpc_net_fd))) != 0)
		return -1;
	// HC_DBG(("Https:negotiate ok.."));
	return 0;
}

int ze_transport_ssl_send(ze_transport_ssl_t *handle, int s, char *buf, int len, int flags)
{
	int ret = 0;
	// HC_DBG(("Https:send %d..", len));
	if ((ret = mbedtls_send(handle->m_context, buf, len)) < 0)
		return -1;
	return ret;
}

int ze_transport_ssl_recv(ze_transport_ssl_t *handle, int s, char *buf, int len, int flags)
{
	return mbedtls_recv(handle->m_context, buf, len);
}

int ze_transport_ssl_recv_pending(ze_transport_ssl_t *handle, int s)
{
	int ret = 0;
	ret = mbedtls_recv_pending(handle->m_context);
	// HC_DBG(("Https:recv pending : %d (bytes)..", ret));
	return ret;
}

int ze_transport_ssl_close(ze_transport_ssl_t *handle, int s)
{
	// HC_DBG(("Https:close.."));
	mbedtls_deinit_context(handle->m_context);
	s = -1;
	handle->m_httpc_net_fd.fd = -1;
	return 0;
}
