/*
File: tinyprintf.c

Copyright (C) 2004  Kustaa Nyholm

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <stdarg.h>
#include <autoconf.h>

#if CONFIG_TPF_ENABLE

typedef void (*putcf)(void *priv, char chr);
typedef void (*putsf)(void *priv, char *str);

/*
 * Configuration
 */

/* Enable long int support */
#define PRINTF_LONG_SUPPORT

/* Enable long long int support (implies long int support) */
#define PRINTF_LONG_LONG_SUPPORT

/* Enable %z (size_t) support */
#define PRINTF_SIZE_T_SUPPORT

/* Enable %f (float) support */
#define PRINTF_FLOAT_SUPPORT

/*
 * Configuration adjustments
 */
#ifdef PRINTF_SIZE_T_SUPPORT
#include <sys/types.h>
#endif

#ifdef PRINTF_LONG_LONG_SUPPORT
# define PRINTF_LONG_SUPPORT
#endif

/* __SIZEOF_<type>__ defined at least by gcc */
#ifdef __SIZEOF_POINTER__
# define SIZEOF_POINTER __SIZEOF_POINTER__
#endif
#ifdef __SIZEOF_LONG_LONG__
# define SIZEOF_LONG_LONG __SIZEOF_LONG_LONG__
#endif
#ifdef __SIZEOF_LONG__
# define SIZEOF_LONG __SIZEOF_LONG__
#endif
#ifdef __SIZEOF_INT__
# define SIZEOF_INT __SIZEOF_INT__
#endif

#ifdef __GNUC__
# define _TFP_GCC_NO_INLINE_  __attribute__ ((noinline))
#else
# define _TFP_GCC_NO_INLINE_
#endif

/*
 * Implementation
 */
struct param {
    char lz:1;              /**<  Leading zeros */
    char alt:1;             /**<  alternate form */
    char uc:1;              /**<  Upper case (for base16 only) */
    char align_left:1;      /**<  0 == align right (default), 1 == align left */
    char int_width;         /**<  integer field width */
    char dec_width;         /**<  decimal field width */
    char sign;              /**<  The sign to display (if any) */
    char base;              /**<  number base (e.g.: 8, 10, 16) */
    char *bf;               /**<  Buffer to output */
};


