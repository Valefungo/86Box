/*
 * esp32_input.c - keyboard/mouse/touch input, pushed into the core.
 *
 * M3 step 3 (real input): two sources are wired up here -
 *
 *  - Touch (ST7123 integrated touch controller on the system I2C bus,
 *    shared with esp32_video.c's IO-expanders/display via the exposed
 *    `esp32_sys_i2c_handle`) -> relative-motion mouse, with tap-to-click
 *    and two-finger-tap-to-right-click, same design as tiny386's
 *    lcd_tab5.c touch_poll() (/home/fungostar/tiny386/esp/main/lcd_tab5.c)
 *    but driven through 86Box's mouse_scale()/mouse_set_buttons_ex()
 *    instead of tiny386's ps2_mouse_event().
 *  - The physical Tab5 I2C keyboard accessory, via esp32_tab5_kbd.cpp
 *    (a separate .cpp file since the M5Stack keyboard component is
 *    C++ - see that file for why it's a second I2C bus, not the shared
 *    one). Wrapped in HID mode (one regular key + modifier bitmask per
 *    event, like tiny386's kbd_tab5.cpp) rather than the module's
 *    Normal Mode (full row/col matrix, one claim per physical key) -
 *    simpler, and the same design tiny386 already boot-tested on real
 *    Tab5 hardware; the known limitation is that only one non-modifier
 *    key can be tracked "held" at a time.
 *
 * USB HID (external USB-A keyboard/mouse) is NOT implemented yet: it
 * needs the ESP-IDF `usb_host` component (usb/usb_host.h), which is not
 * present in this ESP-IDF installation and isn't fetchable here (no
 * network access to the component registry in this environment). See
 * the reference implementation this would be adapted from at
 * /home/fungostar/M5Tab-Macintosh/src/basilisk/input_esp32.cpp
 * (MultiDeviceUsbHost class - plain ESP-IDF C API, not Arduino-specific,
 * so it ports cleanly once the component is available). Do not claim
 * this is done without a real build proving `usb_host.h` resolves.
 */
#include <stdbool.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_st7123.h"

#include <86box/86box.h>
#include <86box/keyboard.h>
#include <86box/mouse.h>

static const char *TAG = "esp32_input";

/* Owned by esp32_video.c, created once there; touch lives on the same
 * physical bus as the IO-expanders/display. */
extern i2c_master_bus_handle_t esp32_sys_i2c_handle;

/* Must match esp32_video.c's logical landscape canvas size - touch
 * coordinates are rotated into the same space the guest video blit
 * uses, not the panel's native portrait resolution. */
#define LCD_WIDTH  1280
#define TAB5_TOUCH_INT 23

#define TAP_MAX_MS 300
#define TAP_MAX_PX 10

extern void esp32_tab5_kbd_init(void);
extern void esp32_tab5_kbd_poll(void);

static esp_lcd_touch_handle_t touch;

static esp_err_t
touch_init(void)
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = {
        .dev_addr = 0x55,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 16,
        .lcd_param_bits = 0,
        .flags.disable_control_phase = 1,
        .scl_speed_hz = 100000,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(esp32_sys_i2c_handle, &tp_io_cfg, &io_handle),
                         TAG, "touch I2C IO init failed");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 720,  /* panel native H (portrait), see esp32_video.c */
        .y_max = 1280, /* panel native V (portrait) */
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = (gpio_num_t) TAB5_TOUCH_INT,
    };

    esp_err_t err = esp_lcd_touch_new_i2c_st7123(io_handle, &tp_cfg, &touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ST7123 touch init failed: %s", esp_err_to_name(err));
        esp_lcd_panel_io_del(io_handle);
        return err;
    }

    ESP_LOGI(TAG, "touch ready (ST7123)");
    return ESP_OK;
}

/*
 * Native portrait touch (px,py) -> logical landscape (lx,ly): a plain
 * transpose (lx=py, ly=px), matching esp32_video.c's corrected blit
 * transform. Originally copied from tiny386's touch driver on the
 * assumption it was already correct, but the first real hardware test
 * (2026-07-27) showed the *display* half of that assumption was wrong
 * (mirrored text) - since this touch transform was defined as the
 * inverse of that same wrong display transform, it carries the same
 * axis-reversal error and needs the matching fix. Not yet independently
 * verified against real touch input on hardware.
 */
static void
touch_poll(void)
{
    static bool    was_touching;
    static int16_t prev_x, prev_y;
    static TickType_t touch_start_tick;
    static int16_t start_x, start_y;
    static bool    was_two_finger;

    if (!touch)
        return;

    esp_lcd_touch_point_data_t pts[5];
    uint8_t cnt = 0;

    esp_lcd_touch_read_data(touch);
    if (esp_lcd_touch_get_data(touch, pts, &cnt, 5) != ESP_OK)
        cnt = 0;

    int16_t x0 = 0, y0 = 0;
    if (cnt >= 1) {
        x0 = (int16_t) pts[0].y;
        y0 = (int16_t) pts[0].x;
    }

    if (cnt >= 2)
        was_two_finger = true;

    if (cnt >= 1) {
        if (was_touching) {
            int dx = (int) x0 - (int) prev_x;
            int dy = (int) y0 - (int) prev_y;
            if (dx != 0 || dy != 0)
                mouse_scale(dx, dy);
        } else {
            touch_start_tick = xTaskGetTickCount();
            start_x = x0;
            start_y = y0;
            was_two_finger = (cnt >= 2);
        }
        prev_x = x0;
        prev_y = y0;
        was_touching = true;
    } else if (was_touching) {
        TickType_t dur    = xTaskGetTickCount() - touch_start_tick;
        int        move_x = abs((int) prev_x - (int) start_x);
        int        move_y = abs((int) prev_y - (int) start_y);

        if (pdTICKS_TO_MS(dur) < TAP_MAX_MS && move_x < TAP_MAX_PX && move_y < TAP_MAX_PX) {
            int button = was_two_finger ? 2 : 1;
            mouse_set_buttons_ex(button);
            mouse_set_buttons_ex(0);
        }
        was_touching   = false;
        was_two_finger = false;
    }
}

void
esp32_input_init(void)
{
    esp_err_t err = touch_init();
    if (err != ESP_OK)
        ESP_LOGW(TAG, "continuing without touch input");

    esp32_tab5_kbd_init();
}

void
esp32_input_poll(void)
{
    touch_poll();
    esp32_tab5_kbd_poll();
}
