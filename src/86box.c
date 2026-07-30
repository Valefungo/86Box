/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Main emulator module where most things are controlled.
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
 *          Jasmine Iwanek, <jriwanek@gmail.com>
 *
 *          Copyright 2008-2020 Sarah Walker.
 *          Copyright 2016-2020 Miran Grca.
 *          Copyright 2017-2020 Fred N. van Kempen.
 *          Copyright 2021      Laci bá'
 *          Copyright 2021      dob205
 *          Copyright 2021      Andreas J. Reichel.
 *          Copyright 2021-2025 Jasmine Iwanek.
 */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <unistd.h>
#include <math.h>

#ifndef _WIN32
#    include <pwd.h>
#endif
#ifdef __APPLE__
#    include <string.h>
#    include <dispatch/dispatch.h>
#    ifdef __aarch64__
#        include <pthread.h>
#    endif
#endif

#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/mem.h>
#include <86box/pic.h>
#include <86box/nmi.h>
#include "cpu.h"
#include "x86.h"
#ifdef USE_DYNAREC
#    include "codegen_public.h"
#endif
#include <86box/io.h>
#include <86box/rom.h>
#include <86box/dma.h>
#include <86box/pci.h>
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/pit.h>
#include <86box/random.h>
#include <86box/nvr.h>
#include <86box/machine.h>
#include <86box/bugger.h>
#include <86box/postcard.h>
#include <86box/unittester.h>
#include <86box/novell_cardkey.h>
#include <86box/isamem.h>
#include <86box/isarom.h>
#include <86box/isartc.h>
#include <86box/lpt.h>
#include <86box/serial.h>
#include <86box/keyboard.h>
#include <86box/mouse.h>
#include <86box/gameport.h>
#include <86box/fdd.h>
#include <86box/fdd_audio.h>
#include <86box/fdc_ext.h>
#include <86box/hdd.h>
#include <86box/hdd_audio.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/scsi.h>
#include <86box/scsi_device.h>
#include <86box/cdrom.h>
#include <86box/cdrom_interface.h>
#include <86box/rdisk.h>
#include <86box/mo.h>
#include <86box/scsi_tape.h>
#include <86box/scsi_disk.h>
#include <86box/thread.h>
#include <86box/network.h>
#include <86box/sound.h>
#include <86box/midi.h>
#include <86box/video.h>
#include <86box/ui.h>
#include <86box/path.h>
#include <86box/plat.h>
#include <86box/plat_dir.h>
#include <86box/version.h>
#include <86box/gdbstub.h>
#include <86box/machine_status.h>
#include <86box/acpi.h>
#include <86box/nv/vid_nv_rivatimer.h>
#include <86box/vfio.h>

/* Stuff that used to be globally declared in plat.h but is now extern there
   and declared here instead. */
int          dopause = 1;  /* system is paused */
/* (Left untagged deliberately: dopause is checked every pc_run()/main-loop
 * iteration via ack_pause() and the ESP32 main loop - keep it in internal
 * SRAM rather than PSRAM given how often it's read.) */
volatile int ESP32_BIG_BSS_ATTR is_quit;  /* system exit requested */
uint64_t     ESP32_BIG_BSS_ATTR timer_freq;
char         ESP32_BIG_BSS_ATTR emu_version[200]; /* version ID string */

#ifdef MTR_ENABLED
int ESP32_BIG_BSS_ATTR tracing_on = 0;
#endif

/* Commandline options. */
int ESP32_BIG_BSS_ATTR dump_on_exit        = 0; /* (O) dump regs on exit */
int ESP32_BIG_BSS_ATTR start_in_fullscreen = 0; /* (O) start in fullscreen */
#ifdef _WIN32
int ESP32_BIG_BSS_ATTR force_debug = 0; /* (O) force debug output */
#endif
int ESP32_BIG_BSS_ATTR settings_only     = 0; /* (O) show only the settings dialog */
int confirm_exit_cmdl = 1; /* (O) do not ask for confirmation on quit if set to 0 */
#ifdef _WIN32
uint64_t ESP32_BIG_BSS_ATTR unique_id   = 0;
uint64_t ESP32_BIG_BSS_ATTR source_hwnd = 0;
#endif
char       ESP32_BIG_BSS_ATTR rom_path[1024]   = { '\0' };     /* (O) full path to ROMs */
rom_path_t ESP32_BIG_BSS_ATTR rom_paths        = { "", NULL }; /* (O) full paths to ROMs */
char       ESP32_BIG_BSS_ATTR asset_path[1024] = { '\0' };     /* (O) full path to assets */
rom_path_t ESP32_BIG_BSS_ATTR asset_paths      = { "", NULL }; /* (O) full paths to assets */
char       ESP32_BIG_BSS_ATTR log_path[1024]   = { '\0' };     /* (O) full path of logfile */
char       ESP32_BIG_BSS_ATTR vm_name[1024]    = { '\0' };     /* (O) display name of the VM */
int      ESP32_BIG_BSS_ATTR do_nothing                             = 0;
int      ESP32_BIG_BSS_ATTR dump_missing                           = 0;
int      ESP32_BIG_BSS_ATTR clear_cmos                             = 0;
#ifdef USE_INSTRUMENT
uint8_t  ESP32_BIG_BSS_ATTR instru_enabled                         = 0;
uint64_t ESP32_BIG_BSS_ATTR instru_run_ms                          = 0;
#endif
int      ESP32_BIG_BSS_ATTR clear_flash                            = 0;
int      ESP32_BIG_BSS_ATTR auto_paused                            = 0;
int      ESP32_BIG_BSS_ATTR auto_dialog_paused                     = 0;

/* Configuration values. */
int      ESP32_BIG_BSS_ATTR window_remember;
int      ESP32_BIG_BSS_ATTR vid_resize;                                              /* (C) allow resizing */
int      ESP32_BIG_BSS_ATTR invert_display                         = 0;              /* (C) invert the display */
int      ESP32_BIG_BSS_ATTR suppress_overscan                      = 0;              /* (C) suppress overscans */
int      ESP32_BIG_BSS_ATTR lang_id                                = 0;              /* (G) language id */
int      ESP32_BIG_BSS_ATTR scale                                  = 0;              /* (C) screen scale factor */
int      ESP32_BIG_BSS_ATTR dpi_scale                              = 0;              /* (C) DPI scaling of the emulated
                                                                         screen */
int      ESP32_BIG_BSS_ATTR vid_api                                = 0;              /* (C) video renderer */
int      ESP32_BIG_BSS_ATTR vid_cga_contrast                       = 0;              /* (C) video */
int      ESP32_BIG_BSS_ATTR video_fullscreen                       = 0;              /* (C) video */
int      ESP32_BIG_BSS_ATTR video_fullscreen_scale                 = 0;              /* (C) video */
int      ESP32_BIG_BSS_ATTR fullscreen_ui_visible                  = 0;              /* (C) video */
int      ESP32_BIG_BSS_ATTR enable_overscan                        = 0;              /* (C) video */
int      ESP32_BIG_BSS_ATTR force_43                               = 0;              /* (C) video */
int      video_filter_method                    = 1;              /* (C) video */
int      ESP32_BIG_BSS_ATTR video_vsync                            = 0;              /* (C) video */
int      video_framerate                        = -1;             /* (C) video */
int      ESP32_BIG_BSS_ATTR video_vk_device                        = 0;              /* (C) video */
int      ESP32_BIG_BSS_ATTR bugger_enabled                         = 0;              /* (C) enable ISAbugger */
int      ESP32_BIG_BSS_ATTR novell_keycard_enabled                 = 0;              /* (C) enable Novell NetWare 2.x key card emulation. */
int      ESP32_BIG_BSS_ATTR postcard_enabled                       = 0;              /* (C) enable POST card */
int      ESP32_BIG_BSS_ATTR unittester_enabled                     = 0;              /* (C) enable unit tester device */
int      ESP32_BIG_BSS_ATTR gameport_type[GAMEPORT_MAX]            = { 0, 0 };       /* (C) enable gameports */
int      ESP32_BIG_BSS_ATTR isamem_type[ISAMEM_MAX]                = { 0, 0, 0, 0 }; /* (C) enable ISA mem cards */
int      ESP32_BIG_BSS_ATTR isarom_type[ISAROM_MAX]                = { 0, 0, 0, 0 }; /* (C) enable ISA ROM cards */
int      ESP32_BIG_BSS_ATTR isartc_type                            = 0;              /* (C) enable ISA RTC card */
int      ESP32_BIG_BSS_ATTR gfxcard[GFXCARD_MAX]                   = { 0, 0 };       /* (C) graphics/video card */
int      show_second_monitors                   = 1;              /* (C) show non-primary monitors */
/* 2026-07-28: retagged after a bisection step showed reverting this pair
 * alone did NOT fix a real ~6x cpu_exec() regression seen on hardware -
 * so the sound_poll()-reads-these-at-48kHz theory was wrong (or at least
 * incomplete). Back to matching the rest of the batch while the real
 * cause is bisected properly. */
int      sound_is_float                         = 1;              /* (C) sound uses FP values */
int      sound_sample_rate                      = FREQ_48000;     /* (C) sound output sample rate */
int      ESP32_BIG_BSS_ATTR ibm8514_standalone_enabled             = 0;              /* (C) video option */
int      ESP32_BIG_BSS_ATTR xga_standalone_enabled                 = 0;              /* (C) video option */
int      ESP32_BIG_BSS_ATTR da2_standalone_enabled                 = 0;              /* (C) video option */
uint32_t ESP32_BIG_BSS_ATTR mem_size                               = 0;              /* (C) memory size (Installed on
                                                                         system board)*/
uint32_t ESP32_BIG_BSS_ATTR isa_mem_size                           = 0;              /* (C) memory size (ISA Memory Cards) */
int      ESP32_BIG_BSS_ATTR cpu_use_dynarec                        = 0;              /* (C) cpu uses/needs Dyna */
int      ESP32_BIG_BSS_ATTR cpu                  = 0;              /* (C) cpu type */
int      ESP32_BIG_BSS_ATTR fpu_type                               = 0;              /* (C) fpu type */
int      ESP32_BIG_BSS_ATTR fpu_softfloat                          = 0;              /* (C) fpu uses softfloat */
int      ESP32_BIG_BSS_ATTR time_sync                              = 0;              /* (C) enable time sync */
int      confirm_reset                          = 1;              /* (G) enable reset confirmation */
int      confirm_exit                           = 1;              /* (G) enable exit confirmation */
int      confirm_save                           = 1;              /* (G) enable save confirmation */
int      ESP32_BIG_BSS_ATTR chd_precache_level                     = 0;              /* (G) CHD precache level */
int      ESP32_BIG_BSS_ATTR enable_discord                         = 0;              /* (C) enable Discord integration */
int      pit_mode                               = -1;             /* (C) force setting PIT mode */
int      ESP32_BIG_BSS_ATTR fm_driver                              = 0;              /* (C) select FM sound driver */
int      ESP32_BIG_BSS_ATTR open_dir_usr_path                      = 0;              /* (G) default file open dialog directory
                                                                         of usr_path */
