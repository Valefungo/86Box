/*
 * esp32_video.c - video_setblit contract implementation for the M5Stack
 * Tab5 (ESP32-P4): ST7121 MIPI-DSI panel, 720x1280 portrait-native.
 *
 * 86Box's guest video modes are landscape, so the guest's dirty rect is
 * rotated 90 degrees in software while converting ARGB8888 -> RGB565 (no
 * hardware rotation path is used yet - DMA2D is enabled per Espressif's
 * own Tab5 reference but only accelerates straight copies/format
 * conversion, not rotation, in this IDF version).
 *
 * The whole bring-up sequence (IO-expander unlock, DSI PHY LDO, DSI bus,
 * ST7121 panel) is adapted from tiny386's lcd_tab5.c
 * (/home/fungostar/tiny386/esp/main/lcd_tab5.c) - the platform layer this
 * project's plan already treats as trustworthy reference material, unlike
 * tiny386's CPU core.
 *
 * RESOLVED (2026-07-30): this file used to call esp_lcd_panel_mirror(panel,
 * true, false) right after init, with (true, false) copied as an untested
 * guess from sibling board drivers (lcd_axs15231b.c, lcd_st7701.c) in the
 * same tiny386 codebase - tiny386's own lcd_tab5.c never calls it at all.
 * Real hardware showed persistent horizontal banding that slowly faded
 * over time, reproducible even on a single static full-panel push
 * (esp32_video_test_pattern(), no 86Box emulation/blit logic involved at
 * all) - ruling out any software tearing/timing bug in this file's own
 * blit path. panel_st7121_mirror() sends a MADCTL command flipping the
 * GS/SS bits, which control the ST7121's *internal* gate/source scan
 * direction - independent of, and apparently out of sync with, the
 * direction the DPI controller feeds it pixel data in. Removed entirely,
 * matching tiny386's untouched reference. If orientation needs flipping
 * after this, fix it in the row/col mapping in esp32_blit() below
 * (software transpose, already independently verified correct via the
 * test pattern) instead of resurrecting this hardware command.
 */
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7121.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"

#include <86box/86box.h>
#include <86box/video.h>

static const char *TAG = "esp32_video";

/* Panel native resolution (portrait) - MIPI-DSI/DPI config and touch/kbd
 * I2C bus pin mapping, per M5Stack Tab5 schematics (see lcd_tab5.c). */
#define TAB5_LCD_H_RES 720
#define TAB5_LCD_V_RES 1280

/* Logical landscape canvas 86Box renders into, before the 90-degree
 * rotation maps it onto the panel's native portrait surface. Guest modes
 * larger than this in either axis are clipped, not scaled - a future
 * improvement, not needed for a first working picture. */
#define LCD_WIDTH  1280
#define LCD_HEIGHT 720

#define TAB5_I2C_SDA  31
#define TAB5_I2C_SCL  32
#define TAB5_I2C_PORT 0

#define TAB5_LCD_BACKLIGHT_GPIO 22

#define TAB5_DSI_PHY_LDO_CHAN 3
#define TAB5_DSI_PHY_LDO_MV   2500

/* ---------- system I2C bus (IO-expanders now, touch/keyboard later) --- */

/* Shared with esp32_input.c once real touch/keyboard support lands (both
 * live on this same physical bus, per Tab5's schematic) - exposed instead
 * of file-local so a second bus isn't created on the same pins. */
i2c_master_bus_handle_t esp32_sys_i2c_handle;

static esp_err_t
tab5_sys_i2c_init(void)
{
    if (esp32_sys_i2c_handle)
        return ESP_OK;

    i2c_master_bus_config_t cfg = {
        .clk_source                  = I2C_CLK_SRC_DEFAULT,
        .sda_io_num                  = TAB5_I2C_SDA,
        .scl_io_num                  = TAB5_I2C_SCL,
        .i2c_port                    = TAB5_I2C_PORT,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &esp32_sys_i2c_handle);
}

/* ---------- PI4IOE5V6416 IO-expander unlock (x2) ----------------------
 *
 * On cold boot both expanders reset to all-input/high-impedance, which
 * holds LCD_RST and TP_RST low - without this, the DSI panel never
 * responds to init commands and panel_init() hangs until the watchdog
 * resets the board. Must run before the DSI bus is created. */

