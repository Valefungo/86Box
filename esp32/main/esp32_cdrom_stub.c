/*
 * esp32_cdrom_stub.c - stub for the zlib/libchdr/sndfile-dependent
 * CD-ROM image backends (cdrom_image.c: plain BIN/CUE via zlib+sndfile;
 * cdrom_chd.c: MAME CHD via zlib+libchdr).
 *
 * None of those libraries exist on ESP-IDF; vendoring them is out of
 * scope for the M3 skeleton build. cdrom.c's cdrom_load() calls these
 * three entry points unconditionally, so provide "format not available"
 * stubs - CCD/AaruFormat images (cdrom_image_ccd.c/cdrom_aaru.c, both
 * dependency-free) still work.
 */
#include <stddef.h>

#include <86box/86box.h>
#include <86box/cdrom.h>
#include <86box/cdrom_image.h>

void *
image_open(cdrom_t *dev, const char *path)
{
    (void) dev;
    (void) path;
    return NULL;
}

void *
chd_image_open(cdrom_t *dev, const char *path)
{
    (void) dev;
    (void) path;
    return NULL;
}

int
cdrom_image_is_chd(const char *fn)
{
    (void) fn;
    return 0;
}
