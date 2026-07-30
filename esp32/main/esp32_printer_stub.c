/*
 * esp32_printer_stub.c - stub for the ESC/P dot-matrix printer device.
 *
 * prt_escp.c needs freetype2 (font rendering for the printed page) and
 * is the only caller of png.c's libpng-based page-dump functions -
 * neither library exists on ESP-IDF, and printer emulation isn't needed
 * to prove the plat_* contract for this M3 skeleton build. char.c's
 * device table references lpt_prt_escp_device unconditionally, so
 * provide a "never available" stand-in instead of vendoring both files.
 */
#include <stddef.h>

#include <86box/86box.h>
#include <86box/device.h>

static int
prt_escp_unavailable(void)
{
    return 0;
}

const device_t lpt_prt_escp_device = {
    .name          = "Generic ESC/P 2 Dot-Matrix Printer",
    .internal_name = "dot_matrix",
    .flags         = DEVICE_LPT,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = prt_escp_unavailable,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
