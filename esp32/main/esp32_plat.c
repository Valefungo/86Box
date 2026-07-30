/*
 * esp32_plat.c - plat_* implementations for the ESP32-P4 platform layer.
 *
 * Mostly a direct port of src/unix/sdl_plat.c: file/path/timer/string
 * helpers that don't need anything ESP32-specific beyond swapping SDL
 * calls for ESP-IDF equivalents. See the M3 plan in
 * /home/fungostar/.claude/plans/indexed-knitting-feigenbaum.md for the
 * full plat_* contract this was derived from.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/nvr.h>
#include <86box/config.h>
#include <86box/hdd.h>
#include <86box/path.h>
#include <86box/plat.h>
#include <86box/version.h>
#include "cpu.h"

/*
 * File system manipulation - identical semantics to desktop, the SD card
 * is mounted under the standard newlib VFS so plain fopen/stat/mkdir work.
 */

int
plat_getcwd(char *bufp, int max)
{
    /* chdir()/getcwd() aren't actually wired up by ESP-IDF's VFS on this
     * target (no implementation anywhere in components/vfs or
     * components/newlib) - a real chdir() call silently doesn't affect
     * later getcwd() calls. pc_init() uses plat_getcwd() to find the
     * directory 86box.cfg lives in (works on desktop because the process
     * launches from that directory); there is no equivalent launch
     * directory here, so just return the fixed SD card path directly,
     * same as plat_get_global_config_dir(). */
    strncpy(bufp, "/sdcard/86box/", max);
    bufp[max - 1] = '\0';
    return 1;
}

int
plat_chdir(char *str)
{
    return chdir(str);
}

FILE *
plat_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