#define PI4IOE1_I2C_ADDR 0x43
#define PI4IOE2_I2C_ADDR 0x44

#define PI4IOE_REG_CHIP_RESET 0x01
#define PI4IOE_REG_IO_DIR     0x03
#define PI4IOE_REG_OUT_SET    0x05
#define PI4IOE_REG_OUT_H_IM   0x07
#define PI4IOE_REG_IN_DEF_STA 0x09
#define PI4IOE_REG_PULL_EN    0x0B
#define PI4IOE_REG_PULL_SEL   0x0D
#define PI4IOE_REG_INT_MASK   0x11

static esp_err_t
pi4ioe_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev, buf, 2, 50);
}

static esp_err_t
tab5_io_expander_init(void)
{
    ESP_RETURN_ON_ERROR(tab5_sys_i2c_init(), TAG, "sys i2c init failed");

    uint8_t                 reg;
    uint8_t                 val;
    i2c_master_dev_handle_t dev = NULL;

    /* PI4IOE1 (0x43): SPK_EN, EXT5V_EN, LCD_RST, TP_RST, CAM_RST */
    i2c_device_config_t dev_cfg1 = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PI4IOE1_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(esp32_sys_i2c_handle, &dev_cfg1, &dev),
                         TAG, "PI4IOE1 add device failed");

    pi4ioe_write(dev, PI4IOE_REG_CHIP_RESET, 0xFF);
    reg = PI4IOE_REG_CHIP_RESET;
    i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 50);
    pi4ioe_write(dev, PI4IOE_REG_IO_DIR, 0b01111111);
    pi4ioe_write(dev, PI4IOE_REG_OUT_H_IM, 0b00000000);
    pi4ioe_write(dev, PI4IOE_REG_PULL_SEL, 0b01111111);
    pi4ioe_write(dev, PI4IOE_REG_PULL_EN, 0b01111111);
    /* P1=SPK_EN, P2=EXT5V_EN, P4=LCD_RST, P5=TP_RST, P6=CAM_RST -> high */
    pi4ioe_write(dev, PI4IOE_REG_OUT_SET, 0b01110110);

    i2c_master_bus_rm_device(dev);

    /* PI4IOE2 (0x44): WLAN_PWR_EN, USB5V_EN, CHG_EN, power-off line */
    i2c_device_config_t dev_cfg2 = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PI4IOE2_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    dev = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(esp32_sys_i2c_handle, &dev_cfg2, &dev),
                         TAG, "PI4IOE2 add device failed");

    pi4ioe_write(dev, PI4IOE_REG_CHIP_RESET, 0xFF);
    reg = PI4IOE_REG_CHIP_RESET;
    i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 50);
    pi4ioe_write(dev, PI4IOE_REG_IO_DIR, 0b10111001);
    pi4ioe_write(dev, PI4IOE_REG_OUT_H_IM, 0b00000110);
    pi4ioe_write(dev, PI4IOE_REG_PULL_SEL, 0b10111001);
    pi4ioe_write(dev, PI4IOE_REG_PULL_EN, 0b11111001);
    pi4ioe_write(dev, PI4IOE_REG_IN_DEF_STA, 0b01000000);
    pi4ioe_write(dev, PI4IOE_REG_INT_MASK, 0b10111111);
    pi4ioe_write(dev, PI4IOE_REG_OUT_SET, 0b10001001);

    i2c_master_bus_rm_device(dev);
    return ESP_OK;
}

static esp_err_t
tab5_reset_lcd_tp(void)
{
    ESP_RETURN_ON_ERROR(tab5_sys_i2c_init(), TAG, "sys i2c init failed");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PI4IOE1_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    i2c_master_dev_handle_t dev = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(esp32_sys_i2c_handle, &dev_cfg, &dev),
                         TAG, "PI4IOE1 add device failed");

    uint8_t reg = PI4IOE_REG_OUT_SET;
    uint8_t val = 0;
    i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 50);

    /* LCD_RST = P4, TP_RST = P5 - pulse both low then high. */
    pi4ioe_write(dev, PI4IOE_REG_OUT_SET, (uint8_t) (val & ~((1 << 4) | (1 << 5))));
    vTaskDelay(pdMS_TO_TICKS(100));

    pi4ioe_write(dev, PI4IOE_REG_OUT_SET, (uint8_t) (val | (1 << 4) | (1 << 5)));
    vTaskDelay(pdMS_TO_TICKS(100));

    i2c_master_bus_rm_device(dev);
    return ESP_OK;
}

