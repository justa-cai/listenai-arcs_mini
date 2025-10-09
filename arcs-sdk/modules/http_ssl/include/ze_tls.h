#ifndef _ZE_TLS_H_
#define _ZE_TLS_H_

#include <mbedtls/debug.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ssl.h>
// #include <mbedtls/net.h>
#include <mbedtls/net_sockets.h>
#include "lwip/sockets.h"
#include <stdio.h>
// #include <fcntl.h>

/**
 * Server certificate(CA/CRL/KEY) container
 */
typedef struct {
	char           *pCa;    /* ca pointer   */
	unsigned int   nCa;     /* ca length    */
	char           *pCert;  /* cert pointer */
	unsigned int   nCert;   /* cert length  */
	char           *pKey;   /* key pointer  */
	unsigned int   nKey;    /* key length   */
} security_server;

/**
 * Client certificate(CA) container
 */
typedef struct {
	char            *pCa;  /* ca pointer */
	unsigned int    nCa;   /* ca length  */
	security_server certs;
} security_client;

/**
 * Container for certificate and Public key container.
 */
typedef union {
	struct {
		mbedtls_x509_crt   ca;       /* CA used for verify server crt */

		mbedtls_x509_crt   cert;     /* crt for oneself */
		mbedtls_pk_context key;      /* key for oneself */
	} cli_cert;

	struct {
		mbedtls_x509_crt   cert;
		mbedtls_pk_context key;      /* Public key container */
	} srv_cert;
} crt_context;

/**
 * mbedtls wrapper context structure
 *
 * The structure ensures that mbedtls api works properly and is dynamically created by
 * api (mbedtls_init_context). It contains all the info needed in the tls process.
 */
typedef struct
{
	int                       is_client;
	crt_context               cert;
	mbedtls_entropy_context   entropy;
	mbedtls_ctr_drbg_context  ctr_drbg;
	mbedtls_ssl_context       ssl;
	mbedtls_ssl_config        conf;
} mbedtls_context;

typedef mbedtls_net_context mbedtls_sock;

#if defined (MBEDTLS_SSL_CLI_C)
#define MBEDTLS_CLIENT
#endif

#if defined (MBEDTLS_SSL_SRV_C)
#define MBEDTLS_SERVER
#endif

#define MBEDTLS_SSL_CLIENT_VERIFY_LEVEL         MBEDTLS_SSL_VERIFY_OPTIONAL
#define MBEDTLS_SSL_SERVER_VERIFY_LEVEL         MBEDTLS_SSL_VERIFY_NONE

mbedtls_sock* mbedtls_socket(int nonblock);

mbedtls_context* mbedtls_init_context(int client);

void mbedtls_deinit_context(mbedtls_context *context);

int mbedtls_closesocket(mbedtls_sock* fd);

int mbedtls_config_context(mbedtls_context *context, void *param, int verify);

int mbedtls_handshake(mbedtls_context *context, mbedtls_sock* fd);

int mbedtls_send(mbedtls_context *context,char *buf, int len);

void mbedtls_set_recv_timeout(mbedtls_context *context, int timeout_ms);

int mbedtls_recv(mbedtls_context *context, char *buf, int len);

int mbedtls_recv_pending(mbedtls_context *context);

int mbedtls_connect(mbedtls_context *context, mbedtls_sock* fd, struct sockaddr *name, int namelen, char *hostname);

int mbedtls_accept(mbedtls_context *context, mbedtls_sock *local_fd, mbedtls_sock *remote_fd);

#endif