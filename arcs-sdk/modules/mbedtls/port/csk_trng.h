#ifndef CSK_TRNG_H
#define CSK_TRNG_H

#if defined (MBEDTLS_ENTROPY_HARDWARE_ALT)

#ifdef __cplusplus
extern "C" {
#endif

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
			  size_t *olen);

#ifdef __cplusplus
}
#endif

#endif  /* MBEDTLS_ENTROPY_HARDWARE_ALT */

#endif /* CSK_TRNG_H */
