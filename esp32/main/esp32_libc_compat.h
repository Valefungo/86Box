/*
 * esp32_libc_compat.h - fseeko64/ftello64 shim.
 *
 * This toolchain's newlib wasn't built with 64-bit stdio support
 * (__LARGE64_FILES is unset, and the _r-suffixed 64-bit implementations
 * aren't in libc.a), and off_t is 32-bit on this ILP32 target regardless.
 * cdrom_image_ccd.c/cdrom_image_viso.c call fseeko64/ftello64 assuming
 * glibc-style availability, so widen the plain 32-bit fseeko/ftello
 * instead of pulling in a real 64-bit implementation - real offsets on
 * this target's SD-card-hosted disk/CD images will never approach 2GB.
 *
 * Force-included via -include (see main/CMakeLists.txt) so no core file
 * needs to know about this.
 */
#ifndef ESP32_LIBC_COMPAT_H
#define ESP32_LIBC_COMPAT_H

#include <stdio.h>

static inline long long
ftello64(FILE *stream)
{
    return (long long) ftello(stream);
}

static inline int
fseeko64(FILE *stream, long long offset, int whence)
{
    return fseeko(stream, (off_t) offset, whence);
}

#endif
