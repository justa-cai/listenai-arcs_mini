#define FIXED_POINT

// #define FRAME_SIZE 160

#define DISABLE_FLOAT_API

#define DISABLE_VBR
// #define DISABLE_WIDEBAND
#define EXPORT

#define RELEASE 1

/* We don't care */
#define EXPORT

#define USE_KISS_FFT

/* Disable DC block if doing SNR testing */
#define DISABLE_HIGHPASS

/* for debug */
#undef DECODE_ONLY
#define VERBOSE_ALLOC

/* EITHER    Allocate from fixed array (C heap not used) */
/*           Enable VERBOSE_ALLOC to see how much is used */
#define MANUAL_ALLOC
#define OS_SUPPORT_CUSTOM

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1
