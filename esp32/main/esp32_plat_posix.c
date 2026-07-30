/*
 * esp32_plat_posix.c - the remaining plat_* functions: ROM/asset search
 * paths, memory mapping, thread naming, and the handful of
 * desktop/Windows-only stubs that have no meaning on this target.
 *
 * Reference: src/unix/sdl_plat_unix.c. See the M3 plan for the full
 * plat_* contract audit this was derived from.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "esp_heap_caps.h"

#include <86box/86box.h>
#include <86box/plat.h>
#include <86box/mem.h>
#include <86box/rom.h>

/*
 * ROM/asset search paths - fixed SD-card locations instead of the
 * desktop build's XDG_DATA_HOME/HOME/XDG_DATA_DIRS walk. The SD card is
 * mounted at /sdcard by board_sd (see board/board_stub.c for now).
 */

void
plat_init_rom_paths(void)
{
    rom_add_path("/sdcard/86box/roms/");
}

void
plat_init_asset_paths(void)
{
    asset_add_path("/sdcard/86box/assets/");
}

/*
 * Block device handling - ESP32 only ever mounts image *files* on the SD
 * card, never a raw block device node, so this is always "not a block
 * device" (same fallback the SDL build takes when the path doesn't stat
 * as one).
 */

int
plat_is_block_device(const char *path)
{
    (void) path;
    return 0;
}

int64_t
plat_get_block_device_size(const char *path)
{
    (void) path;
    return -1;
}

plat_device_vol_locked_t *
plat_lock_volumes(FILE *file)
{
    (void) file;
    return NULL;
}

void
plat_unlock_volumes(plat_device_vol_locked_t *vol)
{
    (void) vol;
}

/*
 * Memory management.
 *
 * plat_mmap's only real caller left in this build is mem.c's guest RAM
 * allocation (always executable=0 - the dynarec/codegen paths that would
 * need executable=1 are compiled out entirely under DYNAREC=OFF, see the
 * M2 work). Guest RAM belongs in PSRAM, not the small internal SRAM.
 */

void *
plat_mmap(size_t size, uint8_t executable)
{
    (void) executable; /* always 0 in this build - see comment above */

    void *ret = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret != NULL)
        memset(ret, 0x00, size);
    return ret;
}

void
plat_munmap(void *ptr, size_t size)
{
    (void) size;
    heap_caps_free(ptr);
}

/*
 * Threads - ESP-IDF's pthread component supports pthread_setname_np the
 * same way glibc does.
 */

void
plat_set_thread_name(void *thread, const char *name)
{
    char truncated[16];

    strncpy(truncated, name, sizeof(truncated) - 1);
    truncated[sizeof(truncated) - 1] = '\0';
    pthread_setname_np(thread ? *((pthread_t *) thread) : pthread_self(), truncated);
}

/*
 * Command execution - spawning an external terminal/process has no
 * meaning on this target (used by src/char/char_stdio.c for virtual
 * serial ports routed to a terminal emulator).
 */

int
plat_run_command(const char *cmd, const char **env, const char *title)
{
    (void) cmd;
    (void) env;
    (void) title;
    return -1;
}