int      ESP32_BIG_BSS_ATTR video_fullscreen_scale_maximized       = 0;              /* (C) Whether fullscreen scaling settings
                                                                         also apply when maximized. */
int      ESP32_BIG_BSS_ATTR do_auto_pause                          = 0;              /* (G) Auto-pause the emulator on focus
                                                                         loss */
int      ESP32_BIG_BSS_ATTR do_auto_dialog_pause                   = 0;              /* (G) Auto-pause the emulator on dialog boxes */
int      ESP32_BIG_BSS_ATTR force_constant_mouse                   = 0;              /* (C) Force constant updating of the mouse */
int      hook_enabled                           = 1;              /* (C) Keyboard hook is enabled */
int      ESP32_BIG_BSS_ATTR test_mode                              = 0;              /* (C) Test mode */
char     ESP32_BIG_BSS_ATTR uuid[MAX_UUID_LEN]                     = { '\0' };       /* (C) UUID or machine identifier */
int      ESP32_BIG_BSS_ATTR sound_muted                            = 0;              /* (C) Is sound muted? */
int      ESP32_BIG_BSS_ATTR jumpered_internal_ecp_dma              = 0;              /* (C) Jumpered internal EPC DMA */
int      ESP32_BIG_BSS_ATTR inhibit_multimedia_keys;                                 /* (G) Inhibit multimedia keys on Windows. */
int      ESP32_BIG_BSS_ATTR force_10ms;                                              /* (C) Force 10ms CPU frame intervals. */
int      ESP32_BIG_BSS_ATTR vmm_disabled                           = 0;              /* (G) disable built-in manager */
char     ESP32_BIG_BSS_ATTR vmm_path_cfg[1024]                     = { '\0' };       /* (G) VMs path (unless -E is used)*/

int      ESP32_BIG_BSS_ATTR other_ide_present = 0;                                   /* IDE controllers from non-IDE cards are
                                                                     present */
int      ESP32_BIG_BSS_ATTR other_scsi_present = 0;                                  /* SCSI controllers from non-SCSI cards are
                                                                     present */

int      ESP32_BIG_BSS_ATTR is_pcjr = 0;                                             /* The current machine is PCjr. */
int      ESP32_BIG_BSS_ATTR portable_mode = 0;                                       /* We are running in portable mode
                                                                     (global dirs = exe path) */
int      ESP32_BIG_BSS_ATTR global_cfg_overridden = 0;                               /* Global config file was overriden on command line */

int      ESP32_BIG_BSS_ATTR monitor_edid = 0;                                        /* (C) Which EDID to use. 0=default, 1=custom. */
char     ESP32_BIG_BSS_ATTR monitor_edid_path[1024] = { 0 };                         /* (C) Path to custom EDID */

double   video_gl_input_scale = 1.0;                              /* (C) OpenGL 3.x input scale */
int      ESP32_BIG_BSS_ATTR video_gl_input_scale_mode = FULLSCR_SCALE_FULL;          /* (C) OpenGL 3.x input stretch mode */
int      ESP32_BIG_BSS_ATTR color_scheme = 0;                                        /* (C) Color scheme of UI (Windows-only) */
int      fdd_sounds_enabled = 1;                                  /* (C) Floppy drive sounds enabled */
int      ESP32_BIG_BSS_ATTR is_new_808x = 0;                                         /* (C) Use the new 808x code. */

// Accelerator key array
struct accelKey ESP32_BIG_BSS_ATTR acc_keys[NUM_ACCELS];

// Default accelerator key values - never written at runtime, const so it
// lives in flash/.rodata instead of RAM (see the extern declaration in
// 86box.h for why this is safe).
const struct accelKey def_acc_keys[NUM_ACCELS] = {
    {
        .name="send_ctrl_alt_del",
        .desc="Send Control+Alt+Del",
        .seq="Ctrl+F12"
    },
    {
        .name="send_ctrl_alt_esc",
        .desc="Send Control+Alt+Escape",
        .seq="Ctrl+F10"
    },
    {
        .name="fullscreen",
        .desc="Toggle fullscreen",
        .seq="Ctrl+Alt+PgUp"
    },
    {
        .name="toggle_ui_fullscreen",
        .desc="Toggle UI in fullscreen",
        .seq="Ctrl+Alt+PgDown"
    },
    {
        .name="screenshot",
        .desc="Take screenshot",
        .seq="Ctrl+F11"
    },
    {
        .name="raw_screenshot",
        .desc="Take raw screenshot",
        .seq=""
    },
    {
        .name="copy_screenshot",
        .desc="Copy screenshot",
        .seq=""
    },
    {
        .name="copy_raw_screenshot",
        .desc="Copy raw screenshot",
        .seq=""
    },
    {
        .name="fast_forward",
        .desc="Fast forward",
        .seq="Ctrl+Alt+F"
    },
    {
        .name="release_mouse",
        .desc="Release mouse pointer",
        .seq="Ctrl+End"
    },
    {
        .name="hard_reset",
        .desc="Hard reset",
        .seq="Ctrl+Alt+F12"
    },
    {
        .name="pause",
        .desc="Toggle pause",
        .seq="Ctrl+Alt+P"
    },
    {
        .name="mute",
        .desc="Toggle mute",
        .seq="Ctrl+Alt+M"
    },
    {
        .name="force_interpretation",
        .desc="Force interpretation",
        .seq="Ctrl+Alt+I"
    },
    {
        .name="toggle_osd",
        .desc="Toggle on-screen display",
        .seq="Ctrl+Alt+O"
    }
,
    {
        .name="exit",
        .desc="Exit",
        .seq=""
    }
};

char ESP32_BIG_BSS_ATTR vmm_path[1024] = { '\0' }; /* VM manager path to scan for VMs */
int  start_vmm = 1;

/* Statistics. */
extern int mmuflush;

/* emulator % */
int ESP32_BIG_BSS_ATTR fps;
int ESP32_BIG_BSS_ATTR framecount;
static uint32_t fps_sample_elapsed_ms = 1000;

extern int output;
int        ESP32_BIG_BSS_ATTR atfullspeed;

extern double exp_pow_table[0x800];

char  ESP32_BIG_BSS_ATTR exe_path[2048]; /* path (dir) of executable */
char  ESP32_BIG_BSS_ATTR usr_path[1024]; /* path (dir) of user data */
char  ESP32_BIG_BSS_ATTR cfg_path[1024]; /* full path of config file */
char  ESP32_BIG_BSS_ATTR global_cfg_path[1024]; /* full path of config file */
FILE *stdlog = NULL;  /* file to log output to */
void (*pclog_hook)(const char *) = NULL; /* optional UI log hook */
#if 0
int   ESP32_BIG_BSS_ATTR scrnsz_x = SCREEN_RES_X; /* current screen size, X */
int   ESP32_BIG_BSS_ATTR scrnsz_y = SCREEN_RES_Y; /* current screen size, Y */
#endif
int ESP32_BIG_BSS_ATTR config_changed; /* config has changed */
int ESP32_BIG_BSS_ATTR title_update;
int ESP32_BIG_BSS_ATTR framecountx        = 0;
int ESP32_BIG_BSS_ATTR hard_reset_pending = 0;

#if 0
int ESP32_BIG_BSS_ATTR unscaled_size_x = SCREEN_RES_X; /* current unscaled size X */
int ESP32_BIG_BSS_ATTR unscaled_size_y = SCREEN_RES_Y; /* current unscaled size Y */
int ESP32_BIG_BSS_ATTR efscrnsz_y = SCREEN_RES_Y;
#endif

__thread int is_cpu_thread = 0;

static ATOMIC_INT ESP32_BIG_BSS_ATTR do_pause_ack = 0;
static ATOMIC_INT ESP32_BIG_BSS_ATTR pause_ack = 0;

#define LOG_SIZE_BUFFER 8192            /* Log size buffer */

#ifndef RELEASE_BUILD

static char ESP32_BIG_BSS_ATTR buff[LOG_SIZE_BUFFER];

static int ESP32_BIG_BSS_ATTR seen = 0;

static int suppr_seen = 1;

// Functions only used in this translation unit
void pclog_ensure_stdlog_open(void);
#endif

/*
    Ensures STDLOG is open for pclog_ex and pclog_ex_cyclic
*/
void pclog_ensure_stdlog_open(void)
{
#ifndef RELEASE_BUILD
    if (stdlog == NULL) {
        if (log_path[0] != '\0') {
            stdlog = plat_fopen(log_path, "w");
            if (stdlog == NULL)
                stdlog = stdout;
        } else
            stdlog = stdout;
    }
#endif
}

/*
 * Log something to the logfile or stdout.
 *
 * To avoid excessively-large logfiles because some
 * module repeatedly logs, we keep track of what is
 * being logged, and catch repeating entries.
 */
void
pclog_ex(UNUSED(const char *fmt), UNUSED(va_list ap))
{
#ifndef RELEASE_BUILD
    char *temp = calloc(1, LOG_SIZE_BUFFER);

    if (!fmt || !fmt[0])
        return;

    pclog_ensure_stdlog_open();

    vsprintf(temp, fmt, ap);
    if (suppr_seen && !strcmp(buff, temp))
        seen++;
    else {
        if (suppr_seen && seen)
            fprintf(stdlog, "*** %d repeats ***\n", seen);
        seen = 0;
        strcpy(buff, temp);
        fprintf(stdlog, "%s", temp);
        if (pclog_hook)
            pclog_hook(temp);
    }

    fflush(stdlog);

    free(temp);
#endif
}

void
pclog_toggle_suppr(void)
{
#ifndef RELEASE_BUILD
    suppr_seen ^= 1;
#endif
}

