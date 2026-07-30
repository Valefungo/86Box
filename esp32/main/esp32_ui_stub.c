/*
 * esp32_ui_stub.c - stubs for the remaining desktop-platform contract
 * pieces that don't fit esp32_plat.c/esp32_video.c/esp32_input.c:
 *
 * - ui_*(): status bar / message box / per-monitor UI hooks (src/ui.h) -
 *   no on-screen chrome on this target yet, so these are no-ops. Message
 *   boxes just log instead of blocking on a dialog.
 * - Globals normally defined by the desktop platform main file
 *   (fixed_size_x/y, rctrl_is_lalt, update_icons, kbd_req_capture,
 *   hide_status_bar, hide_tool_bar): plain storage, matching their
 *   sdl_main.c defaults.
 * - do_stop()/plat_cdrom_ui_update(): platform-level UI hooks, no-ops.
 * - dynld_module()/dynld_close(): dynamic library loading - nothing on
 *   this target ever needs to load a shared object at runtime.
 * - joystick_init/close/process, joystick_state, plat_joystick_state,
 *   joysticks_present: the src/unix/sdl_joystick.c platform contract -
 *   real joystick/gamepad support is a later milestone (this is the same
 *   USB HID path as keyboard/mouse, deferred to M3 step 3).
 * - plip_device: PLIP network device (src/network/net_plip.c) - part of
 *   the network subsystem stubbed out in esp32_network.c.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <86box/86box.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/plat_dynld.h>
#include <86box/gameport.h>
#include <86box/thread.h>
#include <86box/timer.h>
#include <86box/network.h>
#include <86box/ui.h>

int fixed_size_x   = 0;
int fixed_size_y   = 0;
int rctrl_is_lalt  = 0;
int update_icons   = 0;
int kbd_req_capture = 0;
int hide_status_bar = 1;
int hide_tool_bar   = 1;
int mouse_capture   = 0;

/* rdisk/mo/tape eject-reload: UI-triggered "media was removed/changed"
 * hooks (src/include/86box/plat.h) - no on-screen chrome to notify yet. */
void
rdisk_eject(uint8_t id)
{
    (void) id;
}

void
rdisk_reload(uint8_t id)
{
    (void) id;
}

void
mo_eject(uint8_t id)
{
    (void) id;
}

void
mo_reload(uint8_t id)
{
    (void) id;
}

void
tape_eject(uint8_t id)
{
    (void) id;
}

void
tape_reload(uint8_t id)
{
    (void) id;
}

void
do_stop(void)
{
}

void
plat_cdrom_ui_update(uint8_t id, uint8_t reload)
{
    (void) id;
    (void) reload;
}

void *
dynld_module(const char *name, dllimp_t *imports)
{
    (void) name;
    (void) imports;
    return NULL;
}

void
dynld_close(void *handle)
{
    (void) handle;
}

int
ui_msgbox(int flags, char *message)
{
    (void) flags;
    printf("86Box: %s\n", message ? message : "");
    return 0;
}

int
ui_msgbox_header(int flags, char *header, char *message)
{
    (void) flags;
    printf("86Box: %s - %s\n", header ? header : "", message ? message : "");
    return 0;
}

void
ui_emu_status(int speed_percent)
{
    (void) speed_percent;
}

void
ui_hard_reset_completed(void)
{
}

void
ui_init_monitor(int monitor_index)
{
    (void) monitor_index;
}

void
ui_deinit_monitor(int monitor_index)
{
    (void) monitor_index;
}

void
ui_sb_set_ready(int ready)
{
    (void) ready;
}

void
ui_sb_update_panes(void)
{
}

void
ui_sb_update_icon(int tag, int active)
{
    (void) tag;
    (void) active;
}

void
ui_sb_update_icon_write(int tag, int write)
{
    (void) tag;
    (void) write;
}

void
ui_sb_update_icon_state(int tag, int state)
{
    (void) tag;
    (void) state;
}

void
ui_sb_update_icon_wp(int tag, int state)
{
    (void) tag;
    (void) state;
}

void
ui_sb_bugui(char *str)
{
    (void) str;
}

joystick_state_t      ESP32_BIG_BSS_ATTR joystick_state[GAMEPORT_MAX][MAX_JOYSTICKS];
plat_joystick_state_t ESP32_BIG_BSS_ATTR plat_joystick_state[MAX_PLAT_JOYSTICKS];
int                   joysticks_present = 0;

void
joystick_init(void)
{
}

void
joystick_close(void)
{
}

void
joystick_process(uint8_t gp)
{
    (void) gp;
}

static int
plip_unavailable(void)
{
    return 0;
}

const device_t plip_device = {
    .name          = "Parallel Line Internet Protocol",
    .internal_name = "plip",
    .flags         = DEVICE_LPT,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = plip_unavailable,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