FILE *
plat_fopen64(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int
plat_dir_check(char *path)
{
    struct stat stats;
    if (stat(path, &stats) < 0)
        return 0;
    return S_ISDIR(stats.st_mode);
}

int
plat_file_check(const char *path)
{
    struct stat stats;
    if (stat(path, &stats) < 0)
        return 0;
    return !S_ISDIR(stats.st_mode);
}

int
plat_dir_create(char *path)
{
    return mkdir(path, S_IRWXU);
}

void
plat_remove(char *path)
{
    remove(path);
}

/*
 * String localization - no localization on this target, same stub
 * behavior as the SDL build.
 */

char *
plat_get_string(int i)
{
    switch (i) {
        case STRING_PCAP_ERROR_NO_DEVICES:
        case STRING_PCAP_ERROR_INVALID_DEVICE:
            return "Network is not available on this build.";
        case STRING_HW_NOT_AVAILABLE_MACHINE:
            return "Machine \"%s\" is not available due to missing ROMs in the roms/machines directory. Switching to an available machine.";
        case STRING_HW_NOT_AVAILABLE_VIDEO:
            return "Video card \"%s\" is not available due to missing ROMs in the roms/video directory. Switching to an available video card.";
        case STRING_HW_NOT_AVAILABLE_DEVICE:
            return "Device \"%s\" is not available due to missing ROMs. Ignoring the device.";
        case STRING_HW_NOT_AVAILABLE_TITLE:
            return "Hardware not available";
        case STRING_NET_ERROR:
            return "Failed to initialize network driver:\n\n%s\n\nThe network configuration will be switched to the null driver.";
        case STRING_CDROM_OPEN_ISO_ERROR:
            return "Unable to open image or folder \"%s\".";
        case STRING_CDROM_OPEN_CUE_ERROR:
            return "Unable to open cue sheet \"%s\".";
        case STRING_CDROM_OPEN_MDS_ERROR:
            return "Unable to open MDS file \"%s\".";
        case STRING_CDROM_LOAD_IMAGE_ERROR:
            return "Unable to load CD-ROM image \"%s\".";
        case STRING_CDROM_DVD_IN_CD_DRIVE:
            return "The DVD image \"%s\" has been inserted into a drive that does not support DVD media and will be ignored.";
    }
    return "";
}

int
plat_language_code(UNUSED(char *langcode))
{
    return 0;
}

void
plat_language_code_r(UNUSED(int id), UNUSED(char *outbuf), UNUSED(int len))
{
    return;
}

/*
 * Path manipulation - pure string logic, no OS dependency at all.
 */

void
path_slash(char *path)
{
    if (path[strlen(path) - 1] != '/') {
        strcat(path, "/");
    }
    path_normalize(path);
}

const char *
path_get_slash(char *path)
{
    char *ret = "";

    if (path[strlen(path) - 1] != '/')
        ret = "/";

    return ret;
}

char *
path_get_basename(const char *path)
{
    int c = (int) strlen(path);

    while (c > 0) {
        if (path[c] == '/')
            return ((char *) &path[c + 1]);
        c--;
    }

    return ((char *) path);
}

char *
path_get_filename(char *s)
{
    int c = strlen(s) - 1;

    while (c > 0) {
        if (s[c] == '/' || s[c] == '\\')
            return (&s[c + 1]);
        c--;
    }

    return s;
}

char *
path_get_extension(char *s)
{
    int c = strlen(s) - 1;

    if (c <= 0)
        return s;

    while (c && s[c] != '.')
        c--;

    if (!c)
        return (&s[strlen(s)]);

    return (&s[c + 1]);
}

void
path_append_filename(char *dest, const char *s1, const char *s2)
{
    strcpy(dest, s1);
    path_slash(dest);
    strcat(dest, s2);
}

void
path_get_dirname(char *dest, const char *path)
{
    int   c   = (int) strlen(path);
    char *ptr = (char *) path;

    while (c > 0) {
        if (path[c] == '/' || path[c] == '\\') {
            ptr = (char *) &path[c];
            break;
        }
        c--;
    }

    while (path < ptr)
        *dest++ = *path++;
    *dest = '\0';
}

int
path_abs(char *path)
{
    return path[0] == '/';
}

void
path_normalize(UNUSED(char *path))
{
    /* No-op, same as desktop. */
}

/*
 * Common locations - fixed SD-card paths instead of getcwd()/XDG dirs.
 * See esp32_plat_posix.c for the ROM/asset search-path registration.
 */

void
plat_get_exe_name(char *s, int size)
{
    snprintf(s, size, "/sdcard/86box/86box");
}

void
plat_get_global_config_dir(char *outbuf, const size_t len)
{
    strncpy(outbuf, "/sdcard/86box/", len);
    outbuf[len - 1] = '\0';
}

void
plat_get_global_data_dir(char *outbuf, const size_t len)
{
    strncpy(outbuf, "/sdcard/86box/", len);
    outbuf[len - 1] = '\0';
}

void
plat_get_temp_dir(char *outbuf, uint8_t len)
{
    strncpy(outbuf, "/sdcard/86box/tmp/", len);
    outbuf[len - 1] = '\0';
}

void
plat_get_vmm_dir(char *outbuf, const size_t len)
{
    if (len > 0)
        outbuf[0] = 0;
}

void
plat_tempfile(char *bufp, char *prefix, char *suffix)
{
    /* No RTC battery-backed wall clock worth relying on here - use the
     * monotonic microsecond timer instead, unique enough for temp names. */
    int64_t now = esp_timer_get_time();

    if (prefix != NULL)
        snprintf(bufp, 1024, "%s-%lld%s", prefix, (long long) now, suffix);
    else
        snprintf(bufp, 1024, "%lld%s", (long long) now, suffix);
}

/*
 * Timer functions
 */

uint64_t
plat_timer_read(void)
{
    /* esp_timer_get_time() returns microseconds; the core only uses this
     * as a monotonic high-resolution counter (busy-loop timing in a
     * handful of video accelerator devices), not tied to any specific
     * frequency, so returning raw microseconds is fine. */
    return (uint64_t) esp_timer_get_time();
}

uint32_t
plat_get_ticks(void)
{
    return (uint32_t) (esp_timer_get_time() / 1000);
}

void
plat_delay_ms(uint32_t count)
{
    vTaskDelay(pdMS_TO_TICKS(count));
}

/*
 * Emulator support
 */

void
plat_power_off(void)
{
    hdd_image_sync_all();
    nvr_save();
    config_save();

    cycles -= 99999999;

    cpu_thread_run = 0;
}

void
plat_pause(int p)
{
    do_pause(p);
}

char *
plat_vidapi_name(UNUSED(int i))
{
    return "default";
}

void
plat_get_cpu_string(char *outbuf, uint8_t len)
{
    strncpy(outbuf, "ESP32-P4", len);
}

int
plat_vidapi(UNUSED(const char *name))
{
    return 0;
}

void
plat_resize_request(int x, int y, int monitor_index)
{
    /* Fixed physical panel - nothing to resize. Ignored, same as a
     * desktop build running in a non-resizable window would do. */
    (void) x;
    (void) y;
    (void) monitor_index;
}

void
plat_resize(int x, int y, int monitor_index)
{
    (void) x;
    (void) y;
    (void) monitor_index;
}

void
plat_mouse_capture(UNUSED(int on))
{
}

/*
 * Miscellaneous functions
 */

int
stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}

int
strnicmp(const char *s1, const char *s2, size_t n)
{
    return strncasecmp(s1, s2, n);
}