/* ---------- DSI PHY power ---------------------------------------------- */

static esp_err_t
enable_dsi_phy_power(void)
{
    static esp_ldo_channel_handle_t chan = NULL;
    if (chan)
        return ESP_OK;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = TAB5_DSI_PHY_LDO_CHAN,
        .voltage_mv = TAB5_DSI_PHY_LDO_MV,
    };
    return esp_ldo_acquire_channel(&ldo_cfg, &chan);
}

/* ---------- backlight (LEDC PWM) ---------------------------------------- */

static void
backlight_on(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t ch_cfg = {
        .gpio_num   = TAB5_LCD_BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_1,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 4095,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

/* ---------- panel bring-up --------------------------------------------- */

static esp_lcd_panel_handle_t panel;

/* Rotated RGB565 scratch buffer, one dirty rect at a time. Sized for a
 * full-screen redraw (LCD_WIDTH x LCD_HEIGHT) so no dirty rect can ever
 * overflow it; PSRAM-backed since it's ~1.8MB. */
static uint16_t *rot_buf;

/*
 * Tearing fix (2026-07-30): num_fbs=1 means there is exactly one internal
 * frame buffer, continuously scanned out to the panel - there is no second
 * buffer to swap to (no RAM budget for one anyway, ~1.8MB already spent on
 * rot_buf). A real-hardware BIOS POST screen showed clean straight borders
 * (drawn once, never rewritten) but diagonally-sheared/garbled text (drawn
 * repeatedly in large ~720x400 pushes, confirmed via the existing
 * esp32_blit() call-count log - NOT many tiny per-scanline blits as
 * first suspected). Large, unsynchronized draw_bitmap() calls race the
 * continuous DSI scan-out: with no vsync gating, a write can still be
 * copying into the frame buffer while the panel controller is reading
 * that same region for the current scan pass, producing exactly this kind
 * of pattern (post-rotation, an in-progress horizontal write race shows up
 * as a diagonal artifact in the guest's original landscape orientation).
 *
 * Fix without a second full frame buffer: use esp_lcd_dpi_panel's
 * on_refresh_done callback (fires once the internal frame buffer has
 * finished a full scan-out to the physical screen) to gate when writes
 * start, instead of firing them at an arbitrary moment. Binary semaphore,
 * given from ISR context (the callback return value's "higher priority
 * task woken" convention confirms it runs at ISR level) - drained then
 * re-waited (not just taken once) before each draw_bitmap(), because
 * blits happen far less often (~every 300-500ms) than refreshes (~60Hz,
 * ~16.6ms) - a plain take() would almost always return immediately on a
 * long-stale signal from many frames ago, which wouldn't actually bound
 * how far into the *next* scan pass the write might land. Draining first
 * and waiting for a fresh signal ensures the write starts right after a
 * scan just completed, maximizing headroom before the next one begins. */
static SemaphoreHandle_t refresh_done_sem;

static bool
esp32_panel_on_refresh_done(esp_lcd_panel_handle_t panel_handle, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_prio_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(refresh_done_sem, &high_prio_task_woken);
    return high_prio_task_woken == pdTRUE;
}

static void
esp32_panel_init(void)
{
    ESP_LOGI(TAG, "Init IO expanders / panel reset");
    ESP_ERROR_CHECK(tab5_io_expander_init());
    ESP_ERROR_CHECK(tab5_reset_lcd_tp());

    ESP_LOGI(TAG, "Init display");
    ESP_ERROR_CHECK(enable_dsi_phy_power());

    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id           = 0,
        .num_data_lanes   = 2,
        .phy_clk_src      = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 965,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &mipi_dsi_bus));
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_cfg, &io));

    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel = 0,
        .dpi_clk_src     = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 70,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        .in_color_format = LCD_COLOR_FMT_RGB565,