/* Log something. We only do this in non-release builds. */
void
pclog(UNUSED(const char *fmt), ...)
{
#ifndef RELEASE_BUILD
    va_list ap;

    va_start(ap, fmt);
    pclog_ex(fmt, ap);
    va_end(ap);
#endif
}

/* Log something even in release builds. */
void
always_log(const char *fmt, ...)
{
    char    temp[LOG_SIZE_BUFFER];
    va_list ap;

    va_start(ap, fmt);

    if (stdlog == NULL) {
        if (log_path[0] != '\0') {
            stdlog = plat_fopen(log_path, "w");
            if (stdlog == NULL)
                stdlog = stdout;
        } else
            stdlog = stdout;
    }

    vsprintf(temp, fmt, ap);
    fprintf(stdlog, "%s", temp);
    if (pclog_hook)
        pclog_hook(temp);
    fflush(stdlog);
    va_end(ap);
}

/* Log a fatal error, and display a UI message before exiting. */
void
fatal(const char *fmt, ...)
{
    char *temp = calloc(1, LOG_SIZE_BUFFER);
    va_list ap;
    char   *sp;

    va_start(ap, fmt);

    if (stdlog == NULL) {
        if (log_path[0] != '\0') {
            stdlog = plat_fopen(log_path, "w");
            if (stdlog == NULL)
                stdlog = stdout;
        } else
            stdlog = stdout;
    }

    vsprintf(temp, fmt, ap);

    /* Make sure the message does not have a trailing newline. */
    if ((sp = strchr(temp, '\n')) != NULL)
        *sp = '\0';

    /* Re-add the newline into the log. */
    fprintf(stdlog, "%s\n", temp);
    fflush(stdlog);
    va_end(ap);

    nvr_save();

#ifdef ENABLE_808X_LOG
    dumpregs(1);
#endif

    do_pause(2);

    ui_msgbox(MBX_ERROR | MBX_FATAL, temp);

    /* Cleanly terminate all of the emulator's components so as
       to avoid things like threads getting stuck. */
    do_stop();

    fflush(stdlog);
    free(temp);

    exit(-1);
}

void
fatal_ex(const char *fmt, va_list ap)
{
    char  temp[LOG_SIZE_BUFFER];
    char *sp;

    if (stdlog == NULL) {
        if (log_path[0] != '\0') {
            stdlog = plat_fopen(log_path, "w");
            if (stdlog == NULL)
                stdlog = stdout;
        } else
            stdlog = stdout;
    }

    vsprintf(temp, fmt, ap);

    /* Make sure the message does not have a trailing newline. */
    if ((sp = strchr(temp, '\n')) != NULL)
        *sp = '\0';

    /* Re-add the newline into the log. */
    fprintf(stdlog, "%s\n", temp);
    fflush(stdlog);

    nvr_save();

#ifdef ENABLE_808X_LOG
    dumpregs(1);
#endif

    do_pause(2);

    ui_msgbox(MBX_ERROR | MBX_FATAL, temp);

    /* Cleanly terminate all of the emulator's components so as
       to avoid things like threads getting stuck. */
    do_stop();

    fflush(stdlog);
}

/* Log a warning error, and display a UI message without exiting. */
void
warning(const char *fmt, ...)
{
    char *temp = calloc(1, LOG_SIZE_BUFFER);
    va_list ap;
    char   *sp;

    va_start(ap, fmt);

    if (stdlog == NULL) {
        if (log_path[0] != '\0') {
            stdlog = plat_fopen(log_path, "w");
            if (stdlog == NULL)
                stdlog = stdout;
        } else
            stdlog = stdout;
    }

    vsprintf(temp, fmt, ap);

    /* Make sure the message does not have a trailing newline. */
    if ((sp = strchr(temp, '\n')) != NULL)
        *sp = '\0';

    /* Re-add the newline into the log. */
    fprintf(stdlog, "%s\n", temp);
    if (pclog_hook)
        pclog_hook(temp);
    fflush(stdlog);
    va_end(ap);

    do_pause(2);

    ui_msgbox(MBX_WARNING, temp);

    fflush(stdlog);
    free(temp);

    do_pause(0);
}

void
warning_ex(const char *fmt, va_list ap)
{
    char *temp = calloc(1, LOG_SIZE_BUFFER);
    char *sp;

    if (stdlog == NULL) {
        if (log_path[0] != '\0') {
            stdlog = plat_fopen(log_path, "w");
            if (stdlog == NULL)
                stdlog = stdout;
        } else
            stdlog = stdout;
    }

    vsprintf(temp, fmt, ap);

    /* Make sure the message does not have a trailing newline. */
    if ((sp = strchr(temp, '\n')) != NULL)
        *sp = '\0';


    /* Re-add the newline into the log. */
    fprintf(stdlog, "%s\n", temp);
    fflush(stdlog);

    do_pause(2);

    ui_msgbox(MBX_WARNING, temp);

    fflush(stdlog);
    free(temp);

    do_pause(0);
}

#ifdef ENABLE_PC_LOG
int pc_do_log = ENABLE_PC_LOG;

