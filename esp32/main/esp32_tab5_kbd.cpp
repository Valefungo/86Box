/*
 * esp32_tab5_kbd.cpp - wrapper around the M5Stack Tab5 external I2C
 * keyboard (Ext.Port1 add-on, 70-key matrix), translating its HID-mode
 * events into 86Box's keyboard_input(down, xt_scancode) calls.
 *
 * Pin/port values (TAB5_KBD_SDA/SCL/PORT/INT below) are the M5Stack Tab5
 * hardware wiring for this accessory, taken from tiny386's
 * board_tab5.h (/home/fungostar/tiny386/esp/main/board_tab5.h) - note
 * this keyboard sits on I2C port 1 (pins 0/1), a *different* physical
 * bus than the system I2C port 0 (pins 31/32) that esp32_video.c uses
 * for the IO-expanders/display/touch. The m5_tab5_keyboard_component
 * creates and owns this second bus itself (the begin() overload below
 * takes a raw port+pins, not an existing bus handle).
 *
 * HID mode only reports a single "current" regular key plus a modifier
 * bitmask per I2C event, and signals "all keys released" with
 * hid_key_code==0 (it never reports which specific key went up) - so
 * this wrapper tracks the previously-held key/modifier state itself and
 * synthesizes the missing up/down transitions by diffing against each
 * new report, exactly like tiny386's kbd_tab5.cpp (which does the same
 * diffing, just emitting ps2_put_keycode()+Linux KEY_* codes for
 * tiny386's own PS/2 keyboard model instead of 86Box's XT one).
 */
#include "esp_attr.h"
#include "m5_tab5_keyboard.h"

#include "esp32_hid_xt_table.h"

extern "C" {
#include <stdint.h>
void keyboard_input(int down, uint16_t scan);
}

#define TAB5_KBD_SDA  0
#define TAB5_KBD_SCL  1
#define TAB5_KBD_PORT 1
#define TAB5_KBD_INT  50

static EXT_RAM_BSS_ATTR m5::M5Tab5Keyboard kbd;

static volatile uint8_t s_modifier;
static volatile uint8_t s_keycode;
static volatile int     s_ready;

static void
on_key(m5_tab5_key_event_t ev, void *)
{
    /* HID mode never sets ev.pressed; hid_key_code==0 means "no key
     * currently held" (release-all), not which specific key went up. */
    s_modifier = ev.hid_modifier;
    s_keycode  = ev.hid_key_code;
    s_ready    = 1;
}

extern "C" void
esp32_tab5_kbd_init(void)
{
    kbd.begin((i2c_port_t) TAB5_KBD_PORT, M5_TAB5_KB_DEFAULT_ADDR,
              TAB5_KBD_SDA, TAB5_KBD_SCL, M5_TAB5_KB_I2C_FREQ_100K,
              TAB5_KBD_INT, M5_TAB5_KB_INT_MODE_HARDWARE);
    kbd.enableHIDMode(on_key, nullptr);
}

extern "C" void
esp32_tab5_kbd_poll(void)
{
    static uint8_t held_modifier;
    static uint8_t held_keycode;

    if (!s_ready)
        return;
    s_ready = 0;

    uint8_t new_modifier = s_modifier;
    uint8_t new_keycode  = s_keycode;

    /* Diff the modifier bitmask, emit an XT down/up for whatever changed. */
    for (int i = 0; i < 8; i++) {
        uint8_t bit = (uint8_t) (1 << i);
        if ((new_modifier & bit) != (held_modifier & bit))
            keyboard_input((new_modifier & bit) != 0, hid_modifier_to_xt[i]);
    }
    held_modifier = new_modifier;

    if (new_keycode != held_keycode) {
        uint16_t old_xt = held_keycode ? hid_usage_to_xt[held_keycode] : 0;
        uint16_t new_xt = new_keycode ? hid_usage_to_xt[new_keycode] : 0;
        if (held_keycode != 0 && old_xt)
            keyboard_input(0, old_xt);
        if (new_keycode != 0 && new_xt)
            keyboard_input(1, new_xt);
        held_keycode = new_keycode;
    }
}