#else
        .pixel_format    = LCD_COLOR_PIXEL_FORMAT_RGB565,
#endif
        .num_fbs = 1,
        .video_timing = {
            .h_size            = TAB5_LCD_H_RES,
            .v_size            = TAB5_LCD_V_RES,
            .hsync_pulse_width = 2,
            .hsync_back_porch  = 40,
            .hsync_front_porch = 40,
            .vsync_pulse_width = 20,
            .vsync_back_porch  = 24,
            .vsync_front_porch = 200,
        },
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
        .flags = { .use_dma2d = true },
#endif
    };

    st7121_vendor_config_t vendor_cfg = {
        .mipi_config = {
            .dsi_bus    = mipi_dsi_bus,
            .dpi_config = &dpi_cfg,
        },
    };

    esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = -1, /* reset already pulsed via I2C IO-expander */
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config  = &vendor_cfg,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7121(io, &dev_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_enable_dma2d(panel));
#endif
    /* GS (mirror_x) is the one that corrupted pixel data - confirmed
     * independently on tiny386's identical ST7121/Tab5 bring-up (GS always
     * corrupts data in groups of 4 pixels on this panel, DMA2D-independent).
     * SS (mirror_y) alone is clean, but rotates the whole image 180 degrees -
     * undone entirely in software in esp32_blit() below (both axes reversed
     * there), matching tiny386's fix. See the file header comment. */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    refresh_done_sem = xSemaphoreCreateBinary();
    if (!refresh_done_sem)
        ESP_LOGE(TAG, "failed to create refresh_done_sem - tearing fix disabled");
    else {
        esp_lcd_dpi_panel_event_callbacks_t cbs = {
            .on_refresh_done = esp32_panel_on_refresh_done,
        };
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel, &cbs, NULL));
    }

    backlight_on();

    ESP_LOGI(TAG, "panel ready (ST7121, native %dx%d portrait)", TAB5_LCD_H_RES, TAB5_LCD_V_RES);
}

/* ---------- ARGB8888 -> RGB565 + 90-degree rotation blit --------------- */