#ifdef PRINTF_LONG_LONG_SUPPORT
static void _TFP_GCC_NO_INLINE_ ulli2a(unsigned long long int num, struct param *p)
{
    int n = 0;
    unsigned long long int d = 1;
    char *bf = p->bf;
    while (num / d >= p->base)
        d *= p->base;
    while (d != 0) {
        int dgt = num / d;
        num %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
}

static void lli2a(long long int num, struct param *p)
{
    if (num < 0) {
        num = -num;
        p->sign = '-';
    }
    ulli2a(num, p);
}
#endif

#ifdef PRINTF_LONG_SUPPORT
static void uli2a(unsigned long int num, struct param *p)
{
    int n = 0;
    unsigned long int d = 1;
    char *bf = p->bf;
    while (num / d >= p->base)
        d *= p->base;
    while (d != 0) {
        int dgt = num / d;
        num %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
}

static void li2a(long num, struct param *p)
{
    if (num < 0) {
        num = -num;
        p->sign = '-';
    }
    uli2a(num, p);
}
#endif

static void ui2a(unsigned int num, struct param *p)
{
    int n = 0;
    unsigned int d = 1;
    char *bf = p->bf;
    while (num / d >= p->base)
        d *= p->base;
    while (d != 0) {
        int dgt = num / d;
        num %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
}

static void i2a(int num, struct param *p)
{
    if (num < 0) {
        num = -num;
        p->sign = '-';
    }
    ui2a(num, p);
}

#ifdef PRINTF_FLOAT_SUPPORT
static void _TFP_GCC_NO_INLINE_ f2a(double num, struct param *p)
{
    int n = 0;
    unsigned int d = 1;
    char *bf = p->bf;
    if (num < 0) {
        num = -num;
        p->sign = '-';
    }
    int integer = (int)num;
    num -= integer;
    while (integer / d >= p->base)
        d *= p->base;
    while (d != 0) {
        int dgt = integer / d;
        integer %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    if (p->dec_width) {
        *bf++ = '.';
        p->int_width++;
        for (n = 1, d = 1; n < p->dec_width; n++)
            d *= p->base;
        int decimal = (int)(num * d * 10);
        while (d != 0) {
            int dgt = decimal / d;
            decimal %= d;
            d /= p->base;
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
        }
    }
    *bf = 0;
}
#endif

static int a2d(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else
        return -1;
}

static char a2u(char ch, const char **src, int base, unsigned int *nump)
{
    const char *p = *src;
    unsigned int num = 0;
    int digit;
    while ((digit = a2d(ch)) >= 0) {
        if (digit > base)
            break;
        num = num * base + digit;
        ch = *p++;
    }
    *src = p;
    *nump = num;
    return ch;
}

static void putchw(void *putp, putcf putf, struct param *p)
{
    char ch;
    int n = p->int_width + p->dec_width;
    char *bf = p->bf;

    /* Number of filling characters */
    while (*bf++ && n > 0)
        n--;
    if (p->sign)
        n--;
    if (p->alt && p->base == 16)
        n -= 2;
    else if (p->alt && p->base == 8)
        n--;

    /* Fill with space to align to the right, before alternate or sign */
    if (!p->lz && !p->align_left) {
        while (n-- > 0)
            putf(putp, ' ');
    }

    /* print sign */
    if (p->sign)
        putf(putp, p->sign);

    /* Alternate */
    if (p->alt && p->base == 16) {
        putf(putp, '0');
        putf(putp, (p->uc ? 'X' : 'x'));
    } else if (p->alt && p->base == 8) {
        putf(putp, '0');
    }

    /* Fill with zeros, after alternate or sign */
    if (p->lz) {
        while (n-- > 0)
            putf(putp, '0');
    }

    /* Put actual buffer */
    bf = p->bf;
    while ((ch = *bf++))
        putf(putp, ch);

    /* Fill with space to align to the left, after string */
    if (!p->lz && p->align_left) {
        while (n-- > 0)
            putf(putp, ' ');
    }
}

void format(void *putp, putcf putf, const char *fmt, va_list va)
{
    struct param p;
#ifdef PRINTF_LONG_SUPPORT
    char bf[23];  /* long = 64b on some architectures */
#else
    char bf[12];  /* int = 32b on some architectures */
#endif
    char ch;
    p.bf = bf;

    while ((ch = *(fmt++))) {
        if (ch != '%') {
            putf(putp, ch);
        } else {
#ifdef PRINTF_LONG_SUPPORT
            char lng = 0;  /* 1 for long, 2 for long long */
#endif
            /* Init parameter struct */
            p.lz = 0;
            p.alt = 0;
            p.int_width = 0;
            p.dec_width = 0;
            p.align_left = 0;
            p.sign = 0;

            /* Flags */
            while ((ch = *(fmt++))) {
                switch (ch) {
                case '-':
                    p.align_left = 1;
                    continue;
                case '0':
                    p.lz = 1;
                    continue;
                case '#':
                    p.alt = 1;
                    continue;
                default:
                    break;
                }
                break;
            }

            /* Width */
            if (ch >= '0' && ch <= '9') {
                ch = a2u(ch, &fmt, 10, &(p.int_width));
            }

#if 0
            /* We accept 'x.y' format but don't support it completely:
             * we ignore the 'y' digit => this ignores 0-fill
             * size and makes it == int_width (ie. 'x') */
            if (ch == '.') {
              p.lz = 1;  /* zero-padding */
              /* ignore actual 0-fill size: */
              do {
                ch = *(fmt++);
              } while ((ch >= '0') && (ch <= '9'));
            }
#else
            /* support x.y format for float type with radix-10:
             * x for length of integer part
             * y for length of decimal part */
            if (ch == '.') {
                ch = *fmt++;
                p.dec_width = 6; /* default, if y not found */
                ch = a2u(ch, &fmt, 10, &(p.dec_width));
            }
#endif

#ifdef PRINTF_SIZE_T_SUPPORT
# ifdef PRINTF_LONG_SUPPORT
            if (ch == 'z') {
                ch = *(fmt++);
                if (sizeof(size_t) == sizeof(unsigned long int))
                    lng = 1;
#  ifdef PRINTF_LONG_LONG_SUPPORT
                else if (sizeof(size_t) == sizeof(unsigned long long int))
                    lng = 2;
#  endif
            } else
# endif
#endif

#ifdef PRINTF_LONG_SUPPORT
            if (ch == 'l') {
                ch = *(fmt++);
                lng = 1;
#ifdef PRINTF_LONG_LONG_SUPPORT
                if (ch == 'l') {
                  ch = *(fmt++);
                  lng = 2;
                }
#endif
            }
#endif
            switch (ch) {
            case 0:
                return;
            case 'u':
                p.base = 10;
#ifdef PRINTF_LONG_SUPPORT
#ifdef PRINTF_LONG_LONG_SUPPORT
                if (2 == lng)
                    ulli2a(va_arg(va, unsigned long long int), &p);
                else
#endif
                  if (1 == lng)
                    uli2a(va_arg(va, unsigned long int), &p);
                else
#endif
                    ui2a(va_arg(va, unsigned int), &p);
                putchw(putp, putf, &p);
                break;
            case 'd':
            case 'i':
                p.base = 10;
#ifdef PRINTF_LONG_SUPPORT
#ifdef PRINTF_LONG_LONG_SUPPORT
                if (2 == lng)
                    lli2a(va_arg(va, long long int), &p);
                else
#endif
                  if (1 == lng)
                    li2a(va_arg(va, long int), &p);
                else
#endif
                    i2a(va_arg(va, int), &p);
                putchw(putp, putf, &p);
                break;
#ifdef SIZEOF_POINTER
            case 'p':
                p.alt = 1;
# if defined(SIZEOF_INT) && SIZEOF_POINTER <= SIZEOF_INT
                lng = 0;
# elif defined(SIZEOF_LONG) && SIZEOF_POINTER <= SIZEOF_LONG
                lng = 1;
# elif defined(SIZEOF_LONG_LONG) && SIZEOF_POINTER <= SIZEOF_LONG_LONG
                lng = 2;
# endif
#endif
            case 'x':
            case 'X':
                p.base = 16;
                p.uc = (ch == 'X')?1:0;
#ifdef PRINTF_LONG_SUPPORT
#ifdef PRINTF_LONG_LONG_SUPPORT
                if (2 == lng)
                    ulli2a(va_arg(va, unsigned long long int), &p);
                else
#endif
                  if (1 == lng)
                    uli2a(va_arg(va, unsigned long int), &p);
                else
#endif
                    ui2a(va_arg(va, unsigned int), &p);
                putchw(putp, putf, &p);
                break;
            case 'o':
                p.base = 8;
                ui2a(va_arg(va, unsigned int), &p);
                putchw(putp, putf, &p);
                break;
            case 'c':
                putf(putp, (char)(va_arg(va, int)));
                break;
            case 's':
                p.bf = va_arg(va, char *);
                putchw(putp, putf, &p);
                p.bf = bf;
                break;
#ifdef PRINTF_FLOAT_SUPPORT
            case 'f':
                p.base = 10;
                f2a(va_arg(va, double), &p);
                putchw(putp, putf, &p);
                break;
#endif
            case '%':
                putf(putp, ch);
            default:
                break;
            }
        }
    }
}

typedef struct { int num, cap; char *buf; } __tfp_priv_t;
static void _putcf(void *p, char c)
{
    __tfp_priv_t *const priv = p;
    if (priv->buf && (priv->cap < 0 || priv->num < priv->cap))
        priv->buf[priv->num] = c;
    priv->num++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// [v]snprintf(safe) string terminator and return rules:
// 1. if buffer_pointer && buffer_size <= actual_length:
//      - copy formatted string to buffer, total buffer_size-1 bytes
//      - add terminator('\0') to last byte of buffer, string truncated
//      - return buffer_size(exclude terminator)
// 2. if buffer_pointer && buffer_size > actual_length:
//      - copy formatted string to buffer, total actual_length bytes
//      - add terminator('\0') to last byte of buffer, string no truncated
//      - return actual_length(exclude terminator)
// 3. if !buffer_pointer || !buffer_size:
//      - no string output
//      - return actual_length(exclude terminator)
////////////////////////////////////////////////////////////////////////////////////////////////////
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    __tfp_priv_t priv = { .buf = str, .cap = size, .num = 0 };
    format(&priv, _putcf, fmt, ap);
    if (str && size) str[priv.num] = 0;
    return priv.num;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// [v]sprintf(unsafe) string terminator and return rules:
// 1. if buffer_pointer:
//      - copy formatted string to buffer, total actual_length bytes, may cause memory overflow 
//      - add terminator('\0') to buffer[actual_length], may cause memory overflow
//      - return actual_length
// 2. if !buffer_pointer:
//      - no string output
//      - return actual_length
////////////////////////////////////////////////////////////////////////////////////////////////////
int vsprintf(char *str, const char *fmt, va_list ap)
{
    __tfp_priv_t priv = { .buf = str, .cap = -1, .num = 0 };
    format(&priv, _putcf, fmt, ap);
    if (str) str[priv.num] = 0;
    return priv.num;
}

int sprintf(char *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsprintf(str, fmt, ap);
    va_end(ap);
    return ret;
}

#endif//CONFIG_TPF_ENABLE
