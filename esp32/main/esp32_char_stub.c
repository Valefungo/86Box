/*
 * esp32_char_stub.c - stub for the char/ backends that need something
 * only a real host OS provides: char_serial.c/char_stdio.c bridge a
 * serial port to a real host TTY (termios/PTY, no such concept on
 * ESP-IDF), and char_pipe.c needs mkfifo() (no named-pipe concept on
 * ESP-IDF/FAT either - there's no separate host process to pipe to on a
 * standalone microcontroller). char.c's device table references all four
 * by name unconditionally, so provide "never available" stand-ins
 * instead of vendoring the three real files.
 */
#include <stddef.h>

#include <86box/86box.h>
#include <86box/device.h>
#include <86box/char.h>

static int
char_unavailable(void)
{
    return 0;
}

const device_t char_serial_passthrough_com_device = {
    .name          = "Serial Passthrough Device",
    .internal_name = "serial_passthrough",
    .flags         = DEVICE_COM,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = char_unavailable,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t char_stdio_com_device = {
    .name          = "Standard Streams Device",
    .internal_name = "stdio",
    .flags         = DEVICE_COM,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = char_unavailable,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t char_pipe_com_device = {
    .name          = "Named Pipe (COM)",
    .internal_name = "pipe",
    .flags         = DEVICE_COM,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = char_unavailable,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t char_pipe_lpt_device = {
    .name          = "Named Pipe (LPT)",
    .internal_name = "pipe",
    .flags         = DEVICE_LPT,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = char_unavailable,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
