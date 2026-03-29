/* glibc_compat.c - Compatibility shims for PortMaster devices running older glibc.
 *
 * GCC 13 + glibc 2.38 headers emit calls to __isoc23_strtol / __isoc23_sscanf
 * (C23 standard aliases) even in C17 mode. PortMaster target devices don't have
 * GLIBC_2.38, so the binary fails to load.
 *
 * Fix: provide our own __isoc23_* symbols that forward to the classic GLIBC_2.4
 * versions via .symver assembly, bypassing the glibc 2.38 header redirects.
 * This file is only compiled for ARM targets.
 */
#ifdef __arm__
#include <stdarg.h>

/* Forward strtol to the classic GLIBC_2.4 symbol, bypassing the C23 redirect
 * that glibc 2.38 headers inject via #define strtol __isoc23_strtol. */
extern long int __compat_strtol(const char *nptr, char **endptr, int base);
__asm__(".symver __compat_strtol, strtol@GLIBC_2.4");

long int __isoc23_strtol(const char *nptr, char **endptr, int base) {
    return __compat_strtol(nptr, endptr, base);
}

/* Forward sscanf via vsscanf to the classic GLIBC_2.4 symbol. */
extern int __compat_vsscanf(const char *str, const char *fmt, va_list ap);
__asm__(".symver __compat_vsscanf, vsscanf@GLIBC_2.4");

int __isoc23_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = __compat_vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}
#endif /* __arm__ */
