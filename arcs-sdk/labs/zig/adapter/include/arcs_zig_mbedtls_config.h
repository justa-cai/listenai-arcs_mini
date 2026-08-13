/**
 * @file arcs_zig_mbedtls_config.h
 * @brief mbedTLS config bridge for Zig @cImport.
 *
 * Zig @cImport does not receive the CMake target's -imacros autoconf.h flags,
 * so this header imports generated Kconfig symbols when available before
 * delegating to the SDK mbedTLS config header selected by Kconfig.
 */

#ifndef ARCS_ZIG_MBEDTLS_CONFIG_H
#define ARCS_ZIG_MBEDTLS_CONFIG_H

#if defined(__has_include)
#if __has_include("autoconf.h")
#include "autoconf.h"
#endif
#endif

#ifndef CONFIG_MBEDTLS_CFG_FILE
#define CONFIG_MBEDTLS_CFG_FILE "config-tls-generic.h"
#endif

/* Fallback for standalone `zig build` of the adapter, where autoconf.h is absent. */
#ifndef CONFIG_SDK_MODULE_MBEDTLS
#define CONFIG_SDK_MODULE_MBEDTLS 1
#define CONFIG_MBEDTLS_AES_C 1
#define CONFIG_MBEDTLS_CCM_C 1
#define CONFIG_MBEDTLS_GCM_C 1
#define CONFIG_MBEDTLS_CMAC_C 1
#define CONFIG_MBEDTLS_ECDH_C 1
#define CONFIG_MBEDTLS_ECP_C 1
#define CONFIG_MBEDTLS_ECP_DP_SECP256R1_ENABLED 1
#define CONFIG_MBEDTLS_HAVE_TIME 1
#define CONFIG_MBEDTLS_HAVE_TIME_DATE 1
#define CONFIG_MBEDTLS_KEY_EXCHANGE_RSA 1
#define CONFIG_MBEDTLS_KEY_EXCHANGE_ECDHE_RSA 1
#define CONFIG_MBEDTLS_PEM_PARSE_C 1
#define CONFIG_MBEDTLS_SHA512_C 1
#define CONFIG_MBEDTLS_SSL_PROTO_TLS1_2 1
#define CONFIG_MBEDTLS_SSL_ALPN 1
#endif

#include CONFIG_MBEDTLS_CFG_FILE

#endif /* ARCS_ZIG_MBEDTLS_CONFIG_H */