static void
pc_log(const char *fmt, ...)
{
    va_list ap;

    if (pc_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define pc_log(fmt, ...)
#endif

const char *
plat_dir_get_path(plat_dir_t *context)
{
    if (context->path[context->path_dir_len])
        return context->path;
    if (plat_dir_is_special_entry(plat_dir_get_name(context)))
        return context->path;
    const size_t len = context->path_dir_len + strlen(plat_dir_get_name(context)) + 2;
    if (len > context->path_len) {
        char *new_path = (char *) realloc(context->path, len);
        if (new_path == NULL) {
            fatal("new_path == NULL");
            return NULL;
        } else {
            context->path     = new_path;
            context->path_len = len;
        }
    }
    snprintf(&context->path[context->path_dir_len], context->path_len - context->path_dir_len,
#ifdef _WIN32
        "\\"
#else
        "/"
#endif
        "%s", plat_dir_get_name(context));
    return context->path;
}

static void
delete_nvr_file(uint8_t flash)
{
    /* Set up the NVR file's name. */
    const  size_t c = strlen(machine_get_nvr_name()) + 5;
    char  *fn       = (char *) calloc(1, c + 1);

    if (fn == NULL)
        fatal("Error allocating memory for the removal of the %s file\n",
              flash ? "BIOS flash" : "CMOS");

    if (flash)
        sprintf(fn, "%s.bin", machine_get_nvr_name());
    else
        sprintf(fn, "%s.nvr", machine_get_nvr_name());

    remove(nvr_path(fn));

    free(fn);
    fn = NULL;
}

#ifdef _WIN32
void
pc_debug_console(void)
{
    if (!force_debug && AllocConsole()) {
        force_debug = 1;
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    if (force_debug && vm_name[0])
        SetConsoleTitle(vm_name);
}
#endif

extern void  device_find_all_descs(void);

static void
pc_show_usage(void)
{
    char p[8192] = { 0 };

    sprintf(p,
            "\nUsage: 86box [options] [cfg-file]\n\n"
            "Valid options are:\n\n"
            "-? or --help\t\t\t- show this information\n"
            "-A or --assetpath path\t\t- set 'path' to be asset path\n"
#ifdef SHOW_EXTRA_PARAMS
            "-C or --config path\t\t- set 'path' to be config file\n"
#endif
#ifdef _WIN32
            "-D or --debug\t\t\t- force debug output logging\n"
#endif
#ifndef USE_SDL_UI
            "-E or --vmmpath\t\t- vm manager path\n"
#endif
            "-F or --fullscreen\t\t- start in fullscreen mode\n"
            "-G or --lang langid\t\t- start with specified language\n"
            "\t\t\t\t   (e.g. en-US, or system)\n"
#ifdef SHOW_EXTRA_PARAMS
#ifdef _WIN32
            "-H or --hwnd id,hwnd\t\t- sends back the main dialog's hwnd\n"
#endif
#endif
            "-I or --image d:path\t\t- load 'path' as floppy image on drive d\n"
#ifdef USE_INSTRUMENT
            "-J or --instrument name\t- set 'name' to be the profiling instrument\n"
#endif
            "-L or --logfile path\t\t- set 'path' to be the logfile\n"
            "-M or --missing\t\t- dump missing machines and video cards\n"
            "-N or --noconfirm\t\t- do not ask for confirmation on quit\n"
            "-P or --vmpath path\t\t- set 'path' to be root for vm\n"
            "-O or --global path\t\t- set 'path' to be global config file\n"
            "-R or --rompath path\t\t- set 'path' to be ROM path\n"
#ifndef USE_SDL_UI
            "-S or --settings\t\t\t- show only the settings dialog\n"
#endif
#ifdef SHOW_EXTRA_PARAMS
            "-T or --testmode\t\t- test mode: execute the test mode entry\n"
            "\t\t\t\t   point on init/hard reset\n"
#endif
            "-V or --vmname name\t\t- overrides the name of the running VM\n"
#ifdef _WIN32
            "-W or --nohook\t\t- disables keyboard hook\n"
#else
            "-W or --nohook\t\t- alters keyboard behavior\n"
#endif
            "-X or --clear what\t\t- clears the 'what' (cmos/flash/both)\n"
#ifdef SHOW_EXTRA_PARAMS
            "-Y or --donothing\t\t- do not show any UI or run the emulation\n"
#endif
            "-Z or --lastvmpath\t\t- the last param. is VM path rather than config\n"
            "\nA config file can be specified. If none is, the default file will be used.\n");

#ifdef _WIN32
    ui_msgbox(MBX_INFO, p);
#else
    always_log("%s", p);
#endif
}

/*
 * Perform initial startup of the PC.
 *
 * This is the platform-indepenent part of the startup,
 * where we check commandline arguments and load a
 * configuration file.
 */
int
pc_init(int argc, char *argv[])
{
    char            *ppath = NULL;
    char            *rpath = NULL;
    char            *apath = NULL;
    char            *cfg = NULL;
    char            *global = NULL;
    char            *p;
    char             temp[2048];
    char            *fn[FDD_NUM] = { NULL };
    char             drive = 0;
    char            *temp2 = NULL;
    char            *what;
    const struct tm *info;
    time_t           now;
    int              c;
    int              lvmp = 0;
    int lang_init = 0;

    /* Grab the executable's full path. */
    plat_get_exe_name(exe_path, sizeof(exe_path) - 1);
    p  = path_get_filename(exe_path);
    *p = '\0';
#if defined(__APPLE__)
    char contents_path[2048] = {0};
    c = strlen(exe_path);
    if ((c >= 16) && !strcmp(&exe_path[c - 16], "/Contents/MacOS/")) {
        strncpy(contents_path, exe_path, c - 7);
        exe_path[c - 16] = '\0';
        p                = path_get_filename(exe_path);
        *p               = '\0';
    }
#elif !defined(_WIN32)
    /* Grab the actual path if we are an AppImage. */
    p = getenv("APPIMAGE");
    if (p && (p[0] != '\0'))
        path_get_dirname(exe_path, p);
#endif

    path_normalize(exe_path);
    path_slash(exe_path);

    /*
     * Determine if we are running in portable mode.
     *
     * We enable portable mode if the EXE path
     * contains the global config file.
     */
    path_append_filename(temp, exe_path, GLOBAL_CONFIG_FILE);

    FILE *fp = fopen(temp, "r");
    if (fp) {
        portable_mode = 1;
        fclose(fp);
    }

    /*
     * Get the current working directory.
     *
     * This is normally the directory from where the
     * program was run. If we have been started via
     * a shortcut (desktop icon), however, the CWD
     * could have been set to something else.
     */
    plat_getcwd(usr_path, sizeof(usr_path) - 1);
    plat_getcwd(rom_path, sizeof(rom_path) - 1);
    plat_getcwd(asset_path, sizeof(asset_path) - 1);

    for (c = 1; c < argc; c++) {
        if (argv[c][0] != '-')
            break;

        if (!strcasecmp(argv[c], "--help") || !strcasecmp(argv[c], "-?")) {
usage:
            for (uint8_t i = 0; i < FDD_NUM; i++) {
                if (fn[i] != NULL) {
                    free(fn[i]);
                    fn[i] = NULL;
                }
            }

            pc_show_usage();
            return 0;
        } else if (!strcasecmp(argv[c], "--lastvmpath") || !strcasecmp(argv[c], "-Z")) {
            lvmp = 1;
#ifdef _WIN32
        } else if (!strcasecmp(argv[c], "--debug") || !strcasecmp(argv[c], "-D")) {
            pc_debug_console();
#endif
#ifndef USE_SDL_UI
        } else if (!strcasecmp(argv[c], "--vmmpath") ||
                   !strcasecmp(argv[c], "-E")) {
            /* Using this variable for vm manager path
               Temporary solution!*/
            if ((c+1) == argc) goto usage;
            char *vp = argv[++c];
            if ((strlen(vp) + 1) >= sizeof(vmm_path))
                memcpy(vmm_path, vp, sizeof(vmm_path));
            else
                memcpy(vmm_path, vp, strlen(vp) + 1);
#endif
        } else if (!strcasecmp(argv[c], "--fullscreen") || !strcasecmp(argv[c], "-F")) {
            start_in_fullscreen = 1;
        } else if (!strcasecmp(argv[c], "--logfile") || !strcasecmp(argv[c], "-L")) {
            if ((c + 1) == argc)
                goto usage;

            strcpy(log_path, argv[++c]);
        } else if (!strcasecmp(argv[c], "--vmpath") || !strcasecmp(argv[c], "-P")) {
            if ((c + 1) == argc)
                goto usage;

            ppath = argv[++c];
            start_vmm = 0;
        } else if (!strcasecmp(argv[c], "--rompath") || !strcasecmp(argv[c], "-R")) {
            if ((c + 1) == argc)
                goto usage;

            rpath = argv[++c];
            rom_add_path(rpath);
        } else if (!strcasecmp(argv[c], "--assetpath") || !strcasecmp(argv[c], "-A")) {
            if ((c + 1) == argc)
                goto usage;

            apath = argv[++c];
            asset_add_path(apath);
        } else if (!strcasecmp(argv[c], "--config") || !strcasecmp(argv[c], "-C")) {
            if ((c + 1) == argc || plat_dir_check(argv[c + 1]))
                goto usage;

            cfg = argv[++c];
            start_vmm = 0;
        } else if (!strcasecmp(argv[c], "--global") || !strcasecmp(argv[c], "-O")) {
            if ((c + 1) == argc || plat_dir_check(argv[c + 1]))
                goto usage;

            global_cfg_overridden = 1;
            global = argv[++c];
        } else if (!strcasecmp(argv[c], "--image") || !strcasecmp(argv[c], "-I")) {
            if ((c + 1) == argc)
                goto usage;

            temp2 = (char *) calloc(2048, 1);
            if (sscanf(argv[++c], "%c:%2047s", &drive, temp2) != 2) {
                fprintf(stderr, "Invalid input format for --image option.\n");
                free(temp2);
                goto usage;
            }
            const char old_drive = drive;
            drive = (char) (drive & 0x1f);
            if (old_drive > 0x40)
                drive--;
            if (drive < 0)
                drive = 0;
            if (drive >= FDD_NUM)
                drive = FDD_NUM - 1;
            fn[(int) drive] = (char *) calloc(2048, 1);
            strcpy(fn[(int) drive], temp2);
            pclog("Drive %c: %s\n", drive + 0x41, fn[(int) drive]);
            free(temp2);
            temp2 = NULL;
        } else if (!strcasecmp(argv[c], "--vmname") || !strcasecmp(argv[c], "-V")) {
            if ((c + 1) == argc)
                goto usage;

            strcpy(vm_name, argv[++c]);
#ifndef USE_SDL_UI
        } else if (!strcasecmp(argv[c], "--settings") || !strcasecmp(argv[c], "-S")) {
            settings_only = 1;
#endif
        } else if (!strcasecmp(argv[c], "--testmode") || !strcasecmp(argv[c], "-T")) {
            test_mode = 1;
        } else if (!strcasecmp(argv[c], "--noconfirm") || !strcasecmp(argv[c], "-N")) {
            confirm_exit_cmdl = 0;
        } else if (!strcasecmp(argv[c], "--missing") || !strcasecmp(argv[c], "-M")) {
            dump_missing = 1;
        } else if (!strcasecmp(argv[c], "--donothing") || !strcasecmp(argv[c], "-Y")) {
            do_nothing = 1;
        } else if (!strcasecmp(argv[c], "--nohook") || !strcasecmp(argv[c], "-W")) {
            hook_enabled = 0;
        } else if (!strcasecmp(argv[c], "--clear") || !strcasecmp(argv[c], "-X")) {
            if ((c + 1) == argc)
                goto usage;

            what = argv[++c];

            if (!strcasecmp(what, "cmos"))
                clear_cmos = 1;
            else if (!strcasecmp(what, "flash"))
                clear_flash = 1;
            else if (!strcasecmp(what, "both")) {
                clear_cmos = 1;
                clear_flash = 1;
            } else
                goto usage;
#ifdef _WIN32
        } else if (!strcasecmp(argv[c], "--hwnd") || !strcasecmp(argv[c], "-H")) {

            if ((c + 1) == argc)
                goto usage;

            argv[++c][16] = '\0';
            source_hwnd   = strtoull(&(argv[c][17]), NULL, 16);
            unique_id     = strtoull(&(argv[c][0]), NULL, 16);
#endif
        } else if (!strcasecmp(argv[c], "--lang") || !strcasecmp(argv[c], "-G")) {
            // This function is currently unimplemented for *nix but has placeholders.

            lang_init = plat_language_code(argv[++c]);
            if (!lang_init)
                always_log("\nWarning: Invalid language code, ignoring --lang parameter.\n\n");

            // The return value of 0 only means that the code is invalid,
            //   not related to that translation is exists or not for the
            //  selected language.
        } else if (!strcasecmp(argv[c], "--test") || !strcasecmp(argv[c], "-T")) {
            /* some (undocumented) test function here.. */

            /* .. and then exit. */
            return 0;
#ifdef USE_INSTRUMENT
        } else if (!strcasecmp(argv[c], "--instrument") || !strcasecmp(argv[c], "-J")) {
            if ((c + 1) == argc)
                goto usage;
            instru_enabled = 1;
            sscanf(argv[++c], "%llu", &instru_run_ms);
#endif
        }

        /* Uhm... out of options here.. */
        else
            goto usage;
    }

    /* One argument (config file) allowed. */
    if (c < argc) {
        if (lvmp)
            ppath = argv[c++];
        else
            cfg = argv[c++];

        start_vmm = 0;
    }

    if (c != argc)
        goto usage;

    path_slash(usr_path);
    path_slash(rom_path);
    path_slash(asset_path);

    /*
     * If the user provided a path for files, use that
     * instead of the current working directory. We do
     * make sure that if that was a relative path, we
     * make it absolute.
     */
    if (ppath != NULL) {
        if (!path_abs(ppath)) {
            /*
             * This looks like a relative path.
             *
             * Add it to the current working directory
             * to convert it (back) to an absolute path.
             */
            strcat(usr_path, ppath);
        } else {
            /*
             * The user-provided path seems like an
             * absolute path, so just use that.
             */
            strcpy(usr_path, ppath);
        }

        /* If the specified path does not yet exist,
           create it. */
        if (!plat_dir_check(usr_path))
            plat_dir_create(usr_path);
    }

    // Add the VM-local ROM path.
    path_append_filename(temp, usr_path, "roms");
    rom_add_path(temp);

    // Add the standard ROM path in the same directory as the executable.
    path_append_filename(temp, exe_path, "roms");
    rom_add_path(temp);

    plat_init_rom_paths();

    // Add the VM-local asset path.
    path_append_filename(temp, usr_path, "assets");
    asset_add_path(temp);

    // Add the standard asset path in the same directory as the executable.
    path_append_filename(temp, exe_path, "assets");
    asset_add_path(temp);

#if defined(__APPLE__)
    // Add the standard asset path within the app bundle.
    if (contents_path[0] != '\0') {
        path_append_filename(temp, contents_path, "Resources/assets");
        asset_add_path(temp);
    }
#elif !defined(_WIN32)
    // Add the standard asset paths within the AppImage.
    p = getenv("APPDIR");
    if (p && (p[0] != '\0')) {
        path_append_filename(temp, p, "usr/local/share/" EMU_NAME "/assets");
        asset_add_path(temp);
        path_append_filename(temp, p, "usr/share/" EMU_NAME "/assets");
        asset_add_path(temp);
    }
#endif

    plat_init_asset_paths();

    /*
     * If the user provided a path for ROMs, use that
     * instead of the current working directory. We do
     * make sure that if that was a relative path, we
     * make it absolute.
     */
    if (rpath != NULL) {
        if (!path_abs(rpath)) {
            /*
             * This looks like a relative path.
             *
             * Add it to the current working directory
             * to convert it (back) to an absolute path.
             */
            strcat(rom_path, rpath);
        } else {
            /*
             * The user-provided path seems like an
             * absolute path, so just use that.
             */
            strcpy(rom_path, rpath);
        }

        /* If the specified path does not yet exist,
           create it. */
        if (!plat_dir_check(rom_path))
            plat_dir_create(rom_path);
    } else
        rom_path[0] = '\0';

    /*
     * If the user provided a path for ROMs, use that
     * instead of the current working directory. We do
     * make sure that if that was a relative path, we
     * make it absolute.
     */
    if (apath != NULL) {
        if (!path_abs(apath)) {
            /*
             * This looks like a relative path.
             *
             * Add it to the current working directory
             * to convert it (back) to an absolute path.
             */
            strcat(asset_path, apath);
        } else {
            /*
             * The user-provided path seems like an
             * absolute path, so just use that.
             */
            strcpy(asset_path, apath);
        }

        /* If the specified path does not yet exist,
           create it. */
        if (!plat_dir_check(asset_path))
            plat_dir_create(asset_path);
    } else
        asset_path[0] = '\0';

    /* Grab the name of the configuration file. */
    if (cfg == NULL)
        cfg = CONFIG_FILE;

    /*
     * If the configuration file name has (part of)
     * a pathname, consider that to be part of the
     * actual working directory.
     *
     * This can happen when people load a config
     * file using the UI, for example.
     */
    p = path_get_filename(cfg);
    if (cfg != p) {
        /*
         * OK, the configuration file name has a
         * path component. Separate the two, and
         * add the path component to the cfg path.
         */
        *(p - 1) = '\0';

        /*
         * If this is an absolute path, keep it, as
         * there is probably have a reason to do so.
         * Otherwise, assume the pathname given is
         * relative to whatever the usr_path is.
         */
        if (path_abs(cfg))
            strcpy(usr_path, cfg);
        else
            strcat(usr_path, cfg);
    }

    /* Make sure we have a trailing backslash. */
    path_slash(usr_path);
    if (rom_path[0] != '\0')
        path_slash(rom_path);
    if (asset_path[0] != '\0')
        path_slash(asset_path);

    /* At this point, we can safely create the full path name. */
    path_append_filename(cfg_path, usr_path, p);

    /* Build the global configuration file path. */
    if (global == NULL) {
        plat_get_global_config_dir(global_cfg_path, sizeof(global_cfg_path));
        // avoid strcpy global_cfg_path over itself (valgrind says it's bad...)
        // path_append_filename(global_cfg_path, global_cfg_path, GLOBAL_CONFIG_FILE);
        path_slash(global_cfg_path);
        strcat(global_cfg_path, GLOBAL_CONFIG_FILE);
    } else {
        strncpy(global_cfg_path, global, sizeof(global_cfg_path) - 1);
    }

    /*
     * Get the current directory's name
     *
     * At this point usr_path is perfectly initialized.
     * If no --vmname parameter specified we'll use the
     *   working directory name as the VM's name.
     */
    if (!vm_name[0]) {
        char ltemp[1024] = { '\0' };
        path_get_dirname(ltemp, usr_path);
        strcpy(vm_name, path_get_filename(ltemp));
    }

#ifdef _WIN32
    /* Update debug console title with the VM name. */
    if (force_debug)
        pc_debug_console();
#endif

    /*
     * This is where we start outputting to the log file,
     * if there is one. Create a little info header first.
     */
    struct tm time_buf;

    (void) time(&now);
#ifdef _WIN32
    if (localtime_s(&time_buf, &now) == 0)
        info = &time_buf;
    else
        info = NULL;
#else
    info = localtime_r(&now, &time_buf);
#endif

    if (info)
        strftime(temp, sizeof(temp), "%Y/%m/%d %H:%M:%S", info);
    else
        strcpy(temp, "unknown");

    pclog("#\n# %s v%s logfile, created %s\n#\n",
          EMU_NAME, EMU_VERSION_FULL, temp);

    if (portable_mode) {
        pclog("# Portable mode enabled.\n");
    }

    pclog("# Emulator path: %s\n", exe_path);
    pclog("# Global configuration file: %s\n", global_cfg_path);

    /* Initialize the keyboard accelerator list with default values */
    for (int x = 0; x < NUM_ACCELS; x++) {
        strcpy(acc_keys[x].name, def_acc_keys[x].name);
        strcpy(acc_keys[x].desc, def_acc_keys[x].desc);
        strcpy(acc_keys[x].seq, def_acc_keys[x].seq);
    }

    /* Load the global configuration file. */
    config_load_global();
    config_save_global(); // hack

    /* Determine whether to start the VM manager. */
#ifndef USE_SDL_UI
    if (vmm_disabled && start_vmm)
#endif
    {
        start_vmm = 0;
#ifdef __APPLE__
        if (!strncmp(exe_path, "/private/var/folders/", 21)) {
            ui_msgbox_header(MBX_ERROR | MBX_FATAL, "App Translocation", EMU_NAME " cannot determine the emulated machine's location due to a macOS security feature. Please move the " EMU_NAME " app to another folder (not /Applications), or make a copy of it and open that copy instead.");
            return 0;
        }
#endif
    }

#ifndef USE_SDL_UI
    if (strlen(vmm_path) != 0) {
        /* -E specified on the command line. */
        start_vmm = 1;
    } else {
        strncpy(vmm_path, vmm_path_cfg, sizeof(vmm_path) - 1);
        vmm_path[sizeof(vmm_path) - 1] = '\0';
    }

    if (start_vmm) {
        pclog("# VM Manager enabled. Path: %s\n", vmm_path);
        strncpy(usr_path, vmm_path, sizeof(usr_path) - 1);
        usr_path[sizeof(usr_path) - 1] = '\0';
    } else
#endif
    {
        pclog("# VM: %s\n#\n", vm_name);
        pclog("# Configuration file: %s\n#\n\n", cfg_path);
        pclog("# Userfiles path: %s\n", usr_path);

        for (rom_path_t *cur_rom_path = &rom_paths; cur_rom_path != NULL; cur_rom_path = cur_rom_path->next) {
            pclog("# ROM path: %s\n", cur_rom_path->path);
        }

        for (rom_path_t *cur_asset_path = &asset_paths; cur_asset_path != NULL; cur_asset_path = cur_asset_path->next) {
            pclog("# Asset path: %s\n", cur_asset_path->path);
        }

        /*
         * We are about to read the configuration file, which MAY
         * put data into global variables (the hard- and floppy
         * disks are an example) so we have to initialize those
         * modules before we load the config..
         */
        hdd_init();
        network_init();
        mouse_init();
        cdrom_global_init();
        rdisk_global_init();
        mo_global_init();
        tape_global_init();

        /* Load the configuration file. */
        config_load();
        /* To save the global key binds. */
        config_save_global();

        /* Clear the CMOS and/or BIOS flash file, if we were started with
           the relevant parameter(s). */
        if (clear_cmos) {
            delete_nvr_file(0);
            clear_cmos = 0;
        }

        if (clear_flash) {
            delete_nvr_file(1);
            clear_flash = 0;
        }

        for (uint8_t i = 0; i < FDD_NUM; i++) {
            if (fn[i] != NULL) {
                if (strlen(fn[i]) <= 511)
                    strncpy(floppyfns[i], fn[i], 511);
                free(fn[i]);
                fn[i] = NULL;
            }
        }
    }

    /* Load the desired language */
    if (lang_init)
        lang_id = lang_init;

    /* All good! */
    return 1;
}

void
pc_speed_changed(void)
{
    if (cpu_s != NULL) {
        if (cpu_s->cpu_type >= CPU_286)
            pit_set_clock(cpu_s->rspeed);
        else
            pit_set_clock((uint32_t) 14318184.0);
    }
}

void
pc_full_speed(void)
{
    if (!atfullspeed) {
        pc_log("Set fullspeed - %i %i\n", is386, is486);
        pc_speed_changed();
    }
    atfullspeed = 1;
}

/* Initialize modules, ran once, after pc_init. */
int
pc_init_roms(void)
{
    int     c;
    int     m;
    char    tempc[512];

    if (dump_missing) {
        c = 0;
        while (machine_get_internal_name_ex(c) != NULL) {
            m = machine_available(c);
            if (!m)
                pclog("Missing machine: %s\n", machine_getname(c));
            c++;
        }

        c = 0;
        while (video_get_internal_name(c) != NULL) {
            memset(tempc, 0, sizeof(tempc));
            device_get_name(video_card_getdevice(c), 0, tempc);
            if ((c > 1) && !(tempc[0]))
                break;
            m = video_card_available(c);
            if (!m)
                pclog("Missing video card: %s\n", tempc);
            c++;
        }

        dump_missing = 0;
    }

    pc_log("Scanning for ROM images:\n");
    c = 0;
    for (m = 0; m <= machine_count(); m++) {
#ifdef CLAUDE_LOG
        /* This scan can take tens of seconds on real SD card I/O (see the
         * CONFIG_ESP_TASK_WDT_TIMEOUT_S bump in sdkconfig) - a periodic
         * heartbeat here is the only sign of life on the serial console
         * while it runs, since pc_log() itself is a no-op unless
         * ENABLE_PC_LOG is defined. */
        if ((m % 32) == 0)
            pclog("# Scanning ROMs: %d/%d\n", m, machine_count());
#endif
        c += machine_available(m);
    }
    if (c == 0) {
        /* No usable ROMs found, aborting. */
        return 0;
    }
    pc_log("A total of %d ROM sets have been loaded.\n", c);

    return 1;
}

int
pc_init_modules(void)
{
    int  c;
    char temp[512];
    char tempc[512];

    /* Load the ROMs for the selected machine. */
    if (!machine_available(machine)) {
        snprintf(temp, sizeof(temp), plat_get_string(STRING_HW_NOT_AVAILABLE_MACHINE), machine_getname(machine));
        c       = 0;
        machine = -1;
        while (machine_get_internal_name_ex(c) != NULL) {
            if (machine_available(c)) {
                ui_msgbox_header(MBX_WARNING, plat_get_string(STRING_HW_NOT_AVAILABLE_TITLE), temp);
                machine = c;
                config_save();
                break;
            }
            c++;
        }
        if (machine == -1) {
            fatal("No available machines\n");
            exit(-1);
        }
    }

    /* Make sure we have a usable video card. */
#ifdef CLAUDE_LOG
    pclog("# video check: gfxcard[0]=%d internal_name=%s available=%d\n",
          gfxcard[0], video_get_internal_name(gfxcard[0]) ? video_get_internal_name(gfxcard[0]) : "(null)",
          video_card_available(gfxcard[0]));
#endif
    if (!video_card_available(gfxcard[0])) {
        memset(tempc, 0, sizeof(tempc));
        device_get_name(video_card_getdevice(gfxcard[0]), 0, tempc);
        snprintf(temp, sizeof(temp), plat_get_string(STRING_HW_NOT_AVAILABLE_VIDEO), tempc);
        c = 0;
        while (video_get_internal_name(c) != NULL) {
            gfxcard[0] = -1;
            if (video_card_available(c)) {
                ui_msgbox_header(MBX_WARNING, plat_get_string(STRING_HW_NOT_AVAILABLE_TITLE), temp);
                gfxcard[0] = c;
                config_save();
                break;
            }
            c++;
        }
        if (gfxcard[0] == -1) {
            fatal("No available video cards\n");
            exit(-1);
        }
    }

    // TODO
    for (uint8_t i = 1; i < GFXCARD_MAX; i ++) {
        if (!video_card_available(gfxcard[i])) {
            memset(tempc, 0, sizeof(tempc));
            device_get_name(video_card_getdevice(gfxcard[i]), 0, tempc);
            snprintf(temp, sizeof_w(temp), plat_get_string(STRING_HW_NOT_AVAILABLE_DEVICE), tempc);
            ui_msgbox_header(MBX_WARNING, plat_get_string(STRING_HW_NOT_AVAILABLE_TITLE), temp);
            gfxcard[i] = 0;
        }
    }

    atfullspeed = 0;

    random_init();

    mem_init();

#ifdef USE_DYNAREC
#    if defined(__APPLE__) && defined(__aarch64__)
    if (__builtin_available(macOS 11.0, *)) {
        pthread_jit_write_protect_np(0);
    }
#    endif
    codegen_init();
#    if defined(__APPLE__) && defined(__aarch64__)
    if (__builtin_available(macOS 11.0, *)) {
        pthread_jit_write_protect_np(1);
    }
#    endif
#endif

    keyboard_init();
    joystick_init();

    video_init();

    fdd_init();
    
    if (fdd_sounds_enabled) {
#ifndef DISABLE_FDD_AUDIO
        fdd_audio_load_profiles();
#endif
        fdd_audio_init();
    }
    
    hdd_audio_load_profiles();
    hdd_audio_init();

    sound_init();

    hdc_init();

    video_reset_close();

    machine_status_init();

    serial_set_next_inst(0);

    lpt_set_3bc_used(0);
    lpt_set_next_inst(0);

    for (c = 0; c <= 0x7ff; c++) {
        int64_t exp = c - 1023; /* 1023 = BIAS64 */
        exp_pow_table[c] = pow(2.0, (double) exp);
    }

    if (do_nothing) {
        do_nothing = 0;
        exit(-1);
    }

    return 1;
}

void
pc_send_ca(uint16_t sc)
{
    keyboard_toggle_override();

    if (keyboard_mode >= 0x81) {
        /* Use R-Alt because PS/55 DOS and OS/2 assign L-Alt Kanji */
        keyboard_input(1, 0x1D);  /*  Ctrl key pressed */
        if (keyboard_get_in_reset())
            return;
        keyboard_input(1, 0x138); /* R-Alt key pressed */
        if (keyboard_get_in_reset())
            return;
        keyboard_input(1, sc);
        if (keyboard_get_in_reset())
            return;
        usleep(50000);
        if (keyboard_get_in_reset())
            return;
        keyboard_input(0, sc);
        if (keyboard_get_in_reset())
            return;
        keyboard_input(0, 0x138); /* R-Alt key released */
        if (keyboard_get_in_reset())
            return;
        keyboard_input(0, 0x1D);  /*  Ctrl key released */
        if (keyboard_get_in_reset())
            return;
    } else {
        keyboard_input(1, 0x1D); /* Ctrl key pressed */
        if (keyboard_get_in_reset())
            return;
        keyboard_input(1, 0x38); /* Alt key pressed */
        if (keyboard_get_in_reset())
            return;
        keyboard_input(1, sc);
        if (keyboard_get_in_reset())
            return;
        usleep(50000);
        if (keyboard_get_in_reset())
            return;
        keyboard_input(0, sc);
        if (keyboard_get_in_reset())
            return;
        keyboard_input(0, 0x38); /* Alt key released */
        if (keyboard_get_in_reset())
            return;
        keyboard_input(0, 0x1D); /* Ctrl key released */
        if (keyboard_get_in_reset())
            return;
    }

    keyboard_toggle_override();
}

/* Send the machine a Control-Alt-DEL sequence. */
void
pc_send_cad(void)
{
    pc_send_ca(0x153);
}

/* Send the machine a Control-Alt-ESC sequence. */
void
pc_send_cae(void)
{
    pc_send_ca(1);
}

/*
   Currently available API:

   extern void     resetx86(void);
   extern void     softresetx86(void);
   extern void     hardresetx86(void);

   extern void     biu_set_bus_cycle(int bus_cycle);
   extern void     biu_set_bus_state(int bus_state);
   extern void     biu_set_bus_next_state(int bus_next_state);
   extern void     biu_set_cycle_t1(void);
   extern void     biu_set_next_cycle(void);
   extern int      biu_get_bus_cycle(void);
   extern int      biu_get_bus_state(void);
   extern int      biu_get_bus_next_state(void);
   extern void     prefetch_queue_set_pos(int pos);
   extern void     prefetch_queue_set_ip(uint16_t ip);
   extern void     prefetch_queue_set_in(uint16_t in);
   extern void     prefetch_queue_set_suspended(int p);
   extern int      prefetch_queue_get_pos(void);
   extern uint16_t prefetch_queue_get_ip(void);
   extern uint16_t prefetch_queue_get_in(void);
   extern int      prefetch_queue_get_suspended(void);
   extern int      prefetch_queue_get_size(void);
 */
static void
pc_test_mode_entry_point(void)
{
    pclog("Test mode entry point\n=====================\n");
}

void
pc_reset_hard_close(void)
{
    ui_sb_set_ready(0);

    /* Close all the memory mappings. */
    mem_close();

    suppress_overscan = 0;

    /* Turn off timer processing to avoid potential segmentation faults. */
    timer_close();

    lpt_devices_close(0);
    serial_devices_close(0);

    nvr_save();
    nvr_close();

    mouse_close();
    
    sound_close();

    device_close_all();

    scsi_device_close_all();

    midi_out_close();

    midi_in_close();

    cdrom_close();

    rdisk_close();

    mo_close();

    tape_close();

    scsi_disk_close();

    closeal();

    video_reset_close();

    cpu_close();

    serial_set_next_inst(0);

    lpt_set_3bc_used(0);
    lpt_set_next_inst(0);
}

/*
 * This is basically the spot where we start up the actual machine,
 * by issuing a 'hard reset' to the entire configuration. Order is
 * somewhat important here. Functions here should be named _reset
 * really, as that is what they do.
 */
void
pc_reset_hard_init(void)
{
    /*
     * First, we reset the modules that are not part of
     * the actual machine, but which support some of the
     * modules that are.
     */

    keyboard_init();

    /* Reset the IDE and SCSI presences */
    other_ide_present = other_scsi_present = 0;

    /* Mark ACPI as unavailable */
    acpi_enabled = 0;

    /* Reset the general machine support modules. */
    io_init();

    /* Turn on and (re)initialize timer processing. */
    timer_init();

    device_init();

    sound_reset();

    scsi_reset();
    scsi_device_init();

    ide_hard_reset();

    lpt_ports_reset();

    /* Initialize the actual machine and its basic modules. */
    machine_init();

    /* Reset some basic devices. */
    shadowbios = 0;

    /*
     * Once the machine has been initialized, all that remains
     * should be resetting all devices set up for it, to their
     * current configurations !
     *
     * For now, we will call their reset functions here, but
     * that will be a call to device_reset_all() later !
     */

    /* Reset and reconfigure the Sound Card layer. */
    sound_card_reset();

    /* Initialize parallel devices. */
    /* note: PLIP LPT side has to be initialized before the network side */
    lpt_standalone_init();
    lpt_devices_init();

    /* Reset and reconfigure the serial ports. */
    /* note: SLIP COM side has to be initialized before the network side */
    serial_standalone_init();
    serial_devices_init();

    /* Reset and reconfigure the Network Card layer. */
    network_reset();

    /*
     * Reset the mouse, this will attach it to any port needed.
     */
    mouse_reset();

    /* Reset the Hard Disk Controller module. */
    hdc_reset();

    fdc_card_init();

    fdd_reset();

    /* Reset HDD audio to pick up any profile changes */
    hdd_audio_reset();

    /* Reset and reconfigure the SCSI layer. */
    scsi_card_init();

    scsi_disk_hard_reset();

    cdrom_hard_reset();

    /* Reset the CD-ROM Controller module. */
    cdrom_interface_reset();

    mo_hard_reset();

    tape_hard_reset();

    rdisk_hard_reset();


    /* Reset any ISA ROM cards. */
    isarom_reset();

    /* Reset any ISA RTC cards. */
    isartc_reset();

#if defined(USE_VFIO) && defined(__linux__)
    /* Initialize VFIO */
    vfio_init();
#endif

    /* installs first game port if no device provides one, must be late */
    if (joystick_type[0])
        gameport_update_joystick_type(0);

    ui_sb_update_panes();

    if (config_changed) {
        config_save();

        config_changed = 0;
    } else
        ui_sb_set_ready(1);

    /* Needs the status bar... */
    if (bugger_enabled)
        device_add(&bugger_device);
    if (postcard_enabled)
        device_add(&postcard_device);
    if (unittester_enabled)
        device_add(&unittester_device);

    if (novell_keycard_enabled)
        device_add(&novell_keycard_device);

    if (IS_ARCH(machine, MACHINE_BUS_PCI)) {
        pci_register_cards();
        device_reset_all(DEVICE_PCI);
    }

    /* Mark IDE shadow drives (slaves with a present master) as such in case
       the IDE controllers present are not some form of PCI. */
    ide_drives_set_shadow();

    /* Make sure to disable any sound timers with no handlers. */
    sound_recalc_timers();

    /* Reset the CPU module. */
    resetx86();
    dma_reset();
    pci_pic_reset();
    cpu_cache_int_enabled = cpu_cache_ext_enabled = 0;

    atfullspeed = 0;
    pc_full_speed();

    cycles = 0;
#ifdef FPU_CYCLES
    fpu_cycles = 0;
#endif
#ifdef USE_DYNAREC
    cycles_main = 0;
#endif

    if (test_mode)
        pc_test_mode_entry_point();

    ui_hard_reset_completed();
}

void
pc_reset_hard(void)
{
    hard_reset_pending = 1;
}

void
pc_close(UNUSED(thread_t *ptr))
{
    /* Wait a while so things can shut down. */
    plat_delay_ms(200);

    /* Claim the video blitter. */
    startblit();

    /* Terminate the UI thread. */
    is_quit = 1;

    nvr_save();

    plat_mouse_capture(0);

    /* Close all the memory mappings. */
    mem_close();

    /* Turn off timer processing to avoid potential segmentation faults. */
    timer_close();

    lpt_devices_close(0);
    serial_devices_close(0);

    for (uint8_t i = 0; i < FDD_NUM; i++)
        fdd_close(i);

#ifdef ENABLE_808X_LOG
    if (dump_on_exit)
        dumpregs(0);
#endif

    video_close();

    sound_close();

    device_close_all();

    scsi_device_close_all();

    midi_out_close();

    midi_in_close();

    network_close();

    sound_cd_thread_end();

    cdrom_close();

    rdisk_close();

    mo_close();

    tape_close();

    scsi_disk_close();

    gdbstub_close();

}

#ifdef __APPLE__
static void
_ui_emu_status(void *s)
{
    ui_emu_status(*((int *) s));
}
#endif

void
ack_pause(void)
{
    if (ATOMIC_LOAD(do_pause_ack)) {
        ATOMIC_STORE(do_pause_ack, 0);
        ATOMIC_STORE(pause_ack, 1);
    }
}

void
pc_run(void)
{
#ifdef CLAUDE_LOG
    /* Diagnostic: pc_run() as a whole costs ~11ms of wall-clock time per
     * call on real Tab5 hardware (measured via the main-loop heartbeat),
     * far more than the ~1ms of guest CPU time it's meant to simulate,
     * and ruling out esp32_input_poll() (rate-limited separately,
     * doesn't move the needle) as the cause. Break down where the time
     * actually goes: before cpu_exec() (rivatimer/startblit), cpu_exec()
     * itself, and after (mouse/joystick/endblit). Rate-limited to ~once
     * a second. */
#ifdef MEM_COMPACT_TABLES
    extern uint64_t esp32_mem_resolve_time_us;
    extern uint32_t esp32_mem_resolve_calls;
#else
    /* mem_mapping_resolve_read()/_write() (and their timing counters)
     * only exist under MEM_COMPACT_TABLES - this desktop test build has
     * it off, so these always read 0/0 instead of being undefined. */
    static uint64_t esp32_mem_resolve_time_us = 0;
    static uint32_t esp32_mem_resolve_calls   = 0;
#endif
    extern uint64_t esp32_cga_poll_time_us;
    extern uint32_t esp32_cga_poll_calls;
    extern uint64_t esp32_blit_wait_time_us;
    extern uint32_t esp32_blit_wait_calls;
    static uint32_t pc_run_diag_count = 0;
    int             pc_run_diag_log   = (pc_run_diag_count % 100) == 0;
    /* cga_do_blit()'s vsync-triggered blit fires roughly once per ~16.6ms
     * of guest time (one CGA frame), i.e. roughly once every ~16-17
     * pc_run() calls (~1ms of guest time each) - reading a single-call
     * snapshot (the old pc_run_diag_log approach) has only a small chance
     * of landing on the specific call that happens to cross a vsync
     * boundary, and since we only ever sample call #0/#100/#200/...,
     * whether any of those coincides with a blit call is down to phase
     * alignment luck - easily explains blit_wait staying at 0 across an
     * entire test run even though blits are known to really happen (a
     * real CGA screen was photographed on real hardware). Accumulate
     * cga_poll/mem_resolve/blit_wait over the *whole* 100-call window
     * instead (reset at the window's first call, print the cumulative
     * total at its last) - guaranteed to span ~5-6 CGA frames, so a real
     * blit can't be missed by sampling bad luck. */
    int             pc_run_diag_window_end = (pc_run_diag_count % 100) == 99;
    uint64_t        diag_t0 = 0, diag_t1 = 0, diag_t2 = 0, diag_t3 = 0;
    /* Portable equivalent of esp32_main.c's ESP_LOGI "main loop alive"
     * heartbeat (ESP_LOGI doesn't exist outside ESP-IDF, so desktop builds
     * never printed anything like it) - same t=/pc_run_count= shape so
     * ESP32 and desktop logs read the same way side by side. */
    {
        static uint64_t claude_heartbeat_start_us = 0;
        static uint64_t claude_heartbeat_last_us   = 0;
        static uint32_t claude_heartbeat_calls     = 0;
        uint64_t        now_us                     = claude_log_now_us();
        if (claude_heartbeat_start_us == 0) {
            claude_heartbeat_start_us = now_us;
            claude_heartbeat_last_us  = now_us;
        }
        claude_heartbeat_calls++;
        if ((now_us - claude_heartbeat_last_us) >= 3000000ULL) {
            pclog("# main loop alive: t=%lldms pc_run_count=%u\n",
                  (long long) ((now_us - claude_heartbeat_start_us) / 1000ULL),
                  (unsigned) claude_heartbeat_calls);
            claude_heartbeat_last_us = now_us;
        }
    }
    /* m808x_86box_cycle_number() only reflects real state when the
     * bus-accurate "marty" core is active - since m808x_86box_should_use()
     * now always returns false on this platform (see that function's own
     * comment), it silently reads back 0 always, not "no work done". The
     * classic execx86() interpreter's own budget counter is the `cycles`
     * macro (-> cpu_state._cycles): it starts each pc_run() call at
     * whatever residual is left from the previous call (usually near
     * zero/negative, since the loop always exits once it drops <= 0),
     * has the requested budget added, then counts down as instructions
     * execute. Actual cycles consumed = requested + (pre - post). */
    int32_t         diag_cycles_pre = 0, diag_cycles_post = 0;
    if (pc_run_diag_log) {
        diag_t0                   = claude_log_now_us();
        diag_cycles_pre           = cycles;
        esp32_mem_resolve_time_us = 0;
        esp32_mem_resolve_calls   = 0;
        esp32_cga_poll_time_us    = 0;
        esp32_cga_poll_calls      = 0;
        esp32_blit_wait_time_us   = 0;
        esp32_blit_wait_calls     = 0;
    }
#endif

    /* Trigger a hard reset if one is pending. */
    if (hard_reset_pending) {
        hard_reset_pending = 0;
        pc_reset_hard_close();
        pc_reset_hard_init();
    }

    /* Update the guest-CPU independent timer for devices with independent clock speed */
    rivatimer_update_all();

    /* Run a block of code. */
    startblit();
#ifdef CLAUDE_LOG
    if (pc_run_diag_log)
        diag_t1 = claude_log_now_us();
#endif
    cpu_exec((int32_t) cpu_s->rspeed / (force_10ms ? 100 : 1000));
#if defined(CLAUDE_LOG) && !defined(ESP_PLATFORM)
    /* User-requested artificial slowdown (2026-07-29): testing whether the
     * wild CS jump / interrupt-storm bug is a timing-sensitive race rather
     * than a deterministic pointer bug - it reproduced on ESP32 (~13-16x
     * slower than desktop) but not on a clean fast desktop run. Fixed,
     * constant sleep per call (NOT proportional to any measured duration)
     * - the first version measured cpu_exec()'s own wall-clock time and
     * slept 9x that, which fed back catastrophically into 86Box's own
     * timer catch-up logic (rivatimer_update_all() at the top of the next
     * call sees a huge elapsed-real-time gap caused by our own sleep and
     * does a correspondingly huge amount of catch-up work, making the
     * *next* measured duration even bigger - runaway, confirmed on real
     * hardware: pc_run_count stuck at 15 after 4+ hours). A fixed 10ms
     * sleep has nothing to measure and feed back into, so it can't do
     * that. Desktop only - ESP32 is already this slow natively. */
    usleep(10000);
#endif
#ifdef CLAUDE_LOG
    if (pc_run_diag_log) {
        diag_t2          = claude_log_now_us();
        diag_cycles_post = cycles;
    }
#endif
    ack_pause();
#ifdef USE_GDBSTUB /* avoid a KBC FIFO overflow when CPU emulation is stalled */
    if (gdbstub_step == GDBSTUB_EXEC) {
#endif
        if (!mouse_timed)
            mouse_process();
#ifdef USE_GDBSTUB /* avoid a KBC FIFO overflow when CPU emulation is stalled */
    }
#endif
    joystick_process(0); // Gameport 0
    endblit();

#ifdef CLAUDE_LOG
    if (pc_run_diag_log) {
        diag_t3 = claude_log_now_us();
        pclog("# pc_run #%u timing (us): setup=%llu cpu_exec=%llu rest=%llu total=%llu\n",
              (unsigned) pc_run_diag_count,
              (unsigned long long) (diag_t1 - diag_t0),
              (unsigned long long) (diag_t2 - diag_t1),
              (unsigned long long) (diag_t3 - diag_t2),
              (unsigned long long) (diag_t3 - diag_t0));

        /* Everything below is diagnostic printing itself - moved here,
         * AFTER diag_t3 is captured, so it no longer counts against
         * "rest" above (it used to sit between diag_t2 and the
         * ack_pause()/mouse/joystick/endblit block, inflating "rest"
         * with the cost of these very pclog() calls - printing over a
         * slow console is often the single most expensive thing in this
         * whole function, so that self-measurement bias was real, not
         * negligible). diag_print_us below reports that cost honestly
         * instead of hiding it inside "rest". */
        int32_t cyc_requested = (int32_t) cpu_s->rspeed / (force_10ms ? 100 : 1000);
        int32_t cyc_consumed  = cyc_requested + diag_cycles_pre - diag_cycles_post;
        /* Is the CPU actually exploring a wider range of code now that
         * gfxcard is correctly cga (vs. stuck oscillating in the same
         * narrow ~600-byte ROM-checksum-loop range as before the config
         * fix)? Real forward progress into video-init code would show a
         * much wider/changing address range, not the same few bytes.
         * cycle_consumed wildly larger than cycle_requested would mean
         * the classic interpreter's own loop is overrunning its budget
         * (e.g. a LOOP-based construct not yielding cycles back
         * correctly between iterations), not just "each cycle is slow". */
        pclog("# pc_run #%u: cpu_state.pc=%08X cs=%04X es=%04X dx=%04X bx=%04X cx=%04X si=%04X abrt=%d "
              "cycle_requested=%d cycle_consumed=%d mem_resolve_us=%llu mem_resolve_calls=%u "
              "cga_poll_us=%llu cga_poll_calls=%u blit_wait_us=%llu blit_wait_calls=%u\n",
              (unsigned) pc_run_diag_count, cpu_state.pc, CS, ES, DX, BX, CX, SI, cpu_state.abrt,
              cyc_requested, cyc_consumed,
              (unsigned long long) esp32_mem_resolve_time_us,
              (unsigned) esp32_mem_resolve_calls,
              (unsigned long long) esp32_cga_poll_time_us,
              (unsigned) esp32_cga_poll_calls,
              (unsigned long long) esp32_blit_wait_time_us,
              (unsigned) esp32_blit_wait_calls);
        {
            /* Dump the actual opcode bytes at CS:IP (real mode: phys = CS*16+IP)
             * to see what the CPU is really doing while apparently stuck. */
            uint32_t phys = ((uint32_t) CS << 4) + cpu_state.pc;
            uint8_t  b[16];
            for (int bi = 0; bi < 16; bi++)
                b[bi] = mem_readb_phys(phys + bi);
            pclog("#   bytes at %04X:%08X (phys=%08X): "
                  "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  CS, cpu_state.pc, phys,
                  b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                  b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
            pclog("#   pic: imr=%02X irr=%02X isr=%02X int_pending=%u lines0=%u ocw2=%02X "
                  "pic2: imr=%02X irr=%02X isr=%02X int_pending=%u lines0=%u if=%u "
                  "nmi=%d nmi_enable=%d nmi_mask=%d\n",
                  pic.imr, pic.irr, pic.isr, pic.int_pending, pic.lines[0], pic.ocw2,
                  pic2.imr, pic2.irr, pic2.isr, pic2.int_pending, pic2.lines[0],
                  !!(cpu_state.flags & 0x0200),
                  nmi, nmi_enable, nmi_mask);
        }
        pclog("# pc_run #%u diag_print_us=%llu (this is diagnostic printing overhead, "
              "NOT part of setup/cpu_exec/rest/total above)\n",
              (unsigned) pc_run_diag_count,
              (unsigned long long) (claude_log_now_us() - diag_t3));
    }
    if (pc_run_diag_window_end) {
        /* Cumulative totals across the whole 100-call window just ending -
         * see the comment on pc_run_diag_window_end above for why this is
         * needed instead of trusting the single-call snapshot above for
         * blit_wait specifically. Commented out (2026-07-28) - noisy now
         * that the blit_wait/cga_poll question it was added for is
         * answered; re-enable if that data is needed again. */
        /*
        pclog("# pc_run window ending #%u: mem_resolve_us=%llu mem_resolve_calls=%u "
              "cga_poll_us=%llu cga_poll_calls=%u blit_wait_us=%llu blit_wait_calls=%u\n",
              (unsigned) pc_run_diag_count,
              (unsigned long long) esp32_mem_resolve_time_us,
              (unsigned) esp32_mem_resolve_calls,
              (unsigned long long) esp32_cga_poll_time_us,
              (unsigned) esp32_cga_poll_calls,
              (unsigned long long) esp32_blit_wait_time_us,
              (unsigned) esp32_blit_wait_calls);
        */
    }
    pc_run_diag_count++;
#endif

    /* Done with this frame, update statistics. */
    framecount++;
    if (++framecountx >= (force_10ms ? 100 : 1000)) {
        framecountx = 0;
        frames      = 0;
    }

    if (title_update) {
#ifdef __APPLE__
        static
#endif
        int      speed_percent;
        int      target_fps;
        uint32_t elapsed_ms;
        int64_t  numerator;

        target_fps    = force_10ms ? 100 : 1000;
        elapsed_ms    = fps_sample_elapsed_ms ? fps_sample_elapsed_ms : 1;

#ifdef SCREENSHOT_MODE
        if (force_10ms)
            fps = ((fps + 2) / 5) * 5;
        else
            fps = ((fps + 20) / 50) * 50;
#endif
        /*
         * Use real sample duration for title speed percent so delayed timer
         * callbacks do not create false dip/rebound spikes in speed reporting.
         */
        numerator     = (int64_t) fps * 100000LL;
        speed_percent = (int) ((numerator + ((int64_t) elapsed_ms * target_fps / 2)) /
                               ((int64_t) elapsed_ms * target_fps));

#ifdef __APPLE__
        /* Needed due to modifying the UI on the non-main thread is a big no-no. */
        dispatch_async_f(dispatch_get_main_queue(), &speed_percent, _ui_emu_status);
#else
        ui_emu_status(speed_percent);
#endif
        title_update = 0;
    }
}

/* Handler for the 1-second timer to refresh the window title. */
void
pc_onesec(void)
{
    static uint32_t last_sample_ms = 0;
    uint32_t        now_ms         = plat_get_ticks();

    fps_sample_elapsed_ms = last_sample_ms ? (now_ms - last_sample_ms) : 1000;
    if (!fps_sample_elapsed_ms)
        fps_sample_elapsed_ms = 1;
    last_sample_ms = now_ms;

    fps        = framecount;
    framecount = 0;

    title_update = 1;
}

void
set_screen_size_monitor(int x, int y, int monitor_index)
{
    int    temp_overscan_x = monitors[monitor_index].mon_overscan_x;
    int    temp_overscan_y = monitors[monitor_index].mon_overscan_y;
    int    is_svga         = (video_get_type_monitor(monitor_index) == VIDEO_FLAG_TYPE_SPECIAL) ||
                             (video_get_type_monitor(monitor_index) == VIDEO_FLAG_TYPE_8514);
    double dx;
    double dy;
    double dtx;
    double dty;

    /* Make sure we keep usable values. */
#if 0
    pc_log("SetScreenSize(%d, %d) resize=%d\n", x, y, vid_resize);
#endif
    if (x < 320)
        x = 320;
    if (y < 200)
        y = 200;
    if (x > 2048)
        x = 2048;
    if (y > 2048)
        y = 2048;

    /* Save the new values as "real" (unscaled) resolution. */
    monitors[monitor_index].mon_unscaled_size_x = x;
    monitors[monitor_index].mon_efscrnsz_y      = y;

    if (suppress_overscan)
        temp_overscan_x = temp_overscan_y = 0;

    if (force_43) {
        dx  = (double) x;
        dtx = (double) temp_overscan_x;

        dty = (double) temp_overscan_y;

        /* Account for possible overscan. */
        if (!is_svga && (temp_overscan_y == 16)) {
            /* CGA */
            dy = (((dx - dtx) / 4.0) * 3.0) + dty;
        } else if (!is_svga && (temp_overscan_y < 16)) {
            /* MDA/Hercules */
            dy = (dx / 4.0) * 3.0;
        } else {
            if (enable_overscan)
                /* EGA/(S)VGA with overscan */
                dy = (((dx - dtx) / 4.0) * 3.0) + dty;
            else
                /* EGA/(S)VGA without overscan */
                dy = (dx / 4.0) * 3.0;
        }
        monitors[monitor_index].mon_unscaled_size_y = (int) dy;
    } else
        monitors[monitor_index].mon_unscaled_size_y = monitors[monitor_index].mon_efscrnsz_y;

    switch (scale) {
        case 0: /* 50% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x >> 1);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y >> 1);
            break;

        case 1: /* 100% */
            monitors[monitor_index].mon_scrnsz_x = monitors[monitor_index].mon_unscaled_size_x;
            monitors[monitor_index].mon_scrnsz_y = monitors[monitor_index].mon_unscaled_size_y;
            break;

        case 2: /* 150% */
            monitors[monitor_index].mon_scrnsz_x = ((monitors[monitor_index].mon_unscaled_size_x * 3) >> 1);
            monitors[monitor_index].mon_scrnsz_y = ((monitors[monitor_index].mon_unscaled_size_y * 3) >> 1);
            break;

        case 3: /* 200% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x << 1);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y << 1);
            break;

        case 4: /* 300% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x * 3);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y * 3);
            break;

        case 5: /* 400% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x << 2);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y << 2);
            break;

        case 6: /* 500% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x * 5);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y * 5);
            break;

        case 7: /* 600% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x * 6);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y * 6);
            break;

        case 8: /* 700% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x * 7);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y * 7);
            break;

        case 9: /* 800% */
            monitors[monitor_index].mon_scrnsz_x = (monitors[monitor_index].mon_unscaled_size_x << 3);
            monitors[monitor_index].mon_scrnsz_y = (monitors[monitor_index].mon_unscaled_size_y << 3);
            break;

        default:
            break;
    }

    plat_resize_request(monitors[monitor_index].mon_scrnsz_x, monitors[monitor_index].mon_scrnsz_y, monitor_index);
}

void
set_screen_size(int x, int y)
{
    set_screen_size_monitor(x, y, monitor_index_global);
}

void
reset_screen_size_monitor(int monitor_index)
{
    set_screen_size(monitors[monitor_index].mon_unscaled_size_x, monitors[monitor_index].mon_efscrnsz_y);
}

void
reset_screen_size(void)
{
    for (uint8_t i = 0; i < MONITORS_NUM; i++)
        set_screen_size(monitors[i].mon_unscaled_size_x, monitors[i].mon_efscrnsz_y);
}

void
set_screen_size_natural(void)
{
    for (uint8_t i = 0; i < MONITORS_NUM; i++)
        set_screen_size(monitors[i].mon_unscaled_size_x, monitors[i].mon_unscaled_size_y);
}


void
do_pause(int p)
{
    int old_p = dopause;

    if ((p == 1) && !old_p)
        do_pause_ack = p;
    dopause = !!p;
    if ((p == 1) && !old_p) {
        while (!ATOMIC_LOAD(pause_ack))
            ;
    }
    ATOMIC_STORE(pause_ack, 0);
}

// Helper to find an accelerator key and return it's index in acc_keys
int FindAccelerator(const char *name) {
    for (int x = 0; x < NUM_ACCELS; x++) {
        if (!strcmp(acc_keys[x].name, name))
            return x;
    }

    // No key was found
    return -1;
}
