/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "HTTPMbedTLSWrapper.h"
#include "ze_tls.h"

#ifdef HTTPC_SSL

#define HTTP_CLIENT_CA
//#define HTTPC_CERTIFICATE

#if defined(HTTP_CLIENT_CA)
#define CUSTOM_HTTPC_CRT_RSA                                            \
"-----BEGIN CERTIFICATE-----\r\n"  \
"MIIDBzCCAe+gAwIBAgIUbxbr4Mr03InWXtuUp4H6NxKxqaswDQYJKoZIhvcNAQEL\r\n"  \
"BQAwGDEWMBQGA1UEAwwNMTkyLjE2OC4xMzcuMTAeFw0yNTAyMjgwODQyMTZaFw0y\r\n"  \
"NjAyMjgwODQyMTZaMBgxFjAUBgNVBAMMDTE5Mi4xNjguMTM3LjEwggEiMA0GCSqG\r\n"  \
"SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCkYwm6CFkaoPHuZDCtQtStHG5JouJshnvy\r\n"  \
"5PIvln97ERkiH8FGFiexlV/KQzBgb8KjP30jJsxrGmdh/ZjPiIwNGSNDKDC1eyVk\r\n"  \
"/PRF5H4BmDPpbBEBpER/vt3h4WkGM7GYYGQDxfYQ4+1Hq8Ar2xCTHksqbJht9PJT\r\n"  \
"NU717AZP/owxAByXonTouWgEZneE7fPQj2qGsFDKXlUnZ0Is2TAXYh1ONptoDI8F\r\n"  \
"Q6Ks2Zl14EaI4OOC+8ybibKyHf/6j7Flw5PicklExIMhkqFVxc90wM0d9pPnIh6q\r\n"  \
"SIz+Ju/fWj//zeWIPwPzn1dUWHwNATaukPynfKS10mLALQZ8+jEPAgMBAAGjSTBH\r\n"  \
"MBgGA1UdEQQRMA+CDTE5Mi4xNjguMTM3LjEwCwYDVR0PBAQDAgWgMBMGA1UdJQQM\r\n"  \
"MAoGCCsGAQUFBwMBMAkGA1UdEwQCMAAwDQYJKoZIhvcNAQELBQADggEBAB1cX1s2\r\n"  \
"1bIN0d7VnDeCqixE83FzUyEcSiu2MPJ9b7u6KqCpkt5trgV/2aPQuNmszuCwPlb+\r\n"  \
"c7fDPiM6+KydI0/W2y2JjOwS/oK5RX2UrD5kI3N+nEDU+lqFKShd91WuBvUWC1Xy\r\n"  \
"L6naIgaMJI93PcdwXMJ8pYSLJd/Vd1sWiv0K94hlbSTQiG0R1CXdjWFZ1Cp9pHjM\r\n"  \
"+TVf1UK7OIJVUZCwk5sR/WTtdjOvm7SPgYng3OLsFg54LxGTinVmB+FvuNEEoRbS\r\n"  \
"KPXpCk4D1nUsUZC1v+3m8UUiqff0UCz/tfblZwOCUGAwR/9moXF1SRDy7TeNl5T5\r\n"  \
"RaQxZaFcz3G+EEs=\r\n"  \
"-----END CERTIFICATE-----\r\n"

/* Concatenation of all available CA certificates */
const char httpc_custom_cas_pem[] = CUSTOM_HTTPC_CRT_RSA;
const size_t httpc_custom_cas_pem_len = sizeof(httpc_custom_cas_pem);

#define HTTPC_CUSTOM_CAS_PEM          httpc_custom_cas_pem
#define HTTPC_CUSTOM_CAS_PEM_LEN      httpc_custom_cas_pem_len

#if defined(HTTPC_CERTIFICATE)
#define HTTPC_CUSTOM_CA_PEM
#define HTTPC_CUSTOM_CA_PEM_LEN
#define HTTPC_CUSTOM_CRT_PEM
#define HTTPC_CUSTOM_CRT_PEM_LEN
#define HTTPC_CUSTOM_KEY
#define HTTPC_CUSTOM_KEY_LEN
#endif

#else
extern const char mbedtls_test_cas_pem[];
extern const size_t mbedtls_test_cas_pem_len;

#define HTTPC_CUSTOM_CAS_PEM          mbedtls_test_cas_pem
#define HTTPC_CUSTOM_CAS_PEM_LEN      mbedtls_test_cas_pem_len

#if defined(HTTPC_CERTIFICATE)
extern const char *mbedtls_test_srv_key;
extern const size_t mbedtls_test_srv_key_len;
extern const char *mbedtls_test_srv_crt;
extern const size_t mbedtls_test_srv_crt_len;

#define HTTPC_CUSTOM_CA_PEM           mbedtls_test_cas_pem
#define HTTPC_CUSTOM_CA_PEM_LEN       mbedtls_test_cas_pem_len
#define HTTPC_CUSTOM_CRT_PEM          mbedtls_test_srv_crt
#define HTTPC_CUSTOM_CRT_PEM_LEN      mbedtls_test_srv_crt_len
#define HTTPC_CUSTOM_KEY              mbedtls_test_srv_key
#define HTTPC_CUSTOM_KEY_LEN          mbedtls_test_srv_key_len
#endif
#endif