static inline uint16_t
argb8888_to_rgb565(uint32_t argb)
{
    uint8_t r = (uint8_t) (argb >> 16);
    uint8_t g = (uint8_t) (argb >> 8);
    uint8_t b = (uint8_t) argb;

    return (uint16_t) (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

/*
 * Guest (landscape, LCD_WIDTH x LCD_HEIGHT) -> panel (portrait native,
 * TAB5_LCD_H_RES x TAB5_LCD_V_RES): panel_x = guest_y, panel_y = guest_x -
 * a plain transpose, no axis reversal. Originally copied (inverted) from
 * tiny386's touch driver on the theory that touch was the "known good"
 * reference - but the first real hardware test (2026-07-27) showed this
 * produced mirrored (backwards) text, and a dedicated test pattern
 * (esp32_video_test_pattern(), which pushes straight into the native
 * buffer with no reversal at all) came out correctly oriented on the
 * physical panel. So tiny386's touch transform was itself apparently
 * already wrong for this display's actual orientation - see the matching
 * fix in esp32_input.c's touch_poll().
 */
static void
esp32_blit(int x, int y, int w, int h, int monitor_index)
{
    /* Liveness/diagnostic log: confirms the core is actually calling the
     * blit callback at all (vs. e.g. the blit thread never getting
     * spawned/signaled), and with what values, without flooding the
     * console at CGA/VGA redraw rates. */
    static int call_count = 0;
    if (call_count < 5 || (call_count % 200) == 0)
        ESP_LOGI(TAG, "esp32_blit #%d: mon=%d x=%d y=%d w=%d h=%d panel=%p rot_buf=%p",
                 call_count, monitor_index, x, y, w, h, (void *) panel, (void *) rot_buf);
    call_count++;

    if (monitor_index != 0 || !panel || !rot_buf) {
        video_blit_complete_monitor(monitor_index);
        return;
    }

    /* Clip to the logical landscape canvas - guest modes larger than
     * 1280x720 aren't scaled down yet, just clipped. */
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > LCD_WIDTH)
        w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT)
        h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) {
        video_blit_complete_monitor(monitor_index);
        return;
    }

    const bitmap_t *src = monitors[monitor_index].target_buffer;

    /* First real hardware test (2026-07-27) confirmed a plain, no-flip
     * transpose was correct for orientation *at the time* - but that test
     * ran with the panel in its GS-mirrored MADCTL state (since removed,
     * see the file header comment: GS corrupts pixel data in groups of 4
     * on this panel, confirmed independently on tiny386's identical
     * ST7121/Tab5 bring-up). Switching to SS-only (the clean state)
     * rotates the whole image 180 degrees relative to before, so both
     * axes are now reversed here to cancel that out entirely - matching
     * tiny386's fix for the exact same SS-bit behavior (reverse pixel
     * order within each native row, and feed rows in reversed order). */
    for (int row = 0; row < h; row++) {
        const uint32_t *src_row  = &src->line[y + row][x];
        int             dst_col  = (h - 1) - row; /* reversed: cancels SS-bit 180deg rotation */
        for (int col = 0; col < w; col++) {
            int dst_row               = (w - 1) - col; /* reversed: cancels SS-bit 180deg rotation */
            rot_buf[dst_row * h + dst_col] = argb8888_to_rgb565(src_row[col]);
        }
    }

    video_blit_complete_monitor(monitor_index);

    /* Tearing fix - see the comment on refresh_done_sem above. Drain any
     * stale signal (blits are far rarer than refreshes, so one is almost
     * always already pending) then wait for a fresh one, so the write
     * below starts right after a scan just completed. Short timeout as a
     * safety net (never block forever if the callback never fires for
     * some reason) rather than a hard requirement - better to draw
     * slightly torn than to hang the emulator. */
    if (refresh_done_sem) {
        xSemaphoreTake(refresh_done_sem, 0);
        xSemaphoreTake(refresh_done_sem, pdMS_TO_TICKS(50));
    }

    int panel_x0 = y;
    int panel_y0 = x;
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, panel_x0, panel_y0,
                                               panel_x0 + h, panel_y0 + w, rot_buf));
}

void
esp32_video_init(void)
{
    rot_buf = heap_caps_malloc((size_t) LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rot_buf)
        ESP_LOGE(TAG, "failed to allocate rotation scratch buffer");

    esp32_panel_init();

    video_setblit(esp32_blit);
}

/* Diagnostic: push a fixed test pattern straight to the panel, bypassing
 * 86Box's target_buffer/blit-thread/CPU emulation entirely - isolates
 * "does the display path (panel init, DSI push, buffer) work at all"
 * from "does the emulator ever ask for a redraw", since those two things
 * have been conflated for every real-hardware test so far. Four solid
 * quadrants (distinct colors) in the panel's own native portrait
 * orientation - rot_buf is exactly TAB5_LCD_H_RES*TAB5_LCD_V_RES pixels
 * (same total as LCD_WIDTH*LCD_HEIGHT, dimensions swapped), so a single
 * full-panel push covers it with no rotation math needed. */
void
esp32_video_test_pattern(void)
{
    if (!panel || !rot_buf) {
        ESP_LOGE(TAG, "test pattern: panel or rot_buf not ready");
        return;
    }

    static const uint16_t RED   = 0xF800;
    static const uint16_t GREEN = 0x07E0;
    static const uint16_t BLUE  = 0x001F;
    static const uint16_t WHITE = 0xFFFF;

    for (int py = 0; py < TAB5_LCD_V_RES; py++) {
        uint16_t left_color  = (py < TAB5_LCD_V_RES / 2) ? RED : BLUE;
        uint16_t right_color = (py < TAB5_LCD_V_RES / 2) ? GREEN : WHITE;
        for (int px = 0; px < TAB5_LCD_H_RES; px++)
            rot_buf[py * TAB5_LCD_H_RES + px] = (px < TAB5_LCD_H_RES / 2) ? left_color : right_color;
    }

    ESP_LOGI(TAG, "test pattern: pushing full-panel quadrant pattern");
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, TAB5_LCD_H_RES, TAB5_LCD_V_RES, rot_buf));
    ESP_LOGI(TAG, "test pattern: push complete");
}