int HTTPWrapperSSLConnect(HTTP_SSL *handle, int s,const struct sockaddr *name,int namelen,char *hostname)
{
	int ret = 0;
	HC_DBG(("Https:connect.."));
	struct sockaddr *ServerAddress = (struct sockaddr *)name;
	int net_fd = s;
	/* Init client context */
	mbedtls_context *ctx = (mbedtls_context *)mbedtls_init_context(0);
	if (!ctx || !ServerAddress)
		return -1;
	handle->m_context = ctx;

	memset(&(handle->m_client_param), 0, sizeof(handle->m_client_param));

	security_client *user_cert = NULL;
	if ((user_cert = HTTPC_obtain_user_certs()) == NULL) {
		HC_DBG(("https: config defaults certs.."));
		handle->m_client_param.pCa = (char *)HTTPC_CUSTOM_CAS_PEM;
		handle->m_client_param.nCa = HTTPC_CUSTOM_CAS_PEM_LEN;
#if defined(HTTPC_CERTIFICATE)
		handle->m_client_param.certs.pCa = (char *) HTTPC_CUSTOM_CAS_PEM;
		handle->m_client_param.certs.nCa = HTTPC_CUSTOM_CAS_PEM_LEN;
		handle->m_client_param.certs.pCert = (char *) HTTPC_CUSTOM_CRT_PEM;
		handle->m_client_param.certs.nCert = HTTPC_CUSTOM_CRT_PEM_LEN;
		handle->m_client_param.certs.pKey = (char *) HTTPC_CUSTOM_KEY;
		handle->m_client_param.certs.nKey = HTTPC_CUSTOM_KEY_LEN;
#endif
	} else {
		HC_DBG(("https: config user certs.."));
		memcpy(&(handle->m_client_param), user_cert, sizeof(handle->m_client_param));
	}

	int verify_mode = HTTPC_get_ssl_verify_mode();
	if (verify_mode != MBEDTLS_SSL_VERIFY_NONE && verify_mode != MBEDTLS_SSL_VERIFY_OPTIONAL &&
		verify_mode != MBEDTLS_SSL_VERIFY_REQUIRED && verify_mode != MBEDTLS_SSL_VERIFY_UNSET)
		verify_mode = MBEDTLS_SSL_VERIFY_NONE;
	if ((ret = mbedtls_config_context(ctx, (void *) &(handle->m_client_param), verify_mode)) != 0) {
		HC_ERR(("https: config failed.."));
		mbedtls_deinit_context(handle->m_context);
		handle->m_context = NULL;
		return -1;
	}

	if ((ret = mbedtls_connect(ctx, (mbedtls_sock*) &net_fd, ServerAddress, namelen, hostname)) != 0) {
		HC_ERR(("https: connect failed.."));
		mbedtls_deinit_context(handle->m_context);
		handle->m_context = NULL;
		return -1;
	}
	HC_DBG(("Https:connect ok.."));

	return ret;
}

void HTTPWrapperSSLSetRecvTimeout(HTTP_SSL *handle, int timeout_ms)
{
	if (handle && handle->m_context && timeout_ms > 0) {
		mbedtls_set_recv_timeout(handle->m_context, timeout_ms);
	}
}

int HTTPWrapperSSLNegotiate(HTTP_SSL *handle, int s,const struct sockaddr *name,int namelen,char *hostname)
{
	int ret = 0;
	handle->m_httpc_net_fd.fd = s;
	HC_DBG(("Https:negotiate.."));
	if ((ret = mbedtls_handshake(handle->m_context, &(handle->m_httpc_net_fd))) != 0)
		return -1;
	HC_DBG(("Https:negotiate ok.."));
	return 0;
}

int HTTPWrapperSSLSend(HTTP_SSL *handle, int s,char *buf, int len,int flags)
{
	int ret = 0;
	HC_DBG(("Https:send %d..", len));
	if ((ret = mbedtls_send(handle->m_context, buf, len)) < 0)
		return -1;
	return ret;
}

int HTTPWrapperSSLRecv(HTTP_SSL *handle, int s,char *buf, int len,int flags)
{
	int ret = 0;
	HC_DBG(("Https:recv.."));
	if ((ret = mbedtls_recv(handle->m_context, buf, len)) < 0)
		return -1;
	return ret;
}

int HTTPWrapperSSLRecvPending(HTTP_SSL *handle, int s)
{
	int ret = 0;
	ret = mbedtls_recv_pending(handle->m_context);
	HC_DBG(("Https:recv pending : %d (bytes)..", ret));
	return ret;
}

int HTTPWrapperSSLClose(HTTP_SSL *handle, int s)
{
	HC_DBG(("Https:close.. %d", s));
	mbedtls_deinit_context(handle->m_context);
	// s = -1;
	handle->m_httpc_net_fd.fd = -1;
	return 0;
}
#endif /* HTTPC_SSL */